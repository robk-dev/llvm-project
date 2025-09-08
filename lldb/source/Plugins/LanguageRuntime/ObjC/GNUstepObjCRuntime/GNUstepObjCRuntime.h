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
#include "lldb/Core/ModuleList.h"
#include "lldb/Expression/UtilityFunction.h"
#include "lldb/Symbol/DeclVendor.h"
#include "lldb/lldb-private.h"

#include "GNUstepObjCRuntimeIntrospector.h"
#include "GNUstepObjCRuntimeUtilities.h"

#include <chrono>
#include <unordered_map>

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


  void UpdateISAToDescriptorMapIfNeeded() override;

  // ClassDescriptor support
  ClassDescriptorSP
  GetClassDescriptorFromClassName(ConstString class_name) override;
  ClassDescriptorSP GetClassDescriptorFromISA(ObjCISA isa) override;
  ClassDescriptorSP GetClassDescriptor(ValueObject &valobj) override;

  // LanguageRuntime overrides
  void ModulesDidLoad(const ModuleList &module_list) override;

  // Constructor (public for make_unique)
  GNUstepObjCRuntime(Process *process);

  // Get the runtime introspector 
  GNUstepObjCRuntimeIntrospector *GetRuntimeIntrospector() { 
    return m_introspector_up.get(); 
  }

  // Get the runtime function caller
  gnustep_objc_runtime_utilities::RuntimeFunctionCaller *GetRuntimeFunctionCaller() {
    return m_runtime_caller.get();
  }


  // Override from LanguageRuntime - this is the critical hook that IRForTarget
  // uses

private:
  // Helper method to call runtime functions with string arguments (delegates to utilities)
  lldb::addr_t CallRuntimeFunction(const char *function_name,
                                   const char *string_arg);

  std::unique_ptr<GNUstepObjCRuntimeIntrospector> m_introspector_up;
  std::unique_ptr<DeclVendor> m_decl_vendor_up;
  bool m_has_read_objc_library = false;
  bool m_gnustep_library_loaded = false;

  // Reentrancy protection flags
  bool m_in_object_description = false;
  bool m_in_dynamic_type_check = false;
  bool m_updating_isa_to_descriptor = false;
  // Consolidated runtime function caller
  std::unique_ptr<gnustep_objc_runtime_utilities::RuntimeFunctionCaller> m_runtime_caller;

  // Caching for performance
  struct ObjectDescriptionCache {
    std::unordered_map<lldb::addr_t, std::string> descriptions;
    std::chrono::steady_clock::time_point last_invalidation =
        std::chrono::steady_clock::now();

    void InvalidateIfStale() {
      auto now = std::chrono::steady_clock::now();
      auto age = std::chrono::duration_cast<std::chrono::seconds>(
          now - last_invalidation);
      if (age.count() > 30) { // Invalidate after 30 seconds
        descriptions.clear();
        last_invalidation = now;
      }
    }
  } m_object_description_cache;

  // Symbol resolution now handled entirely by runtime introspection
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_H
