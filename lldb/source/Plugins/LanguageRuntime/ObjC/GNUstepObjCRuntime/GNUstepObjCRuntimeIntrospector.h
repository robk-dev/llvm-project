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

  // Given an isa pointer, return the class name.
  std::string GetClassName(lldb::addr_t isa_addr);
  
  // Find a class by name in the runtime
  lldb::addr_t FindClass(const std::string &class_name);
  
  // Check if this looks like a valid GNUstep runtime
  bool IsValidGNUstepRuntime();

private:
  Process *m_process;
  
  // Helper method to call functions in the target process
  lldb::addr_t CallRuntimeFunction(const std::string &function_name,
                                   const std::vector<lldb::addr_t> &args);
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEINTROSPECTOR_H
