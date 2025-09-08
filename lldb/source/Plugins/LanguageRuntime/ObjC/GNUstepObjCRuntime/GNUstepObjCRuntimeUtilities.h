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
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>


// Hash function for std::pair<lldb::addr_t, std::string>
namespace std {
template <>
struct hash<std::pair<lldb::addr_t, std::string>> {
  size_t operator()(const std::pair<lldb::addr_t, std::string> &p) const {
    size_t h1 = std::hash<lldb::addr_t>{}(p.first);
    size_t h2 = std::hash<std::string>{}(p.second);
    return h1 ^ (h2 << 1);  // Combine hashes
  }
};
}

namespace lldb_private {
namespace gnustep_objc_runtime_utilities {

/// Create safe expression evaluation options for GNUstep runtime operations
/// These settings are critical for Windows to avoid first-chance exception
/// issues
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

/// RAII helper for allocating typed data in target process memory
template <typename T> class TargetDataAllocator {
public:
  explicit TargetDataAllocator(Process *process)
      : m_process(process), m_address(LLDB_INVALID_ADDRESS),
        m_needs_cleanup(false) {}

  ~TargetDataAllocator() {
    if (m_needs_cleanup && m_address != LLDB_INVALID_ADDRESS) {
      m_process->DeallocateMemory(m_address);
    }
  }

  // Non-copyable, movable
  TargetDataAllocator(const TargetDataAllocator &) = delete;
  TargetDataAllocator &operator=(const TargetDataAllocator &) = delete;
  TargetDataAllocator(TargetDataAllocator &&other) noexcept
      : m_process(other.m_process), m_address(other.m_address),
        m_needs_cleanup(other.m_needs_cleanup) {
    other.m_needs_cleanup = false;
  }

  /// Allocates memory and optionally writes initial value
  Status Allocate(const T *initial_value = nullptr) {
    Status error;
    m_address = m_process->AllocateMemory(
        sizeof(T), lldb::ePermissionsReadable | lldb::ePermissionsWritable,
        error);

    if (error.Fail() || m_address == LLDB_INVALID_ADDRESS) {
      return error;
    }

    m_needs_cleanup = true;

    if (initial_value) {
      size_t bytes_written =
          m_process->WriteMemory(m_address, initial_value, sizeof(T), error);
      if (bytes_written != sizeof(T) || error.Fail()) {
        return error;
      }
    }

    return error;
  }

  /// Writes value to allocated memory
  Status WriteValue(const T &value) {
    if (m_address == LLDB_INVALID_ADDRESS) {
      return Status("Memory not allocated");
    }

    Status error;
    size_t bytes_written =
        m_process->WriteMemory(m_address, &value, sizeof(T), error);
    if (bytes_written != sizeof(T) || error.Fail()) {
      return error;
    }

    return error;
  }

  /// Reads value from allocated memory
  Status ReadValue(T &value) {
    if (m_address == LLDB_INVALID_ADDRESS) {
      return Status("Memory not allocated");
    }

    Status error;
    size_t bytes_read =
        m_process->ReadMemory(m_address, &value, sizeof(T), error);
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
  static lldb_private::Log *GetLanguageLog();
  static lldb_private::Log *GetProcessLog();

  // Scoped logging with automatic prefix
  class ScopedLogger {
  public:
    ScopedLogger(const char *function_name, const char *prefix = "[GNUstep]");
    ~ScopedLogger();

    template <typename... Args>
    void LogMessage(const char *format, Args &&...args) const {
      if (m_log) {
        LLDB_LOG(m_log, "{0} {1}: {2}", m_prefix, m_function_name,
                 llvm::formatv(format, std::forward<Args>(args)...));
      }
    }

  private:
    lldb_private::Log *m_log;
    std::string m_prefix;
    std::string m_function_name;
  };
};

/// Symbol resolution helper with error handling
class SymbolResolver {
public:
  SymbolResolver(Target &target) : m_target(target) {}

  // Find symbol with fallback search and logging
  const Symbol *
  FindSymbolWithFallback(const ConstString &symbol_name,
                         lldb::SymbolType symbol_type = lldb::eSymbolTypeCode,
                         lldb::ModuleSP preferred_module = nullptr);

  // Find symbols across all modules with logging
  void FindSymbolsAcrossModules(const ConstString &symbol_name,
                                lldb::SymbolType symbol_type,
                                SymbolContextList &sc_list);

private:
  Target &m_target;
};

// Forward declaration 
struct FormatterCache;

/// Consolidated runtime function caller - replaces 3 different implementations
class RuntimeFunctionCaller {
public:
  explicit RuntimeFunctionCaller(Process *process);
  ~RuntimeFunctionCaller() = default;

  /// Call a runtime function with a single string argument (objc_getClass, sel_getUid)
  lldb::addr_t CallRuntimeFunction(const char *function_name, const char *string_arg);
  
  /// Call a runtime function with multiple arguments
  lldb::addr_t CallRuntimeFunction(const std::string &function_name, 
                                   const std::vector<lldb::addr_t> &args);

  /// Get or resolve a runtime function address with caching
  lldb::addr_t GetRuntimeFunctionAddress(const char *function_name);

  /// Call objc_msgSend to invoke a method on an object
  /// Returns the result address, or LLDB_INVALID_ADDRESS on failure
  lldb::addr_t CallObjCMethod(lldb::addr_t object_addr, const char *selector_name);
  
  /// Get a selector ID for a selector name using sel_getUid
  lldb::addr_t GetSelectorForName(const char *selector_name);
  
  /// Batch operations for collections - fetch multiple elements at once
  /// Returns vector of element addresses
  std::vector<lldb::addr_t> GetArrayElements(lldb::addr_t array_addr, 
                                             uint32_t start_idx, 
                                             uint32_t count);
  
  /// Get all dictionary keys and values in one operation
  /// Returns pair of <keys, values> vectors
  std::pair<std::vector<lldb::addr_t>, std::vector<lldb::addr_t>> 
    GetDictionaryKeysAndValues(lldb::addr_t dict_addr);
  
  /// Get all set elements in one operation
  std::vector<lldb::addr_t> GetSetElements(lldb::addr_t set_addr);

  /// Find GNUstep runtime modules (libobjc2, libgnustep-base)
  lldb::ModuleSP FindObjCModule() const;
  lldb::ModuleSP FindFoundationModule() const;

  // Public access to formatter cache for use by formatters
  FormatterCache &GetFormatterCache() { return *m_formatter_cache; }
  
private:
  Process *m_process;
  RuntimeSymbolCache m_symbol_cache;
  mutable std::atomic<bool> m_in_function_call{false};
  mutable std::mutex m_runtime_mutex;  // Mutex for thread-safe runtime calls
  mutable std::unique_ptr<FormatterCache> m_formatter_cache;  // Cache for formatter optimization

  /// Core implementation for calling runtime functions
  lldb::addr_t CallRuntimeFunctionImpl(const char *function_name,
                                       const CompilerType &return_type,
                                       ValueList &args,
                                       ExecutionContext &exe_ctx,
                                       Status &error);
};

/// Multi-level cache for formatter performance optimization
struct FormatterCache {
  // Cache for object counts (arrays, dictionaries, sets)
  std::unordered_map<lldb::addr_t, uint32_t> count_cache;
  
  // Cache for child elements (avoid repeated objc_msgSend calls)
  struct ChildrenCache {
    std::vector<lldb::addr_t> elements;
    uint32_t stop_id;
  };
  std::unordered_map<lldb::addr_t, ChildrenCache> children_cache;
  
  // Cache for selector IDs (avoid repeated sel_getUid calls)
  std::unordered_map<std::string, lldb::addr_t> selector_cache;
  
  // Cache for method results (allKeys, allObjects, etc.)
  std::unordered_map<std::pair<lldb::addr_t, std::string>, lldb::addr_t> method_cache;
  
  // Collection-specific batch caches for improved performance
  struct CollectionBatch {
    uint32_t start_idx;
    uint32_t count;
    std::vector<lldb::addr_t> elements;
    uint32_t cached_at_stop_id;
    
    bool ContainsIndex(uint32_t idx) const {
      return idx >= start_idx && idx < start_idx + count;
    }
    
    lldb::addr_t GetElement(uint32_t idx) const {
      if (!ContainsIndex(idx) || idx - start_idx >= elements.size())
        return LLDB_INVALID_ADDRESS;
      return elements[idx - start_idx];
    }
  };
  
  struct CollectionCacheEntry {
    std::vector<CollectionBatch> batches;  // Multiple batches per collection
    uint32_t total_count = 0;  // Total elements in collection
    uint32_t last_accessed_stop_id = 0;
    
    // Find element in cached batches
    lldb::addr_t GetElement(uint32_t idx) const {
      for (const auto& batch : batches) {
        if (batch.ContainsIndex(idx))
          return batch.GetElement(idx);
      }
      return LLDB_INVALID_ADDRESS;
    }
    
    // Check if index is cached
    bool HasElement(uint32_t idx) const {
      for (const auto& batch : batches) {
        if (batch.ContainsIndex(idx))
          return true;
      }
      return false;
    }
    
    // Add a new batch
    void AddBatch(uint32_t start_idx, std::vector<lldb::addr_t>&& elements, uint32_t stop_id) {
      CollectionBatch batch;
      batch.start_idx = start_idx;
      batch.count = elements.size();
      batch.elements = std::move(elements);
      batch.cached_at_stop_id = stop_id;
      batches.push_back(std::move(batch));
    }
  };
  
  struct CollectionCache {
    // Array cache with batching
    std::unordered_map<lldb::addr_t, CollectionCacheEntry> array_cache;
    
    // Dictionary cache (keys and values are often accessed together)
    struct DictCacheEntry {
      std::vector<lldb::addr_t> keys;
      std::vector<lldb::addr_t> values;
      uint32_t cached_at_stop_id;
    };
    std::unordered_map<lldb::addr_t, DictCacheEntry> dict_cache;
    
    // Set cache (usually fetch all at once)
    struct SetCacheEntry {
      std::vector<lldb::addr_t> elements;
      uint32_t cached_at_stop_id;
    };
    std::unordered_map<lldb::addr_t, SetCacheEntry> set_cache;
    
    // Performance metrics
    struct CacheStats {
      uint64_t hits = 0;
      uint64_t misses = 0;
      uint64_t batch_fetches = 0;
      uint64_t invalidations = 0;
    } stats;
  };
  CollectionCache collection_cache;
  
  // Cache validity tracking
  uint32_t cached_stop_id = UINT32_MAX;
  std::chrono::steady_clock::time_point last_update;
  
  // Batch operation configuration
  static constexpr uint32_t BATCH_SIZE = 20;  // Minimum batch size for collection fetching
  static constexpr uint32_t MAX_BATCH_SIZE = 100;  // Maximum batch size to avoid excessive memory
  
  // Invalidate cache if process state changed
  void InvalidateIfNeeded(Process *process) {
    if (!process) return;
    
    uint32_t current_stop_id = process->GetStopID();
    if (current_stop_id != cached_stop_id) {
      // Process stopped at different point, selectively invalidate
      
      // Always clear per-stop caches
      count_cache.clear();
      children_cache.clear();
      method_cache.clear();
      
      // For collections, only clear if stop ID is significantly different
      // This allows caching across minor stops (like stepping through code)
      if (cached_stop_id == UINT32_MAX || current_stop_id > cached_stop_id + 5) {
        collection_cache.array_cache.clear();
        collection_cache.dict_cache.clear();
        collection_cache.set_cache.clear();
        collection_cache.stats.invalidations++;
      } else {
        // Keep collection cache but mark as potentially stale
        // Individual entries will revalidate on access
      }
      
      // Keep selector cache as selectors don't change
      cached_stop_id = current_stop_id;
      last_update = std::chrono::steady_clock::now();
    }
  }
  
  // Invalidate specific collection
  void InvalidateCollection(lldb::addr_t collection_addr) {
    collection_cache.array_cache.erase(collection_addr);
    collection_cache.dict_cache.erase(collection_addr);
    collection_cache.set_cache.erase(collection_addr);
  }
  
  // Check if cache is still valid
  bool IsValid(Process *process) const {
    if (!process) return false;
    return process->GetStopID() == cached_stop_id;
  }
};

/// Process state guard to ensure safe runtime calls
/// Prevents crashes when process continues during formatter evaluation
class ProcessStateGuard {
public:
  explicit ProcessStateGuard(Process *process) 
      : m_process(process), m_is_valid(false) {
    if (m_process) {
      m_initial_state = m_process->GetState();
      // Only valid if process is stopped
      m_is_valid = (m_initial_state == lldb::eStateStopped);
      if (m_is_valid) {
        // Increment process stop ID to detect state changes
        m_stop_id = m_process->GetStopID();
      }
    }
  }
  
  ~ProcessStateGuard() = default;
  
  bool IsValid() const { 
    if (!m_is_valid || !m_process)
      return false;
    
    // Check if process state changed
    if (m_process->GetState() != m_initial_state)
      return false;
      
    // Check if stop ID changed (process continued and stopped again)
    if (m_process->GetStopID() != m_stop_id)
      return false;
      
    return true;
  }
  
  lldb::StateType GetState() const { return m_initial_state; }
  
private:
  Process *m_process;
  lldb::StateType m_initial_state;
  uint32_t m_stop_id;
  bool m_is_valid;
};

// Helper functions for testing - these don't require runtime
inline bool IsTaggedPointer(uint64_t ptr) {
  // In GNUstep, tagged pointers have the low bit set
  return (ptr & 1) != 0;
}

inline uint64_t GetTaggedPointerSlot(uint64_t ptr) {
  // Bits 1-3 contain the slot index
  return (ptr >> 1) & 0x7;
}

inline uint32_t GetPointerSize(uint32_t address_size) {
  return address_size;
}

inline uint64_t AlignTo(uint64_t value, uint64_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

inline std::string ExtractClassName(const std::string &mangled) {
  // Extract class name from mangled Objective-C symbols
  size_t pos = mangled.find("_OBJC_CLASS_$_");
  if (pos != std::string::npos) {
    return mangled.substr(pos + 14);
  }
  return "";
}

inline std::string CleanSelectorName(const std::string &selector) {
  // Clean up selector names (already clean in GNUstep)
  return selector;
}

inline std::string FormatRuntimeError(const std::string &function,
                                      const std::string &error) {
  return "Runtime error in " + function + ": " + error;
}

inline bool IsAligned(uint64_t value, uint64_t alignment) {
  return (value % alignment) == 0;
}

inline uint32_t SwapBytes32(uint32_t value) {
  return ((value & 0xFF000000) >> 24) |
         ((value & 0x00FF0000) >> 8) |
         ((value & 0x0000FF00) << 8) |
         ((value & 0x000000FF) << 24);
}

inline uint64_t SwapBytes64(uint64_t value) {
  return ((value & 0xFF00000000000000ULL) >> 56) |
         ((value & 0x00FF000000000000ULL) >> 40) |
         ((value & 0x0000FF0000000000ULL) >> 24) |
         ((value & 0x000000FF00000000ULL) >> 8) |
         ((value & 0x00000000FF000000ULL) << 8) |
         ((value & 0x0000000000FF0000ULL) << 24) |
         ((value & 0x000000000000FF00ULL) << 40) |
         ((value & 0x00000000000000FFULL) << 56);
}

inline std::string GetPluginName() {
  return "gnustep-objc-runtime";
}

inline std::string GetPluginDescription() {
  return "GNUstep Objective-C runtime support plugin for LLDB";
}

} // namespace gnustep_objc_runtime_utilities
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEUTILITIES_H
