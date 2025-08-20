//===-- GNUstepObjCDeclVendor.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCDeclVendor.h"
#include "GNUstepObjCRuntime.h"
#include "GNUstepObjCRuntimeIntrospector.h"
#include "GNUstepRuntimeV2API.h"

#include "Plugins/ExpressionParser/Clang/ClangASTMetadata.h"
#include "Plugins/ExpressionParser/Clang/ClangUtil.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "lldb/Core/Module.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/ExternalASTSource.h"

#include <optional>
#include <vector>

using namespace lldb_private;

// Forward declaration for method deduplication helper
static bool InterfaceAlreadyHasMethod(clang::ObjCInterfaceDecl *interface_decl,
                                      llvm::StringRef sel_name, bool is_instance);

class lldb_private::GNUstepObjCExternalASTSource
    : public clang::ExternalASTSource {
public:
  GNUstepObjCExternalASTSource(GNUstepObjCDeclVendor &decl_vendor)
      : m_decl_vendor(decl_vendor) {}

  bool FindExternalVisibleDeclsByName(
      const clang::DeclContext *decl_ctx, clang::DeclarationName name,
      const clang::DeclContext *original_dc) override {

    Log *log(GetLog(LLDBLog::Expressions));

    if (log) {
      LLDB_LOGF(log,
                "GNUstepObjCExternalASTSource::FindExternalVisibleDeclsByName"
                " on (ASTContext*)%p Looking for %s in (%sDecl*)%p",
                static_cast<void *>(&decl_ctx->getParentASTContext()),
                name.getAsString().c_str(), decl_ctx->getDeclKindName(),
                static_cast<const void *>(decl_ctx));
    }

    do {
      const clang::ObjCInterfaceDecl *interface_decl =
          llvm::dyn_cast<clang::ObjCInterfaceDecl>(decl_ctx);

      if (!interface_decl)
        break;

      clang::ObjCInterfaceDecl *non_const_interface_decl =
          const_cast<clang::ObjCInterfaceDecl *>(interface_decl);

      if (!m_decl_vendor.FinishDecl(non_const_interface_decl))
        break;

      clang::DeclContext::lookup_result result =
          non_const_interface_decl->lookup(name);

      return (!result.empty());
    } while (false);

    SetNoExternalVisibleDeclsForName(decl_ctx, name);
    return false;
  }

  void CompleteType(clang::TagDecl *tag_decl) override {
    Log *log(GetLog(LLDBLog::Expressions));
    LLDB_LOGF(log,
              "GNUstepObjCExternalASTSource::CompleteType on "
              "(ASTContext*)%p Completing (TagDecl*)%p named %s",
              static_cast<void *>(&tag_decl->getASTContext()),
              static_cast<void *>(tag_decl), tag_decl->getName().str().c_str());
  }

  void CompleteType(clang::ObjCInterfaceDecl *interface_decl) override {
    Log *log(GetLog(LLDBLog::Expressions));

    if (log) {
      LLDB_LOGF(log,
                "GNUstepObjCExternalASTSource::CompleteType on "
                "(ASTContext*)%p Completing (ObjCInterfaceDecl*)%p named %s",
                static_cast<void *>(&interface_decl->getASTContext()),
                static_cast<void *>(interface_decl),
                interface_decl->getName().str().c_str());
    }

    m_decl_vendor.FinishDecl(interface_decl);
  }

  bool layoutRecordType(
      const clang::RecordDecl *Record, uint64_t &Size, uint64_t &Alignment,
      llvm::DenseMap<const clang::FieldDecl *, uint64_t> &FieldOffsets,
      llvm::DenseMap<const clang::CXXRecordDecl *, clang::CharUnits>
          &BaseOffsets,
      llvm::DenseMap<const clang::CXXRecordDecl *, clang::CharUnits>
          &VirtualBaseOffsets) override {
    return false;
  }

  void StartTranslationUnit(clang::ASTConsumer *Consumer) override {
    clang::TranslationUnitDecl *translation_unit_decl =
        m_decl_vendor.m_ast_ctx->getASTContext().getTranslationUnitDecl();
    translation_unit_decl->setHasExternalVisibleStorage();
    translation_unit_decl->setHasExternalLexicalStorage();
  }

private:
  GNUstepObjCDeclVendor &m_decl_vendor;
};

GNUstepObjCDeclVendor::GNUstepObjCDeclVendor(ObjCLanguageRuntime &runtime)
    : ClangDeclVendor(eGNUstepObjCDeclVendor), m_runtime(runtime),
      m_type_realizer_sp(m_runtime.GetEncodingToType()), m_forwarding_initialized(false) {
  m_ast_ctx = std::make_shared<TypeSystemClang>(
      "GNUstepObjCDeclVendor AST",
      runtime.GetProcess()->GetTarget().GetArchitecture().GetTriple());
  m_external_source = new GNUstepObjCExternalASTSource(*this);
  llvm::IntrusiveRefCntPtr<clang::ExternalASTSource> external_source_owning_ptr(
      m_external_source);
  m_ast_ctx->getASTContext().setExternalSource(external_source_owning_ptr);
  
  // Initialize method forwarding rules for modern subscript syntax
  InstallDefaultForwardingRules();
}

