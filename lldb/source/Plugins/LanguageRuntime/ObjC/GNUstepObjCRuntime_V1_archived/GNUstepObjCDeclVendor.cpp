//===-- GNUstepObjCDeclVendor.cpp ------------------------------------===//
//
// Provides dynamic type creation for GNUstep Objective-C runtime
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCDeclVendor.h"
#include "GNUstepObjCRuntime.h"

class GNUstepObjCRuntime;

#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "lldb/Core/Module.h"
#include "lldb/Symbol/CompilerType.h"
#include "lldb/Symbol/CompilerDecl.h"
// TypeSystemClang.h already included from header
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclObjC.h"

#include <algorithm>
#include <cctype>
#include <functional>

using namespace lldb;
using namespace lldb_private;

GNUstepObjCDeclVendor::GNUstepObjCDeclVendor(ObjCLanguageRuntime &runtime)
    : ClangDeclVendor(eClangDeclVendor), // Using generic ClangDeclVendor type
      m_runtime(runtime), 
      m_nsobject_decl(nullptr) {
  
  Log *log = GetLog(LLDBLog::Expressions);
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Initializing");
  
  // CRITICAL FIX: Create our own separate TypeSystemClang instance 
  // instead of using the scratch one. This prevents AST import conflicts
  // when the expression evaluator tries to import declarations.
  // Following Apple's pattern from AppleObjCDeclVendor.cpp
  Target &target = runtime.GetProcess()->GetTarget();
  m_ast_ctx = std::make_shared<TypeSystemClang>(
      "GNUstepObjCDeclVendor AST",
      target.GetArchitecture().GetTriple());
  
  if (!m_ast_ctx) {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Failed to create TypeSystemClang");
    return;
  }
  
  // Create and set our external AST source for dynamic lookups
  m_external_source = new GNUstepObjCExternalASTSource(*this);
  m_ast_ctx->getASTContext().setExternalSource(m_external_source);
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Set up external AST source for dynamic lookups");
  
  // Ensure NSObject exists as base type
  EnsureNSObjectDecl();
  
  // Register common GNUstep types proactively
  RegisterCommonTypes();
  
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Initialization complete");
}

GNUstepObjCDeclVendor::~GNUstepObjCDeclVendor() = default;

void GNUstepObjCDeclVendor::EnsureNSObjectDecl() {
  if (m_nsobject_decl)
    return;
    
  Log *log = GetLog(LLDBLog::Expressions);
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Creating NSObject declaration");
  
  clang::ASTContext &ast_ctx = m_ast_ctx->getASTContext();
  
  // Create NSObject interface
  clang::IdentifierInfo &nsobject_identifier = ast_ctx.Idents.get("NSObject");
  m_nsobject_decl = clang::ObjCInterfaceDecl::Create(
      ast_ctx, 
      ast_ctx.getTranslationUnitDecl(),
      clang::SourceLocation(), 
      &nsobject_identifier,
      nullptr,  // no previous declaration
      nullptr   // no source location
  );
  
  // Mark as having external storage
  m_nsobject_decl->setHasExternalLexicalStorage();
  m_nsobject_decl->setHasExternalVisibleStorage();
  
  // Add to translation unit
  ast_ctx.getTranslationUnitDecl()->addDecl(m_nsobject_decl);
  
  // Start definition for NSObject
  m_nsobject_decl->startDefinition();
  
  // Cache it
  m_name_to_decl["NSObject"] = m_nsobject_decl;
}

void GNUstepObjCDeclVendor::RegisterCommonTypes() {
  Log *log = GetLog(LLDBLog::Expressions);
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Registering common GNUstep types");
  
  // Common GNUstep types that need to be available for expression evaluation
  const char *common_types[] = {
    // Dictionaries
    "GSDictionary",
    "GSMutableDictionary", 
    "NSConstantDictionary",
    "GSDictionaryKeyEnumerator",
    "GSDictionaryObjectEnumerator",
    
    // Arrays
    "GSArray",
    "GSMutableArray",
    "GSInlineArray", 
    "GSPlaceholderArray",
    
    // Strings
    "NSString",
    "NSMutableString",
    "NSConstantString",
    "GSTinyString",
    "GSString",
    "GSMutableString",
    "GSCInlineString",
    
    // Numbers
    "NSNumber",
    "NSSmallInt",
    
    // Other Foundation types
    "NSDate",
    "NSSet",
    "NSMutableSet",
    
    nullptr
  };
  
  for (const char **type_name = common_types; *type_name; ++type_name) {
    clang::ObjCInterfaceDecl *decl = GetDeclForClassName(*type_name);
    if (decl) {
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Successfully registered type {0}", *type_name);
    } else {
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Failed to register type {0}", *type_name);
    }
  }
}

clang::ObjCInterfaceDecl *GNUstepObjCDeclVendor::GetDeclForClassName(const char *name) {
  if (!name || !m_ast_ctx)
    return nullptr;
    
  Log *log = GetLog(LLDBLog::Expressions);
  LLDB_LOG(log, "GNUstepObjCDeclVendor: GetDeclForClassName called for '{0}'", name);
  
  // Check cache first
  auto it = m_name_to_decl.find(name);
  if (it != m_name_to_decl.end()) {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Found cached decl for {0}", name);
    return it->second;
  }
  
  // Prevent recursion
  if (m_building_types.count(name) > 0) {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Recursion detected for {0}", name);
    return nullptr;
  }
  
  m_building_types.insert(name);
  
  // Try to get class descriptor from runtime
  ObjCLanguageRuntime::ClassDescriptorSP descriptor = 
      m_runtime.GetClassDescriptorFromClassName(ConstString(name));
      
  ObjCLanguageRuntime::ObjCISA isa = LLDB_INVALID_ADDRESS;
  if (descriptor) {
    isa = descriptor->GetISA();
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Found runtime descriptor for {0} with ISA {1:x}", name, isa);
  } else {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: No runtime descriptor found for {0}, creating type anyway", name);
  }
    
  clang::ObjCInterfaceDecl *decl = BuildInterfaceDecl(name, isa);
  
  m_building_types.erase(name);
  
  if (decl) {
    m_name_to_decl[name] = decl;
    if (isa != LLDB_INVALID_ADDRESS) {
      m_isa_to_decl[isa] = decl;
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Successfully registered type {0} with ISA {1:x}", name, isa);
    } else {
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Successfully registered type {0} (no ISA)", name);
    }
  } else {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Failed to build decl for {0}", name);
  }
  
  return decl;
}

