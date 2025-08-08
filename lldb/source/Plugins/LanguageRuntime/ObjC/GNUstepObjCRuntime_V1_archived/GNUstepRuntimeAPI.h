//===-- GNUstepRuntimeAPI.h ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef GNUSTEP_RUNTIME_API_H
#define GNUSTEP_RUNTIME_API_H

#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Symbol/CompilerType.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/lldb-defines.h"
#include "lldb/lldb-forward.h"
#include "lldb/lldb-types.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace lldb_private {

/// GNUstepRuntimeAPI provides safe, thread-safe access to libobjc2 runtime
/// functions. This class eliminates the need for manual memory layout
/// assumptions by using the runtime's own introspection capabilities.
///
/// CRITICAL PRINCIPLE: NO MEMORY LAYOUT GUESSING
/// This wrapper uses runtime symbols to let libobjc2 handle its own complexity.
/// All data structures are discovered dynamically at runtime.
class GNUstepRuntimeAPI {
public:
  /// Runtime function result wrapper for error handling
  template <typename T> struct RuntimeResult {
    T value;
    bool success;
    std::string error_message;

    RuntimeResult() : value{}, success(false) {}
    
    // Explicit constructors to avoid ambiguity
    static RuntimeResult Success(const T &val) {
      RuntimeResult result;
      result.value = val;
      result.success = true;
      return result;
    }
    
    static RuntimeResult Error(const std::string &error) {
      RuntimeResult result;
      result.success = false;
      result.error_message = error;
      return result;
    }

    explicit operator bool() const { return success; }
    T operator*() const { return value; }
  };

  /// Opaque handle for runtime objects (matches libobjc2 types)
  using Class = void *;
  using Ivar = void *;
  using Object = void *;

  /// Instance variable metadata
  struct IvarInfo {
    std::string name;
    std::string type_encoding;
    ptrdiff_t offset;
    size_t size;
  };

  /// Class metadata
  struct ClassInfo {
    std::string name;
    Class class_ptr;
    Class superclass_ptr;
    std::string superclass_name;
    size_t instance_size;
    std::vector<IvarInfo> ivars;
  };

  /// Initialize the runtime API for a given process
  /// @param process The LLDB process to operate on
  /// @return Shared pointer to the runtime API, or nullptr on failure
  static std::shared_ptr<GNUstepRuntimeAPI> Create(Process *process);

  ~GNUstepRuntimeAPI() = default;

  /// Get class information by name
  /// @param class_name Name of the class (e.g., "NSString", "BankAccount")
  /// @return ClassInfo on success, error on failure
  RuntimeResult<ClassInfo> GetClassInfo(const std::string &class_name);

  /// Get class information from an object instance
  /// @param object_addr Address of the object instance
  /// @return ClassInfo on success, error on failure
  RuntimeResult<ClassInfo> GetObjectClassInfo(lldb::addr_t object_addr);

  /// Get the value of an instance variable from an object
  /// @param object_addr Address of the object
  /// @param ivar_name Name of the instance variable
  /// @return Address of the ivar value, or LLDB_INVALID_ADDRESS on failure
  RuntimeResult<lldb::addr_t> GetIvarValue(lldb::addr_t object_addr,
                                           const std::string &ivar_name);

  /// Get all classes currently loaded in the runtime
  /// @return Vector of all loaded class names
  RuntimeResult<std::vector<std::string>> GetAllClassNames();

  /// Check if a class exists in the runtime
  /// @param class_name Name of the class to check
  /// @return True if class exists, false otherwise
  bool ClassExists(const std::string &class_name);

  /// Get runtime version information
  /// @return String describing the runtime version
  std::string GetRuntimeVersion();

  /// Check if the runtime API is properly initialized
  /// @return True if all required symbols were found
  bool IsValid() const { return m_is_valid; }

  /// Get last error message
  /// @return Last error encountered by the API
  std::string GetLastError() const { return m_last_error; }

  /// Thread-safe access to the underlying process
  Process *GetProcess() const { return m_process; }

private:
  explicit GNUstepRuntimeAPI(Process *process);
  
  /// Initialize runtime symbols - finds all required libobjc2 functions
  bool InitializeRuntimeSymbols();

  /// Find a runtime symbol and return its address
  /// @param symbol_name Name of the symbol to find
  /// @return Symbol address, or LLDB_INVALID_ADDRESS if not found
  lldb::addr_t FindRuntimeSymbol(const std::string &symbol_name);

  /// Execute a runtime function call safely
  /// @param symbol_addr Address of the runtime function
  /// @param args Function arguments
  /// @return Function result, or error on failure
  template <typename T, typename... Args>
  RuntimeResult<T> CallRuntimeFunction(lldb::addr_t symbol_addr, Args... args);

  /// Internal helper to get class pointer by name
  RuntimeResult<Class> GetClassByName(const std::string &class_name);

  /// Internal helper to get class name from class pointer
  RuntimeResult<std::string> GetClassName(Class class_ptr);

  /// Internal helper to get instance variables for a class
  RuntimeResult<std::vector<IvarInfo>> GetClassIvars(Class class_ptr);

  /// Internal helper to get superclass pointer
  RuntimeResult<Class> GetSuperclass(Class class_ptr);

  /// Internal helper to get class instance size
  RuntimeResult<size_t> GetInstanceSize(Class class_ptr);

  /// Set last error message
  void SetError(const std::string &error);

  /// Clear last error
  void ClearError();

private:
  Process *m_process;                      ///< LLDB process handle
  Target *m_target;                        ///< LLDB target handle
  bool m_is_valid;                         ///< Whether API is fully initialized
  std::string m_last_error;                ///< Last error message
  mutable std::mutex m_mutex;              ///< Thread safety

  // Runtime function addresses (discovered at initialization)
  lldb::addr_t m_objc_getClass_addr;           ///< objc_getClass function
  lldb::addr_t m_class_getName_addr;           ///< class_getName function  
  lldb::addr_t m_class_getSuperclass_addr;     ///< class_getSuperclass function
  lldb::addr_t m_class_getInstanceSize_addr;   ///< class_getInstanceSize function
  lldb::addr_t m_class_getInstanceVariable_addr; ///< class_getInstanceVariable function
  lldb::addr_t m_class_copyIvarList_addr;      ///< class_copyIvarList function
  lldb::addr_t m_object_getClass_addr;         ///< object_getClass function
  lldb::addr_t m_object_getIvar_addr;          ///< object_getIvar function
  lldb::addr_t m_ivar_getName_addr;            ///< ivar_getName function
  lldb::addr_t m_ivar_getOffset_addr;          ///< ivar_getOffset function
  lldb::addr_t m_ivar_getTypeEncoding_addr;    ///< ivar_getTypeEncoding function
  lldb::addr_t m_objc_copyClassList_addr;      ///< objc_copyClassList function
  lldb::addr_t m_free_addr;                    ///< free function

  // Symbol lookup cache to avoid repeated searches
  std::unordered_map<std::string, lldb::addr_t> m_symbol_cache;
  
  // Class info cache to avoid repeated runtime calls
  std::unordered_map<std::string, ClassInfo> m_class_cache;
};

/// Convenience typedef for shared pointer
using GNUstepRuntimeAPISP = std::shared_ptr<GNUstepRuntimeAPI>;

} // namespace lldb_private

#endif // GNUSTEP_RUNTIME_API_H