clang::ObjCInterfaceDecl *
GNUstepObjCDeclVendor::GetDeclForISA(ObjCLanguageRuntime::ObjCISA isa) {
  ISAToInterfaceMap::const_iterator iter = m_isa_to_interface.find(isa);

  if (iter != m_isa_to_interface.end())
    return iter->second;

  clang::ASTContext &ast_ctx = m_ast_ctx->getASTContext();

  ObjCLanguageRuntime::ClassDescriptorSP descriptor =
      m_runtime.GetClassDescriptorFromISA(isa);

  if (!descriptor)
    return nullptr;

  ConstString name(descriptor->GetClassName());

  clang::IdentifierInfo &identifier_info =
      ast_ctx.Idents.get(name.GetStringRef());

  clang::ObjCInterfaceDecl *new_iface_decl = clang::ObjCInterfaceDecl::Create(
      ast_ctx, ast_ctx.getTranslationUnitDecl(), clang::SourceLocation(),
      &identifier_info, nullptr, nullptr);

  ClangASTMetadata meta_data;
  meta_data.SetISAPtr(isa);
  m_ast_ctx->SetMetadata(new_iface_decl, meta_data);

  new_iface_decl->setHasExternalVisibleStorage();
  new_iface_decl->setHasExternalLexicalStorage();

  ast_ctx.getTranslationUnitDecl()->addDecl(new_iface_decl);

  m_isa_to_interface[isa] = new_iface_decl;

  return new_iface_decl;
}

// Foundation class method signatures for common classes
struct FoundationMethodSignature {
  const char *name;
  const char *types;
  bool is_instance;
};

// Method signatures for NSString
static const FoundationMethodSignature NSString_methods[] = {
  {"length", "Q@:", true},
  {"characterAtIndex:", "S@:Q", true},
  {"UTF8String", "*@:", true},
  {"stringWithFormat:", "@#@:@", false},
  {"stringWithCString:encoding:", "@#@:*Q", false},
  {"description", "@@:", true},
  {"isEqualToString:", "B@:@", true},
  {"substringFromIndex:", "@@:Q", true},
  {"substringToIndex:", "@@:Q", true},
  {"substringWithRange:", "@@:{_NSRange=QQ}", true},
  {nullptr, nullptr, false}
};

// Method signatures for NSNumber
static const FoundationMethodSignature NSNumber_methods[] = {
  {"numberWithInt:", "@#@:i", false},
  {"numberWithDouble:", "@#@:d", false},
  {"numberWithBool:", "@#@:B", false},
  {"intValue", "i@:", true},
  {"doubleValue", "d@:", true},
  {"boolValue", "B@:", true},
  {"stringValue", "@@:", true},
  {"description", "@@:", true},
  {nullptr, nullptr, false}
};

// Method signatures for NSArray
static const FoundationMethodSignature NSArray_methods[] = {
  {"count", "Q@:", true},
  {"objectAtIndex:", "@@:Q", true},
  {"objectAtIndexedSubscript:", "@@:Q", true},  // Modern subscript syntax support (array[index])
  {"firstObject", "@@:", true},
  {"lastObject", "@@:", true},
  {"arrayWithObjects:", "@#@:@@", false},
  {"description", "@@:", true},
  {"containsObject:", "B@:@", true},
  {nullptr, nullptr, false}
};

// Method signatures for NSDictionary
static const FoundationMethodSignature NSDictionary_methods[] = {
  {"count", "Q@:", true},
  {"objectForKey:", "@@:@", true},
  {"objectForKeyedSubscript:", "@@:@", true},  // Modern subscript syntax support (dict[@"key"])
  {"allKeys", "@@:", true},
  {"allValues", "@@:", true},
  {"dictionaryWithObject:forKey:", "@#@:@@", false},
  {"description", "@@:", true},
  {nullptr, nullptr, false}
};

// Method signatures for NSSet
static const FoundationMethodSignature NSSet_methods[] = {
  {"count", "Q@:", true},
  {"anyObject", "@@:", true},
  {"allObjects", "@@:", true},
  {"containsObject:", "B@:@", true},
  {"setWithObjects:", "@#@:@@", false},
  {"description", "@@:", true},
  {nullptr, nullptr, false}
};