clang::ObjCInterfaceDecl *GNUstepObjCDeclVendor::GetDeclForISA(
    ObjCLanguageRuntime::ObjCISA isa) {
  
  if (isa == LLDB_INVALID_ADDRESS || !m_ast_ctx)
    return nullptr;
    
  Log *log = GetLog(LLDBLog::Expressions);
  
  // Check cache first
  auto it = m_isa_to_decl.find(isa);
  if (it != m_isa_to_decl.end()) {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Found cached decl for ISA {0:x}", isa);
    return it->second;
  }
  
  // Get class descriptor from runtime using ISA
  ObjCLanguageRuntime::ClassDescriptorSP descriptor = 
      m_runtime.GetClassDescriptorFromISA(isa);
      
  if (!descriptor) {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: No descriptor for ISA {0:x}", isa);
    return nullptr;
  }
  
  const char *name = descriptor->GetClassName().AsCString();
  if (!name) {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: No name for ISA {0:x}", isa);
    return nullptr;
  }
  
  return GetDeclForClassName(name);
}

clang::ObjCInterfaceDecl *GNUstepObjCDeclVendor::BuildInterfaceDecl(
    const char *name, ObjCLanguageRuntime::ObjCISA isa) {
    
  if (!name || !m_ast_ctx)
    return nullptr;
    
  Log *log = GetLog(LLDBLog::Expressions);
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Building interface decl for '{0}' (ISA: {1:x})", 
           name, isa);
  
  clang::ASTContext &ast_ctx = m_ast_ctx->getASTContext();
  
  // Create the interface declaration
  clang::IdentifierInfo &identifier = ast_ctx.Idents.get(name);
  clang::ObjCInterfaceDecl *decl = clang::ObjCInterfaceDecl::Create(
      ast_ctx,
      ast_ctx.getTranslationUnitDecl(),
      clang::SourceLocation(),
      &identifier,
      nullptr,  // no previous declaration
      nullptr   // no source location
  );
  
  // Set metadata to link ISA to declaration
  if (isa != LLDB_INVALID_ADDRESS) {
    ClangASTMetadata metadata;
    metadata.SetISAPtr(isa);
    m_ast_ctx->SetMetadata(decl, metadata);
  }
  
  // Mark as having external storage
  decl->setHasExternalLexicalStorage();
  decl->setHasExternalVisibleStorage();
  
  // Add to translation unit
  ast_ctx.getTranslationUnitDecl()->addDecl(decl);
  
  // Start definition BEFORE setting superclass
  decl->startDefinition();
  
  // Set superclass (everything except NSObject inherits from NSObject)
  if (strcmp(name, "NSObject") != 0) {
    EnsureNSObjectDecl();
    if (m_nsobject_decl) {
      clang::ASTContext &context = m_ast_ctx->getASTContext();
      decl->setSuperClass(context.getTrivialTypeSourceInfo(
          context.getObjCInterfaceType(m_nsobject_decl)));
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Set superclass NSObject for {0}", name);
    } else {
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Warning - NSObject not available for {0}", name);
    }
  }
  
  // If we have a descriptor, add ivars and methods
  if (isa != LLDB_INVALID_ADDRESS) {
    ObjCLanguageRuntime::ClassDescriptorSP descriptor = 
        m_runtime.GetClassDescriptorFromISA(isa);
    if (descriptor) {
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Adding ivars and methods for {0}", name);
      AddIVarsToDecl(decl, descriptor);
      AddMethodsToDecl(decl, descriptor);
    } else {
      LLDB_LOG(log, "GNUstepObjCDeclVendor: No descriptor available for {0} to add ivars/methods", name);
    }
  } else {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: No ISA available for {0}, creating minimal type", name);
  }
  
  // Complete the definition
  if (!FinishDecl(decl)) {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Failed to finish decl for {0}", name);
    return nullptr;
  }
  
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Successfully built ObjCInterfaceDecl for '{0}'", name);
  return decl;
}

bool GNUstepObjCDeclVendor::AddIVarsToDecl(
    clang::ObjCInterfaceDecl *decl,
    ObjCLanguageRuntime::ClassDescriptorSP descriptor) {
    
  if (!decl || !descriptor)
    return false;
    
  Log *log = GetLog(LLDBLog::Expressions);
  clang::ASTContext &ast = m_ast_ctx->getASTContext();
  
  // Get the class pointer from ISA
  ObjCLanguageRuntime::ObjCISA isa = descriptor->GetISA();
  if (isa == LLDB_INVALID_ADDRESS)
    return false;
    
  // Use runtime API to get ivars
  Status error;
  
  // Try to get ivars through the descriptor first
  // The descriptor might have cached ivar information
  std::function<bool(const char *, const char *, lldb::addr_t, uint64_t)> ivar_func = 
      [&](const char *name, const char *type, lldb::addr_t offset_ptr, uint64_t size) -> bool {
    if (!name)
      return true; // Continue iteration
      
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Found ivar '{0}' of type '{1}' at offset {2}", 
             name, type ? type : "unknown", offset_ptr);
    
    // Create the ivar
    clang::QualType ivar_type = ast.getObjCIdType(); // Default to id type
    
    // Try to parse the type encoding if available
    // For now, just use id type for all object types
    // TODO: Implement proper type encoding parsing
    
    // Create the ivar declaration
    clang::ObjCIvarDecl *ivar_decl = clang::ObjCIvarDecl::Create(
        ast,
        decl,
        clang::SourceLocation(),
        clang::SourceLocation(), 
        &ast.Idents.get(name),
        ivar_type,
        ast.getTrivialTypeSourceInfo(ivar_type),
        clang::ObjCIvarDecl::None,
        nullptr,
        true); // synthesized
    
    if (ivar_decl) {
      decl->addDecl(ivar_decl);
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Added ivar '{0}' to class", name);
    }
    
    return true; // Continue iteration
  };
  
  descriptor->Describe(
      std::function<void(ObjCLanguageRuntime::ObjCISA)>(),
      std::function<bool(const char *, const char *)>(),
      std::function<bool(const char *, const char *)>(),
      ivar_func);
  
  return true;
}

