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
#include "lldb/Expression/FunctionCaller.h"
#include <memory>
#include <unordered_map>

namespace lldb_private {

// Forward declarations
class ExecutionContext;
class CompilerType;
class ValueList;
class Status;

class GNUstepObjCRuntimeIntrospector {
public:
  GNUstepObjCRuntimeIntrospector(Process *process);
  ~GNUstepObjCRuntimeIntrospector() = default;

  // Extract ISA from a ValueObject
  lldb::addr_t GetISAFromObject(ValueObject &valobj);
  
  // Given an isa pointer, return the class name.
  std::string GetClassName(lldb::addr_t isa_addr);
  
  // Get class name from ISA with caching support
  lldb_private::ConstString GetClassNameFromISA(lldb::addr_t isa_addr);
  
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
  
  // Cache for function callers to avoid repeated compilation
  struct FunctionCallerCache {
    std::unique_ptr<FunctionCaller> objc_lookup_class_caller;
    std::unique_ptr<FunctionCaller> class_getName_caller;
    std::unique_ptr<FunctionCaller> object_getClass_caller;
    std::unique_ptr<FunctionCaller> class_getSuperclass_caller;
    // Generic cache for other functions
    std::unordered_map<std::string, std::unique_ptr<FunctionCaller>> generic_callers;
  };
  
  mutable FunctionCallerCache m_function_cache;
  
  // Cache for ISA to class name mapping
  mutable std::unordered_map<lldb::addr_t, lldb_private::ConstString> m_isa_to_name_cache;
  
  // Helper method to call functions in the target process (existing interface)
  lldb::addr_t CallRuntimeFunction(const std::string &function_name,
                                   const std::vector<lldb::addr_t> &args);
  
  // New implementation methods for function calling
  lldb::addr_t CallRuntimeFunctionImpl(
      const char *function_name,
      const CompilerType &return_type,
      const ValueList &args,
      ExecutionContext &exe_ctx,
      Status &error) const;
      
  std::unique_ptr<FunctionCaller>& GetOrCreateFunctionCaller(
      const char *function_name,
      const CompilerType &return_type,
      const ValueList &arg_types,
      ExecutionContext &exe_ctx,
      Status &error) const;
      
  bool SetupExecutionContext(ExecutionContext &exe_ctx) const;
  
  // Helper to get modules
  lldb::ModuleSP GetObjCModule() const;
  lldb::ModuleSP GetFoundationModule() const;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEINTROSPECTOR_H