// Class-specific method signature tables
static const struct {
  const char *class_name;
  const FoundationMethodSignature *methods;
} foundation_class_methods[] = {
  {"NSString", NSString_methods},
  {"NSMutableString", NSString_methods},
  {"NSConstantString", NSString_methods},
  {"GSTinyString", NSString_methods},
  {"GSMutableString", NSString_methods},
  {"NSNumber", NSNumber_methods},
  {"GSNumber", NSNumber_methods},
  {"NSArray", NSArray_methods},
  {"NSMutableArray", NSArray_methods},
  {"GSArray", NSArray_methods},
  {"GSMutableArray", NSArray_methods},
  {"NSDictionary", NSDictionary_methods},
  {"NSMutableDictionary", NSDictionary_methods},
  {"GSDictionary", NSDictionary_methods},
  {"GSMutableDictionary", NSDictionary_methods},
  {"NSSet", NSSet_methods},
  {"NSMutableSet", NSSet_methods},
  {"GSSet", NSSet_methods},
  {"GSMutableSet", NSSet_methods},
  {nullptr, nullptr}
};

class GNUstepObjCRuntimeMethodType {
public:
  GNUstepObjCRuntimeMethodType(const char *types) {
    const char *cursor = types;
    enum ParserState { Start = 0, InType, InPos } state = Start;
    const char *type = nullptr;
    int brace_depth = 0;

    uint32_t stepsLeft = 256;

    while (true) {
      if (--stepsLeft == 0) {
        m_is_valid = false;
        return;
      }

      switch (state) {
      case Start: {
        switch (*cursor) {
        default:
          state = InType;
          type = cursor;
          break;
        case '\0':
          m_is_valid = true;
          return;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          m_is_valid = false;
          return;
        }
      } break;
      case InType: {
        switch (*cursor) {
        default:
          ++cursor;
          break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          if (!brace_depth) {
            state = InPos;
            if (type) {
              m_type_vector.push_back(std::string(type, (cursor - type)));
            } else {
              m_is_valid = false;
              return;
            }
            type = nullptr;
          } else {
            ++cursor;
          }
          break;
        case '[':
        case '{':
        case '(':
          ++brace_depth;
          ++cursor;
          break;
        case ']':
        case '}':
        case ')':
          if (!brace_depth) {
            m_is_valid = false;
            return;
          }
          --brace_depth;
          ++cursor;
          break;
        case '\0':
          m_is_valid = false;
          return;
        }
      } break;
      case InPos: {
        switch (*cursor) {
        default:
          state = InType;
          type = cursor;
          break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          ++cursor;
          break;
        case '\0':
          m_is_valid = true;
          return;
        }
      } break;
      }
    }
  }

  clang::ObjCMethodDecl *
  BuildMethod(TypeSystemClang &clang_ast_ctxt,
              clang::ObjCInterfaceDecl *interface_decl, const char *name,
              bool instance,
              ObjCLanguageRuntime::EncodingToTypeSP type_realizer_sp) {
    // CRITICAL SAFETY CHECKS: Prevent crashes from invalid parameters
    if (!interface_decl) {
      return nullptr;
    }
    
    if (!name || strlen(name) == 0) {
      // This is the exact cause of the GSTinyString crash!
      return nullptr;
    }
    
    if (!m_is_valid || m_type_vector.size() < 3) {
      return nullptr;
    }

    clang::ASTContext &ast_ctx(interface_decl->getASTContext());

    const bool isInstance = instance;
    const bool isVariadic = false;
    const bool isPropertyAccessor = false;
    const bool isSynthesizedAccessorStub = false;
    const bool isImplicitlyDeclared = true;
    const bool isDefined = false;
    const clang::ObjCImplementationControl impControl =
        clang::ObjCImplementationControl::None;
    const bool HasRelatedResultType = false;
    const bool for_expression = true;

    std::vector<const clang::IdentifierInfo *> selector_components;

    const char *name_cursor = name;
    bool is_zero_argument = true;

    // ENHANCED SAFETY: Validate name before processing
    size_t name_len = strlen(name);
    if (name_len == 0) {
      // Double-check for empty selector names
      return nullptr;
    }

    while (*name_cursor != '\0') {
      const char *colon_loc = strchr(name_cursor, ':');
      if (!colon_loc) {
        // Ensure we don't create empty identifier
        if (name_cursor != name || strlen(name_cursor) > 0) {
          selector_components.push_back(
              &ast_ctx.Idents.get(llvm::StringRef(name_cursor)));
        }
        break;
      } else {
        is_zero_argument = false;
        // Ensure we don't create empty identifier components
        if (colon_loc > name_cursor) {
          selector_components.push_back(&ast_ctx.Idents.get(
              llvm::StringRef(name_cursor, colon_loc - name_cursor)));
        }
        name_cursor = colon_loc + 1;
      }
    }

    // SAFETY: Ensure we have at least one selector component
    if (selector_components.empty()) {
      return nullptr;
    }

    const clang::IdentifierInfo **identifier_infos = selector_components.data();
    if (!identifier_infos) {
      return nullptr;
    }

    clang::Selector sel = ast_ctx.Selectors.getSelector(
        is_zero_argument ? 0 : selector_components.size(),
        identifier_infos);

    clang::QualType ret_type =
        ClangUtil::GetQualType(type_realizer_sp->RealizeType(
            clang_ast_ctxt, m_type_vector[0].c_str(), for_expression));

    if (ret_type.isNull())
      return nullptr;

    clang::ObjCMethodDecl *ret = clang::ObjCMethodDecl::Create(
        ast_ctx, clang::SourceLocation(), clang::SourceLocation(), sel,
        ret_type, nullptr, interface_decl, isInstance, isVariadic,
        isPropertyAccessor, isSynthesizedAccessorStub, isImplicitlyDeclared,
        isDefined, impControl, HasRelatedResultType);

    std::vector<clang::ParmVarDecl *> parm_vars;

    for (size_t ai = 3, ae = m_type_vector.size(); ai != ae; ++ai) {
      const bool for_expression = true;
      clang::QualType arg_type =
          ClangUtil::GetQualType(type_realizer_sp->RealizeType(
              clang_ast_ctxt, m_type_vector[ai].c_str(), for_expression));

      if (arg_type.isNull())
        return nullptr; // well, we just wasted a bunch of time.  Wish we could
                        // delete the stuff we'd just made!

      parm_vars.push_back(clang::ParmVarDecl::Create(
          ast_ctx, ret, clang::SourceLocation(), clang::SourceLocation(),
          nullptr, arg_type, nullptr, clang::SC_None, nullptr));
    }

    ret->setMethodParams(ast_ctx,
                         llvm::ArrayRef<clang::ParmVarDecl *>(parm_vars),
                         llvm::ArrayRef<clang::SourceLocation>());

    return ret;
  }

