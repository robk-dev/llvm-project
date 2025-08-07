//===-- GNUstepObjCRuntime.h ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPOBJCRUNTIME_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPOBJCRUNTIME_H

#include "lldb/Target/LanguageRuntime.h"
#include "lldb/lldb-private.h"

#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/DataFormatters/TypeSynthetic.h"
#include "lldb/DataFormatters/FormatManager.h"
#include "lldb/DataFormatters/DataVisualization.h"
#include "lldb/Symbol/CompilerType.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"

#include <optional>
#include <mutex>
#include <map>
#include <memory>
#include <vector>

namespace lldb_private {
// Forward declarations
class GNUstepObjCDeclVendor;
}

// Forward declarations
namespace lldb_private {
class ISAResolver;
class RuntimeIntrospector;
}

namespace lldb_private {

class GNUstepObjCRuntime : public lldb_private::ObjCLanguageRuntime {
public:
  ~GNUstepObjCRuntime() override;

  // Nested ClassDescriptor implementation for GNUstep
  class GNUstepClassDescriptor : public ObjCLanguageRuntime::ClassDescriptor {
  public:
    GNUstepClassDescriptor(ObjCISA isa, const ConstString &name)
        : m_isa(isa), m_name(name) {}
    
    ConstString GetClassName() override { return m_name; }
    
    ClassDescriptorSP GetSuperclass() override { 
      return ClassDescriptorSP(); 
    }
    
    ClassDescriptorSP GetMetaclass() const override { 
      return ClassDescriptorSP(); 
    }
    
    ObjCISA GetISA() override { return m_isa; }
    
    bool IsValid() override { return m_isa != 0; }
    
    bool GetTaggedPointerInfo(uint64_t *info_bits = nullptr,
                            uint64_t *value_bits = nullptr,
                            uint64_t *payload = nullptr) override {
      return false;
    }
    
    bool GetTaggedPointerInfoSigned(uint64_t *info_bits = nullptr,
                                  int64_t *value_bits = nullptr,
                                  uint64_t *payload = nullptr) override {
      return false;
    }
    
    uint64_t GetInstanceSize() override { return 0; }
    
  private:
    ObjCISA m_isa;
    ConstString m_name;
  };

  //
  //  PluginManager, PluginInterface and LLVM RTTI implementation
  //

  static char ID;

  static void Initialize();

  static void Terminate();

  static lldb_private::LanguageRuntime *
  CreateInstance(Process *process, lldb::LanguageType language);

  static llvm::StringRef GetPluginNameStatic() {
    return "gnustep-objc-libobjc2";
  }

  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }

  void ModulesDidLoad(const ModuleList &module_list) override;

  bool isA(const void *ClassID) const override {
    return ClassID == &ID || ObjCLanguageRuntime::isA(ClassID);
  }

  static bool classof(const LanguageRuntime *runtime) {
    return runtime->isA(&ID);
  }

  //
  // LanguageRuntime implementation
  //
  llvm::Error GetObjectDescription(Stream &str, Value &value,
                                   ExecutionContextScope *exe_scope) override;

  llvm::Error GetObjectDescription(Stream &str, ValueObject &object) override;

  bool CouldHaveDynamicValue(ValueObject &in_value) override;

  bool GetDynamicTypeAndAddress(ValueObject &in_value,
                                lldb::DynamicValueType use_dynamic,
                                TypeAndOrName &class_type_or_name,
                                Address &address, Value::ValueType &value_type) override;

  TypeAndOrName FixUpDynamicType(const TypeAndOrName &type_and_or_name,
                                 ValueObject &static_value) override;

  lldb::BreakpointResolverSP
  CreateExceptionResolver(const lldb::BreakpointSP &bkpt, bool catch_bp,
                          bool throw_bp) override;

  lldb::ThreadPlanSP GetStepThroughTrampolinePlan(Thread &thread,
                                                  bool stop_others) override;

  //
  // ObjCLanguageRuntime implementation
  //

  bool IsModuleObjCLibrary(const lldb::ModuleSP &module_sp) override;

  bool ReadObjCLibrary(const lldb::ModuleSP &module_sp) override;

  bool HasReadObjCLibrary() override { return m_objc_module_sp != nullptr; }

  llvm::Expected<std::unique_ptr<UtilityFunction>>
  CreateObjectChecker(std::string name, ExecutionContext &exe_ctx) override;

  ObjCRuntimeVersions GetRuntimeVersion() const override {
    return ObjCRuntimeVersions::eGNUstep_libobjc2;
  }

  void UpdateISAToDescriptorMapIfNeeded() override;

  // Enhanced class descriptor implementation with ISA resolution
  ClassDescriptorSP GetClassDescriptor(ValueObject &valobj) override;
  
  // Enhanced ISA pointer resolution
  std::string GetClassNameFromISA(lldb::addr_t isa_ptr);
  std::string GetClassNameFromObject(lldb::addr_t obj_addr);
  
