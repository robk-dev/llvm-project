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

using namespace lldb;
using namespace lldb_private;

GNUstepObjCDeclVendor::GNUstepObjCDeclVendor(ObjCLanguageRuntime &runtime)
    : ClangDeclVendor(eClangDeclVendor), // Using generic ClangDeclVendor type
      m_runtime(runtime), 
      m_nsobject_decl(nullptr) {
  
  Log *log = GetLog(LLDBLog::Expressions);
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Initializing");
  
  // Create a TypeSystemClang instance for creating types
  Target &target = runtime.GetProcess()->GetTarget();
  auto type_system_or_err = target.GetScratchTypeSystemForLanguage(eLanguageTypeObjC);
  
  if (auto error = type_system_or_err.takeError()) {
    LLDB_LOG_ERROR(log, std::move(error), 
                   "GNUstepObjCDeclVendor: Failed to get scratch TypeSystemClang");
    return;
  }
  
  // Can't use dynamic_pointer_cast with -fno-rtti, use static cast
  m_ast_ctx = std::static_pointer_cast<TypeSystemClang>(type_system_or_err->get()->shared_from_this());
  if (!m_ast_ctx) {
    LLDB_LOG(log, "GNUstepObjCDeclVendor: Failed to cast to TypeSystemClang");
    return;
  }
  
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
    
  // TODO: Iterate through ivars and add them to the declaration
  // This requires runtime introspection of the class structure
  
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
  // This is the old logic but cleaned up without hardcoded BankAccount
  if (class_name.find("Array") != std::string::npos) {
    AddArrayMethods(decl, ast_ctx);
  } else if (class_name.find("Dictionary") != std::string::npos) {
    AddDictionaryMethods(decl, ast_ctx);
  } else if (class_name.find("String") != std::string::npos) {
    AddStringMethods(decl, ast_ctx);
  }
  // Note: Removed hardcoded BankAccount logic - it was causing the crashes
  // Now all classes get at least the basic NSObject methods
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
  LLDB_LOG(log, "GNUstepObjCDeclVendor: FindDecls called for '{0}' (append={1}, max_matches={2})", 
           name, append, max_matches);
  
  clang::ObjCInterfaceDecl *decl = GetDeclForClassName(name.AsCString());
  if (decl) {
    // Convert clang::NamedDecl to CompilerDecl
    CompilerDecl compiler_decl(m_ast_ctx.get(), decl);
    decls.push_back(compiler_decl);
    LLDB_LOG(log, "GNUstepObjCDeclVendor: FindDecls found type {0}, returning 1 match", name);
    return 1;
  }
  
  LLDB_LOG(log, "GNUstepObjCDeclVendor: FindDecls found no matches for {0}", name);
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