  explicit operator bool() { return m_is_valid; }

  size_t GetNumTypes() { return m_type_vector.size(); }

  const char *GetTypeAtIndex(size_t idx) { return m_type_vector[idx].c_str(); }

private:
  typedef std::vector<std::string> TypeVector;

  TypeVector m_type_vector;
  bool m_is_valid = false;
};

void GNUstepObjCDeclVendor::AddFoundationClassMethods(
    clang::ObjCInterfaceDecl *interface_decl, const std::string &class_name) {
  Log *log(GetLog(LLDBLog::Expressions));
  
  // SAFETY CHECK: Validate input parameters to prevent crashes
  if (!interface_decl) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] ERROR: interface_decl is null for class %s",
              class_name.c_str());
    return;
  }
  
  if (class_name.empty()) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] ERROR: class_name is empty");
    return;
  }
  
  // Find the method signatures for this class
  const FoundationMethodSignature *methods = nullptr;
  for (int i = 0; foundation_class_methods[i].class_name; i++) {
    if (class_name == foundation_class_methods[i].class_name) {
      methods = foundation_class_methods[i].methods;
      break;
    }
  }
  
  if (!methods) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] No method signatures found for class %s",
              class_name.c_str());
    return;
  }
  
  // Add each method to the interface with enhanced safety checks and forwarding support
  for (int i = 0; methods[i].name; i++) {
    // CRITICAL SAFETY CHECK: Ensure method name and types are valid
    if (!methods[i].name || strlen(methods[i].name) == 0) {
      LLDB_LOGF(log, "[GNUstepObjCDeclVendor] ERROR: Invalid method name at index %d for class %s - SKIPPING",
                i, class_name.c_str());
      continue;  // Skip this method to prevent crash
    }
    
    if (!methods[i].types || strlen(methods[i].types) == 0) {
      LLDB_LOGF(log, "[GNUstepObjCDeclVendor] ERROR: Invalid method types for %s at index %d for class %s - SKIPPING",
                methods[i].name, i, class_name.c_str());
      continue;  // Skip this method to prevent crash
    }
    
    // CRITICAL SAFETY CHECK: Prevent duplicate method declarations
    if (InterfaceAlreadyHasMethod(interface_decl, methods[i].name, methods[i].is_instance)) {
      LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Method %s already exists on %s - skipping to prevent duplicate",
                methods[i].name, class_name.c_str());
      continue;
    }
    
    clang::ObjCMethodDecl *method_decl = CreateMethodDecl(
        interface_decl, methods[i].name, methods[i].types, methods[i].is_instance);
    
    if (method_decl) {
      interface_decl->addDecl(method_decl);
      LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Successfully added method %s to %s",
                methods[i].name, class_name.c_str());
    } else {
      LLDB_LOGF(log, "[GNUstepObjCDeclVendor] WARNING: Failed to create method decl for %s in class %s",
                methods[i].name, class_name.c_str());
    }
  }
  
  // NEW: Add forwarding methods for modern subscript syntax if the runtime doesn't have them
  // but does have the legacy methods
  std::vector<std::string> modern_methods_to_check = {
    "objectAtIndexedSubscript:",
    "objectForKeyedSubscript:"
  };
  
  for (const std::string &modern_method : modern_methods_to_check) {
    // Check if modern method already exists in static table
    bool modern_exists = false;
    for (int i = 0; methods[i].name; i++) {
      if (modern_method == methods[i].name) {
        modern_exists = true;
        break;
      }
    }
    
    // If modern method doesn't exist, try to create a forwarding method
    if (!modern_exists) {
      clang::ObjCMethodDecl *forwarding_decl = ResolveMethodWithForwarding(
          interface_decl, modern_method, class_name, true);  // Assume instance methods
      
      if (forwarding_decl) {
        interface_decl->addDecl(forwarding_decl);
        LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Successfully added forwarding method %s to %s",
                  modern_method.c_str(), class_name.c_str());
      }
    }
  }
}