  // Get runtime introspector for ivar offset discovery
  RuntimeIntrospector* GetRuntimeIntrospector() { return m_runtime_introspector.get(); }
  
  // Get ivar offset from cached metadata (discovered during UpdateISAToDescriptorMap)
  ptrdiff_t GetCachedIvarOffset(const std::string &class_name, const std::string &ivar_name);

  // Method dispatch support for expression evaluation - GNUstep specific methods
  lldb::addr_t GetGNUstepMethodDispatcher();
  std::shared_ptr<UtilityFunction> GetGNUstepObjectChecker();

  // Ivar discovery for synthetic children
  struct IvarInfo {
    std::string name;
    ptrdiff_t offset;
    std::string type_encoding;
    lldb::addr_t ivar_ptr;
  };
  
  std::vector<IvarInfo> GetClassIvars(lldb::addr_t class_ptr, const ExecutionContext *exe_ctx = nullptr);
  std::vector<IvarInfo> GetObjectIvars(ValueObject *obj_valobj, const ExecutionContext *exe_ctx = nullptr);
  
  // Helper methods for ivar discovery with fallback approaches
  std::vector<IvarInfo> GetObjectIvarsViaRuntime(ValueObject *obj_valobj, const ExecutionContext *exe_ctx_param,
                                                 const std::string &class_name, const char* var_name);
  std::vector<IvarInfo> GetObjectIvarsViaDirectMemory(ValueObject *obj_valobj, const std::string &class_name, lldb::addr_t obj_addr);
  
  // Direct runtime introspection methods (no expression evaluation)
  std::vector<IvarInfo> CallClassCopyIvarListDirectly(lldb::addr_t class_ptr, const ExecutionContext &exe_ctx);
  std::vector<IvarInfo> ParseClassMetadataDirectly(lldb::addr_t class_ptr);
  
  // Factory function for creating synthetic children
  static SyntheticChildren *CreateGNUstepSyntheticChildren();
  
  // Factory function for creating summary providers  
  static lldb::TypeSummaryImplSP CreateNSStringSummaryProvider();

  // Register synthetic providers and summary providers with LLDB
  void RegisterSyntheticProviders();
  
  // Get the DeclVendor for type creation
  DeclVendor *GetDeclVendor() override;
  void RegisterSummaryProvidersOnly();
  
  // Collection detection methods - temporarily disabled
  // bool IsArrayType(const char* class_name);
  // bool IsDictionaryType(const char* class_name); 
  // bool IsStringType(const char* class_name);
  
  // Type resolution with GNUstep->Foundation mapping - temporarily disabled
  // CompilerType GetFoundationType(const char* gnustep_class);
  // std::string GetRuntimeClassName(lldb::addr_t obj_addr);
  
  // Known GNUstep to Foundation type mappings - temporarily disabled
  // const char* GetFoundationTypeName(const char* gnustep_class);

protected:
  // Call CreateInstance instead.
  GNUstepObjCRuntime(Process *process);

  lldb::ModuleSP m_objc_module_sp;

  // Enhanced ISA resolution system
  std::unique_ptr<ISAResolver> m_isa_resolver;
  std::unique_ptr<RuntimeIntrospector> m_runtime_introspector;
  std::unique_ptr<lldb_private::GNUstepObjCDeclVendor> m_decl_vendor;
  
  // Class metadata cache - stores ivar offsets discovered during UpdateISAToDescriptorMap
  struct ClassMetadata {
    std::string name;
    lldb::addr_t class_ptr;
    std::map<std::string, ptrdiff_t> ivar_offsets;  // ivar_name -> offset
  };
  std::map<std::string, ClassMetadata> m_class_metadata;  // class_name -> metadata

private:
  // Runtime function addresses
  lldb::addr_t m_objc_copyClassList_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_class_getName_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_free_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_class_copyIvarList_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_ivar_getName_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_ivar_getOffset_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_ivar_getTypeEncoding_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_objc_getClass_addr = LLDB_INVALID_ADDRESS;
  
  // Has the runtime been fully scanned?
  bool m_isa_to_descriptor_complete = false;
  
  // Track if synthetic providers have been registered to avoid duplicates
  bool m_providers_registered = false;
  
  // Helper methods
  void LoadRuntimeSymbols();
  void UpdateISAToDescriptorMap();
  
  // Dynamic class discovery methods  
  bool CallObjCCopyClassList(lldb_private::Log *log);
  bool ShouldIncludeClass(const std::string &class_name);
  
  uint32_t ReadMemoryUnsigned(lldb::addr_t addr, size_t size);
  lldb::addr_t ReadPointer(lldb::addr_t addr);
  std::string ReadCString(lldb::addr_t addr, size_t max_len = 256);
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPOBJCRUNTIME_H