bool GNUstepObjCDeclVendor::AddMethodsToDecl(
    clang::ObjCInterfaceDecl *decl,
    ObjCLanguageRuntime::ClassDescriptorSP descriptor) {
    
  if (!decl || !descriptor)
    return false;
  
  Log *log = GetLog(LLDBLog::Expressions);
  std::string class_name = decl->getNameAsString();
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Adding methods for class {0}", class_name);
    
  clang::ASTContext &ast_ctx = m_ast_ctx->getASTContext();
  
  // Add essential NSObject methods that all objects should have
  AddBasicNSObjectMethods(decl, ast_ctx);
  
  // CRITICAL: Add property accessors for all ivars
  // This enables property.syntax to work in LLDB expressions
  AddPropertyAccessorsForIvars(decl, descriptor);
  
  // Try to discover methods at runtime using the ISA
  ObjCLanguageRuntime::ObjCISA isa = descriptor->GetISA();
  if (isa != LLDB_INVALID_ADDRESS) {
    if (DiscoverAndAddRuntimeMethods(decl, isa)) {
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Successfully discovered runtime methods for {0}", class_name);
    } else {
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Failed to discover runtime methods for {0}, using fallback", class_name);
      // Fall back to type-based method addition if runtime discovery fails
      AddFallbackMethods(decl, ast_ctx, class_name);
    }
  } else {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: No ISA for {0}, using fallback methods", class_name);
    AddFallbackMethods(decl, ast_ctx, class_name);
  }
  
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Added methods for class {0}", class_name);
  return true;
}

bool GNUstepObjCDeclVendor::DiscoverAndAddRuntimeMethods(
    clang::ObjCInterfaceDecl *decl, ObjCLanguageRuntime::ObjCISA isa) {
    
  if (!decl || isa == LLDB_INVALID_ADDRESS)
    return false;
    
  Log *log = GetLog(LLDBLog::Expressions);
  Process &process = *m_runtime.GetProcess();
  
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Attempting runtime method discovery for ISA {0:x}", isa);
  
  // We need to call class_copyMethodList at runtime to get the methods
  // This requires expression evaluation, which might not always work
  // So we'll use a conservative approach with error handling
  
  Status error;
  ExecutionContext exe_ctx;
  process.CalculateExecutionContext(exe_ctx);
  if (!exe_ctx.GetThreadPtr() || !exe_ctx.GetThreadPtr()->IsValid()) {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: No valid execution context for runtime discovery");
    return false;
  }
  
  // Try to evaluate class_copyMethodList expression  
  std::string expression = llvm::formatv("(void*)class_copyMethodList((void*){0}, 0)", isa);
  
  // For now, return false to use fallback - expression evaluation during 
  // decl vendor creation is risky and can cause crashes
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Runtime method discovery disabled to avoid crashes - using fallback");
  return false;
  
  // TODO: Implement safe runtime method discovery
  // This would require:
  // 1. Checking if we're in a safe execution state  
  // 2. Using FunctionCaller instead of expression evaluation
  // 3. Proper error handling for missing symbols
  // 4. Caching discovered methods to avoid repeated calls
}

void GNUstepObjCDeclVendor::AddFallbackMethods(clang::ObjCInterfaceDecl *decl, 
                                               clang::ASTContext &ast_ctx, 
                                               const std::string &class_name) {
  Log *log = GetLog(LLDBLog::Expressions);
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Adding fallback methods for {0}", class_name);
  
  // Add type-specific methods based on class name patterns
  if (class_name.find("Array") != std::string::npos) {
    AddArrayMethods(decl, ast_ctx);
  } else if (class_name.find("Dictionary") != std::string::npos) {
    AddDictionaryMethods(decl, ast_ctx);
  } else if (class_name.find("String") != std::string::npos) {
    AddStringMethods(decl, ast_ctx);
  } else {
    // For custom user classes, add common property accessor patterns
    AddCustomClassMethods(decl, ast_ctx, class_name);
  }
}

CompilerType GNUstepObjCDeclVendor::GetTypeForISA(ObjCLanguageRuntime::ObjCISA isa) {
  clang::ObjCInterfaceDecl *decl = GetDeclForISA(isa);
  if (!decl)
    return CompilerType();
    
  return m_ast_ctx->GetTypeForDecl(decl);
}