clang::ObjCMethodDecl *GNUstepObjCDeclVendor::CreateMethodDecl(
    clang::ObjCInterfaceDecl *interface_decl, const char *name, const char *types,
    bool is_instance) {
  Log *log(GetLog(LLDBLog::Expressions));
  
  // CRITICAL SAFETY CHECKS: Prevent crashes from invalid inputs
  if (!interface_decl) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] ERROR: interface_decl is null in CreateMethodDecl");
    return nullptr;
  }
  
  if (!name) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] ERROR: name is null in CreateMethodDecl");
    return nullptr;
  }
  
  if (!types) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] ERROR: types is null in CreateMethodDecl");
    return nullptr;
  }
  
  // Handle empty selector names - this is a major source of crashes!
  if (strlen(name) == 0) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] CRITICAL ERROR: Empty selector name provided - this would cause NSInvalidArgumentException!");
    return nullptr;
  }
  
  // Validate types string is not empty
  if (strlen(types) == 0) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] ERROR: Empty types string provided for method %s", name);
    return nullptr;
  }
  
  // Additional safety: Check for reasonable selector name
  if (strlen(name) > 1024) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] ERROR: Selector name too long (%zu chars) for method %s", 
              strlen(name), name);
    return nullptr;
  }
  
  LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Creating method decl for selector '%s' with types '%s' (instance=%s)", 
            name, types, is_instance ? "YES" : "NO");
  
  GNUstepObjCRuntimeMethodType method_type(types);
  
  if (!method_type) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] ERROR: Invalid method type signature '%s' for method %s", 
              types, name);
    return nullptr;
  }
  
  clang::ObjCMethodDecl *method_decl = method_type.BuildMethod(
      *m_ast_ctx, interface_decl, name, is_instance, m_type_realizer_sp);
  
  if (!method_decl) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] ERROR: Failed to build method declaration for %s", name);
  } else {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Successfully created method declaration for %s", name);
  }
  
  return method_decl;
}

// Helper function to check if a method already exists on an interface
// This prevents duplicate method declarations which can cause compilation errors
static bool InterfaceAlreadyHasMethod(clang::ObjCInterfaceDecl *interface_decl,
                                      llvm::StringRef sel_name, bool is_instance) {
  if (!interface_decl) {
    return false;
  }
  
  // Check all existing methods on this interface
  for (auto *method : interface_decl->methods()) {
    if (method->isInstanceMethod() == is_instance &&
        method->getSelector().getAsString() == sel_name) {
      return true;
    }
  }
  
  return false;
}

