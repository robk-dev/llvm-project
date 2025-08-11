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

#include "GNUstepObjCRuntimeIntrospector.h"
#include "GNUstepObjCDeclVendor.h"
#include "GNUstepRuntimeV2API.h"

namespace lldb_private {

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

  DeclVendor *GetDeclVendor() override;

  llvm::Expected<std::unique_ptr<UtilityFunction>>
  CreateObjectChecker(std::string name, ExecutionContext &exe_ctx) override;

  void UpdateISAToDescriptorMapIfNeeded() override;
  
  // ClassDescriptor support
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

private:
  std::unique_ptr<GNUstepObjCRuntimeIntrospector> m_introspector_up;
  std::unique_ptr<GNUstepRuntimeV2API> m_runtime_api_up;
  std::unique_ptr<DeclVendor> m_decl_vendor_up;
  bool m_has_read_objc_library = false;
  bool m_formatters_registered = false;
  bool m_gnustep_library_loaded = false;
  
  // Helper method to register formatters
  void RegisterFormatters();
  
  // Initialize runtime API
  void InitializeRuntimeAPI();
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_H
