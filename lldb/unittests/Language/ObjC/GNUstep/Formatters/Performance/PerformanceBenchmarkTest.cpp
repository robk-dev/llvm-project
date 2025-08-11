//===-- PerformanceBenchmarkTest.cpp -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"

#include "lldb/Core/Debugger.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/Stream.h"
#include "lldb/DataFormatters/TypeSummary.h"

#include "../Common/FormatterTestHelpers.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepArrayFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDictionaryFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepSetFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepNumberFormatters.h"

#include <chrono>
#include <memory>
#include <vector>
#include <thread>
#include <random>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::test;

namespace {

class PerformanceBenchmarkTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
  }
  
  void TearDown() override {
    HostInfo::Terminate();
    FileSystem::Terminate();
  }
  
  // PERFORMANCE REQUIREMENT: All formatters must respond within 50ms
  static constexpr int64_t MAX_FORMATTER_TIME_MS = 50;
  
  // Test data sizes for performance validation
  static constexpr size_t SMALL_COLLECTION_SIZE = 10;
  static constexpr size_t MEDIUM_COLLECTION_SIZE = 100;
  static constexpr size_t LARGE_COLLECTION_SIZE = 1000;
  static constexpr size_t STRESS_COLLECTION_SIZE = 10000;
  
  struct PerformanceResult {
    int64_t duration_ms;
    size_t data_size;
    std::string formatter_type;
    bool within_requirements;
    std::string output_preview;
    
    PerformanceResult(int64_t duration, size_t size, const std::string& type, 
                     const std::string& output = "") 
      : duration_ms(duration), data_size(size), formatter_type(type),
        within_requirements(duration <= MAX_FORMATTER_TIME_MS), 
        output_preview(output.substr(0, 100)) {}
  };
  
  std::vector<PerformanceResult> performance_results;
  
  PerformanceResult BenchmarkFormatter(const std::string& formatter_name,
                                     size_t data_size,
                                     std::function<bool()> test_function) {
    auto start = std::chrono::high_resolution_clock::now();
    
    bool success = test_function();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    PerformanceResult result(duration.count(), data_size, formatter_name, 
                           success ? "SUCCESS" : "FAILED");
    
    performance_results.push_back(result);
    return result;
  }
  
  void PrintPerformanceReport() {
    printf("\n=== PERFORMANCE BENCHMARK REPORT ===\n");
    printf("Maximum allowed time: %ld ms\n\n", static_cast<long>(MAX_FORMATTER_TIME_MS));
    
    for (const auto& result : performance_results) {
      printf("%-25s | Size: %6zu | Time: %3ld ms | %s | %s\n",
             result.formatter_type.c_str(),
             result.data_size,
             static_cast<long>(result.duration_ms),
             result.within_requirements ? "✓ PASS" : "✗ FAIL",
             result.output_preview.c_str());
    }
    
    size_t passed = 0;
    for (const auto& result : performance_results) {
      if (result.within_requirements) passed++;
    }
    
    printf("\nSUMMARY: %zu/%zu tests passed performance requirements\n", 
           passed, performance_results.size());
  }
};

