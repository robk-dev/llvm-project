//===-- GNUstepObjCDeclVendor.h --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCDECLVENDOR_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCDECLVENDOR_H

#include "Plugins/ExpressionParser/Clang/ClangDeclVendor.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "GNUstepObjCRuntimeIntrospector.h"
#include "GNUstepRuntimeV2API.h"
#include "../ObjCLanguageRuntime.h"
#include <unordered_map>

namespace clang {
class ObjCInterfaceDecl;
class ExternalASTSource;
}

namespace lldb_private {

class GNUstepObjCExternalASTSource;

class GNUstepObjCDeclVendor : public ClangDeclVendor {
public:
  GNUstepObjCDeclVendor(ObjCLanguageRuntime &runtime);
  ~GNUstepObjCDeclVendor() override = default;

  uint32_t FindDecls(ConstString name, bool append, uint32_t max_matches,
                     std::vector<CompilerDecl> &decls) override;

  clang::ObjCInterfaceDecl *GetDeclForISA(ObjCLanguageRuntime::ObjCISA isa);
  bool FinishDecl(clang::ObjCInterfaceDecl *interface_decl);

  std::shared_ptr<TypeSystemClang> m_ast_ctx;

private:
  typedef std::unordered_map<ObjCLanguageRuntime::ObjCISA,
                             clang::ObjCInterfaceDecl *> ISAToInterfaceMap;

  ObjCLanguageRuntime &m_runtime;
  GNUstepObjCExternalASTSource *m_external_source;
  ISAToInterfaceMap m_isa_to_interface;
  ObjCLanguageRuntime::EncodingToTypeSP m_type_realizer_sp;

  // Helper methods
  void AddFoundationClassMethods(clang::ObjCInterfaceDecl *interface_decl,
                                 const std::string &class_name);
  clang::ObjCMethodDecl *CreateMethodDecl(clang::ObjCInterfaceDecl *interface_decl,
                                           const char *name, const char *types,
                                           bool is_instance);

  // Method forwarding infrastructure for modern subscript syntax
  struct MethodForwardingInfo {
    std::string modern_method;      // e.g., "objectAtIndexedSubscript:"
    std::string legacy_method;      // e.g., "objectAtIndex:"
    std::string class_prefix;       // e.g., "NSArray" or "*" for all classes
    bool enabled;
  };

  // Method resolution and forwarding
  clang::ObjCMethodDecl *ResolveMethodWithForwarding(
      clang::ObjCInterfaceDecl *interface_decl,
      const std::string &method_name,
      const std::string &class_name,
      bool is_instance);

  // Check if a method should be forwarded to another method
  std::optional<std::string> GetForwardingTarget(
      const std::string &method_name, 
      const std::string &class_name);

  // Install default method forwarding rules
  void InstallDefaultForwardingRules();

  // Runtime method existence checking
  bool DoesClassRespondToSelector(const std::string &class_name,
                                  const std::string &selector_name);

public:
  // Expression evaluation support
  void EnsureRuntimeDecls(TypeSystemClang &ts);
  void EnsureMinimalFoundationInterfaces(TypeSystemClang &ts);
  
  // Runtime-based interface population
  bool PopulateInterfaceFromRuntime(TypeSystemClang &ts, const std::string &class_name);

private:
  // Method forwarding table
  std::vector<MethodForwardingInfo> m_method_forwarding_rules;
  bool m_forwarding_initialized;
  
  // Track if we've injected runtime decls and foundation interfaces
  bool m_runtime_decls_injected = false;
  bool m_foundation_minimals_injected = false;
  
  // Runtime API for dynamic class/method discovery
  std::unique_ptr<GNUstepRuntimeV2API> m_runtime_api;
  
  // CRITICAL: Direct memory introspector for breaking expression evaluation recursion
  std::unique_ptr<GNUstepObjCRuntimeIntrospector> m_introspector;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCDECLVENDOR_H
