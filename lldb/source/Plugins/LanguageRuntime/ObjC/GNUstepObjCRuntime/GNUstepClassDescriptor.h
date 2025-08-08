//===-- GNUstepClassDescriptor.h --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the GNUstepClassDescriptor which bridges the
// GNUstepRuntimeV2API to LLDB's ObjCLanguageRuntime::ClassDescriptor interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPCLASSDESCRIPTOR_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPCLASSDESCRIPTOR_H

#include "../ObjCLanguageRuntime.h"
#include "GNUstepRuntimeV2API.h"
#include "lldb/lldb-private.h"
#include <memory>
#include <mutex>
#include <vector>

namespace lldb_private {

class GNUstepObjCRuntime;

/// GNUstepClassDescriptor bridges GNUstepRuntimeV2API::ClassInfo to
/// LLDB's ObjCLanguageRuntime::ClassDescriptor interface.
///
/// This class provides a lazy-loading implementation that fetches class
/// information from the GNUstep runtime on demand and caches it for
/// performance.
class GNUstepClassDescriptor : public ObjCLanguageRuntime::ClassDescriptor {
public:
  friend class GNUstepObjCRuntime;

  ~GNUstepClassDescriptor() override = default;

  // === Required ObjCLanguageRuntime::ClassDescriptor Methods ===

  /// Get the class name
  ConstString GetClassName() override;

  /// Get the superclass descriptor
  ObjCLanguageRuntime::ClassDescriptorSP GetSuperclass() override;

  /// Get the metaclass descriptor
  ObjCLanguageRuntime::ClassDescriptorSP GetMetaclass() const override;

  /// Check if this descriptor is valid
  bool IsValid() override;

  /// Get tagged pointer information (GNUstep doesn't use tagged pointers
  /// in the same way as Apple's runtime)
  bool GetTaggedPointerInfo(uint64_t *info_bits = nullptr,
                           uint64_t *value_bits = nullptr,
                           uint64_t *payload = nullptr) override {
    return false;
  }

  /// Get tagged pointer information with signed values
  bool GetTaggedPointerInfoSigned(uint64_t *info_bits = nullptr,
                                  int64_t *value_bits = nullptr,
                                  uint64_t *payload = nullptr) override {
    return false;
  }

  /// Get the instance size for this class
  uint64_t GetInstanceSize() override;

  /// Get the ISA pointer for this class
  ObjCLanguageRuntime::ObjCISA GetISA() override { return m_class_ptr; }

  /// Get the implementation language (always Objective-C for GNUstep)
  lldb::LanguageType GetImplementationLanguage() const override {
    return lldb::eLanguageTypeObjC;
  }

  /// Describe the class hierarchy and methods
  bool Describe(
      std::function<void(ObjCLanguageRuntime::ObjCISA)> const &superclass_func,
      std::function<bool(const char *, const char *)> const
          &instance_method_func,
      std::function<bool(const char *, const char *)> const &class_method_func,
      std::function<bool(const char *, const char *, lldb::addr_t,
                         uint64_t)> const &ivar_func) const override;

  /// Get the number of instance variables
  size_t GetNumIVars() override;

  /// Get instance variable at index
  iVarDescriptor GetIVarAtIndex(size_t idx) override;

protected:
  /// Ensure class info is loaded from runtime
  void EnsureClassInfoLoaded() const;

  /// Load ivars if not already loaded
  void LoadIVars() const;

private:
  /// Constructor - should only be called by GNUstepObjCRuntime
  GNUstepClassDescriptor(GNUstepObjCRuntime &runtime,
                        ObjCLanguageRuntime::ObjCISA class_ptr,
                        const char *class_name);

  /// Constructor with pre-loaded ClassInfo
  GNUstepClassDescriptor(GNUstepObjCRuntime &runtime,
                        const GNUstepRuntimeV2API::ClassInfo &class_info);

  // Runtime and API references
  GNUstepObjCRuntime &m_runtime;
  mutable GNUstepRuntimeV2API *m_runtime_api;
  
  // Basic class information
  mutable ObjCLanguageRuntime::ObjCISA m_class_ptr;
  mutable ConstString m_class_name;
  
  // Cached class info from runtime
  mutable std::unique_ptr<GNUstepRuntimeV2API::ClassInfo> m_class_info;
  mutable bool m_class_info_loaded;
  
  // Cached ivar descriptors
  mutable std::vector<iVarDescriptor> m_ivar_descriptors;
  mutable bool m_ivars_loaded;
  
  // Thread safety
  mutable std::recursive_mutex m_mutex;
};

/// Tagged pointer descriptor for GNUstep (if needed in future)
/// Currently GNUstep doesn't use tagged pointers like Apple's runtime
class GNUstepClassDescriptorTagged : public ObjCLanguageRuntime::ClassDescriptor {
public:
  GNUstepClassDescriptorTagged(ConstString class_name, uint64_t payload)
      : m_class_name(class_name), m_payload(payload), m_valid(true) {}

  ~GNUstepClassDescriptorTagged() override = default;

  ConstString GetClassName() override { return m_class_name; }

  ObjCLanguageRuntime::ClassDescriptorSP GetSuperclass() override {
    return ObjCLanguageRuntime::ClassDescriptorSP();
  }

  ObjCLanguageRuntime::ClassDescriptorSP GetMetaclass() const override {
    return ObjCLanguageRuntime::ClassDescriptorSP();
  }

  bool IsValid() override { return m_valid; }

  bool IsKVO() override { return false; }

  bool IsCFType() override { return false; }

  bool GetTaggedPointerInfo(uint64_t *info_bits = nullptr,
                           uint64_t *value_bits = nullptr,
                           uint64_t *payload = nullptr) override {
    if (payload)
      *payload = m_payload;
    return true;
  }

  bool GetTaggedPointerInfoSigned(uint64_t *info_bits = nullptr,
                                  int64_t *value_bits = nullptr,
                                  uint64_t *payload = nullptr) override {
    if (payload)
      *payload = m_payload;
    return true;
  }

  uint64_t GetInstanceSize() override { return 8; }

  ObjCLanguageRuntime::ObjCISA GetISA() override { return 0; }

private:
  ConstString m_class_name;
  uint64_t m_payload;
  bool m_valid;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPCLASSDESCRIPTOR_H