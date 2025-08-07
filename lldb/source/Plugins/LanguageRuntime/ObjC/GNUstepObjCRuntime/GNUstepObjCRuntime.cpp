//===-- GNUstepObjCRuntime.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCRuntime.h"
#include "GNUstepObjCRuntimeIntrospector.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/StreamString.h"

using namespace lldb;
using namespace lldb_private;

void GNUstepObjCRuntime::Initialize() {
  PluginManager::RegisterPlugin(GetPluginName(), "GNUstep Objective-C V2 Runtime",
                                CreateInstance);
  // Add debug output to verify our plugin is being loaded
  printf("[DEBUG] GNUstepObjCRuntime::Initialize() called\n");
}

void GNUstepObjCRuntime::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}

lldb::LanguageRuntimeSP
GNUstepObjCRuntime::CreateInstance(Process *process,
                                     lldb::LanguageType language) {
  printf("[DEBUG] GNUstepObjCRuntime::CreateInstance() called for language %d\n", language);
  
  if (language != eLanguageTypeObjC || !process)
    return nullptr;

  // Check if this process is using the GNUstep runtime
  // Look for libobjc.so.2 or related GNUstep libraries
  Target &target = process->GetTarget();
  ModuleList &modules = target.GetImages();
  
  bool found_gnustep_runtime = false;
  
  // Check for GNUstep runtime libraries
  for (size_t i = 0; i < modules.GetSize(); ++i) {
    ModuleSP module_sp = modules.GetModuleAtIndex(i);
    if (module_sp) {
      const char *module_name = module_sp->GetFileSpec().GetFilename().GetCString();
      if (module_name && 
          (strstr(module_name, "libobjc.so") || 
           strstr(module_name, "libgnustep-base.so") ||
           strstr(module_name, "libobjc2"))) {
        found_gnustep_runtime = true;
        printf("[DEBUG] GNUstepObjCRuntime: Found GNUstep runtime module: %s\n", module_name);
        break;
      }
    }
  }
  
  if (found_gnustep_runtime) {
    printf("[DEBUG] GNUstepObjCRuntime: Creating instance for GNUstep runtime\n");
    return std::make_shared<GNUstepObjCRuntime>(process);
  }
  
  printf("[DEBUG] GNUstepObjCRuntime: No GNUstep runtime found, not creating instance\n");
  return nullptr;
}

GNUstepObjCRuntime::GNUstepObjCRuntime(Process *process)
    : ObjCLanguageRuntime(process) {
  printf("[DEBUG] GNUstepObjCRuntime constructor called\n");
  if (process)
    m_introspector_up = std::make_unique<GNUstepObjCRuntimeIntrospector>(process);
}

GNUstepObjCRuntime::~GNUstepObjCRuntime() {}

bool GNUstepObjCRuntime::GetObjectDescription(Stream &str,
                                                ValueObject &object) {
  printf("[DEBUG] GNUstepObjCRuntime::GetObjectDescription called\n");
  
  if (!m_introspector_up)
    return false;

  // Get the ISA
  lldb::addr_t isa_addr = object.GetPointerValue();
  if (isa_addr == LLDB_INVALID_ADDRESS)
    return false;

  // Get the class name from the introspector
  std::string class_name = m_introspector_up->GetClassName(isa_addr);

  if (class_name.empty())
    return false;

  str.Printf("(%s *) 0x%" PRIx64, class_name.c_str(), object.GetPointerValue());
  printf("[DEBUG] GNUstepObjCRuntime: Formatted object as (%s *) 0x%" PRIx64 "\n", 
         class_name.c_str(), object.GetPointerValue());
  return true;
}

bool GNUstepObjCRuntime::GetObjectDescription(
    Stream &str, Value &value, lldb::DynamicValueType use_dynamic) {
  // This version is less critical for now, but we can forward it.
  if (value.GetValueType() != Value::eValueTypeScalar)
    return false;

  ExecutionContext exe_ctx(m_process);
  ValueObjectSP val_obj_sp =
      ValueObject::CreateValueObjectFromValue("temp", value, exe_ctx);
  if (val_obj_sp)
    return GetObjectDescription(str, *val_obj_sp);

  return false;
}

lldb::BreakpointResolverSP GNUstepObjCRuntime::CreateExceptionBreakpointResolver(
    const Breakpoint &bkpt, bool is_catch, bool is_throw) {
  // Stub implementation
  return nullptr;
}

lldb::ThreadPlanSP GNUstepObjCRuntime::GetStepThroughTrampolinePlan(
    Thread &thread, bool stop_others) {
  // Stub implementation
  return nullptr;
}

LLDB_PLUGIN_DEFINE(GNUstepObjCRuntime)