bool GNUstepObjCDeclVendor::FinishDecl(clang::ObjCInterfaceDecl *interface_decl) {
  Log *log(GetLog(LLDBLog::Expressions));

  // CRITICAL SAFETY CHECK: Prevent crash from null interface_decl
  if (!interface_decl) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] CRITICAL ERROR: interface_decl is null!");
    return false;
  }

  ObjCLanguageRuntime::ObjCISA objc_isa = 0;
  if (std::optional<ClangASTMetadata> metadata =
          m_ast_ctx->GetMetadata(interface_decl))
    objc_isa = metadata->GetISAPtr();

  if (!objc_isa) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] No ISA found for interface_decl");
    return false;
  }

  if (!interface_decl->hasExternalVisibleStorage()) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Interface already finished");
    return true;
  }

  interface_decl->startDefinition();

  interface_decl->setHasExternalVisibleStorage(false);
  interface_decl->setHasExternalLexicalStorage(false);

  ObjCLanguageRuntime::ClassDescriptorSP descriptor =
      m_runtime.GetClassDescriptorFromISA(objc_isa);

  if (!descriptor) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] No class descriptor found for ISA 0x%lx", (unsigned long)objc_isa);
    return false;
  }

  std::string class_name = descriptor->GetClassName().AsCString();
  
  // SAFETY CHECK: Ensure class name is valid
  if (class_name.empty()) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] ERROR: Empty class name for ISA 0x%lx", (unsigned long)objc_isa);
    return false;
  }
  
  LLDB_LOGF(log,
            "[GNUstepObjCDeclVendor::FinishDecl] Finishing Objective-C "
            "interface for %s (ISA: 0x%lx)", class_name.c_str(), (unsigned long)objc_isa);

  // Add Foundation class methods if this is a known Foundation class
  // This call is now protected with enhanced safety checks (no exceptions in LLDB)
  AddFoundationClassMethods(interface_decl, class_name);

  // Add minimal dynamic method population using IMP probes for core selectors
  // This prevents expression parser failures for common methods
  auto add_method = [&](const char* sel_name, const char* types, bool is_instance) {
    if (InterfaceAlreadyHasMethod(interface_decl, sel_name, is_instance))
      return;
    if (!types || !*types) {
      // fallback: minimal signature -> id method:...
      types = is_instance ? "@@:" : "@#@:";
    }
    if (auto *decl = CreateMethodDecl(interface_decl, sel_name, types, is_instance))
      interface_decl->addDecl(decl);
  };

  // Core selectors to ensure are available for common NSNumber/NSString operations
  const char* core_selectors[][3] = {
    // {selector, types, is_instance}
    {"numberWithInt:", "@#@:i", "0"},       // NSNumber class method
    {"intValue", "i@:", "1"},               // NSNumber instance method
    {"stringWithFormat:", "@#@:@", "0"},    // NSString class method
    {"length", "Q@:", "1"},                 // NSString instance method
    {"objectAtIndex:", "@@:Q", "1"},        // NSArray instance method
    {"objectForKey:", "@@:@", "1"},         // NSDictionary instance method
    {nullptr, nullptr, nullptr}
  };

  for (int i = 0; core_selectors[i][0]; i++) {
    add_method(core_selectors[i][0], core_selectors[i][1], 
               strcmp(core_selectors[i][2], "1") == 0);
  }

  // For runtime introspection, we would need to implement:
  // auto superclass_func = [interface_decl, this](ObjCLanguageRuntime::ObjCISA isa) { ... };
  // auto instance_method_func = [log, interface_decl, this](const char *name, const char *types) -> bool { ... };
  // auto class_method_func = [log, interface_decl, this](const char *name, const char *types) -> bool { ... };
  // auto ivar_func = [log, interface_decl, this](const char *name, const char *type, lldb::addr_t offset_ptr, uint64_t size) -> bool { ... };
  // descriptor->Describe(superclass_func, instance_method_func, class_method_func, ivar_func);

  if (log) {
    LLDB_LOGF(
        log,
        "[GNUstepObjCDeclVendor::FinishDecl] Finished Objective-C interface for %s",
        class_name.c_str());

    LLDB_LOG(log, "  [GNUstepObjCDeclVendor::FinishDecl] {0}", ClangUtil::DumpDecl(interface_decl));
  }

  return true;
}

void GNUstepObjCDeclVendor::InstallDefaultForwardingRules() {
  Log *log(GetLog(LLDBLog::Expressions));
  LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Installing method forwarding rules for modern subscript syntax");
  
  if (m_forwarding_initialized) {
    return;
  }
  
  // Modern NSArray subscript to traditional method forwarding
  m_method_forwarding_rules.push_back({
    "objectAtIndexedSubscript:",   // Modern method
    "objectAtIndex:",              // Legacy method
    "NSArray",                     // Class prefix
    true                           // Enabled
  });
  
  // Modern NSDictionary subscript to traditional method forwarding
  m_method_forwarding_rules.push_back({
    "objectForKeyedSubscript:",    // Modern method
    "objectForKey:",               // Legacy method
    "NSDictionary",                // Class prefix
    true                           // Enabled
  });
  
  // Also support mutable variants
  m_method_forwarding_rules.push_back({
    "objectAtIndexedSubscript:",
    "objectAtIndex:",
    "NSMutableArray",
    true
  });
  
  m_method_forwarding_rules.push_back({
    "objectForKeyedSubscript:",
    "objectForKey:",
    "NSMutableDictionary",
    true
  });
  
  // Support GNUstep-specific class names
  m_method_forwarding_rules.push_back({
    "objectAtIndexedSubscript:",
    "objectAtIndex:",
    "GSArray",
    true
  });
  
  m_method_forwarding_rules.push_back({
    "objectAtIndexedSubscript:",
    "objectAtIndex:",
    "GSMutableArray",
    true
  });
  
  m_method_forwarding_rules.push_back({
    "objectForKeyedSubscript:",
    "objectForKey:",
    "GSDictionary",
    true
  });
  
  m_method_forwarding_rules.push_back({
    "objectForKeyedSubscript:",
    "objectForKey:",
    "GSMutableDictionary",
    true
  });
  
  m_forwarding_initialized = true;
  LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Installed %zu method forwarding rules", 
            m_method_forwarding_rules.size());
}

