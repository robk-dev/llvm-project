//===-- GNUstepClassDescriptorV2.h -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPCLASSDESCRIPTORV2_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPCLASSDESCRIPTORV2_H

#include "GNUstepObjCRuntime.h"
#include "GNUstepObjCRuntimeIntrospector.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "lldb/lldb-private.h"
#include <mutex>
#include <vector>

namespace lldb_private {

class GNUstepClassDescriptorV2 : public ObjCLanguageRuntime::ClassDescriptor {
public:
  GNUstepClassDescriptorV2(GNUstepObjCRuntime &runtime, ObjCLanguageRuntime::ObjCISA isa, 
                           const std::string &name);
  
  ~GNUstepClassDescriptorV2() override = default;

  ConstString GetClassName() override { return ConstString(m_class_name); }
  
  ObjCLanguageRuntime::ClassDescriptorSP GetSuperclass() override;
  
  ObjCLanguageRuntime::ClassDescriptorSP GetMetaclass() const override;
  
  bool IsValid() override { return m_isa != 0; }
  
  bool IsKVO() override { return false; }
  
  bool IsCFType() override { return false; }
  
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
  
  uint64_t GetInstanceSize() override;
  
  ObjCLanguageRuntime::ObjCISA GetISA() override { return m_isa; }
  
  // The critical method - properly implement this to enumerate ivars
  bool Describe(
      std::function<void(ObjCLanguageRuntime::ObjCISA)> const &superclass_func,
      std::function<bool(const char *, const char *)> const &instance_method_func,
      std::function<bool(const char *, const char *)> const &class_method_func,
      std::function<bool(const char *, const char *, lldb::addr_t,
                         uint64_t)> const &ivar_func) const override;

private:
  GNUstepObjCRuntime &m_runtime;
  ObjCLanguageRuntime::ObjCISA m_isa;
  std::string m_class_name;
  mutable std::once_flag m_ivars_fetched;
  mutable std::vector<GNUstepObjCRuntimeIntrospector::IvarInfo> m_ivars;
  
  void FetchIvars() const;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPCLASSDESCRIPTORV2_H