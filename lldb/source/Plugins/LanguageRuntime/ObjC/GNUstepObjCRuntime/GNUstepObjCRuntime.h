//===-- GNUstepObjCRuntime.h ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_H

#include "../ObjCLanguageRuntime.h"
#include "lldb/lldb-private.h"
#include "lldb/Core/ModuleList.h"
#include "lldb/Symbol/DeclVendor.h"
#include "lldb/Expression/UtilityFunction.h"

#include "GNUstepObjCRuntimeIntrospector.h"
#include "GNUstepObjCDeclVendor.h"
#include "GNUstepRuntimeV2API.h"

#include <map>
#include <string>

namespace lldb_private {

// Forward declarations
class ExecutionContext;
class TypeSystemClang;

class GNUstepObjCRuntime : public ObjCLanguageRuntime {
public:
  ~GNUstepObjCRuntime() override;

  // Static Functions
  static void Initialize();
  static void Terminate();
  static LanguageRuntime *CreateInstance(Process *process,
                                         lldb::LanguageType language);

  // PluginInterface
  llvm::StringRef GetPluginName() override { return "gnu-objc-v2"; }

  // LanguageRuntime
  llvm::Error GetObjectDescription(Stream &str, ValueObject &object) override;
  llvm::Error GetObjectDescription(Stream &str, Value &value,
                                   ExecutionContextScope *exe_scope) override;

  bool GetDynamicTypeAndAddress(ValueObject &in_value,
                                lldb::DynamicValueType use_dynamic,
                                TypeAndOrName &class_type_or_name,
                                Address &address,
                                Value::ValueType &value_type) override;

  TypeAndOrName FixUpDynamicType(const TypeAndOrName &type_and_or_name,
                                 ValueObject &static_value) override;

  bool CouldHaveDynamicValue(ValueObject &in_value) override;

  lldb::BreakpointResolverSP
  CreateExceptionResolver(const lldb::BreakpointSP &bkpt, bool catch_bp,
                          bool throw_bp) override;

  lldb::ThreadPlanSP GetStepThroughTrampolinePlan(Thread &thread,
                                                  bool stop_others) override;

  // ObjCLanguageRuntime
  bool IsModuleObjCLibrary(const lldb::ModuleSP &module_sp) override;
  bool ReadObjCLibrary(const lldb::ModuleSP &module_sp) override;
  bool HasReadObjCLibrary() override;
  
  // Support for modern ObjC literals and subscripting
  bool CalculateHasNewLiteralsAndIndexing() override;

  DeclVendor *GetDeclVendor() override;

  llvm::Expected<std::unique_ptr<UtilityFunction>>
  CreateObjectChecker(std::string name, ExecutionContext &exe_ctx) override;
  
  // Create utility functions for modern subscript syntax support
  llvm::Expected<std::unique_ptr<UtilityFunction>>
  CreateSubscriptUtilityFunctions(ExecutionContext &exe_ctx);

  void UpdateISAToDescriptorMapIfNeeded() override;
  
  // ClassDescriptor support
  ClassDescriptorSP GetClassDescriptorFromClassName(ConstString class_name) override;
  ClassDescriptorSP GetClassDescriptorFromISA(ObjCISA isa) override;
  ClassDescriptorSP GetClassDescriptor(ValueObject &valobj) override;
  
  // LanguageRuntime overrides
  void ModulesDidLoad(const ModuleList &module_list) override;

  // Constructor (public for make_unique)
  GNUstepObjCRuntime(Process *process);
  
  // Get the runtime API (for GNUstepClassDescriptor)
  GNUstepRuntimeV2API *GetRuntimeAPI() { return m_runtime_api_up.get(); }
  
  // Get the runtime introspector (for direct runtime function calls)
  GNUstepObjCRuntimeIntrospector *GetRuntimeIntrospector() { 
    return m_introspector_up.get(); 
  }
  
  // Get cached runtime symbol addresses for IR rewriting
  std::map<std::string, lldb::addr_t> GetObjCRuntimeAddresses();
  
  // Override from LanguageRuntime - this is the critical hook that IRForTarget uses
  lldb::addr_t LookupRuntimeSymbol(ConstString name) override;

  // Process lifecycle hooks to ensure expression evaluation is ready
  void DidLaunch();
  void DidAttach(ArchSpec &arch_spec);

private:
  // IRForTarget provides all runtime function calls dynamically

  std::unique_ptr<GNUstepObjCRuntimeIntrospector> m_introspector_up;
  std::unique_ptr<GNUstepRuntimeV2API> m_runtime_api_up;
  std::unique_ptr<DeclVendor> m_decl_vendor_up;
  bool m_has_read_objc_library = false;
  bool m_formatters_registered = false;
  bool m_gnustep_library_loaded = false;
  bool m_subscript_mapping_enabled = false;
  bool m_expression_hooks_installed = false;
  
  // Cached runtime symbol addresses for expression evaluation
  lldb::addr_t m_objc_msgSend_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_objc_msgSend_stret_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_objc_msgSend_fpret_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_objc_getClass_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_sel_getUid_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_object_getClass_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_class_getMethodImplementation_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_cfstring_create_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_class_addMethod_addr = LLDB_INVALID_ADDRESS;
  
  // Optional ARC helpers (non-fatal if not found)
  lldb::addr_t m_objc_retain_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_objc_release_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_objc_autoreleaseReturnValue_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_objc_retainAutoreleasedReturnValue_addr = LLDB_INVALID_ADDRESS;
  
  // CFString fallback utility function if needed
  std::unique_ptr<UtilityFunction> m_cfstring_utility_fn;
  
  // Dynamic runtime discovery replaces hardcoded utilities
  // Deleted: m_array_literal_utility_fn, m_dict_literal_utility_fn 
  // Deleted: m_subscript_utils_fn, m_diagnostic_utility_fn
  // Deleted: Associated addresses and flags
  // All functionality now handled by IRForTarget with objc_getClass calls
  
  // Helper method to register formatters
  void RegisterFormatters();
  
  // Initialize runtime API
  void InitializeRuntimeAPI();
  
  // Install subscript method mapping
  void InstallSubscriptMethodMapping();
  
  // Install expression evaluation hooks for subscript forwarding
  void InstallExpressionEvaluationHooks();
  
  // Helper methods for expression evaluation setup
  void ResolveAndCacheRuntimeSymbols();
  void EnsureCFStringCreateWithBytes();
  
  // Dynamic runtime introspection eliminates need for hardcoded methods
  // Deleted: EnsureArrayDictionaryLiteralSupport()
  // Deleted: CreateAndInstallSubscriptShims()
  // Deleted: InjectRuntimeFunctionDecls()
  // Deleted: CreateDiagnosticUtility()
  // Deleted: RegisterSymbolsWithIRForTarget()
  // Deleted: CallRuntimeFunction()
  void ArmEarlyInstall();
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_H
