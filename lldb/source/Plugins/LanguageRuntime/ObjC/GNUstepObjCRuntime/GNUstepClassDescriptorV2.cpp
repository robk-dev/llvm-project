//===-- GNUstepClassDescriptorV2.cpp -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepClassDescriptorV2.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include <cstdio>

using namespace lldb;
using namespace lldb_private;

GNUstepClassDescriptorV2::GNUstepClassDescriptorV2(
    GNUstepObjCRuntime &runtime, ObjCLanguageRuntime::ObjCISA isa, const std::string &name)
    : m_runtime(runtime), m_isa(isa), m_class_name(name) {
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "[GNUstepClassDescriptorV2] Created for class '{0}' (ISA: 0x{1:x})",
           name, isa);
}

ObjCLanguageRuntime::ClassDescriptorSP 
GNUstepClassDescriptorV2::GetSuperclass() {
  GNUstepObjCRuntimeIntrospector *introspector = m_runtime.GetRuntimeIntrospector();
  if (!introspector)
    return ObjCLanguageRuntime::ClassDescriptorSP();
  
  // Get superclass using runtime introspection
  std::vector<lldb::addr_t> args = {m_isa};
  lldb::addr_t superclass_isa = introspector->CallRuntimeFunction("class_getSuperclass", args);
  
  if (superclass_isa == 0 || superclass_isa == LLDB_INVALID_ADDRESS)
    return ObjCLanguageRuntime::ClassDescriptorSP();
  
  return m_runtime.GetClassDescriptorFromISA(superclass_isa);
}

ObjCLanguageRuntime::ClassDescriptorSP 
GNUstepClassDescriptorV2::GetMetaclass() const {
  GNUstepObjCRuntimeIntrospector *introspector = m_runtime.GetRuntimeIntrospector();
  if (!introspector)
    return ObjCLanguageRuntime::ClassDescriptorSP();
  
  // Get metaclass using object_getClass on the class itself
  std::vector<lldb::addr_t> args = {m_isa};
  lldb::addr_t metaclass_isa = introspector->CallRuntimeFunction("object_getClass", args);
  
  if (metaclass_isa == 0 || metaclass_isa == LLDB_INVALID_ADDRESS)
    return ObjCLanguageRuntime::ClassDescriptorSP();
  
  return m_runtime.GetClassDescriptorFromISA(metaclass_isa);
}

uint64_t GNUstepClassDescriptorV2::GetInstanceSize() {
  GNUstepObjCRuntimeIntrospector *introspector = m_runtime.GetRuntimeIntrospector();
  if (!introspector)
    return 0;
  
  std::vector<lldb::addr_t> args = {m_isa};
  lldb::addr_t size = introspector->CallRuntimeFunction("class_getInstanceSize", args);
  
  if (size == LLDB_INVALID_ADDRESS)
    return 0;
  
  return (uint64_t)size;
}

void GNUstepClassDescriptorV2::FetchIvars() const {
  
  GNUstepObjCRuntimeIntrospector *introspector = m_runtime.GetRuntimeIntrospector();
  if (!introspector) {
    return;
  }
  
  // Get all ivars including inherited ones
  auto ivars_result = introspector->GetAllIvarsIncludingInherited((void*)m_isa);
  if (!ivars_result) {
    return;
  }
  
  m_ivars = std::move(*ivars_result);
  
  for (const auto &ivar : m_ivars) {
  }
}

bool GNUstepClassDescriptorV2::Describe(
    std::function<void(ObjCLanguageRuntime::ObjCISA)> const &superclass_func,
    std::function<bool(const char *, const char *)> const &instance_method_func,
    std::function<bool(const char *, const char *)> const &class_method_func,
    std::function<bool(const char *, const char *, lldb::addr_t,
                       uint64_t)> const &ivar_func) const {
  
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "[GNUstepClassDescriptorV2::Describe] Called for class '{0}'", m_class_name);
  
  GNUstepObjCRuntimeIntrospector *introspector = m_runtime.GetRuntimeIntrospector();
  if (!introspector) {
    return false;
  }
  
  // Handle superclass callback
  if (superclass_func) {
    std::vector<lldb::addr_t> args = {m_isa};
    lldb::addr_t superclass_isa = introspector->CallRuntimeFunction("class_getSuperclass", args);
    
    if (superclass_isa != 0 && superclass_isa != LLDB_INVALID_ADDRESS) {
      superclass_func(superclass_isa);
    }
  }
  
  // Handle instance methods callback
  if (instance_method_func) {
    auto methods = introspector->GetInstanceMethods(m_isa);
    
    for (const auto &method : methods) {
      if (!instance_method_func(method.selector_name.c_str(), 
                               method.type_encoding.c_str())) {
        break;
      }
    }
  }
  
  // Handle class methods callback
  if (class_method_func) {
    auto methods = introspector->GetClassMethods(m_isa);
    
    for (const auto &method : methods) {
      if (!class_method_func(method.selector_name.c_str(), 
                            method.type_encoding.c_str())) {
        break;
      }
    }
  }
  
  // Handle ivars callback - this is the critical part
  if (ivar_func) {
    // Fetch ivars if not already fetched
    std::call_once(m_ivars_fetched, [this]() { FetchIvars(); });
    
    
    for (const auto &ivar : m_ivars) {
      
      // The ivar callback expects: name, type, offset, size
      // We don't have size directly, so pass 0 for now
      if (!ivar_func(ivar.name.c_str(), ivar.type_encoding.c_str(), 
                    (lldb::addr_t)ivar.offset, 0)) {
        break;
      }
    }
  }
  
  return true;
}