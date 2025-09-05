//===-- GNUstepClassDescriptor.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepClassDescriptor.h"
#include "GNUstepObjCRuntime.h"
#include "lldb/Core/Module.h"
#include "lldb/Symbol/CompilerType.h"
#include "lldb/Symbol/TypeSystem.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"

#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

using namespace lldb;
using namespace lldb_private;

// Constructor with ISA and optional name
GNUstepClassDescriptor::GNUstepClassDescriptor(
    GNUstepObjCRuntime &runtime, ObjCLanguageRuntime::ObjCISA class_ptr,
    const char *class_name)
    : m_runtime(runtime), m_runtime_api(nullptr), m_class_ptr(class_ptr),
      m_class_name(class_name), m_class_info_loaded(false),
      m_ivars_loaded(false) {
  // We'll get the runtime API lazily when needed
}

// Constructor with pre-loaded ClassInfo
GNUstepClassDescriptor::GNUstepClassDescriptor(
    GNUstepObjCRuntime &runtime,
    const GNUstepRuntimeV2API::ClassInfo &class_info)
    : m_runtime(runtime), m_runtime_api(nullptr),
      m_class_ptr(
          reinterpret_cast<ObjCLanguageRuntime::ObjCISA>(class_info.class_ptr)),
      m_class_name(class_info.name.c_str()),
      m_class_info(
          std::make_unique<GNUstepRuntimeV2API::ClassInfo>(class_info)),
      m_class_info_loaded(true), m_ivars_loaded(false) {}

void GNUstepClassDescriptor::EnsureClassInfoLoaded() const {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);

  if (m_class_info_loaded)
    return;

  // Get runtime API if we don't have it
  if (!m_runtime_api) {
    m_runtime_api = m_runtime.GetRuntimeAPI();
    if (!m_runtime_api) {
      // Runtime API not initialized yet
      return;
    }
  }

  // Try to load class info from the runtime
  if (m_class_ptr) {
    auto class_info_or_error = m_runtime_api->GetClassInfoFromPointer(
        reinterpret_cast<GNUstepRuntimeV2API::Class>(m_class_ptr));

    if (class_info_or_error) {
      m_class_info = std::make_unique<GNUstepRuntimeV2API::ClassInfo>(
          std::move(*class_info_or_error));
      m_class_info_loaded = true;

      // Update class name if we didn't have it
      if (!m_class_name && !m_class_info->name.empty()) {
        m_class_name = ConstString(m_class_info->name.c_str());
      }
    }
  } else if (m_class_name) {
    // Try to load by name
    auto class_info_or_error =
        m_runtime_api->GetObjCClassInfo(m_class_name.GetCString());

    if (class_info_or_error) {
      m_class_info = std::make_unique<GNUstepRuntimeV2API::ClassInfo>(
          std::move(*class_info_or_error));
      m_class_ptr = reinterpret_cast<ObjCLanguageRuntime::ObjCISA>(
          m_class_info->class_ptr);
      m_class_info_loaded = true;
    }
  }
}

ConstString GNUstepClassDescriptor::GetClassName() {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);

  if (m_class_name)
    return m_class_name;

  EnsureClassInfoLoaded();

  if (m_class_info && !m_class_info->name.empty()) {
    m_class_name = ConstString(m_class_info->name.c_str());
  }

  return m_class_name;
}

ObjCLanguageRuntime::ClassDescriptorSP GNUstepClassDescriptor::GetSuperclass() {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);

  // Strategy 1: Use cached class info if available
  EnsureClassInfoLoaded();

  if (m_class_info && m_class_info->superclass_ptr) {
    // Create a new descriptor for the superclass
    ObjCLanguageRuntime::ObjCISA superclass_isa =
        reinterpret_cast<ObjCLanguageRuntime::ObjCISA>(
            m_class_info->superclass_ptr);

    const char *superclass_name = m_class_info->superclass_name.empty()
                                      ? nullptr
                                      : m_class_info->superclass_name.c_str();

    return ObjCLanguageRuntime::ClassDescriptorSP(
        new GNUstepClassDescriptor(m_runtime, superclass_isa, superclass_name));
  }

  // Strategy 2: Direct runtime API call if class info not loaded but class_ptr
  // available
  if (m_class_ptr && m_runtime_api) {
    auto class_info_result = m_runtime_api->GetClassInfoFromPointer(
        reinterpret_cast<GNUstepRuntimeV2API::Class>(m_class_ptr));

    if (class_info_result && class_info_result->superclass_ptr) {
      ObjCLanguageRuntime::ObjCISA superclass_isa =
          reinterpret_cast<ObjCLanguageRuntime::ObjCISA>(
              class_info_result->superclass_ptr);

      const char *superclass_name =
          class_info_result->superclass_name.empty()
              ? nullptr
              : class_info_result->superclass_name.c_str();

      return ObjCLanguageRuntime::ClassDescriptorSP(new GNUstepClassDescriptor(
          m_runtime, superclass_isa, superclass_name));
    }
  }

  return ObjCLanguageRuntime::ClassDescriptorSP();
}

ObjCLanguageRuntime::ClassDescriptorSP
GNUstepClassDescriptor::GetMetaclass() const {
  // GNUstep runtime handles metaclasses differently than Apple's runtime
  // For now, return null - can be implemented if needed
  return ObjCLanguageRuntime::ClassDescriptorSP();
}

bool GNUstepClassDescriptor::IsValid() {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);

  // Valid if we have either a class pointer or a class name
  if (!m_class_ptr && !m_class_name)
    return false;

  // Try to load class info to validate
  EnsureClassInfoLoaded();

  // If we loaded class info successfully, we're valid
  return m_class_info_loaded;
}

uint64_t GNUstepClassDescriptor::GetInstanceSize() {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);

  // Strategy 1: Use cached class info if available
  EnsureClassInfoLoaded();

  if (m_class_info) {
    return m_class_info->instance_size;
  }

  // Strategy 2: Direct runtime API call if class info not loaded but class_ptr
  // available
  if (m_class_ptr && m_runtime_api) {
    auto class_info_result = m_runtime_api->GetClassInfoFromPointer(
        reinterpret_cast<GNUstepRuntimeV2API::Class>(m_class_ptr));

    if (class_info_result) {
      return class_info_result->instance_size;
    }
  }

  return 0;
}

void GNUstepClassDescriptor::LoadIVars() const {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);

  if (m_ivars_loaded)
    return;

  // Strategy 1: Use cached class info if available
  EnsureClassInfoLoaded();

  if (m_class_info) {
    m_ivar_descriptors.clear();

    // Convert GNUstepRuntimeV2API::IvarInfo to iVarDescriptor
    for (const auto &ivar_info : m_class_info->all_ivars) {
      iVarDescriptor descriptor;
      descriptor.m_name = ConstString(ivar_info.name.c_str());
      descriptor.m_offset = static_cast<int32_t>(ivar_info.offset);
      descriptor.m_size = ivar_info.size;

      // Type encoding would need to be converted to CompilerType
      // This is a simplified version - real implementation would need
      // proper type conversion through TypeSystemClang

      m_ivar_descriptors.push_back(descriptor);
    }

    m_ivars_loaded = true;
    return;
  }

  // Strategy 2: Direct runtime API call if class_ptr available
  if (m_class_ptr && m_runtime_api) {
    auto class_info_result = m_runtime_api->GetClassInfoFromPointer(
        reinterpret_cast<GNUstepRuntimeV2API::Class>(m_class_ptr));

    if (class_info_result) {
      m_ivar_descriptors.clear();

      // Convert runtime ivar info to descriptors
      for (const auto &ivar_info : class_info_result->all_ivars) {
        iVarDescriptor descriptor;
        descriptor.m_name = ConstString(ivar_info.name.c_str());
        descriptor.m_offset = static_cast<int32_t>(ivar_info.offset);
        descriptor.m_size = ivar_info.size;

        m_ivar_descriptors.push_back(descriptor);
      }

      m_ivars_loaded = true;
      return;
    }
  }

  // Mark as loaded even if we couldn't get ivars to avoid repeated attempts
  m_ivars_loaded = true;
}

size_t GNUstepClassDescriptor::GetNumIVars() {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);

  LoadIVars();
  return m_ivar_descriptors.size();
}

ObjCLanguageRuntime::ClassDescriptor::iVarDescriptor
GNUstepClassDescriptor::GetIVarAtIndex(size_t idx) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);

  LoadIVars();

  if (idx >= m_ivar_descriptors.size())
    return iVarDescriptor();

  return m_ivar_descriptors[idx];
}

bool GNUstepClassDescriptor::Describe(
    std::function<void(ObjCLanguageRuntime::ObjCISA)> const &superclass_func,
    std::function<bool(const char *, const char *)> const &instance_method_func,
    std::function<bool(const char *, const char *)> const &class_method_func,
    std::function<bool(const char *, const char *, lldb::addr_t,
                       uint64_t)> const &ivar_func) const {

  std::lock_guard<std::recursive_mutex> guard(m_mutex);

  EnsureClassInfoLoaded();

  if (!m_class_info)
    return false;

  // Report superclass
  if (superclass_func && m_class_info->superclass_ptr) {
    superclass_func(reinterpret_cast<ObjCLanguageRuntime::ObjCISA>(
        m_class_info->superclass_ptr));
  }

  // Report instance methods
  if (instance_method_func) {
    for (const auto &method : m_class_info->all_methods) {
      if (!method.selector_name.empty() && !method.type_encoding.empty()) {
        if (!instance_method_func(method.selector_name.c_str(),
                                  method.type_encoding.c_str()))
          break;
      }
    }
  }

  // Report class methods (would need metaclass info)
  // GNUstep handles this differently than Apple's runtime

  // Report ivars
  if (ivar_func) {
    for (const auto &ivar : m_class_info->all_ivars) {
      if (!ivar.name.empty()) {
        if (!ivar_func(ivar.name.c_str(), ivar.type_encoding.c_str(),
                       ivar.offset, ivar.size))
          break;
      }
    }
  }

  return true;
}