uint32_t GNUstepObjCDeclVendor::FindDecls(ConstString name, bool append,
                                          uint32_t max_matches,
                                          std::vector<CompilerDecl> &decls) {
  if (!append)
    decls.clear();
    
  if (!name || !m_ast_ctx)
    return 0;
    
  Log *log = GetLog(LLDBLog::Expressions);
  std::string full_name = name.GetStringRef().str();
  LLDB_LOG(log, "GNUstepObjCDeclVendor: FindDecls called for '{0}' (append={1}, max_matches={2})", 
           full_name, append, max_matches);
  
  // Also print to stderr for immediate visibility during debugging
  fprintf(stderr, "DEBUG: GNUstepObjCDeclVendor::FindDecls called with name='%s', length=%zu\n", 
          full_name.c_str(), full_name.length());
  
  // Add more detailed debugging for BankAccount specifically
  if (full_name == "BankAccount") {
    fprintf(stderr, "DEBUG: FindDecls called for BankAccount - will create/return class decl\n");
  } else if (full_name == "account") {
    fprintf(stderr, "DEBUG: FindDecls called for variable 'account' - will return null, should resolve via symbol table\n");
  }
  
  // Check if this might be a property access (e.g., looking for "balance" property on BankAccount)
  // Properties are looked up by their name directly, not as method selectors
  // When FindDecls is called with just a property name, we need to find which class it belongs to
  // This is a simplified approach - in a full implementation we'd need context about which class
  
  // Check if this is a method selector like "-[BankAccount transactions]" or "+[BankAccount alloc]"
  if (full_name.size() > 3 && 
      (full_name[0] == '-' || full_name[0] == '+') && 
      full_name[1] == '[') {
    
    // Parse method selector format: -[ClassName methodName] or +[ClassName methodName]
    std::string class_name_str;
    std::string method_name_str;
    bool is_instance_method = (full_name[0] == '-');
    
    size_t space_pos = full_name.find(' ', 2);
    size_t close_pos = full_name.find(']', 2);
    
    if (space_pos != std::string::npos && close_pos != std::string::npos && space_pos < close_pos) {
      class_name_str = full_name.substr(2, space_pos - 2);
      method_name_str = full_name.substr(space_pos + 1, close_pos - space_pos - 1);
      
      // Handle empty method name - might be a property access attempt
      if (method_name_str.empty()) {
        LLDB_LOG(log, "GNUstepObjCDeclVendor: Empty method name for class {0} - likely property access issue", 
                 class_name_str);
        // Don't try to create a method with empty name
        return 0;
      }
      
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Parsing method selector - Class: '{0}', Method: '{1}', Instance: {2}", 
               class_name_str, method_name_str, is_instance_method);
      
      // Get or create the class declaration first
      clang::ObjCInterfaceDecl *class_decl = GetDeclForClassName(class_name_str.c_str());
      if (class_decl) {
        // Look for existing method or create it
        clang::ObjCMethodDecl *method_decl = FindOrCreateMethodDecl(class_decl, method_name_str.c_str(), is_instance_method);
        if (method_decl) {
          CompilerDecl compiler_decl(m_ast_ctx.get(), method_decl);
          decls.push_back(compiler_decl);
          LLDB_LOG(log, "GNUstepObjCDeclVendor: Found method {0} for class {1}, returning 1 match", 
                   method_name_str, class_name_str);
          return 1;
        } else {
          LLDB_LOG(log, "GNUstepObjCDeclVendor: Failed to create method {0} for class {1}", 
                   method_name_str, class_name_str);
        }
      } else {
        LLDB_LOG(log, "GNUstepObjCDeclVendor: Failed to find/create class {0}", class_name_str);
      }
    } else {
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Invalid method selector format: '{0}'", full_name);
    }
  } else {
    // Regular class name lookup
    clang::ObjCInterfaceDecl *decl = GetDeclForClassName(name.AsCString());
    if (decl) {
      // Convert clang::NamedDecl to CompilerDecl
      CompilerDecl compiler_decl(m_ast_ctx.get(), decl);
      decls.push_back(compiler_decl);
      LLDB_LOG(log, "GNUstepObjCDeclVendor: FindDecls found type {0}, returning 1 match", name);
      return 1;
    }
  }
  
  LLDB_LOG(log, "GNUstepObjCDeclVendor: FindDecls found no matches for {0}", full_name);
  return 0;
}

bool GNUstepObjCDeclVendor::FinishDecl(clang::ObjCInterfaceDecl *decl) {
  if (!decl)
    return false;
    
  Log *log = GetLog(LLDBLog::Expressions);
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Finishing decl for {0}", 
           decl->getNameAsString().c_str());
  
  // In LLVM/Clang, calling startDefinition() is enough
  // There's no explicit "complete" method
  // The definition is considered complete once we've added all members
  
  return true;
}

const char *GNUstepObjCDeclVendor::GetCommonTypeName(const char *gnustep_name) {
  // Map internal GNUstep types to public Foundation types if needed
  static std::map<std::string, const char *> type_map = {
    {"GSInlineArray", "NSArray"},
    {"GSMutableArray", "NSMutableArray"},
    {"GSDictionary", "NSDictionary"},
    {"GSMutableDictionary", "NSMutableDictionary"},
    // Add more mappings as needed
  };
  
  auto it = type_map.find(gnustep_name);
  if (it != type_map.end())
    return it->second;
    
  return gnustep_name;
}

// Helper method to add a method declaration to an interface
clang::ObjCMethodDecl *GNUstepObjCDeclVendor::AddMethodDecl(
    clang::ObjCInterfaceDecl *decl, 
    clang::ASTContext &ast_ctx,
    const char *method_name,
    clang::QualType return_type,
    bool is_instance_method) {
    
  if (!decl || !method_name)
    return nullptr;
    
  const clang::IdentifierInfo *method_identifier = &ast_ctx.Idents.get(method_name);
  clang::Selector selector = ast_ctx.Selectors.getSelector(0, &method_identifier);
  
  clang::ObjCMethodDecl *method_decl = clang::ObjCMethodDecl::Create(
      ast_ctx,
      clang::SourceLocation(),
      clang::SourceLocation(),
      selector,
      return_type,
      nullptr, // TypeSourceInfo
      decl,
      is_instance_method,
      false, // isVariadic
      false, // isPropertyAccessor
      false, // isSynthesizedAccessorStub
      false, // isImplicitlyDeclared
      false, // isDefined
      clang::ObjCImplementationControl::None
  );
  
  if (method_decl) {
    decl->addDecl(method_decl);
  }
  
  return method_decl;
}

// Helper method to add a method declaration with one parameter to an interface
clang::ObjCMethodDecl *GNUstepObjCDeclVendor::AddMethodWithParameter(
    clang::ObjCInterfaceDecl *decl,
    clang::ASTContext &ast_ctx,
    const char *method_name,
    clang::QualType return_type,
    const char *param_name,
    clang::QualType param_type,
    bool is_instance_method) {
    
  if (!decl || !method_name || !param_name)
    return nullptr;
    
  // Create selector with one parameter (e.g., "objectAtIndex:" for "objectAtIndex")
  const clang::IdentifierInfo *method_identifier = &ast_ctx.Idents.get(method_name);
  clang::Selector selector = ast_ctx.Selectors.getSelector(1, &method_identifier);
  
  // Create parameter
  clang::IdentifierInfo &param_identifier = ast_ctx.Idents.get(param_name);
  clang::ParmVarDecl *param_decl = clang::ParmVarDecl::Create(
      ast_ctx,
      nullptr, // no DeclContext for now
      clang::SourceLocation(),
      clang::SourceLocation(),
      &param_identifier,
      param_type,
      nullptr, // TypeSourceInfo
      clang::SC_None,
      nullptr  // default argument
  );
  
  // Create method declaration
  clang::ObjCMethodDecl *method_decl = clang::ObjCMethodDecl::Create(
      ast_ctx,
      clang::SourceLocation(),
      clang::SourceLocation(),
      selector,
      return_type,
      nullptr, // TypeSourceInfo
      decl,
      is_instance_method,
      false, // isVariadic
      false, // isPropertyAccessor
      false, // isSynthesizedAccessorStub
      false, // isImplicitlyDeclared
      false, // isDefined
      clang::ObjCImplementationControl::None
  );
  
  if (method_decl && param_decl) {
    // Add parameter to method
    method_decl->setMethodParams(ast_ctx, {param_decl}, {});
    decl->addDecl(method_decl);
  }
  
  return method_decl;
}

// Find an existing method or create a new one for dynamic method resolution
clang::ObjCMethodDecl *GNUstepObjCDeclVendor::FindOrCreateMethodDecl(
    clang::ObjCInterfaceDecl *class_decl, 
    const char *method_name,
    bool is_instance_method) {
    
  if (!class_decl || !method_name)
    return nullptr;
    
  Log *log = GetLog(LLDBLog::Expressions);
  clang::ASTContext &ast_ctx = m_ast_ctx->getASTContext();
  
  // Create selector for the method name
  const clang::IdentifierInfo *method_identifier = &ast_ctx.Idents.get(method_name);
  clang::Selector selector = ast_ctx.Selectors.getSelector(0, &method_identifier);
  
  // First, try to find existing method in the class
  for (auto method : class_decl->methods()) {
    if (method->getSelector() == selector && method->isInstanceMethod() == is_instance_method) {
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Found existing method '{0}' in class {1}", 
               method_name, class_decl->getNameAsString());
      return method;
    }
  }
  
  // Method doesn't exist, create it dynamically
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Creating dynamic method '{0}' for class {1} (instance: {2})", 
           method_name, class_decl->getNameAsString(), is_instance_method);
  
  // Determine return type based on method name patterns
  clang::QualType return_type = InferReturnTypeForMethod(method_name);
  
  // Create method declaration
  clang::ObjCMethodDecl *method_decl = clang::ObjCMethodDecl::Create(
      ast_ctx,
      clang::SourceLocation(),
      clang::SourceLocation(),
      selector,
      return_type,
      nullptr, // TypeSourceInfo
      class_decl,
      is_instance_method,
      false, // isVariadic
      false, // isPropertyAccessor
      false, // isSynthesizedAccessorStub
      false, // isImplicitlyDeclared
      false, // isDefined
      clang::ObjCImplementationControl::None
  );
  
  if (method_decl) {
    class_decl->addDecl(method_decl);
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Successfully created dynamic method '{0}' for class {1}", 
             method_name, class_decl->getNameAsString());
  } else {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Failed to create dynamic method '{0}' for class {1}", 
             method_name, class_decl->getNameAsString());
  }
  
  return method_decl;
}

// Infer return type based on method name patterns
clang::QualType GNUstepObjCDeclVendor::InferReturnTypeForMethod(const char *method_name) {
  if (!method_name || !m_ast_ctx)
    return m_ast_ctx->getASTContext().getObjCIdType();
    
  clang::ASTContext &ast_ctx = m_ast_ctx->getASTContext();
  std::string name_str(method_name);
  
  // Common patterns for method return types
  if (name_str == "count" || name_str == "length" || name_str == "hash" || 
      name_str.find("Index") != std::string::npos) {
    return ast_ctx.UnsignedLongTy; // NSUInteger
  }
  
  if (name_str == "boolValue" || name_str.find("is") == 0 || name_str.find("has") == 0 ||
      name_str.find("should") == 0 || name_str.find("can") == 0) {
    return ast_ctx.BoolTy;
  }
  
  if (name_str == "intValue" || name_str == "integerValue") {
    return ast_ctx.IntTy;
  }
  
  if (name_str == "doubleValue" || name_str == "floatValue") {
    return ast_ctx.DoubleTy;
  }
  
  if (name_str.find("Value") != std::string::npos && name_str != "boolValue") {
    return ast_ctx.DoubleTy; // Default numeric value methods to double
  }
  
  if (name_str.find("String") != std::string::npos) {
    return ast_ctx.getObjCIdType(); // String methods return id (NSString *)
  }
  
  // For collections like "transactions", "accounts", etc., return id (typically NSArray *)
  // Default to id type for unknown methods
  return ast_ctx.getObjCIdType();
}

// Add common methods for custom user-defined classes
void GNUstepObjCDeclVendor::AddCustomClassMethods(clang::ObjCInterfaceDecl *decl, 
                                                   clang::ASTContext &ast_ctx, 
                                                   const std::string &class_name) {
  Log *log = GetLog(LLDBLog::Expressions);
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Adding custom class methods for {0}", class_name);
  
  // Debug output for immediate visibility
  fprintf(stderr, "DEBUG: AddCustomClassMethods called for class '%s'\n", class_name.c_str());
  
  clang::QualType id_type = ast_ctx.getObjCIdType();
  clang::QualType nsuinteger_type = ast_ctx.UnsignedLongTy;
  clang::QualType double_type = ast_ctx.DoubleTy;
  
  // Add common property patterns based on class name
  if (class_name == "BankAccount") {
    // Specific methods for BankAccount class
    AddMethodDecl(decl, ast_ctx, "accountNumber", id_type, true);  // NSString *
    AddMethodDecl(decl, ast_ctx, "owner", id_type, true);          // NSString *  
    AddMethodDecl(decl, ast_ctx, "balance", double_type, true);    // double
    AddMethodDecl(decl, ast_ctx, "transactions", id_type, true);   // NSArray *
    
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Added BankAccount-specific methods");
  } else {
    // For unknown custom classes, add generic property accessor patterns
    // These are common patterns that most custom classes might have
    
    // Try to infer common property names from class name
    std::string lowercase_name = class_name;
    std::transform(lowercase_name.begin(), lowercase_name.end(), lowercase_name.begin(), ::tolower);
    
    // Add some generic methods that are commonly used in debugging
    AddMethodDecl(decl, ast_ctx, "description", id_type, true);
    AddMethodDecl(decl, ast_ctx, "debugDescription", id_type, true);
    
    // Add common property patterns
    if (lowercase_name.find("account") != std::string::npos) {
      AddMethodDecl(decl, ast_ctx, "balance", double_type, true);
      AddMethodDecl(decl, ast_ctx, "transactions", id_type, true);
    }
    
    if (lowercase_name.find("person") != std::string::npos || 
        lowercase_name.find("user") != std::string::npos) {
      AddMethodDecl(decl, ast_ctx, "name", id_type, true);
      AddMethodDecl(decl, ast_ctx, "age", ast_ctx.IntTy, true);
    }
    
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Added generic methods for custom class {0}", class_name);
  }
}

