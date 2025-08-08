//===-- GNUstepObjCRuntimeIntrospector.h ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEINTROSPECTOR_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEINTROSPECTOR_H

#include "lldb/lldb-private.h"
#include "lldb/Target/Process.h"

namespace lldb_private {

class GNUstepObjCRuntimeIntrospector {
public:
  GNUstepObjCRuntimeIntrospector(Process *process);
  ~GNUstepObjCRuntimeIntrospector() = default;

  // Extract ISA from a ValueObject
  lldb::addr_t GetISAFromObject(ValueObject &valobj);
  
  // Given an isa pointer, return the class name.
  std::string GetClassName(lldb::addr_t isa_addr);
  
  // Get class name directly from a ValueObject
  std::string GetClassNameFromObject(ValueObject &valobj);
  
  // Find a class by name in the runtime
  lldb::addr_t FindClass(const std::string &class_name);
  
  // Check if this looks like a valid GNUstep runtime
  bool IsValidGNUstepRuntime();
  
  // Check if an object is a tagged pointer
  bool IsTaggedPointer(lldb::addr_t obj_addr);
  
  // Decode tagged pointer data for strings
  std::string DecodeTaggedString(lldb::addr_t obj_addr);
  
  // Check if an address represents a valid object
  bool IsValidObjectPointer(lldb::addr_t obj_addr);

private:
  Process *m_process;
  uint32_t m_address_size;
  lldb::ByteOrder m_byte_order;
  
  // Helper method to call functions in the target process
  lldb::addr_t CallRuntimeFunction(const std::string &function_name,
                                   const std::vector<lldb::addr_t> &args);
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEINTROSPECTOR_H
