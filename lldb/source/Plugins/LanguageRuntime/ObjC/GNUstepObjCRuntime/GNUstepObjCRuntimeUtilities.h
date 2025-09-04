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

/// RAII helper for allocating typed data in target process memory
template<typename T>
class TargetDataAllocator {
public:
  explicit TargetDataAllocator(Process *process) 
      : m_process(process), m_address(LLDB_INVALID_ADDRESS), m_needs_cleanup(false) {}
  
  ~TargetDataAllocator() {
    if (m_needs_cleanup && m_address != LLDB_INVALID_ADDRESS) {
      m_process->DeallocateMemory(m_address);
    }
  }
  
  // Non-copyable, movable
  TargetDataAllocator(const TargetDataAllocator&) = delete;
  TargetDataAllocator& operator=(const TargetDataAllocator&) = delete;
  TargetDataAllocator(TargetDataAllocator&& other) noexcept 
      : m_process(other.m_process), m_address(other.m_address), m_needs_cleanup(other.m_needs_cleanup) {
    other.m_needs_cleanup = false;
  }
  
  /// Allocates memory and optionally writes initial value
  Status Allocate(const T* initial_value = nullptr) {
    Status error;
    m_address = m_process->AllocateMemory(
        sizeof(T), 
        lldb::ePermissionsReadable | lldb::ePermissionsWritable, 
        error);
    
    if (error.Fail() || m_address == LLDB_INVALID_ADDRESS) {
      return error;
    }
    
    m_needs_cleanup = true;
    
    if (initial_value) {
      size_t bytes_written = m_process->WriteMemory(m_address, initial_value, sizeof(T), error);
      if (bytes_written != sizeof(T) || error.Fail()) {
        return error;
      }
    }
    
    return error;
  }
  
  /// Writes value to allocated memory
  Status WriteValue(const T& value) {
    if (m_address == LLDB_INVALID_ADDRESS) {
      return Status("Memory not allocated");
    }
    
    Status error;
    size_t bytes_written = m_process->WriteMemory(m_address, &value, sizeof(T), error);
    if (bytes_written != sizeof(T) || error.Fail()) {
      return error;
    }
    
    return error;
  }
  
  /// Reads value from allocated memory
  Status ReadValue(T& value) {
    if (m_address == LLDB_INVALID_ADDRESS) {
      return Status("Memory not allocated");
    }
    
    Status error;
    size_t bytes_read = m_process->ReadMemory(m_address, &value, sizeof(T), error);
    if (bytes_read != sizeof(T) || error.Fail()) {
      return error;
    }
    
    return error;
  }
  
  /// Get the allocated memory address
  lldb::addr_t GetAddress() const { return m_address; }
  
  /// Check if allocation was successful
  bool IsValid() const { return m_address != LLDB_INVALID_ADDRESS; }

private:
  Process *m_process;
  lldb::addr_t m_address;
  bool m_needs_cleanup;
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

/// Logging helpers for consistent formatting
class GNUStepLogger {
public:
  // Get logger with standard GNUstep categories
  static lldb_private::Log* GetLanguageLog();
  static lldb_private::Log* GetProcessLog();
  
  // Scoped logging with automatic prefix
  class ScopedLogger {
  public:
    ScopedLogger(const char* function_name, const char* prefix = "[GNUstep]");
    ~ScopedLogger();
    
    template<typename... Args>
    void LogMessage(const char* format, Args&&... args) const {
      if (m_log) {
        LLDB_LOG(m_log, "{0} {1}: {2}", m_prefix, m_function_name, 
                 llvm::formatv(format, std::forward<Args>(args)...));
      }
    }
    
  private:
    lldb_private::Log* m_log;
    std::string m_prefix;
    std::string m_function_name;
  };
};

/// Symbol resolution helper with error handling
class SymbolResolver {
public:
  SymbolResolver(Target& target) : m_target(target) {}
  
  // Find symbol with fallback search and logging
  const Symbol* FindSymbolWithFallback(
      const ConstString& symbol_name,
      lldb::SymbolType symbol_type = lldb::eSymbolTypeCode,
      lldb::ModuleSP preferred_module = nullptr);
  
  // Find symbols across all modules with logging
  void FindSymbolsAcrossModules(
      const ConstString& symbol_name,
      lldb::SymbolType symbol_type,
      SymbolContextList& sc_list);
      
private:
  Target& m_target;
};

} // namespace gnustep_objc_runtime_utilities
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEUTILITIES_H
