//===-- FormatterTestHelpers.h ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_UNITTESTS_LANGUAGE_OBJC_GNUSTEP_FORMATTERTESTHELPERS_H
#define LLDB_UNITTESTS_LANGUAGE_OBJC_GNUSTEP_FORMATTERTESTHELPERS_H

#include "lldb/ValueObject/ValueObject.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Core/Value.h"
#include "llvm/ADT/StringRef.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <memory>
#include <vector>

namespace lldb_private {
namespace formatters {
namespace test {

// Mock Target for testing
class MockTarget : public Target {
public:
  MockTarget(Debugger &debugger, const ArchSpec &target_arch,
             const lldb::PlatformSP &platform_sp)
      : Target(debugger, target_arch, platform_sp, true) {}
};

// Mock Process for testing with resource management
class MockProcess : public Process {
private:
  static constexpr size_t kMaxMemorySize = 1024 * 1024; // 1MB limit
  mutable std::mutex m_memory_mutex;
  size_t m_total_memory_size{0};
  
public:
  MockProcess(lldb::TargetSP target_sp, lldb::ListenerSP listener_sp)
      : Process(target_sp, listener_sp) {}

  // Required virtual methods
  size_t DoReadMemory(lldb::addr_t addr, void *buf, size_t size,
                      Status &error) override {
    std::lock_guard<std::mutex> guard(m_memory_mutex);
    auto it = m_memory.find(addr);
    if (it != m_memory.end() && it->second.size() >= size) {
      memcpy(buf, it->second.data(), size);
      error.Clear();
      return size;
    }
    error = Status::FromErrorString("Memory not found");
    return 0;
  }

  void SetMemory(lldb::addr_t addr, const void *data, size_t size) {
    if (!data || size == 0) return;
    
    std::lock_guard<std::mutex> guard(m_memory_mutex);
    
    // Check memory limits to prevent unbounded growth
    if (m_total_memory_size + size > kMaxMemorySize) {
      // Clear some memory to make space (simple LRU-style)
      ClearMemoryInternal();
    }
    
    // Remove existing entry if any
    auto it = m_memory.find(addr);
    if (it != m_memory.end()) {
      m_total_memory_size -= it->second.size();
      m_memory.erase(it);
    }
    
    // Add new memory
    std::vector<uint8_t> buffer(size);
    memcpy(buffer.data(), data, size);
    m_memory[addr] = std::move(buffer);
    m_total_memory_size += size;
  }

  void ClearMemory() {
    std::lock_guard<std::mutex> guard(m_memory_mutex);
    ClearMemoryInternal();
  }

private:
  void ClearMemoryInternal() {
    m_memory.clear();
    m_total_memory_size = 0;
  }

  // Other required overrides (stubs)
  bool CanDebug(lldb::TargetSP target, bool plugin_specified_by_name) override {
    return true;
  }
  Status DoDestroy() override { return Status(); }
  void RefreshStateAfterStop() override {}
  size_t DoWriteMemory(lldb::addr_t addr, const void *buf, size_t size,
                       Status &error) override {
    SetMemory(addr, buf, size);
    error.Clear();
    return size;
  }
  bool DoUpdateThreadList(ThreadList &old_thread_list,
                         ThreadList &new_thread_list) override {
    return true;
  }
  llvm::StringRef GetPluginName() override { return "MockProcess"; }

public:
  // Architecture stub
  const ArchSpec &GetArchitecture() const {
    static ArchSpec arch("x86_64-pc-linux");
    return arch;
  }

private:
  std::map<lldb::addr_t, std::vector<uint8_t>> m_memory;
};

// Helper to create a mock ValueObject
class MockValueObject {
public:
  static lldb::ValueObjectSP Create(lldb::TargetSP target,
                                    const std::string &name,
                                    lldb::addr_t address,
                                    const std::string &type_name) {
    // For unit tests, create a simple value pointing to the address
    // The formatters just need to read memory from this address
    Value value;
    value.SetValueType(Value::ValueType::LoadAddress);
    value.GetScalar() = address;
    
    ExecutionContextScope *exe_scope = target ? target.get() : nullptr;
    return ValueObjectConstResult::Create(exe_scope, value, ConstString(name));
  }
};

// Helper to set up memory for GNUstep objects
class GNUstepMemoryHelper {
private:
  // Platform-independent constants
  static constexpr size_t kPointerSize = sizeof(void*);
  static constexpr size_t kNSStringStructSize = 5 * kPointerSize;  // isa + len + len2 + str_ptr + padding
  
public:
  static void SetupNSString(MockProcess &process, lldb::addr_t addr,
                            const std::string &string_value) {
    // Create NSConstantString layout
    struct {
      uint64_t isa;
      uint64_t length;
      uint64_t length2;
      uint64_t str_ptr;
    } string_obj = {
      0x1000,  // ISA
      string_value.length(),
      string_value.length(),
      addr + sizeof(string_obj)
    };
    
    process.SetMemory(addr, &string_obj, sizeof(string_obj));
    process.SetMemory(addr + sizeof(string_obj), string_value.c_str(), 
                     string_value.length() + 1);
  }