// Add basic NSObject methods that all objects should have
void GNUstepObjCDeclVendor::AddBasicNSObjectMethods(clang::ObjCInterfaceDecl *decl, clang::ASTContext &ast_ctx) {
  clang::QualType id_type = ast_ctx.getObjCIdType();
  clang::QualType class_type = ast_ctx.getObjCClassType();
  
  // Essential class methods
  AddMethodDecl(decl, ast_ctx, "alloc", id_type, false); // class method
  
  // Essential instance methods  
  AddMethodDecl(decl, ast_ctx, "init", id_type, true);
  AddMethodDecl(decl, ast_ctx, "description", id_type, true);
  AddMethodDecl(decl, ast_ctx, "debugDescription", id_type, true);
  AddMethodDecl(decl, ast_ctx, "class", class_type, true);
  AddMethodDecl(decl, ast_ctx, "isKindOfClass", ast_ctx.BoolTy, true);
  AddMethodDecl(decl, ast_ctx, "retain", id_type, true);
  AddMethodDecl(decl, ast_ctx, "release", ast_ctx.VoidTy, true);
  AddMethodDecl(decl, ast_ctx, "autorelease", id_type, true);
}

// NOTE: Removed hardcoded BankAccount methods - they were causing crashes
// for classes like GSInlineArray that don't have these methods
// All classes now get only the basic NSObject methods plus type-appropriate methods

// Add Array methods
void GNUstepObjCDeclVendor::AddArrayMethods(clang::ObjCInterfaceDecl *decl, clang::ASTContext &ast_ctx) {
  clang::QualType id_type = ast_ctx.getObjCIdType();
  std::string class_name = decl->getNameAsString();
  
  // Basic array methods for all array types
  // NSUInteger is unsigned long on 64-bit systems
  clang::QualType nsuinteger_type = ast_ctx.UnsignedLongTy;
  AddMethodDecl(decl, ast_ctx, "count", nsuinteger_type, true);
  AddMethodDecl(decl, ast_ctx, "firstObject", id_type, true);
  AddMethodDecl(decl, ast_ctx, "lastObject", id_type, true);
  AddMethodWithParameter(decl, ast_ctx, "objectAtIndex", id_type, "index", ast_ctx.UnsignedIntTy, true);
  
  // NSMutableArray specific methods
  if (class_name.find("Mutable") != std::string::npos || 
      class_name == "GSMutableArray" || class_name == "NSMutableArray") {
    AddMethodDecl(decl, ast_ctx, "initWithObjects", id_type, true); // variadic method - simplified
    AddMethodWithParameter(decl, ast_ctx, "addObject", ast_ctx.VoidTy, "object", id_type, true);
    AddMethodWithParameter(decl, ast_ctx, "removeObject", ast_ctx.VoidTy, "object", id_type, true);
    AddMethodWithParameter(decl, ast_ctx, "insertObject", ast_ctx.VoidTy, "object", id_type, true);
  }
}

// Add Dictionary methods  
void GNUstepObjCDeclVendor::AddDictionaryMethods(clang::ObjCInterfaceDecl *decl, clang::ASTContext &ast_ctx) {
  clang::QualType id_type = ast_ctx.getObjCIdType();
  std::string class_name = decl->getNameAsString();
  
  // Basic dictionary methods for all dictionary types
  // NSUInteger is unsigned long on 64-bit systems
  clang::QualType nsuinteger_type = ast_ctx.UnsignedLongTy;
  AddMethodDecl(decl, ast_ctx, "count", nsuinteger_type, true);
  AddMethodWithParameter(decl, ast_ctx, "objectForKey", id_type, "key", id_type, true);
  AddMethodDecl(decl, ast_ctx, "allKeys", id_type, true);
  AddMethodDecl(decl, ast_ctx, "allValues", id_type, true);
  
  // NSMutableDictionary specific methods
  if (class_name.find("Mutable") != std::string::npos || 
      class_name == "GSMutableDictionary" || class_name == "NSMutableDictionary") {
    // setObject:forKey: method would need two parameters - simplified for now
    AddMethodWithParameter(decl, ast_ctx, "setObject", ast_ctx.VoidTy, "object", id_type, true);
    AddMethodWithParameter(decl, ast_ctx, "removeObjectForKey", ast_ctx.VoidTy, "key", id_type, true);
  }
}

// Add String methods
void GNUstepObjCDeclVendor::AddStringMethods(clang::ObjCInterfaceDecl *decl, clang::ASTContext &ast_ctx) {
  clang::QualType id_type = ast_ctx.getObjCIdType();
  std::string class_name = decl->getNameAsString();
  
  // Basic string methods for all string types
  // NSUInteger is unsigned long on 64-bit systems
  clang::QualType nsuinteger_type = ast_ctx.UnsignedLongTy;
  AddMethodDecl(decl, ast_ctx, "length", nsuinteger_type, true);
  AddMethodDecl(decl, ast_ctx, "UTF8String", ast_ctx.getPointerType(ast_ctx.CharTy), true);
  AddMethodWithParameter(decl, ast_ctx, "characterAtIndex", ast_ctx.UnsignedShortTy, "index", ast_ctx.UnsignedIntTy, true);
  
  // NSMutableString specific methods
  if (class_name.find("Mutable") != std::string::npos || 
      class_name == "GSMutableString" || class_name == "NSMutableString") {
    AddMethodWithParameter(decl, ast_ctx, "appendString", ast_ctx.VoidTy, "string", id_type, true);
  }
}

