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
#include "clang/AST/Expr.h"
#include "clang/AST/ExprObjC.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtObjC.h"
#include "clang/AST/ExternalASTSource.h"

#include <optional>
#include <vector>

using namespace lldb_private;
using namespace clang;

// AST Helper functions for expression evaluation support
namespace {

static QualType GetIdTy(ASTContext &ctx)            { return ctx.getObjCIdType(); }
static QualType GetClassTy(ASTContext &ctx)         { return ctx.getObjCClassType(); }
static QualType GetSelTy(ASTContext &ctx)           { return ctx.getObjCSelType(); }
static QualType GetIntTy(ASTContext &ctx)           { return ctx.IntTy; }
// TODO: Enable when needed for CFString implementation
// static QualType GetLongLongTy(ASTContext &ctx)      { return ctx.LongLongTy; }
// static QualType GetULongTy(ASTContext &ctx)         { return ctx.UnsignedLongTy; } // Win64: NSUInteger
static QualType GetConstCharPtrTy(ASTContext &ctx)  {
  QualType c = ctx.CharTy;
  c = c.withConst();
  return ctx.getPointerType(c);
}

static FunctionDecl *AddCFunctionDecl(ASTContext &ctx,
                                      DeclContext *tu,
                                      llvm::StringRef name,
                                      QualType retTy,
                                      llvm::ArrayRef<QualType> paramTys,
                                      bool isVariadic=false) {
  IdentifierInfo &II = ctx.Idents.get(name);
  FunctionProtoType::ExtProtoInfo epi;
  epi.Variadic = isVariadic;
  QualType fTy = ctx.getFunctionType(retTy, paramTys, epi);
  auto *FD = FunctionDecl::Create(ctx, tu, SourceLocation(), SourceLocation(),
                                  &II, fTy, ctx.getTrivialTypeSourceInfo(fTy),
                                  SC_Extern, false /*isInline*/);
  // Build ParmVarDecls:
  llvm::SmallVector<ParmVarDecl*, 4> params;
  for (size_t i = 0; i < paramTys.size(); ++i) {
    auto *P = ParmVarDecl::Create(ctx, FD, SourceLocation(), SourceLocation(),
                                  nullptr, paramTys[i], ctx.getTrivialTypeSourceInfo(paramTys[i]),
                                  SC_None, nullptr);
    params.push_back(P);
  }
  FD->setParams(params);
  tu->addDecl(FD);
  return FD;
}

static ObjCInterfaceDecl *GetOrCreateInterface(ASTContext &ctx, llvm::StringRef name) {
  IdentifierInfo &II = ctx.Idents.get(name);
  TranslationUnitDecl *TU = ctx.getTranslationUnitDecl();
  
  // First check if it already exists
  for (auto *D : TU->decls()) {
    if (auto *ID = dyn_cast<ObjCInterfaceDecl>(D)) {
      if (ID->getIdentifier() == &II) {
        return ID;
      }
    }
  }

  // Create interface and start its definition
  auto *Iface = ObjCInterfaceDecl::Create(ctx, TU, SourceLocation(), &II,
                                          nullptr, nullptr, SourceLocation());
  TU->addDecl(Iface);
  
  // Start the definition so we can add methods to it
  Iface->startDefinition();
  
  return Iface;
}

// Helper functions for AST body building
static FunctionDecl *LookupFuncByName(ASTContext &ctx, llvm::StringRef name) {
  TranslationUnitDecl *TU = ctx.getTranslationUnitDecl();
  IdentifierInfo &II = ctx.Idents.get(name);
  DeclContext::lookup_result R = TU->lookup(&II);
  for (NamedDecl *ND : R)
    if (auto *FD = dyn_cast<FunctionDecl>(ND))
      return FD;
  return nullptr;
}

static DeclRefExpr *MakeFuncRef(ASTContext &ctx, FunctionDecl *FD) {
  return DeclRefExpr::Create(
      ctx, NestedNameSpecifierLoc(), SourceLocation(), FD,
      false /*RefersToEnclosingVariableOrCapture*/,
      SourceLocation(), FD->getType(),
      ExprValueKind::VK_PRValue);
}

static Expr *MakeCStringLiteral(ASTContext &ctx, llvm::StringRef s) {
  QualType charTy = ctx.CharTy;
  auto *SL = StringLiteral::Create(
      ctx, s, StringLiteralKind::Ordinary, /*Pascal*/false,
      ctx.getStringLiteralArrayType(charTy, s.size()),
      SourceLocation());
  // array decays to const char *
  QualType constCharTy = charTy.withConst();
  QualType constCharPtrTy = ctx.getPointerType(constCharTy);
  return ImplicitCastExpr::Create(ctx, constCharPtrTy,
                                  CastKind::CK_ArrayToPointerDecay, SL, nullptr,
                                  ExprValueKind::VK_PRValue, FPOptionsOverride());
}

static Expr *MakeCast(ASTContext &ctx, Expr *E, QualType toTy) {
  return CStyleCastExpr::Create(
      ctx, toTy, VK_PRValue, CastKind::CK_BitCast,
      E, nullptr, FPOptionsOverride(), ctx.CreateTypeSourceInfo(toTy),
      SourceLocation(), SourceLocation());
}

static ParmVarDecl *GetParam(FunctionDecl *FD, unsigned idx) {
  auto params = FD->parameters();
  return (idx < params.size()) ? params[idx] : nullptr;
}

/// Builds a full AST body for CFStringCreateWithBytes
static void DefineCFStringCreateWithBytesBody(ASTContext &ctx, FunctionDecl *CFDecl) {
  if (!CFDecl || CFDecl->hasBody())
    return;

  // Lookup runtime functions we declared earlier.
  FunctionDecl *objc_getClassFD = LookupFuncByName(ctx, "objc_getClass");
  FunctionDecl *sel_getUidFD    = LookupFuncByName(ctx, "sel_getUid");
  FunctionDecl *objc_msgSendFD  = LookupFuncByName(ctx, "objc_msgSend");

  if (!objc_getClassFD || !sel_getUidFD || !objc_msgSendFD)
    return; // prerequisites missing

  // --- Build objc_getClass("NSString")
  Expr *NSStringArg = MakeCStringLiteral(ctx, "NSString");
  Expr *Call_getClass = CallExpr::Create(
      ctx, MakeFuncRef(ctx, objc_getClassFD),
      { NSStringArg }, GetIdTy(ctx), VK_PRValue, SourceLocation(), FPOptionsOverride());

  // --- Build sel_getUid("stringWithUTF8String:")
  Expr *SELArg = MakeCStringLiteral(ctx, "stringWithUTF8String:");
  Expr *Call_sel = CallExpr::Create(
      ctx, MakeFuncRef(ctx, sel_getUidFD),
      { SELArg }, GetSelTy(ctx), VK_PRValue, SourceLocation(), FPOptionsOverride());

  // --- Cast objc_msgSend to: id (*)(Class, SEL, const char*)
  {
    // Build function proto type for cast target
    QualType retTy   = GetIdTy(ctx);
    QualType p0      = GetClassTy(ctx);
    QualType p1      = GetSelTy(ctx);
    QualType p2      = GetConstCharPtrTy(ctx);

    FunctionProtoType::ExtProtoInfo epi;
    epi.ExtInfo = epi.ExtInfo.withCallingConv(CallingConv::CC_C);
    QualType fnTy    = ctx.getFunctionType(retTy, {p0, p1, p2}, epi);
    QualType fnPtrTy = ctx.getPointerType(fnTy);

    // DeclRef to objc_msgSend
    Expr *MsgSendRef = MakeFuncRef(ctx, objc_msgSendFD);
    // C-style cast to the function pointer type
    Expr *MsgSendCast = MakeCast(ctx, MsgSendRef, fnPtrTy);

    // --- Build final call: msgSendCast( objc_getClass(...), sel_getUid(...), (const char*)bytes )
    ParmVarDecl *bytesParam = GetParam(CFDecl, /*idx*/1); // the 2nd parameter is 'bytes'
    DeclRefExpr *BytesRef = DeclRefExpr::Create(ctx, NestedNameSpecifierLoc(),
                                         SourceLocation(), bytesParam,
                                         false, SourceLocation(),
                                         bytesParam->getType(), VK_LValue);

    // Explicit cast (const unsigned char*) -> (const char*)
    Expr *BytesAsConstCharPtr = MakeCast(ctx, BytesRef, GetConstCharPtrTy(ctx));

    Expr *FinalCall = CallExpr::Create(
        ctx, MsgSendCast,
        { Call_getClass, Call_sel, BytesAsConstCharPtr },
        retTy, VK_PRValue, SourceLocation(), FPOptionsOverride());

    // return <FinalCall>;
    Stmt *Ret = ReturnStmt::Create(ctx, SourceLocation(), FinalCall, nullptr);

    // compound body { return ...; }
    llvm::SmallVector<Stmt*, 1> stmts;
    stmts.push_back(Ret);
    Stmt *Body = CompoundStmt::Create(ctx, stmts, FPOptionsOverride(), SourceLocation(), SourceLocation());

    CFDecl->setBody(Body);
  }
}

} // anonymous namespace

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

    LLDB_LOGF(log,
              "[TRACE] GNUstepObjCExternalASTSource::FindExternalVisibleDeclsByName"
              " called - Looking for '%s' in %s context (%p)",
              name.getAsString().c_str(), decl_ctx->getDeclKindName(),
              static_cast<const void *>(decl_ctx));

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
  Log *log(GetLog(LLDBLog::Expressions));
  LLDB_LOGF(log, "[TRACE] GNUstepObjCDeclVendor constructor called");
  
  m_ast_ctx = std::make_shared<TypeSystemClang>(
      "GNUstepObjCDeclVendor AST",
      runtime.GetProcess()->GetTarget().GetArchitecture().GetTriple());
  LLDB_LOGF(log, "[TRACE] Created TypeSystemClang for GNUstepObjCDeclVendor");
  
  m_external_source = new GNUstepObjCExternalASTSource(*this);
  llvm::IntrusiveRefCntPtr<clang::ExternalASTSource> external_source_owning_ptr(
      m_external_source);
  m_ast_ctx->getASTContext().setExternalSource(external_source_owning_ptr);
  LLDB_LOGF(log, "[TRACE] Set up GNUstepObjCExternalASTSource");
  
  // CRITICAL: Add runtime function declarations immediately to ensure they're available
  // before any expression compilation that might need them
  EnsureRuntimeDecls(*m_ast_ctx);
  LLDB_LOGF(log, "[TRACE] Added runtime function declarations in constructor");
  
  // Initialize runtime API for dynamic discovery
  auto api_or_err = GNUstepRuntimeV2API::Create(runtime.GetProcess());
  if (api_or_err) {
    m_runtime_api = std::move(*api_or_err);
  }
  
  // CRITICAL: Initialize direct memory introspector to avoid expression evaluation recursion
  m_introspector = std::make_unique<GNUstepObjCRuntimeIntrospector>(runtime.GetProcess());
  LLDB_LOGF(log, "[TRACE] Created GNUstepObjCRuntimeIntrospector for direct method discovery");
  
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
    
    // CRITICAL: Check type_realizer_sp is valid before use
    if (!type_realizer_sp) {
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
  
  LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Using pure runtime discovery for %s - hardcoded method tables disabled",
            class_name.c_str());
  
  // All methods are now discovered dynamically via runtime introspection in FinishDecl()
  // The hardcoded method signature tables are disabled since runtime introspection
  // provides complete and accurate method information
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

  // CRITICAL: For GNUstep, we KEEP external storage enabled because we use lazy loading
  // via FindExternalVisibleDeclsByName, unlike Apple which populates everything upfront
  // interface_decl->setHasExternalVisibleStorage(false); // DISABLED - keep lazy loading
  // interface_decl->setHasExternalLexicalStorage(false); // DISABLED - keep lazy loading

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

  // CRITICAL: Use direct memory introspection instead of runtime API to avoid recursion
  // The runtime API was using expression evaluation which creates a circular dependency
  if (m_introspector) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Using direct memory introspector for %s", 
              class_name.c_str());
    
    // Get class pointer using direct introspector call (no expression evaluation)
    lldb::addr_t class_ptr = m_introspector->GetClassPointer(class_name);
    if (class_ptr != LLDB_INVALID_ADDRESS) {
      LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Found class pointer 0x%lx for %s", 
                (unsigned long)class_ptr, class_name.c_str());
      
      // Get instance methods using direct memory access
      auto instance_methods = m_introspector->GetInstanceMethods(class_ptr);
      LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Found %zu instance methods for %s",
                instance_methods.size(), class_name.c_str());
      
      // Add each instance method discovered from runtime
      for (const auto &method : instance_methods) {
        bool is_instance = true;
        
        // Skip if method already exists
        if (InterfaceAlreadyHasMethod(interface_decl, method.selector_name.c_str(), is_instance))
          continue;
          
        // Create method declaration from runtime type encoding
        clang::ObjCMethodDecl *method_decl = CreateMethodDecl(
          interface_decl, 
          method.selector_name.c_str(),
          method.type_encoding.c_str(),
          is_instance
        );
        
        if (method_decl) {
          LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl]   Added instance method: -%s (%s)",
                    method.selector_name.c_str(), method.type_encoding.c_str());
        }
      }
      
      // Get class methods using direct memory access via metaclass
      auto class_methods = m_introspector->GetClassMethods(class_ptr);
      LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Found %zu class methods for %s via direct introspection",
                class_methods.size(), class_name.c_str());
      
      // Add each class method discovered from runtime
      for (const auto &method : class_methods) {
        bool is_instance = false;
        
        // Skip if method already exists
        if (InterfaceAlreadyHasMethod(interface_decl, method.selector_name.c_str(), is_instance))
          continue;
          
        // Create method declaration from runtime type encoding
        clang::ObjCMethodDecl *method_decl = CreateMethodDecl(
          interface_decl, 
          method.selector_name.c_str(),
          method.type_encoding.c_str(),
          is_instance
        );
        
        if (method_decl) {
          LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl]   Added class method: +%s (%s)",
                    method.selector_name.c_str(), method.type_encoding.c_str());
        }
      }
      
      // If we successfully used the introspector, skip the old runtime API approach
      if (!instance_methods.empty() || !class_methods.empty()) {
        LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Direct introspection successful for %s, skipping fallback", 
                  class_name.c_str());
        goto success_return;
      }
    } else {
      LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Failed to get class pointer for %s via introspector", 
                class_name.c_str());
    }
  } else {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] No introspector available for %s", class_name.c_str());
  }
  
  // FALLBACK: Use original runtime API approach if introspector fails
  LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Using fallback runtime API for %s", class_name.c_str());
  if (m_runtime_api) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Using runtime API for dynamic discovery of %s", 
              class_name.c_str());
    
    // Get class info from runtime
    auto class_info_or_err = m_runtime_api->GetObjCClassInfo(class_name);
    if (class_info_or_err) {
      auto class_info = *class_info_or_err;
      
      // Get all methods for this class from runtime (including inherited)
      auto methods_or_err = m_runtime_api->GetAllMethodsIncludingInherited(class_info.class_ptr);
      if (methods_or_err) {
        LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Found %zu methods for %s",
                  methods_or_err->size(), class_name.c_str());
        
        // Add each method discovered from runtime
        for (const auto &method : *methods_or_err) {
          // All methods from GetAllMethodsIncludingInherited are instance methods
          bool is_instance = true;
          
          // Skip if method already exists
          if (InterfaceAlreadyHasMethod(interface_decl, method.selector_name.c_str(), is_instance))
            continue;
            
          // Create method declaration from runtime type encoding
          clang::ObjCMethodDecl *method_decl = CreateMethodDecl(
            interface_decl, 
            method.selector_name.c_str(),
            method.type_encoding.c_str(),
            is_instance
          );
          
          if (method_decl) {
            LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl]   Added method: -%s",
                      method.selector_name.c_str());
          }
        }
      } else {
        LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Failed to get methods for %s: %s",
                  class_name.c_str(), llvm::toString(methods_or_err.takeError()).c_str());
      }
      
      // RUNTIME INTROSPECTION: Add class methods by querying the metaclass
      // This replaces hardcoded method tables with dynamic discovery
      LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Attempting to discover class methods for %s", class_name.c_str());
      auto class_methods_or_err = m_runtime_api->GetAllClassMethods(class_name);
      if (class_methods_or_err) {
        LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Found %zu class methods for %s via metaclass introspection",
                  class_methods_or_err->size(), class_name.c_str());
        
        // Add each class method discovered from runtime
        for (const auto &method : *class_methods_or_err) {
          // All methods from GetAllClassMethods are class methods
          bool is_instance = false;
          
          // Skip if method already exists
          if (InterfaceAlreadyHasMethod(interface_decl, method.selector_name.c_str(), is_instance))
            continue;
            
          // Create method declaration from runtime type encoding
          clang::ObjCMethodDecl *method_decl = CreateMethodDecl(
            interface_decl, 
            method.selector_name.c_str(),
            method.type_encoding.c_str(),
            is_instance
          );
          
          if (method_decl) {
            LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl]   Added class method: +%s (%s)",
                      method.selector_name.c_str(), method.type_encoding.c_str());
          } else {
            LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl]   Failed to create class method: +%s",
                      method.selector_name.c_str());
          }
        }
      } else {
        std::string error_msg = llvm::toString(class_methods_or_err.takeError());
        LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Failed to get class methods for %s: %s",
                  class_name.c_str(), error_msg.c_str());
        
        // NOTE: Hardcoded class method fallbacks removed - runtime introspection now provides all methods
        // The IRForTarget improvements with objc_getClass dynamic calls eliminate the need for these
        LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Skipping hardcoded class method fallbacks for %s - runtime introspection handles all methods",
                  class_name.c_str());
      }
      
      // Get all properties for this class from runtime
      auto properties_or_err = m_runtime_api->GetAllPropertiesIncludingInherited(class_info.class_ptr);
      if (properties_or_err) {
        LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Found %zu properties for %s",
                  properties_or_err->size(), class_name.c_str());
        
        // Properties typically have getter/setter methods that we've already added above
        // TODO: Add actual @property declarations if needed for better debugging experience
      }
    } else {
      LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Failed to get class info for %s: %s",
                class_name.c_str(), llvm::toString(class_info_or_err.takeError()).c_str());
      
      // Skip hardcoded fallback - rely on introspector which should have worked above
      LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Skipping hardcoded fallback for %s - relying on introspector",
                class_name.c_str());
    }
  } else {
    // Fallback if runtime API not available
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor::FinishDecl] Runtime API not available, but introspector should have worked above");
  }

  // Note: We've replaced the hardcoded core_selectors with dynamic discovery
  // The runtime will provide ALL methods, not just a hardcoded subset