  static void SetupNSNumber(MockProcess &process, lldb::addr_t addr,
                           int64_t value) {
    // Create NSNumber layout
    struct {
      uint64_t isa;
      int64_t value;
    } number_obj = {
      0x2000,  // ISA
      value
    };
    
    process.SetMemory(addr, &number_obj, sizeof(number_obj));
  }

  static void SetupNSArray(MockProcess &process, lldb::addr_t addr,
                          const std::vector<lldb::addr_t> &elements) {
    // Create GSArray layout
    struct {
      uint64_t isa;
      uint64_t count;
      uint64_t objects_ptr;
    } array_obj = {
      0x3000,  // ISA
      elements.size(),
      addr + sizeof(array_obj)
    };
    
    process.SetMemory(addr, &array_obj, sizeof(array_obj));
    
    // Write element pointers if any
    if (!elements.empty()) {
      process.SetMemory(addr + sizeof(array_obj), elements.data(), 
                       elements.size() * sizeof(lldb::addr_t));
    }
  }
};

// Performance measurement helper with monotonic guarantees
class PerformanceTimer {
public:
  PerformanceTimer() noexcept : m_start(std::chrono::steady_clock::now()) {}
  
  double ElapsedMilliseconds() const noexcept {
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - m_start).count();
  }
  
  void AssertUnder(double max_ms, const std::string &operation) const {
    double elapsed = ElapsedMilliseconds();
    ASSERT_LT(elapsed, max_ms) 
        << operation << " took " << elapsed << "ms (max: " << max_ms << "ms)";
  }

  // Reset timer for reuse
  void Reset() noexcept {
    m_start = std::chrono::steady_clock::now();
  }

private:
  std::chrono::steady_clock::time_point m_start;
};

// Common test assertions
class FormatterAssertions {
public:
  static void AssertSummaryContains(const std::string &summary,
                                    const std::string &expected) {
    ASSERT_TRUE(summary.find(expected) != std::string::npos)
        << "Summary '" << summary << "' does not contain '" << expected << "'";
  }
  
  static void AssertSummaryEquals(const std::string &summary,
                                  const std::string &expected) {
    ASSERT_EQ(summary, expected)
        << "Summary mismatch. Expected: '" << expected 
        << "', Got: '" << summary << "'";
  }
  
  static void AssertChildCount(lldb::ValueObjectSP valobj,
                               size_t expected_count) {
    auto child_count = valobj->GetNumChildren();
    if (child_count) {
      ASSERT_EQ(*child_count, expected_count)
          << "Child count mismatch. Expected: " << expected_count
          << ", Got: " << *child_count;
    } else {
      FAIL() << "Failed to get child count";
    }
  }
};

} // namespace test
} // namespace formatters
} // namespace lldb_private

#endif // LLDB_UNITTESTS_LANGUAGE_OBJC_GNUSTEP_FORMATTERTESTHELPERS_H