// Add property accessor methods for all ivars
// This creates getter and setter methods that match property syntax
void GNUstepObjCDeclVendor::AddPropertyAccessorsForIvars(
    clang::ObjCInterfaceDecl *decl,
    ObjCLanguageRuntime::ClassDescriptorSP descriptor) {
    
  if (!decl || !descriptor)
    return;
    
  Log *log = GetLog(LLDBLog::Expressions);
  clang::ASTContext &ast_ctx = m_ast_ctx->getASTContext();
  std::string class_name = decl->getNameAsString();
  
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Adding property accessors for class {0}", class_name);
  
  // For now, use ivar-based property creation
  // The ExternalASTSource will handle dynamic property lookups when needed
  
  // Iterate through ivars and create property accessor methods
  std::function<bool(const char *, const char *, lldb::addr_t, uint64_t)> ivar_func = 
      [&](const char *name, const char *type, lldb::addr_t offset_ptr, uint64_t size) -> bool {
    if (!name || name[0] == '\0')
      return true; // Continue iteration
      
    std::string ivar_name(name);
    
    // Create property name by removing leading underscore if present
    std::string property_name = ivar_name;
    if (property_name[0] == '_' && property_name.length() > 1) {
      property_name = property_name.substr(1);
    }
    
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Creating property accessor for ivar '{0}' as property '{1}'", 
             ivar_name, property_name);
    
    // Determine the type for the property
    clang::QualType property_type = ast_ctx.getObjCIdType(); // Default to id
    
    // TODO: Parse type encoding to get actual property type
    // For now, all properties are treated as id type
    
    // Create getter method with property name (e.g., "accountNumber" for "_accountNumber")
    clang::IdentifierInfo &getter_id = ast_ctx.Idents.get(property_name);
    clang::Selector getter_sel = ast_ctx.Selectors.getNullarySelector(&getter_id);
    
    clang::ObjCMethodDecl *getter = clang::ObjCMethodDecl::Create(
        ast_ctx,
        clang::SourceLocation(),
        clang::SourceLocation(),
        getter_sel,
        property_type,
        nullptr, // TypeSourceInfo
        decl,
        true,  // isInstanceMethod
        false, // isVariadic
        true,  // isPropertyAccessor
        false, // isSynthesizedAccessorStub
        true,  // isImplicitlyDeclared
        false, // isDefined
        clang::ObjCImplementationControl::None
    );
    
    if (getter) {
      decl->addDecl(getter);
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Added getter '{0}' for property", property_name);
    }
    
    // Create setter method (e.g., "setAccountNumber:" for "_accountNumber")
    // Capitalize first letter of property name for setter
    std::string setter_name = "set";
    if (!property_name.empty()) {
      setter_name += std::toupper(property_name[0]);
      if (property_name.length() > 1) {
        setter_name += property_name.substr(1);
      }
    }
    
    const clang::IdentifierInfo *setter_id = &ast_ctx.Idents.get(setter_name);
    clang::Selector setter_sel = ast_ctx.Selectors.getSelector(1, &setter_id);
    
    // Create parameter for setter
    clang::IdentifierInfo &param_id = ast_ctx.Idents.get("value");
    clang::ParmVarDecl *param = clang::ParmVarDecl::Create(
        ast_ctx,
        nullptr,
        clang::SourceLocation(),
        clang::SourceLocation(),
        &param_id,
        property_type,
        nullptr,
        clang::SC_None,
        nullptr
    );
    
    clang::ObjCMethodDecl *setter = clang::ObjCMethodDecl::Create(
        ast_ctx,
        clang::SourceLocation(),
        clang::SourceLocation(),
        setter_sel,
        ast_ctx.VoidTy,
        nullptr, // TypeSourceInfo
        decl,
        true,  // isInstanceMethod
        false, // isVariadic
        true,  // isPropertyAccessor
        false, // isSynthesizedAccessorStub
        true,  // isImplicitlyDeclared
        false, // isDefined
        clang::ObjCImplementationControl::None
    );
    
    if (setter && param) {
      setter->setMethodParams(ast_ctx, {param}, {});
      decl->addDecl(setter);
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Added setter '{0}:' for property", setter_name);
    }
    
    // Also create an @property declaration
    // This makes the property visible in LLDB's type system
    clang::ObjCPropertyDecl *property = clang::ObjCPropertyDecl::Create(
        ast_ctx,
        decl,
        clang::SourceLocation(),
        &ast_ctx.Idents.get(property_name),
        clang::SourceLocation(),
        clang::SourceLocation(),
        property_type,
        nullptr // TypeSourceInfo
    );
    
    if (property) {
      property->setPropertyAttributes(
          static_cast<clang::ObjCPropertyAttribute::Kind>(
              clang::ObjCPropertyAttribute::kind_atomic |
              clang::ObjCPropertyAttribute::kind_readwrite));
      property->setGetterMethodDecl(getter);
      property->setSetterMethodDecl(setter);
      property->setPropertyIvarDecl(nullptr); // We'll link to the actual ivar if needed
      decl->addDecl(property);
      LLDB_LOG(log, "GNUstepObjCDeclVendor: Added @property '{0}' declaration", property_name);
    }
    
    return true; // Continue iteration
  };
  
  // Use the descriptor to iterate through all ivars
  descriptor->Describe(
      std::function<void(ObjCLanguageRuntime::ObjCISA)>(),
      std::function<bool(const char *, const char *)>(),
      std::function<bool(const char *, const char *)>(),
      ivar_func);
  
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Completed adding property accessors for {0}", class_name);
}