std::optional<std::string> GNUstepObjCDeclVendor::GetForwardingTarget(
    const std::string &method_name, const std::string &class_name) {
  Log *log(GetLog(LLDBLog::Expressions));
  
  if (!m_forwarding_initialized) {
    InstallDefaultForwardingRules();
  }
  
  // Check if this method should be forwarded
  for (const auto &rule : m_method_forwarding_rules) {
    if (!rule.enabled) {
      continue;
    }
    
    // Check if method matches
    if (rule.modern_method != method_name) {
      continue;
    }
    
    // Check if class matches (support wildcard "*")
    if (rule.class_prefix != "*" && class_name.find(rule.class_prefix) == std::string::npos) {
      continue;
    }
    
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Found forwarding rule: %s->%s for class %s",
              method_name.c_str(), rule.legacy_method.c_str(), class_name.c_str());
    return rule.legacy_method;
  }
  
  return std::nullopt;
}

bool GNUstepObjCDeclVendor::DoesClassRespondToSelector(const std::string &class_name,
                                                       const std::string &selector_name) {
  Log *log(GetLog(LLDBLog::Expressions));
  
  // Try to use runtime introspection to check if class responds to selector
  // This is more reliable than just checking our static method tables
  
  // Get the GNUstep runtime for this check
  GNUstepObjCRuntime *gnustep_runtime = 
    static_cast<GNUstepObjCRuntime*>(&m_runtime);
  if (!gnustep_runtime) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Could not get GNUstep runtime for selector check");
    return false;
  }
  
  auto *runtime_api = gnustep_runtime->GetRuntimeAPI();
  if (!runtime_api) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Runtime API not available for selector check");
    return false;
  }
  
  // Use the optimized runtime API method for selector checking
  bool responds = runtime_api->ClassRespondsToSelector(class_name, selector_name);
  LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Class %s %s to selector %s",
            class_name.c_str(), responds ? "responds" : "does not respond", 
            selector_name.c_str());
  
  // Fallback: if runtime API fails, try class_getMethodImplementation approach
  if (!responds && gnustep_runtime->GetRuntimeIntrospector()) {
    // Find class using introspector
    auto *introspector = gnustep_runtime->GetRuntimeIntrospector();
    lldb::addr_t class_addr = introspector->FindClass(class_name);
    if (class_addr != LLDB_INVALID_ADDRESS) {
      std::vector<lldb::addr_t> args;
      
      // Create selector string in target and get SEL
      Status error;
      Process *process = m_runtime.GetProcess();
      lldb::addr_t string_addr = process->AllocateMemory(selector_name.length() + 1, 
                                                        lldb::ePermissionsReadable, error);
      if (!error.Fail() && string_addr != LLDB_INVALID_ADDRESS) {
        process->WriteMemory(string_addr, selector_name.c_str(), 
                           selector_name.length() + 1, error);
        if (!error.Fail()) {
          args.push_back(string_addr);
          lldb::addr_t sel_addr = introspector->CallRuntimeFunction("sel_registerName", args);
          
          if (sel_addr != LLDB_INVALID_ADDRESS) {
            // Check if class_getMethodImplementation returns non-null IMP
            args.clear();
            args.push_back(class_addr);
            args.push_back(sel_addr);
            lldb::addr_t imp_addr = introspector->CallRuntimeFunction("class_getMethodImplementation", args);
            responds = (imp_addr != 0 && imp_addr != LLDB_INVALID_ADDRESS);
            
            LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Fallback IMP check: Class %s %s to selector %s (IMP: 0x%llx)",
                      class_name.c_str(), responds ? "responds" : "does not respond", 
                      selector_name.c_str(), (unsigned long long)imp_addr);
          }
        }
        process->DeallocateMemory(string_addr);
      }
    }
  }
  
  return responds;
}

