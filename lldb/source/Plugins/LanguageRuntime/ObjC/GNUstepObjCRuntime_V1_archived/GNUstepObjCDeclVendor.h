//===-- GNUstepObjCDeclVendor.h ------------------------------------*- C++ -*-===//
//
// Provides dynamic type creation for GNUstep Objective-C runtime
// Creates clang::ObjCInterfaceDecl objects for runtime classes
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPOBJCDECLVENDOR_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPOBJCDECLVENDOR_H

#include "lldb/lldb-private.h"

#include "Plugins/ExpressionParser/Clang/ClangDeclVendor.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

#include <map>
#include <memory>
#include <set>

#include "clang/AST/ExternalASTSource.h"

class ClangASTImporter;

namespace lldb_private {

// Forward declaration
class GNUstepObjCDeclVendor;

// External AST source for dynamic property/method lookups
class GNUstepObjCExternalASTSource : public clang::ExternalASTSource {
public:
  GNUstepObjCExternalASTSource(GNUstepObjCDeclVendor &decl_vendor)
      : m_decl_vendor(decl_vendor) {}

  // Called when Clang needs to find declarations in a context
  bool FindExternalVisibleDeclsByName(
      const clang::DeclContext *decl_ctx, clang::DeclarationName name,
      const clang::DeclContext *original_dc) override;

  // Called to complete an interface
  void CompleteInterface(clang::ObjCInterfaceDecl *interface_decl);

private:
  GNUstepObjCDeclVendor &m_decl_vendor;
};

class GNUstepObjCDeclVendor : public ClangDeclVendor {
public:
  GNUstepObjCDeclVendor(ObjCLanguageRuntime &runtime);
  ~GNUstepObjCDeclVendor() override;

  // DeclVendor interface
  uint32_t FindDecls(ConstString name, bool append, uint32_t max_matches,
                     std::vector<CompilerDecl> &decls) override;

  // Create type for a specific ISA
  clang::ObjCInterfaceDecl *GetDeclForISA(ObjCLanguageRuntime::ObjCISA isa);
  
  // Create type for a class name
  clang::ObjCInterfaceDecl *GetDeclForClassName(const char *name);

  // Get CompilerType for an ISA
  CompilerType GetTypeForISA(ObjCLanguageRuntime::ObjCISA isa);

  // Get the TypeSystemClang instance
  std::shared_ptr<TypeSystemClang> GetTypeSystemClang() { return m_ast_ctx; }

  // Register common GNUstep types proactively
  void RegisterCommonTypes();

private:
  // Build ObjC interface declaration for a runtime class
  clang::ObjCInterfaceDecl *BuildInterfaceDecl(const char *name, 
                                                ObjCLanguageRuntime::ObjCISA isa);

  // Add instance variables to a declaration
  bool AddIVarsToDecl(clang::ObjCInterfaceDecl *decl,
                      ObjCLanguageRuntime::ClassDescriptorSP descriptor);

  // Add methods to a declaration  
  bool AddMethodsToDecl(clang::ObjCInterfaceDecl *decl,
                        ObjCLanguageRuntime::ClassDescriptorSP descriptor);

  // Ensure base NSObject type exists
  void EnsureNSObjectDecl();
  
  // Finish declaration (complete definition)
  bool FinishDecl(clang::ObjCInterfaceDecl *decl);

  // Runtime method discovery
  bool DiscoverAndAddRuntimeMethods(clang::ObjCInterfaceDecl *decl, 
                                    ObjCLanguageRuntime::ObjCISA isa);
  
  // Method addition helpers
  void AddBasicNSObjectMethods(clang::ObjCInterfaceDecl *decl, clang::ASTContext &ast_ctx);
  void AddFallbackMethods(clang::ObjCInterfaceDecl *decl, clang::ASTContext &ast_ctx, 
                         const std::string &class_name);
  void AddArrayMethods(clang::ObjCInterfaceDecl *decl, clang::ASTContext &ast_ctx);
  void AddDictionaryMethods(clang::ObjCInterfaceDecl *decl, clang::ASTContext &ast_ctx);
  void AddStringMethods(clang::ObjCInterfaceDecl *decl, clang::ASTContext &ast_ctx);
  void AddCustomClassMethods(clang::ObjCInterfaceDecl *decl, clang::ASTContext &ast_ctx, 
                            const std::string &class_name);
  
  // Add property accessor methods for all ivars
  void AddPropertyAccessorsForIvars(clang::ObjCInterfaceDecl *decl,
                                   ObjCLanguageRuntime::ClassDescriptorSP descriptor);
  
  // Helper to add a method declaration
  clang::ObjCMethodDecl *AddMethodDecl(clang::ObjCInterfaceDecl *decl, 
                                      clang::ASTContext &ast_ctx,
                                      const char *method_name,
                                      clang::QualType return_type,
                                      bool is_instance_method = true);
  
  // Helper to add a method with one parameter
  clang::ObjCMethodDecl *AddMethodWithParameter(clang::ObjCInterfaceDecl *decl,
                                               clang::ASTContext &ast_ctx,
                                               const char *method_name,
                                               clang::QualType return_type,
                                               const char *param_name,
                                               clang::QualType param_type,
                                               bool is_instance_method = true);
  
  // Find or create a method declaration for dynamic method resolution
  clang::ObjCMethodDecl *FindOrCreateMethodDecl(clang::ObjCInterfaceDecl *class_decl,
                                               const char *method_name,
                                               bool is_instance_method);
  
  // Infer return type based on method name patterns  
  clang::QualType InferReturnTypeForMethod(const char *method_name);

  // Allow external source to access our internals
  friend class GNUstepObjCExternalASTSource;

  ObjCLanguageRuntime &m_runtime;
  std::shared_ptr<TypeSystemClang> m_ast_ctx;
  llvm::IntrusiveRefCntPtr<GNUstepObjCExternalASTSource> m_external_source;
  
  // Cache of ISA to declaration mappings
  std::map<ObjCLanguageRuntime::ObjCISA, clang::ObjCInterfaceDecl *> m_isa_to_decl;
  
  // Cache of name to declaration mappings
  std::map<std::string, clang::ObjCInterfaceDecl *> m_name_to_decl;
  
  // Set of types we've started building (for recursion detection)
  std::set<std::string> m_building_types;
  
  // NSObject declaration (base for all ObjC objects)
  clang::ObjCInterfaceDecl *m_nsobject_decl;
  
  static const char *GetCommonTypeName(const char *gnustep_name);
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPOBJCDECLVENDOR_H