//===-- GNUstepObjCRuntimeUtilities.h ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEUTILITIES_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEUTILITIES_H

#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/Status.h"
#include "lldb/lldb-private.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

// Hash function for std::pair<lldb::addr_t, std::string>
namespace std {
template <> struct hash<std::pair<lldb::addr_t, std::string>> {
  size_t operator()(const std::pair<lldb::addr_t, std::string> &p) const {
    size_t h1 = std::hash<lldb::addr_t>{}(p.first);
    size_t h2 = std::hash<std::string>{}(p.second);
    return h1 ^ (h2 << 1); // Combine hashes
  }
};
} // namespace std

namespace lldb_private {
namespace gnustep_objc_runtime_utilities {

/// Create safe expression evaluation options for GNUstep runtime operations.
/// These settings are critical for Windows to avoid first-chance exception
/// issues and ensure proper timeout handling.
/// \param for_utility_expression Whether this is for a utility expression
/// \return Configured expression options safe for GNUstep runtime
EvaluateExpressionOptions
MakeSafeExpressionOptions(bool for_utility_expression = false);

/// RAII helper for allocating and managing strings in target process memory
class TargetStringAllocator {
public:
  TargetStringAllocator(Process *process, const std::string &str);
  ~TargetStringAllocator();

  // Non-copyable, movable
  TargetStringAllocator(const TargetStringAllocator &) = delete;
  TargetStringAllocator &operator=(const TargetStringAllocator &) = delete;
  TargetStringAllocator(TargetStringAllocator &&other) noexcept;
  TargetStringAllocator &operator=(TargetStringAllocator &&other) noexcept;

  /// Get the address of the allocated string in target memory
  lldb::addr_t GetAddress() const { return m_address; }

  /// Check if allocation was successful
  bool IsValid() const { return m_address != LLDB_INVALID_ADDRESS; }

  /// Get any allocation error
  const Status &GetError() const { return m_error; }

private:
  Process *m_process;
  lldb::addr_t m_address;
  Status m_error;

  void Cleanup();
};

/// Non-thread-safe reentrancy guard for single-threaded contexts
/// Use this only when you're certain the code runs on a single thread
class SimpleReentrancyGuard {
public:
  explicit SimpleReentrancyGuard(bool &flag) : m_flag(flag), m_acquired(false) {
    if (!m_flag) {
      m_flag = true;
      m_acquired = true;
    }
  }

  ~SimpleReentrancyGuard() {
    if (m_acquired) {
      m_flag = false;
    }
  }

  bool IsAcquired() const { return m_acquired; }

  // Non-copyable, non-movable
  SimpleReentrancyGuard(const SimpleReentrancyGuard &) = delete;
  SimpleReentrancyGuard &operator=(const SimpleReentrancyGuard &) = delete;
  SimpleReentrancyGuard(SimpleReentrancyGuard &&) = delete;
  SimpleReentrancyGuard &operator=(SimpleReentrancyGuard &&) = delete;

private:
  bool &m_flag;
  bool m_acquired;
};

/// Helper to setup execution context for runtime function calls
bool SetupRuntimeExecutionContext(Process *process, ExecutionContext &exe_ctx);

/// Consolidated runtime function caller - replaces 3 different implementations
class RuntimeFunctionCaller {
public:
  explicit RuntimeFunctionCaller(Process *process);
  ~RuntimeFunctionCaller() = default;

  /// Call a runtime function with a single string argument (objc_getClass,
  /// sel_getUid)
  lldb::addr_t CallRuntimeFunction(const char *function_name,
                                   const char *string_arg);

  /// Call a runtime function with multiple arguments
  lldb::addr_t CallRuntimeFunction(const std::string &function_name,
                                   const std::vector<lldb::addr_t> &args);

  /// Get or resolve a runtime function address with caching
  lldb::addr_t GetRuntimeFunctionAddress(const char *function_name);

  /// Call objc_msgSend to invoke a method on an object
  /// Returns the result address, or LLDB_INVALID_ADDRESS on failure
  lldb::addr_t CallObjCMethod(lldb::addr_t object_addr,
                              const char *selector_name);

  /// Get a selector ID for a selector name using sel_getUid
  lldb::addr_t GetSelectorForName(const char *selector_name);

  /// Find GNUstep runtime modules (libobjc2, libgnustep-base)
  lldb::ModuleSP FindObjCModule() const;
  lldb::ModuleSP FindFoundationModule() const;

private:
  Process *m_process;
  std::unordered_map<std::string, lldb::addr_t> m_symbol_cache;
  mutable std::mutex
      m_runtime_mutex; // Mutex for thread-safe runtime calls and reentrancy

  /// Core implementation for calling runtime functions
  lldb::addr_t CallRuntimeFunctionImpl(const char *function_name,
                                       const CompilerType &return_type,
                                       ValueList &args,
                                       ExecutionContext &exe_ctx,
                                       Status &error);
};

// Helper functions for testing - these don't require runtime
inline bool IsTaggedPointer(uint64_t ptr) {
  // In GNUstep, tagged pointers have the low bit set
  return (ptr & 1) != 0;
}

} // namespace gnustep_objc_runtime_utilities
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEUTILITIES_H