TEST_F(PerformanceBenchmarkTest, NSArrayFormatterPerformance) {
  // Test NSArray formatter performance across different data sizes
  
  auto test_array_small = [this]() {
    auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
    
    // Create mock array with SMALL_COLLECTION_SIZE elements
    lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                         ArchSpec("x86_64"), PlatformSP());
    lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
    
    // Mock array structure
    struct {
      uint64_t isa;
      uint64_t contents_ptr; 
      uint64_t count;
    } array_obj = {0x5000, 0x6000, SMALL_COLLECTION_SIZE};
    
    // Mock element pointers  
    std::vector<uint64_t> elements;
    for (size_t i = 0; i < SMALL_COLLECTION_SIZE; ++i) {
      elements.push_back(0x7000 + i * 8); // Mock object addresses
    }
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &array_obj, sizeof(array_obj));
    static_cast<MockProcess*>(process.get())->SetMemory(0x6000, elements.data(), 
                                                        elements.size() * sizeof(uint64_t));
    
    lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "small_array", 
                                                               0x8000, "NSArray");
    
    StreamString output;
    TypeSummaryOptions options;
    return provider->FormatObject(*array_obj_sp, output, options);
  };
  
  auto result_small = BenchmarkFormatter("NSArray (Small)", SMALL_COLLECTION_SIZE, test_array_small);
  EXPECT_TRUE(result_small.within_requirements) 
    << "Small NSArray formatting took " << result_small.duration_ms << "ms (max: " 
    << MAX_FORMATTER_TIME_MS << "ms)";
  
  // Test medium size array
  auto test_array_medium = [this]() {
    auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
    
    lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                         ArchSpec("x86_64"), PlatformSP());
    lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
    
    struct {
      uint64_t isa;
      uint64_t contents_ptr; 
      uint64_t count;
    } array_obj = {0x5000, 0x6000, MEDIUM_COLLECTION_SIZE};
    
    std::vector<uint64_t> elements(MEDIUM_COLLECTION_SIZE);
    for (size_t i = 0; i < MEDIUM_COLLECTION_SIZE; ++i) {
      elements[i] = 0x7000 + i * 8;
    }
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &array_obj, sizeof(array_obj));
    static_cast<MockProcess*>(process.get())->SetMemory(0x6000, elements.data(), 
                                                        elements.size() * sizeof(uint64_t));
    
    lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "medium_array", 
                                                               0x8000, "NSArray");
    
    StreamString output;
    TypeSummaryOptions options;
    return provider->FormatObject(*array_obj_sp, output, options);
  };
  
  auto result_medium = BenchmarkFormatter("NSArray (Medium)", MEDIUM_COLLECTION_SIZE, test_array_medium);
  EXPECT_TRUE(result_medium.within_requirements)
    << "Medium NSArray formatting took " << result_medium.duration_ms << "ms (max: " 
    << MAX_FORMATTER_TIME_MS << "ms)";
  
  // Test large size array - should still meet requirements
  auto test_array_large = [this]() {
    auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
    
    lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                         ArchSpec("x86_64"), PlatformSP());
    lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
    
    struct {
      uint64_t isa;
      uint64_t contents_ptr; 
      uint64_t count;
    } array_obj = {0x5000, 0x6000, LARGE_COLLECTION_SIZE};
    
    // Only create first few elements to avoid excessive memory usage
    std::vector<uint64_t> elements(std::min(LARGE_COLLECTION_SIZE, size_t(100)));
    for (size_t i = 0; i < elements.size(); ++i) {
      elements[i] = 0x7000 + i * 8;
    }
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &array_obj, sizeof(array_obj));
    static_cast<MockProcess*>(process.get())->SetMemory(0x6000, elements.data(), 
                                                        elements.size() * sizeof(uint64_t));
    
    lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "large_array", 
                                                               0x8000, "NSArray");
    
    StreamString output;
    TypeSummaryOptions options;
    return provider->FormatObject(*array_obj_sp, output, options);
  };
  
  auto result_large = BenchmarkFormatter("NSArray (Large)", LARGE_COLLECTION_SIZE, test_array_large);
  EXPECT_TRUE(result_large.within_requirements)
    << "Large NSArray formatting took " << result_large.duration_ms << "ms (max: " 
    << MAX_FORMATTER_TIME_MS << "ms)";
}

TEST_F(PerformanceBenchmarkTest, NSStringFormatterPerformance) {
  // Test NSString formatter performance with different string lengths
  
  auto test_short_string = [this]() {
    auto provider = std::make_unique<GNUstepNSStringSummaryProvider>();
    
    lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                         ArchSpec("x86_64"), PlatformSP());
    lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
    
    std::string test_str = "Hello World!";
    struct {
      uint64_t isa;
      uint64_t length;
      uint64_t chars_ptr;
    } string_obj = {0x5000, test_str.length(), 0x6000};
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &string_obj, sizeof(string_obj));
    static_cast<MockProcess*>(process.get())->SetMemory(0x6000, test_str.c_str(), test_str.length());
    
    lldb::ValueObjectSP string_obj_sp = MockValueObject::Create(target, "short_string", 
                                                                0x8000, "NSString");
    
    StreamString output;
    TypeSummaryOptions options;
    return provider->FormatObject(*string_obj_sp, output, options);
  };
  
  auto result_short = BenchmarkFormatter("NSString (Short)", 12, test_short_string);
  EXPECT_TRUE(result_short.within_requirements)
    << "Short NSString formatting took " << result_short.duration_ms << "ms";
    
  auto test_long_string = [this]() {
    auto provider = std::make_unique<GNUstepNSStringSummaryProvider>();
    
    lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                         ArchSpec("x86_64"), PlatformSP());
    lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
    
    // Create a long string (1KB)
    std::string test_str(1024, 'A');
    test_str += " - This is a very long string for performance testing";
    
    struct {
      uint64_t isa;
      uint64_t length;
      uint64_t chars_ptr;
    } string_obj = {0x5000, test_str.length(), 0x6000};
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &string_obj, sizeof(string_obj));
    static_cast<MockProcess*>(process.get())->SetMemory(0x6000, test_str.c_str(), 
                                                        std::min(test_str.length(), size_t(512)));
    
    lldb::ValueObjectSP string_obj_sp = MockValueObject::Create(target, "long_string", 
                                                                0x8000, "NSString");
    
    StreamString output;
    TypeSummaryOptions options;
    return provider->FormatObject(*string_obj_sp, output, options);
  };
  
  auto result_long = BenchmarkFormatter("NSString (Long)", 1024, test_long_string);
  EXPECT_TRUE(result_long.within_requirements)
    << "Long NSString formatting took " << result_long.duration_ms << "ms";
}