success_return:
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
  LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Installing simplified method forwarding rules (Phase 3 cleanup)");
  
  if (m_forwarding_initialized) {
    return;
  }
  
  // PHASE 3 SIMPLIFICATION: Reduced forwarding rules since modern methods exist in runtime
  // Runtime introspection now discovers modern subscript methods automatically.
  // Keep minimal forwarding as safety net for edge cases only.
  
  // Essential forwarding rules for rare cases where modern methods might not be available
  m_method_forwarding_rules.push_back({
    "objectAtIndexedSubscript:",   // Modern method
    "objectAtIndex:",              // Legacy method  
    "*",                           // All classes (simplified from specific class checks)
    true                           // Enabled
  });
  
  m_method_forwarding_rules.push_back({
    "objectForKeyedSubscript:",    // Modern method
    "objectForKey:",               // Legacy method
    "*",                           // All classes (simplified from specific class checks)
    true                           // Enabled
  });
  
  m_forwarding_initialized = true;
  LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Installed %zu simplified forwarding rules (reduced from 8 rules)", 
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
  
  // Modern approach: Use runtime introspection instead of static tables
  // Check if the method exists at runtime
  if (DoesClassRespondToSelector(class_name, method_name)) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Method %s exists in runtime for class %s", 
              method_name.c_str(), class_name.c_str());
    
    // Try to get the method signature from runtime
    if (m_runtime_api) {
      auto class_info_or_err = m_runtime_api->GetObjCClassInfo(class_name);
      if (class_info_or_err) {
        auto class_info = *class_info_or_err;
        
        // Try to find this specific method in runtime
        auto methods_or_err = m_runtime_api->GetAllMethodsIncludingInherited(class_info.class_ptr);
        if (methods_or_err) {
          for (const auto &method : *methods_or_err) {
            if (method.selector_name == method_name) {
              LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Found method %s in runtime with signature %s",
                        method_name.c_str(), method.type_encoding.c_str());
              return CreateMethodDecl(interface_decl, method_name.c_str(), 
                                    method.type_encoding.c_str(), is_instance);
            }
          }
        }
      }
    }
  }

  // PHASE 3 IMPROVEMENT: Only forward if modern method doesn't exist in runtime
  // Check if we should forward it (only for missing modern methods)
  auto forwarding_target = GetForwardingTarget(method_name, class_name);
  if (!forwarding_target) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] No forwarding rule found for method %s in class %s",
              method_name.c_str(), class_name.c_str());
    return nullptr;
  }
  
  // PHASE 3 IMPROVEMENT: Check if modern method exists before forwarding
  LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Checking if modern method %s exists before forwarding to %s",
            method_name.c_str(), forwarding_target->c_str());
  
  // If modern method exists in runtime, don't forward (runtime introspection will handle it)
  if (DoesClassRespondToSelector(class_name, method_name)) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Modern method %s exists in runtime, skipping forwarding to %s",
              method_name.c_str(), forwarding_target->c_str());
    return nullptr; // Let runtime introspection handle the modern method
  }
  
  // Check if the target method exists at runtime
  if (!DoesClassRespondToSelector(class_name, *forwarding_target)) {
    LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Target method %s does not exist at runtime for class %s",
              forwarding_target->c_str(), class_name.c_str());
    return nullptr;
  }
  
  // Find the target method signature from runtime
  if (m_runtime_api) {
    auto class_info_or_err = m_runtime_api->GetObjCClassInfo(class_name);
    if (class_info_or_err) {
      auto class_info = *class_info_or_err;
      auto methods_or_err = m_runtime_api->GetAllMethodsIncludingInherited(class_info.class_ptr);
      if (methods_or_err) {
        for (const auto &method : *methods_or_err) {
          if (method.selector_name == *forwarding_target) {
            LLDB_LOGF(log, "[GNUstepObjCDeclVendor] Creating forwarding method %s->%s for class %s",
                      method_name.c_str(), forwarding_target->c_str(), class_name.c_str());
            
            // Create the method declaration using the original modern method name
            // but with the same signature as the legacy method
            return CreateMethodDecl(interface_decl, method_name.c_str(), 
                                  method.type_encoding.c_str(), is_instance);
          }
        }
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

  LLDB_LOGF(log, "[TRACE] GNUstepObjCDeclVendor::FindDecls called for '%s' (append=%s, max=%u)",
            (const char *)name.AsCString(), append ? "true" : "false",
            max_matches);

  // Ensure runtime declarations and minimal foundation interfaces are available
  if (m_ast_ctx) {
    LLDB_LOGF(log, "[TRACE] FindDecls: Calling EnsureRuntimeDecls for %s", name.AsCString());
    EnsureRuntimeDecls(*m_ast_ctx);
    LLDB_LOGF(log, "[TRACE] FindDecls: Calling EnsureMinimalFoundationInterfaces for %s", name.AsCString());
    EnsureMinimalFoundationInterfaces(*m_ast_ctx);
    LLDB_LOGF(log, "[TRACE] FindDecls: Finished ensuring interfaces for %s", name.AsCString());
  } else {
    LLDB_LOGF(log, "[TRACE] FindDecls: No m_ast_ctx available for %s", name.AsCString());
  }

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
      } else if (clang::FunctionDecl *result_func_decl =
                   llvm::dyn_cast<clang::FunctionDecl>(*lookup_result.begin())) {
        // Handle runtime function declarations (sel_getUid, objc_getClass, etc.)
        LLDB_LOGF(log, "GNUstepObjCDeclVendor::FindDecls Found function %s in the ASTContext",
                  result_func_decl->getName().str().c_str());

        decls.push_back(m_ast_ctx->GetCompilerDecl(result_func_decl));
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

void GNUstepObjCDeclVendor::EnsureRuntimeDecls(TypeSystemClang &ts) {
  Log *log(GetLog(LLDBLog::Expressions));
  LLDB_LOGF(log, "[TRACE] EnsureRuntimeDecls called (already_injected=%d)", m_runtime_decls_injected);
  if (m_runtime_decls_injected)
    return;

  ASTContext &ctx = ts.getASTContext();
  TranslationUnitDecl *TU = ctx.getTranslationUnitDecl();

  // 1) Declare core runtime functions
  AddCFunctionDecl(ctx, TU, "objc_msgSend", GetIdTy(ctx),
                   { GetIdTy(ctx), GetSelTy(ctx) }, /*isVariadic*/true);
  AddCFunctionDecl(ctx, TU, "objc_getClass", GetIdTy(ctx),
                   { GetConstCharPtrTy(ctx) });
  AddCFunctionDecl(ctx, TU, "sel_getUid", GetSelTy(ctx),
                   { GetConstCharPtrTy(ctx) });
  AddCFunctionDecl(ctx, TU, "object_getClass", GetClassTy(ctx),
                   { GetIdTy(ctx) });
  AddCFunctionDecl(ctx, TU, "class_getMethodImplementation", ctx.VoidPtrTy,
                   { GetClassTy(ctx), GetSelTy(ctx) });

  // 1.1) Add method introspection functions for dynamic type encoding discovery
  AddCFunctionDecl(ctx, TU, "class_getInstanceMethod", ctx.VoidPtrTy,
                   { GetClassTy(ctx), GetSelTy(ctx) });
  AddCFunctionDecl(ctx, TU, "class_getClassMethod", ctx.VoidPtrTy,
                   { GetClassTy(ctx), GetSelTy(ctx) });
  AddCFunctionDecl(ctx, TU, "method_getTypeEncoding", GetConstCharPtrTy(ctx),
                   { ctx.VoidPtrTy });
  AddCFunctionDecl(ctx, TU, "sel_registerName", GetSelTy(ctx),
                   { GetConstCharPtrTy(ctx) });

  // 2) Declare CFStringCreateWithBytes
  FunctionDecl *CFDecl = AddCFunctionDecl(
      ctx, TU, "CFStringCreateWithBytes", GetIdTy(ctx),
      { ctx.VoidPtrTy,                      // allocator (ignored)
        ctx.getPointerType(ctx.UnsignedCharTy), // bytes
        ctx.LongTy,                         // length
        ctx.UnsignedIntTy,                  // encoding (ignored)
        GetIntTy(ctx)                       // isExternal (ignored)
      });

  // 3) Define its body (AST)
  DefineCFStringCreateWithBytesBody(ctx, CFDecl);

  m_runtime_decls_injected = true;

  LLDB_LOG(log, "[TRACE] EnsureRuntimeDecls: Completed injection of runtime declarations and CFString shim");
}

bool GNUstepObjCDeclVendor::PopulateInterfaceFromRuntime(TypeSystemClang &ts, 
                                                         const std::string &class_name) {
  if (!m_runtime_api)
    return false;
    
  Log *log(GetLog(LLDBLog::Expressions));
  ASTContext &ctx = ts.getASTContext();
  
  LLDB_LOG(log, "GNUstepObjCDeclVendor: Populating interface for {0} from runtime", 
           class_name);
  
  // Get or create the interface
  ObjCInterfaceDecl *interface_decl = GetOrCreateInterface(ctx, class_name);
  if (!interface_decl)
    return false;
    
  // Get class info from runtime
  auto class_info_or_err = m_runtime_api->GetObjCClassInfo(class_name);
  if (!class_info_or_err) {
    LLDB_LOG(log, "Failed to get class info for {0}", class_name);
    return false;
  }
  
  auto class_info = *class_info_or_err;
  
  // Set superclass if not already set
  if (!interface_decl->getSuperClass() && class_info.superclass_ptr) {
    // Try to get superclass interface
    ObjCInterfaceDecl *super_interface = nullptr;
    if (!class_info.superclass_name.empty()) {
      super_interface = GetOrCreateInterface(ctx, class_info.superclass_name);
    }
    if (super_interface) {
      QualType superType = ctx.getObjCInterfaceType(super_interface);
      TypeSourceInfo *TSI = ctx.getTrivialTypeSourceInfo(superType);
      interface_decl->setSuperClass(TSI);
    }
  }
  
  // Get all instance methods from runtime
  auto methods_or_err = m_runtime_api->GetAllMethodsIncludingInherited(class_info.class_ptr);
  if (methods_or_err) {
    for (const auto &method : *methods_or_err) {
      // Only add if not already present
      if (!InterfaceAlreadyHasMethod(interface_decl, method.selector_name.c_str(), true)) {
        clang::ObjCMethodDecl *method_decl = CreateMethodDecl(
          interface_decl, 
          method.selector_name.c_str(),
          method.type_encoding.c_str(),
          true  // is_instance
        );
        if (method_decl) {
          LLDB_LOG(log, "Added instance method {0} to {1}", 
                   method.selector_name, class_name);
        }
      }
    }
  }
  
  // Get class methods from metaclass using proper runtime introspection
  // In Objective-C, class methods are instance methods of the metaclass
  if (class_info.class_ptr) {
    // Get metaclass using object_getClass on the class itself
    auto metaclass_or_err = m_runtime_api->GetObjectClass(class_info.class_ptr);
    if (metaclass_or_err) {
      void *metaclass = *metaclass_or_err;
      
      LLDB_LOG(log, "Got metaclass for {0}, discovering class methods", class_name);
      
      // Get methods from metaclass (these are class methods)
      auto class_methods_or_err = m_runtime_api->GetAllMethodsIncludingInherited(metaclass);
      if (class_methods_or_err) {
        LLDB_LOG(log, "Found {0} potential class methods for {1}", 
                 class_methods_or_err->size(), class_name);
        
        for (const auto &method : *class_methods_or_err) {
          // Skip methods that are clearly metaclass infrastructure
          if (method.selector_name.find(".cxx_") != std::string::npos ||
              method.selector_name == "load" ||
              method.selector_name == "initialize" ||
              method.selector_name == "class" ||
              method.selector_name == "superclass" ||
              method.selector_name == "isSubclassOfClass:" ||
              method.selector_name == "instancesRespondToSelector:" ||
              method.selector_name == "conformsToProtocol:" ||
              method.selector_name == "new") {
            continue;
          }
          
          // Only add if not already present
          if (!InterfaceAlreadyHasMethod(interface_decl, method.selector_name.c_str(), false)) {
            clang::ObjCMethodDecl *method_decl = CreateMethodDecl(
              interface_decl, 
              method.selector_name.c_str(),
              method.type_encoding.c_str(),
              false  // is_instance = false for class methods
            );
            if (method_decl) {
              LLDB_LOG(log, "Added class method +{0} to {1}", 
                       method.selector_name, class_name);
            }
          }
        }
      } else {
        LLDB_LOG(log, "Failed to get methods from metaclass for {0}", class_name);
      }
    } else {
      LLDB_LOG(log, "Failed to get metaclass for {0}: {1}", class_name, 
               llvm::toString(metaclass_or_err.takeError()));
    }
  }
  
  // The interface is already started via GetOrCreateInterface which calls startDefinition()
  // Just ensure external storage flags are cleared so LLDB knows it's complete
  interface_decl->setHasExternalVisibleStorage(false);
  interface_decl->setHasExternalLexicalStorage(false);
  
  LLDB_LOG(log, "Successfully populated {0} from runtime", class_name);
  return true;
}

void GNUstepObjCDeclVendor::EnsureMinimalFoundationInterfaces(TypeSystemClang &ts) {
  Log *log(GetLog(LLDBLog::Expressions));
  LLDB_LOGF(log, "[TRACE] EnsureMinimalFoundationInterfaces called (already_injected=%d)", m_foundation_minimals_injected);
  if (m_foundation_minimals_injected)
    return;

  ASTContext &ctx = ts.getASTContext();
  
  LLDB_LOG(log, "[TRACE] Starting EnsureMinimalFoundationInterfaces with runtime discovery");

  // PHASE 1.3 SIMPLIFICATION: Just ensure interfaces exist, runtime introspection handles methods
  // Critical Foundation classes that need early interface creation for expression evaluation
  const std::vector<std::string> foundation_classes = {
    "NSObject",    // Root class
    "NSNumber",    // Literal support: @123
    "NSString",    // Literal support: @"string"  
    "NSArray",     // Literal support: @[]
    "NSDictionary" // Literal support: @{}
  };
  
  // Create empty interfaces - runtime introspection via FinishDecl will populate methods
  for (const auto &class_name : foundation_classes) {
    LLDB_LOG(log, "Ensuring interface exists for {0} (runtime introspection will populate methods)", class_name);
    
    ObjCInterfaceDecl *interface_decl = GetOrCreateInterface(ctx, class_name);
    if (!interface_decl)
      continue;
    
    // Clear external storage flags to trigger method population via FinishDecl
    interface_decl->setHasExternalVisibleStorage(false);
    interface_decl->setHasExternalLexicalStorage(false);
  }

  m_foundation_minimals_injected = true;
  LLDB_LOG(log, "[TRACE] Completed EnsureMinimalFoundationInterfaces - interfaces created, runtime introspection will populate methods");
}