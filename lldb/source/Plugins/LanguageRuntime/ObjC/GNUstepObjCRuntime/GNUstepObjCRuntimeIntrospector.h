//===-- GNUstepObjCRuntimeIntrospector.h ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEINTROSPECTOR_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEINTROSPECTOR_H

#include "GNUstepObjCRuntimeUtilities.h"
#include "lldb/Expression/FunctionCaller.h"
#include "lldb/Target/Process.h"
#include "lldb/lldb-private.h"
#include "llvm/Support/Error.h"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace lldb_private {

// Forward declarations
class ExecutionContext;
class CompilerType;
class ValueList;
class Status;

// Import the utilities namespace for convenience
using namespace gnustep_objc_runtime_utilities;

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

  // CRITICAL: Direct method introspection using runtime.h functions
  // These avoid expression evaluation during interface declaration to break
  // recursion
  struct MethodInfo {
    std::string selector_name;
    std::string type_encoding;
    lldb::addr_t implementation;
  };

  // Get all instance methods from a class pointer (direct memory access)
  std::vector<MethodInfo> GetInstanceMethods(lldb::addr_t class_ptr);

  // Get all class methods from a class pointer via metaclass (direct memory
  // access)
  std::vector<MethodInfo> GetClassMethods(lldb::addr_t class_ptr);

  // Get class pointer by name using objc_getClass (direct runtime call)
  lldb::addr_t GetClassPointer(const std::string &class_name);

  // Get runtime function address by name with caching
  lldb::addr_t GetRuntimeFunctionAddress(const char *function_name);

  // Check if an object is a tagged pointer
  bool IsTaggedPointer(lldb::addr_t obj_addr);

  // Decode tagged pointer data for strings
  std::string DecodeTaggedString(lldb::addr_t obj_addr);

  // Get the class pointer for a tagged pointer using runtime functions
  lldb::addr_t GetTaggedPointerClass(lldb::addr_t obj_addr);

  // Get the class name for a tagged pointer
  std::string GetTaggedPointerClassName(lldb::addr_t obj_addr);

  // Check if an address represents a valid object
  bool IsValidObjectPointer(lldb::addr_t obj_addr);

  // Helper to call functions in the target process (delegates to utilities)
  lldb::addr_t CallRuntimeFunction(const std::string &function_name,
                                   const std::vector<lldb::addr_t> &args);

  // === Enhanced Runtime API (merged from GNUstepRuntimeV2API) ===

  // Runtime type aliases matching libobjc2
  using Class = void *;
  using Ivar = void *;
  using Method = void *;
  using Property = void *;
  using SEL = void *;

  // Information about an instance variable
  struct IvarInfo {
    std::string name;
    std::string type_encoding;
    ptrdiff_t offset;
    size_t size;
    Class defining_class; // Which class in hierarchy defines this
    std::string defining_class_name;
  };

  // === Core Enhanced Runtime Methods ===

  // Get all runtime classes as Class pointers
  llvm::Expected<std::vector<Class>> GetAllClasses();

  // Get all runtime classes with their ISA addresses and names
  llvm::Expected<std::vector<std::pair<lldb::addr_t, std::string>>>
  GetAllClassesWithISAs();

  // Get all ivars including inherited ones
  llvm::Expected<std::vector<IvarInfo>>
  GetAllIvarsIncludingInherited(Class cls);

  // Find a class by name (enhanced version returning Class pointer)

private:
  Process *m_process;
  uint32_t m_address_size;
  lldb::ByteOrder m_byte_order;

  // Consolidated runtime function caller
  std::unique_ptr<RuntimeFunctionCaller> m_runtime_caller;

  // Thread safety for enhanced functionality
  mutable std::recursive_mutex m_mutex;

  // Cache for function callers to avoid repeated compilation
  struct FunctionCallerCache {
    std::unique_ptr<FunctionCaller> objc_lookup_class_caller;
    std::unique_ptr<FunctionCaller> class_getName_caller;
    std::unique_ptr<FunctionCaller> object_getClass_caller;
    std::unique_ptr<FunctionCaller> class_getSuperclass_caller;
    // Generic cache for other functions
    std::unordered_map<std::string, std::unique_ptr<FunctionCaller>>
        generic_callers;
  };

  mutable FunctionCallerCache m_function_cache;

  // Cache for ISA to class name mapping
  mutable std::unordered_map<lldb::addr_t, lldb_private::ConstString>
      m_isa_to_name_cache;

  // New implementation methods for function calling
  lldb::addr_t CallRuntimeFunctionImpl(const char *function_name,
                                       const CompilerType &return_type,
                                       const ValueList &args,
                                       ExecutionContext &exe_ctx,
                                       Status &error) const;

  std::unique_ptr<FunctionCaller> &
  GetOrCreateFunctionCaller(const char *function_name,
                            const CompilerType &return_type,
                            const ValueList &arg_types,
                            ExecutionContext &exe_ctx, Status &error) const;

  // Helper to get modules
  lldb::ModuleSP GetObjCModule() const;
  lldb::ModuleSP GetFoundationModule() const;

  // === Enhanced Private Methods (merged from GNUstepRuntimeV2API) ===

  // Call a runtime function in target process with comprehensive error handling
  template <typename ReturnType>
  llvm::Expected<ReturnType>
  CallRuntimeFunction(lldb::addr_t function_addr,
                      const std::vector<lldb::addr_t> &args);

  // Read a C string from target memory with error handling
  llvm::Expected<std::string> ReadCStringFromTarget(lldb::addr_t addr);

  // Read memory from target with comprehensive error handling
  llvm::Expected<std::vector<uint8_t>> ReadMemory(lldb::addr_t addr,
                                                  size_t size);

  // Direct memory reading approach for class enumeration
  llvm::Expected<std::vector<Class>> GetAllClassesDirect();

  // Enhanced runtime modules
  lldb::ModuleSP m_objc_module;
  lldb::ModuleSP m_foundation_module;

  // Cache for all runtime classes to avoid repeated objc_copyClassList calls
  mutable std::vector<std::pair<lldb::addr_t, std::string>> m_all_classes_cache;
  mutable bool m_all_classes_cached = false;
  mutable uint32_t m_cached_stop_id = UINT32_MAX;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEINTROSPECTOR_H