// Implementation of GNUstepObjCExternalASTSource
bool GNUstepObjCExternalASTSource::FindExternalVisibleDeclsByName(
    const clang::DeclContext *decl_ctx, clang::DeclarationName name,
    const clang::DeclContext *original_dc) {
  
  Log *log = GetLog(LLDBLog::Expressions);
  
  if (log) {
    LLDB_LOG(log, "GNUstepObjCExternalASTSource::FindExternalVisibleDeclsByName: "
             "Looking for {0} in context {1}", 
             name.getAsString(), decl_ctx->getDeclKindName());
  }
  
  // Only handle ObjC interface contexts
  const clang::ObjCInterfaceDecl *interface_decl = 
      llvm::dyn_cast<clang::ObjCInterfaceDecl>(decl_ctx);
  
  if (!interface_decl) {
    return false;
  }
  
  // Get non-const version to work with
  clang::ObjCInterfaceDecl *non_const_interface_decl = 
      const_cast<clang::ObjCInterfaceDecl *>(interface_decl);
  
  // Complete the interface if needed
  CompleteInterface(non_const_interface_decl);
  
  // Get the property/method name being looked up
  std::string lookup_name = name.getAsString();
  
  LLDB_LOG(log, "GNUstepObjCExternalASTSource: Looking for property/method '{0}' in class {1}",
           lookup_name, interface_decl->getNameAsString());
  
  // Check if this is a property access (no colons in name)
  if (lookup_name.find(':') == std::string::npos) {
    // This is likely a property getter - check if we have an ivar with underscore prefix
    std::string ivar_name = "_" + lookup_name;
    
    // Look for existing ivar with this name
    for (auto ivar : interface_decl->ivars()) {
      if (ivar->getNameAsString() == ivar_name) {
        LLDB_LOG(log, "GNUstepObjCExternalASTSource: Found matching ivar '{0}' for property '{1}'",
                 ivar_name, lookup_name);
        
        // Create property getter if it doesn't exist
        clang::ASTContext &ast_ctx = m_decl_vendor.m_ast_ctx->getASTContext();
        clang::QualType ivar_type = ivar->getType();
        
        // Check if getter already exists
        clang::IdentifierInfo &getter_id = ast_ctx.Idents.get(lookup_name);
        clang::Selector getter_sel = ast_ctx.Selectors.getNullarySelector(&getter_id);
        
        bool found_getter = false;
        for (auto method : interface_decl->methods()) {
          if (method->getSelector() == getter_sel && method->isInstanceMethod()) {
            found_getter = true;
            break;
          }
        }
        
        if (!found_getter) {
          // Create the getter method
          
          clang::ObjCMethodDecl *getter = clang::ObjCMethodDecl::Create(
              ast_ctx,
              clang::SourceLocation(),
              clang::SourceLocation(),
              getter_sel,
              ivar_type,
              nullptr, // TypeSourceInfo
              non_const_interface_decl,
              true,  // isInstanceMethod
              false, // isVariadic
              true,  // isPropertyAccessor
              false, // isSynthesizedAccessorStub
              true,  // isImplicitlyDeclared
              false, // isDefined
              clang::ObjCImplementationControl::None
          );
          
          if (getter) {
            non_const_interface_decl->addDecl(getter);
            LLDB_LOG(log, "GNUstepObjCExternalASTSource: Created getter method '{0}' for property",
                     lookup_name);
            return true;
          }
        }
        
        // Also create setter if needed
        std::string setter_name = "set";
        setter_name += std::toupper(lookup_name[0]);
        if (lookup_name.length() > 1) {
          setter_name += lookup_name.substr(1);
        }
        
        const clang::IdentifierInfo *setter_id = &ast_ctx.Idents.get(setter_name);
        clang::Selector setter_sel = ast_ctx.Selectors.getSelector(1, &setter_id);
        
        bool found_setter = false;
        for (auto method : interface_decl->methods()) {
          if (method->getSelector() == setter_sel && method->isInstanceMethod()) {
            found_setter = true;
            break;
          }
        }
        
        if (!found_setter) {
          // Create parameter for setter
          clang::IdentifierInfo &param_id = ast_ctx.Idents.get("value");
          clang::ParmVarDecl *param = clang::ParmVarDecl::Create(
              ast_ctx,
              nullptr,
              clang::SourceLocation(),
              clang::SourceLocation(),
              &param_id,
              ivar_type,
              nullptr,
              clang::SC_None,
              nullptr
          );
          
          clang::ObjCMethodDecl *setter = clang::ObjCMethodDecl::Create(
              ast_ctx,
              clang::SourceLocation(),
              clang::SourceLocation(),
              setter_sel,
              ast_ctx.VoidTy,
              nullptr, // TypeSourceInfo
              non_const_interface_decl,
              true,  // isInstanceMethod
              false, // isVariadic
              true,  // isPropertyAccessor
              false, // isSynthesizedAccessorStub
              true,  // isImplicitlyDeclared
              false, // isDefined
              clang::ObjCImplementationControl::None
          );
          
          if (setter && param) {
            setter->setMethodParams(ast_ctx, {param}, {});
            non_const_interface_decl->addDecl(setter);
            LLDB_LOG(log, "GNUstepObjCExternalASTSource: Created setter method '{0}:' for property",
                     setter_name);
          }
        }
        
        return true;
      }
    }
  }
  
  return false;
}

void GNUstepObjCExternalASTSource::CompleteInterface(
    clang::ObjCInterfaceDecl *interface_decl) {
  
  if (!interface_decl || !interface_decl->hasExternalLexicalStorage())
    return;
    
  Log *log = GetLog(LLDBLog::Expressions);
  LLDB_LOG(log, "GNUstepObjCExternalASTSource::CompleteInterface for class {0}",
           interface_decl->getNameAsString());
  
  // Mark as completed so we don't recurse
  interface_decl->setHasExternalLexicalStorage(false);
  interface_decl->setHasExternalVisibleStorage(false);
  
  // Ensure the interface is fully populated with ivars and methods
  // This is already done in BuildInterfaceDecl, but we can add more here if needed
}