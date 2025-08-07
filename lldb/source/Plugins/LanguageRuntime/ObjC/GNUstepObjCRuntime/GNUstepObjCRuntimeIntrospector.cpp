//===-- GNUstepObjCRuntimeIntrospector.cpp ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCRuntimeIntrospector.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Core/Module.h"

using namespace lldb;
using namespace lldb_private;

GNUstepObjCRuntimeIntrospector::GNUstepObjCRuntimeIntrospector(Process *process)
    : m_process(process) {}

std::string GNUstepObjCRuntimeIntrospector::GetClassName(lldb::addr_t isa_addr) {
  if (!m_process || isa_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }

  // Based on libobjc2/class.h, the structure of an objc_class is:
  // struct objc_class {
  //   Class isa;          // 0 * address_size
  //   Class super_class;  // 1 * address_size
  //   const char *name;   // 2 * address_size  <- This is what we want!
  //   ...
  // };
  // We need to read the 'name' pointer, which is the third pointer in the
  // structure.

  Status error;
  const uint32_t address_size = m_process->GetAddressByteSize();

  // The 'name' field is at an offset of 2 * address_size from the start of the
  // class structure.
  const lldb::addr_t name_ptr_addr = isa_addr + (2 * address_size);

  const lldb::addr_t name_addr = m_process->ReadPointerFromMemory(name_ptr_addr, error);

  if (error.Fail() || name_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }

  // Now read the C-string from the 'name' pointer.
  std::string class_name;
  m_process->ReadCStringFromMemory(name_addr, class_name, error);

  if (error.Fail()) {
    return "";
  }

  return class_name;
}

lldb::addr_t GNUstepObjCRuntimeIntrospector::FindClass(const std::string &class_name) {
  if (!m_process || class_name.empty()) {
    return LLDB_INVALID_ADDRESS;
  }

  // Try to call objc_lookup_class function in the target
  // This is more reliable than trying to parse the class table ourselves
  std::vector<lldb::addr_t> args;
  
  // First, we need to create a string in the target process memory
  Status error;
  lldb::addr_t string_addr = m_process->AllocateMemory(class_name.length() + 1, 
                                                       lldb::ePermissionsReadable, error);
  if (error.Fail() || string_addr == LLDB_INVALID_ADDRESS) {
    return LLDB_INVALID_ADDRESS;
  }

  // Write the class name to target memory
  size_t bytes_written = m_process->WriteMemory(string_addr, class_name.c_str(), 
                                               class_name.length() + 1, error);
  if (error.Fail() || bytes_written != class_name.length() + 1) {
    m_process->DeallocateMemory(string_addr);
    return LLDB_INVALID_ADDRESS;
  }

  args.push_back(string_addr);
  lldb::addr_t class_addr = CallRuntimeFunction("objc_lookup_class", args);

  // Clean up the allocated string
  m_process->DeallocateMemory(string_addr);

  return class_addr;
}

bool GNUstepObjCRuntimeIntrospector::IsValidGNUstepRuntime() {
  if (!m_process) {
    return false;
  }

  // Try to find objc_lookup_class function - this indicates GNUstep runtime
  Target &target = m_process->GetTarget();
  SymbolContextList sc_list;
  target.GetImages().FindSymbolsWithNameAndType(ConstString("objc_lookup_class"),
                                               lldb::eSymbolTypeCode, sc_list);
  
  return sc_list.GetSize() > 0;
}

lldb::addr_t GNUstepObjCRuntimeIntrospector::CallRuntimeFunction(
    const std::string &function_name, const std::vector<lldb::addr_t> &args) {
  
  if (!m_process) {
    return LLDB_INVALID_ADDRESS;
  }

  Target &target = m_process->GetTarget();
  
  // Find the function symbol
  SymbolContextList sc_list;
  target.GetImages().FindSymbolsWithNameAndType(ConstString(function_name),
                                               lldb::eSymbolTypeCode, sc_list);
  
  if (sc_list.GetSize() == 0) {
    return LLDB_INVALID_ADDRESS;
  }

  SymbolContext sc;
  sc_list.GetContextAtIndex(0, sc);
  
  if (!sc.symbol) {
    return LLDB_INVALID_ADDRESS;
  }

  lldb::addr_t func_addr = sc.symbol->GetLoadAddress(&target);
  if (func_addr == LLDB_INVALID_ADDRESS) {
    return LLDB_INVALID_ADDRESS;
  }

  // For now, return the function address. In a full implementation,
  // we would use the process's thread to actually call the function.
  // This requires more complex setup with the expression evaluator.
  
  // TODO: Implement actual function calling using ExecutionContext
  // and ThreadPlanCallFunction
  
  return LLDB_INVALID_ADDRESS; // Stub for now
}