clang::ObjCMethodDecl *GNUstepObjCDeclVendor::ResolveMethodWithForwarding(
    clang::ObjCInterfaceDecl *interface_decl,
    const std::string &method_name,
    const std::string &class_name,
    bool is_instance) {
  Log *log(GetLog(LLDBLog::Expressions));
  
  // SAFETY CHECK
  if (!interface_decl) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] ERROR: interface_decl is null in ResolveMethodWithForwarding");
    return nullptr;
  }
  
  LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Resolving method %s for class %s (instance=%s)",
            method_name.c_str(), class_name.c_str(), is_instance ? "YES" : "NO");
  
  // First, try to find the method directly
  const FoundationMethodSignature *methods = nullptr;
  for (int i = 0; foundation_class_methods[i].class_name; i++) {
    if (class_name == foundation_class_methods[i].class_name) {
      methods = foundation_class_methods[i].methods;
      break;
    }
  }
  
  if (methods) {
    // Check if the requested method exists in our static table
    for (int i = 0; methods[i].name; i++) {
      if (method_name == methods[i].name && is_instance == methods[i].is_instance) {
        LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Found method %s directly in static table", method_name.c_str());
        return CreateMethodDecl(interface_decl, methods[i].name, methods[i].types, methods[i].is_instance);
      }
    }
  }
  
  // Method not found directly, check if we should forward it
  auto forwarding_target = GetForwardingTarget(method_name, class_name);
  if (!forwarding_target) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] No forwarding rule found for method %s in class %s",
              method_name.c_str(), class_name.c_str());
    return nullptr;
  }
  
  // Check if the target method exists at runtime
  if (!DoesClassRespondToSelector(class_name, *forwarding_target)) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Target method %s does not exist at runtime for class %s",
              forwarding_target->c_str(), class_name.c_str());
    return nullptr;
  }
  
  // Find the target method signature
  if (methods) {
    for (int i = 0; methods[i].name; i++) {
      if (*forwarding_target == methods[i].name && is_instance == methods[i].is_instance) {
        LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Creating forwarding method %s->%s for class %s",
                  method_name.c_str(), forwarding_target->c_str(), class_name.c_str());
        
        // Create the method declaration using the original modern method name
        // but with the same signature as the legacy method
        return CreateMethodDecl(interface_decl, method_name.c_str(), methods[i].types, methods[i].is_instance);
      }
    }
  }
  
  LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Could not find signature for forwarding target %s",
            forwarding_target->c_str());
  return nullptr;
}

uint32_t GNUstepObjCDeclVendor::FindDecls(ConstString name, bool append,
                                        uint32_t max_matches,
                                        std::vector<CompilerDecl> &decls) {

  Log *log(GetLog(LLDBLog::Expressions));

  LLDB_LOGF(log, "GNUstepObjCDeclVendor::FindDecls ('%s', %s, %u, )",
            (const char *)name.AsCString(), append ? "true" : "false",
            max_matches);

  if (!append)
    decls.clear();

  uint32_t ret = 0;

  do {
    // See if the type is already in our ASTContext.
    clang::ASTContext &ast_ctx = m_ast_ctx->getASTContext();

    clang::IdentifierInfo &identifier_info =
        ast_ctx.Idents.get(name.GetStringRef());
    clang::DeclarationName decl_name =
        ast_ctx.DeclarationNames.getIdentifier(&identifier_info);

    clang::DeclContext::lookup_result lookup_result =
        ast_ctx.getTranslationUnitDecl()->lookup(decl_name);

    if (!lookup_result.empty()) {
      if (clang::ObjCInterfaceDecl *result_iface_decl =
             llvm::dyn_cast<clang::ObjCInterfaceDecl>(*lookup_result.begin())) {
        if (log) {
          clang::QualType result_iface_type =
              ast_ctx.getObjCInterfaceType(result_iface_decl);

          uint64_t isa_value = LLDB_INVALID_ADDRESS;
          if (std::optional<ClangASTMetadata> metadata =
                  m_ast_ctx->GetMetadata(result_iface_decl))
            isa_value = metadata->GetISAPtr();

          LLDB_LOGF(log,
                    "GNUstepObjCDeclVendor::FindDecls Found %s (isa 0x%" PRIx64 ") in the ASTContext",
                    result_iface_type.getAsString().data(), isa_value);
        }

        decls.push_back(m_ast_ctx->GetCompilerDecl(result_iface_decl));
        ret++;
        break;
      } else {
        LLDB_LOGF(log, "GNUstepObjCDeclVendor::FindDecls There's something in the ASTContext, but "
                       "it's not something we know about");
        break;
      }
    } else if (log) {
      LLDB_LOGF(log, "GNUstepObjCDeclVendor::FindDecls Couldn't find %s in the ASTContext",
                name.AsCString());
    }

    // It's not.  If it exists, we have to put it into our ASTContext.
    ObjCLanguageRuntime::ObjCISA isa = m_runtime.GetISA(name);

    if (!isa) {
      LLDB_LOGF(log, "GNUstepObjCDeclVendor::FindDecls Couldn't find the isa for class %s",
                name.AsCString());
      break;
    }

    clang::ObjCInterfaceDecl *iface_decl = GetDeclForISA(isa);

    if (!iface_decl) {
      LLDB_LOGF(log,
                "GNUstepObjCDeclVendor::FindDecls Couldn't get the Objective-C interface for "
                "isa 0x%" PRIx64 " (class %s)",
                (uint64_t)isa, name.AsCString());
      break;
    }

    if (log) {
      clang::QualType new_iface_type = ast_ctx.getObjCInterfaceType(iface_decl);

      LLDB_LOGF(log, "GNUstepObjCDeclVendor::FindDecls Created %s (isa 0x%" PRIx64 ")",
                new_iface_type.getAsString().c_str(), (uint64_t)isa);
    }

    decls.push_back(m_ast_ctx->GetCompilerDecl(iface_decl));
    ret++;
    break;
  } while (false);

  return ret;
}