TEST_F(PerformanceBenchmarkTest, ConcurrentFormatterAccess) {
  // Test thread safety and performance under concurrent access
  
  std::atomic<int> success_count{0};
  std::atomic<int> total_count{0};
  
  auto thread_function = [&success_count, &total_count](int thread_id) {
    for (int i = 0; i < 10; ++i) {
      total_count++;
      
      // Removed try-catch as exceptions are disabled
      auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
      
      lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                           ArchSpec("x86_64"), PlatformSP());
      lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
      
      struct {
        uint64_t isa;
        uint64_t contents_ptr; 
        uint64_t count;
      } array_obj = {0x5000, 0x6000, 10};
      
      static_cast<MockProcess*>(process.get())->SetMemory(0x8000 + thread_id * 0x1000, 
                                                          &array_obj, sizeof(array_obj));
      
      lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "concurrent_array", 
                                                                 0x8000 + thread_id * 0x1000, 
                                                                 "NSArray");
      
      StreamString output;
      TypeSummaryOptions options;
      if (provider->FormatObject(*array_obj_sp, output, options)) {
        success_count++;
      }
      
      // Small delay to encourage race conditions
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
  };
  
  auto start = std::chrono::high_resolution_clock::now();
  
  // Create multiple threads accessing formatters concurrently
  std::vector<std::thread> threads;
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back(thread_function, i);
  }
  
  // Wait for all threads to complete
  for (auto& thread : threads) {
    thread.join();
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  // Verify thread safety
  EXPECT_GT(success_count.load(), 0) << "Some concurrent operations should succeed";
  EXPECT_EQ(total_count.load(), 50) << "All 50 operations should be attempted";
  
  // Verify reasonable performance under concurrency
  EXPECT_LT(duration.count(), 500) << "Concurrent access should complete in reasonable time";
  
  auto result = PerformanceResult(duration.count(), 50, "Concurrent Access", 
                                 std::to_string(success_count.load()) + "/" + 
                                 std::to_string(total_count.load()) + " succeeded");
  performance_results.push_back(result);
}

TEST_F(PerformanceBenchmarkTest, StressTestLargeDatasets) {
  // Stress test with very large datasets
  
  auto stress_test = [this]() {
    auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
    
    lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                         ArchSpec("x86_64"), PlatformSP());
    lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
    
    struct {
      uint64_t isa;
      uint64_t contents_ptr; 
      uint64_t count;
    } array_obj = {0x5000, 0x6000, STRESS_COLLECTION_SIZE};
    
    // Don't actually allocate all elements, just test count handling
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &array_obj, sizeof(array_obj));
    
    lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "stress_array", 
                                                               0x8000, "NSArray");
    
    StreamString output;
    TypeSummaryOptions options;
    return provider->FormatObject(*array_obj_sp, output, options);
  };
  
  auto result_stress = BenchmarkFormatter("NSArray (Stress)", STRESS_COLLECTION_SIZE, stress_test);
  
  // Stress test may take longer but should still complete
  EXPECT_LT(result_stress.duration_ms, 200) 
    << "Stress test should complete in reasonable time";
    
  // Should handle large counts gracefully
  EXPECT_TRUE(result_stress.within_requirements || result_stress.duration_ms < 200)
    << "Stress test formatting took " << result_stress.duration_ms << "ms";
}

TEST_F(PerformanceBenchmarkTest, MemoryPressureTest) {
  // Test performance under memory pressure
  
  std::vector<std::unique_ptr<GNUstepNSArraySummaryProvider>> providers;
  
  auto memory_pressure_test = [&providers, this]() {
    // Create many formatter instances to simulate memory pressure
    for (int i = 0; i < 1000; ++i) {
      providers.push_back(std::make_unique<GNUstepNSArraySummaryProvider>());
    }
    
    // Test formatting under memory pressure
    auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
    
    lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                         ArchSpec("x86_64"), PlatformSP());
    lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
    
    struct {
      uint64_t isa;
      uint64_t contents_ptr; 
      uint64_t count;
    } array_obj = {0x5000, 0x6000, 100};
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &array_obj, sizeof(array_obj));
    
    lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "pressure_array", 
                                                               0x8000, "NSArray");
    
    StreamString output;
    TypeSummaryOptions options;
    return provider->FormatObject(*array_obj_sp, output, options);
  };
  
  auto result_pressure = BenchmarkFormatter("Memory Pressure", 1000, memory_pressure_test);
  
  EXPECT_TRUE(result_pressure.within_requirements)
    << "Formatting under memory pressure took " << result_pressure.duration_ms << "ms";
  
  // Clean up memory
  providers.clear();
}

TEST_F(PerformanceBenchmarkTest, PrintPerformanceReportTest) {
  // This test runs last and prints the complete performance report
  PrintPerformanceReport();
  
  // Validate overall performance requirements
  size_t passed = 0;
  for (const auto& result : performance_results) {
    if (result.within_requirements) passed++;
  }
  
  double pass_rate = static_cast<double>(passed) / performance_results.size();
  EXPECT_GE(pass_rate, 0.8) 
    << "At least 80% of performance tests should pass. Current: " 
    << (pass_rate * 100) << "%";
}

} // namespace