//===-- GNUstepObjCRuntimeUtilities.h ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEUTILITIES_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEUTILITIES_H

#include "lldb/lldb-private.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/Status.h"
#include <atomic>
#include <memory>

namespace lldb_private {
namespace gnustep_objc_runtime_utilities {

/// Create safe expression evaluation options for GNUstep runtime operations
/// These settings are critical for Windows to avoid first-chance exception issues
EvaluateExpressionOptions MakeSafeExpressionOptions(bool for_utility_expression = false);

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

/// Thread-safe reentrancy guard using atomic operations
/// This version works correctly across threads unlike the bool version
class ReentrancyGuard {
public:
  explicit ReentrancyGuard(std::atomic<bool> &flag) : m_flag(flag) {
    bool expected = false;
    m_acquired = m_flag.compare_exchange_strong(expected, true);
  }
  
  ~ReentrancyGuard() {
    if (m_acquired) {
      m_flag.store(false);
    }
  }
  
  bool IsAcquired() const { return m_acquired; }
  
  // Non-copyable, non-movable
  ReentrancyGuard(const ReentrancyGuard &) = delete;
  ReentrancyGuard &operator=(const ReentrancyGuard &) = delete;
  ReentrancyGuard(ReentrancyGuard &&) = delete;
  ReentrancyGuard &operator=(ReentrancyGuard &&) = delete;
  
private:
  std::atomic<bool> &m_flag;
  bool m_acquired;
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

/// Cache for runtime function addresses to avoid repeated symbol lookups
class RuntimeSymbolCache {
public:
  explicit RuntimeSymbolCache(Process *process);
  
  /// Get cached address or resolve and cache it
  lldb::addr_t GetSymbolAddress(const char *symbol_name);
  
  /// Clear all cached addresses (call when modules are loaded/unloaded)
  void InvalidateCache();
  
  /// Check if essential runtime symbols are available
  bool HasEssentialSymbols() const;

private:
  Process *m_process;
  std::unordered_map<std::string, lldb::addr_t> m_symbol_cache;
  
  lldb::addr_t ResolveSymbol(const char *symbol_name);
};

} // namespace gnustep_objc_runtime_utilities
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEUTILITIES_H
