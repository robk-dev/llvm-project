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
#include "llvm/ADT/StringRef.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <memory>
#include <vector>

namespace lldb_private {
namespace formatters {
namespace test {

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
    // Create a simple pointer type
    CompilerType pointer_type = target->GetScratchTypeSystemForLanguage(
        lldb::eLanguageTypeObjC)
        ->GetBuiltinTypeForEncodingAndBitSize(lldb::eEncodingUint, 64);
    
    // Create value object
    DataExtractor data;
    data.SetData(&address, sizeof(address), target->GetArchitecture().GetByteOrder());
    
    return ValueObjectConstResult::Create(
        target.get(), pointer_type, ConstString(name), data);
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
    // Create platform-independent NSConstantString layout using DataExtractor
    const ArchSpec &arch = process.GetArchitecture();
    const lldb::ByteOrder byte_order = arch.GetByteOrder();
    const uint32_t addr_size = arch.GetAddressByteSize();
    
    // Allocate buffer for the string object
    std::vector<uint8_t> string_buffer(kNSStringStructSize, 0);
    
    // Use DataExtractor for platform-independent serialization
    DataExtractor extractor(string_buffer.data(), string_buffer.size(), 
                           byte_order, addr_size);
    
    lldb::offset_t offset = 0;
    // Write ISA pointer
    extractor.PutAddress(offset, 0x1000);
    offset += addr_size;
    
    // Write length (platform-appropriate size)
    if (addr_size == 8) {
      extractor.PutU64(offset, string_value.length());
      offset += 8;
      extractor.PutU64(offset, string_value.length());  // len2
      offset += 8;
    } else {
      extractor.PutU32(offset, static_cast<uint32_t>(string_value.length()));
      offset += 4;
      extractor.PutU32(offset, static_cast<uint32_t>(string_value.length()));  // len2
      offset += 4;
    }
    
    // Write string data pointer
    extractor.PutAddress(offset, addr + kNSStringStructSize);
    
    process.SetMemory(addr, string_buffer.data(), string_buffer.size());
    process.SetMemory(addr + kNSStringStructSize, string_value.c_str(), 
                     string_value.length() + 1);
  }

  static void SetupNSNumber(MockProcess &process, lldb::addr_t addr,
                           int64_t value) {
    // Create platform-independent NSNumber layout
    const ArchSpec &arch = process.GetArchitecture();
    const lldb::ByteOrder byte_order = arch.GetByteOrder();
    const uint32_t addr_size = arch.GetAddressByteSize();
    
    const size_t number_struct_size = 2 * addr_size;  // isa + value
    std::vector<uint8_t> number_buffer(number_struct_size, 0);
    
    DataExtractor extractor(number_buffer.data(), number_buffer.size(),
                           byte_order, addr_size);
    
    lldb::offset_t offset = 0;
    // Write ISA pointer
    extractor.PutAddress(offset, 0x2000);
    offset += addr_size;
    
    // Write value (sign-extend appropriately)
    if (addr_size == 8) {
      extractor.PutU64(offset, static_cast<uint64_t>(value));
    } else {
      extractor.PutU32(offset, static_cast<uint32_t>(value));
    }
    
    process.SetMemory(addr, number_buffer.data(), number_buffer.size());
  }

  static void SetupNSArray(MockProcess &process, lldb::addr_t addr,
                          const std::vector<lldb::addr_t> &elements) {
    // Create platform-independent GSArray layout
    const ArchSpec &arch = process.GetArchitecture();
    const lldb::ByteOrder byte_order = arch.GetByteOrder();
    const uint32_t addr_size = arch.GetAddressByteSize();
    
    const size_t array_struct_size = 3 * addr_size;  // isa + count + objects_ptr
    std::vector<uint8_t> array_buffer(array_struct_size, 0);
    
    DataExtractor extractor(array_buffer.data(), array_buffer.size(),
                           byte_order, addr_size);
    
    lldb::offset_t offset = 0;
    // Write ISA pointer
    extractor.PutAddress(offset, 0x3000);
    offset += addr_size;
    
    // Write count
    extractor.PutAddress(offset, elements.size());
    offset += addr_size;
    
    // Write objects array pointer
    const lldb::addr_t objects_addr = addr + array_struct_size;
    extractor.PutAddress(offset, objects_addr);
    
    process.SetMemory(addr, array_buffer.data(), array_buffer.size());
    
    // Write element pointers if any
    if (!elements.empty()) {
      std::vector<uint8_t> elements_buffer(elements.size() * addr_size, 0);
      DataExtractor elem_extractor(elements_buffer.data(), elements_buffer.size(),
                                   byte_order, addr_size);
      
      offset = 0;
      for (lldb::addr_t element_addr : elements) {
        elem_extractor.PutAddress(offset, element_addr);
        offset += addr_size;
      }
      
      process.SetMemory(objects_addr, elements_buffer.data(), elements_buffer.size());
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
    ASSERT_EQ(valobj->GetNumChildren(), expected_count)
        << "Child count mismatch. Expected: " << expected_count
        << ", Got: " << valobj->GetNumChildren();
  }
};

} // namespace test
} // namespace formatters
} // namespace lldb_private

#endif // LLDB_UNITTESTS_LANGUAGE_OBJC_GNUSTEP_FORMATTERTESTHELPERS_H