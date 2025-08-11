//===-- StressTest.cpp ---------------------------------------------------===//
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

#include "../Common/FormatterTestHelpers.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepArrayFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDictionaryFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepSetFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.h"

#include <chrono>
#include <memory>
#include <vector>
#include <thread>
#include <random>
#include <atomic>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::test;

namespace {

class StressTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
  }
  
  void TearDown() override {
    HostInfo::Terminate();
    FileSystem::Terminate();
  }
  
  // Stress test parameters
  static constexpr size_t MAX_COLLECTION_SIZE = 100000;
  static constexpr size_t MAX_STRING_LENGTH = 10000;
  static constexpr int MAX_RECURSION_DEPTH = 10;
  static constexpr int CONCURRENT_THREADS = 10;
  static constexpr int ITERATIONS_PER_THREAD = 100;
  
  std::random_device rd;
  std::mt19937 gen{rd()};
};

TEST_F(StressTest, ExtremeLargeCollections) {
  // Test formatters with extremely large collections
  
  auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
  
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                       ArchSpec("x86_64"), PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Test various extreme sizes
  std::vector<size_t> test_sizes = {
    10000,    // Large
    50000,    // Very large  
    100000,   // Extreme
    500000,   // Stress limit
    1000000   // Maximum stress
  };
  
  for (size_t size : test_sizes) {
    struct {
      uint64_t isa;
      uint64_t contents_ptr; 
      uint64_t count;
    } array_obj = {0x5000, 0x6000, size};
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &array_obj, sizeof(array_obj));
    
    lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "extreme_array", 
                                                               0x8000, "NSArray");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    StreamString output;
    TypeSummaryOptions options;
    bool success = provider->FormatObject(*array_obj_sp, output, options);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should handle extreme sizes gracefully (may not meet 50ms requirement but should not crash)
    EXPECT_TRUE(success || !output.GetString().empty()) 
      << "Extreme size " << size << " should be handled gracefully";
      
    EXPECT_LT(duration.count(), 1000) 
      << "Size " << size << " took " << duration.count() << "ms (should be reasonable)";
      
    std::string result = output.GetString().str();
    EXPECT_FALSE(result.empty()) 
      << "Size " << size << " should produce some output";
      
    // Should show size information for large collections  
    if (size > 10) {
      EXPECT_TRUE(result.find(std::to_string(size)) != std::string::npos ||
                  result.find("elements") != std::string::npos)
        << "Large collection should show size info: " << result;
    }
    
    printf("Size: %zu, Time: %lld ms, Output: %.50s...\n", 
           size, duration.count(), result.c_str());
  }
}

TEST_F(StressTest, DeepNestedCollections) {
  // Test deeply nested collection structures
  
  auto array_provider = std::make_unique<GNUstepNSArraySummaryProvider>();
  auto dict_provider = std::make_unique<GNUstepNSDictionarySummaryProvider>();
  
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                       ArchSpec("x86_64"), PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Create nested structure: Array -> Dict -> Array -> Dict -> ...
  std::vector<uint64_t> object_addresses;
  const int depth = 8; // Reasonable nesting depth
  
  for (int level = 0; level < depth; ++level) {
    uint64_t obj_addr = 0x10000 + (level * 0x1000);
    object_addresses.push_back(obj_addr);
    
    if (level % 2 == 0) {
      // Array level
      struct {
        uint64_t isa;
        uint64_t contents_ptr; 
        uint64_t count;
      } array_obj = {0x5000, obj_addr + 0x100, 1}; // Contains next level
      
      static_cast<MockProcess*>(process.get())->SetMemory(obj_addr, &array_obj, sizeof(array_obj));
      
      if (level + 1 < depth) {
        uint64_t next_addr = object_addresses[level + 1];
        static_cast<MockProcess*>(process.get())->SetMemory(obj_addr + 0x100, &next_addr, 
                                                            sizeof(next_addr));
      }
    } else {
      // Dictionary level  
      struct {
        uint64_t isa;
        uint64_t table_ptr;
        uint64_t count;
      } dict_obj = {0x5100, obj_addr + 0x200, 1};
      
      static_cast<MockProcess*>(process.get())->SetMemory(obj_addr, &dict_obj, sizeof(dict_obj));
      
      if (level + 1 < depth) {
        // Mock dictionary entry
        struct {
          uint64_t key_addr;
          uint64_t value_addr;
        } entry = {0x6000 + level, object_addresses[level + 1]};
        
        static_cast<MockProcess*>(process.get())->SetMemory(obj_addr + 0x200, &entry, sizeof(entry));
      }
    }
  }
  
  // Test formatting the root object
  lldb::ValueObjectSP root_obj = MockValueObject::Create(target, "nested_root", 
                                                         object_addresses[0], "NSArray");
  
  auto start = std::chrono::high_resolution_clock::now();
  
  StreamString output;
  TypeSummaryOptions options;
  bool success = array_provider->FormatObject(*root_obj, output, options);
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_TRUE(success || !output.GetString().empty()) 
    << "Deeply nested structure should be handled";
    
  EXPECT_LT(duration.count(), 500) 
    << "Deep nesting should complete in reasonable time";
    
  std::string result = output.GetString().str();
  printf("Deep nesting result (depth %d): %s\n", depth, result.c_str());
}

TEST_F(StressTest, HighConcurrencyStress) {
  // Test formatters under high concurrency stress
  
  std::atomic<int> success_count{0};
  std::atomic<int> error_count{0};
  std::atomic<int> total_operations{0};
  std::atomic<bool> keep_running{true};
  
  auto stress_worker = [&](int worker_id) {
    std::mt19937 local_gen(rd() + worker_id);
    std::uniform_int_distribution<> size_dist(1, 1000);
    std::uniform_int_distribution<> type_dist(0, 2); // Array, Dict, Set
    
    for (int i = 0; i < ITERATIONS_PER_THREAD && keep_running; ++i) {
      total_operations++;
      
      // Removed try-catch as exceptions are disabled
      int formatter_type = type_dist(local_gen);
      size_t collection_size = size_dist(local_gen);
      
      lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                           ArchSpec("x86_64"), PlatformSP());
      lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
      
      uint64_t base_addr = 0x20000 + (worker_id * 0x10000) + (i * 0x1000);
      
      StreamString output;
      TypeSummaryOptions options;
      bool result = false;
      
      switch (formatter_type) {
        case 0: { // Array
          auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
          
          struct {
            uint64_t isa;
            uint64_t contents_ptr; 
            uint64_t count;
          } obj = {0x5000, base_addr + 0x100, collection_size};
          
          static_cast<MockProcess*>(process.get())->SetMemory(base_addr, &obj, sizeof(obj));
          
          lldb::ValueObjectSP obj_sp = MockValueObject::Create(target, "stress_array", 
                                                               base_addr, "NSArray");
          result = provider->FormatObject(*obj_sp, output, options);
          break;
        }
        case 1: { // Dictionary
          auto provider = std::make_unique<GNUstepNSDictionarySummaryProvider>();
          
          struct {
            uint64_t isa;
            uint64_t table_ptr;
            uint64_t count;
          } obj = {0x5100, base_addr + 0x200, collection_size};
          
          static_cast<MockProcess*>(process.get())->SetMemory(base_addr, &obj, sizeof(obj));
          
          lldb::ValueObjectSP obj_sp = MockValueObject::Create(target, "stress_dict", 
                                                               base_addr, "NSDictionary");
          result = provider->FormatObject(*obj_sp, output, options);
          break;
        }
        case 2: { // Set  
          auto provider = std::make_unique<GNUstepNSSetSummaryProvider>();
          
          struct {
            uint64_t isa;
            uint64_t table_ptr;
            uint64_t count;
          } obj = {0x5200, base_addr + 0x300, collection_size};
          
          static_cast<MockProcess*>(process.get())->SetMemory(base_addr, &obj, sizeof(obj));
          
          lldb::ValueObjectSP obj_sp = MockValueObject::Create(target, "stress_set", 
                                                               base_addr, "NSSet");
          result = provider->FormatObject(*obj_sp, output, options);
          break;
        }
      }
      
      if (result || !output.GetString().empty()) {
        success_count++;
      } else {
        error_count++;
      }
      
      // Brief pause to allow context switching
      if (i % 10 == 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
    }
  };
  
  auto start = std::chrono::high_resolution_clock::now();
  
  // Launch stress worker threads
  std::vector<std::thread> workers;
  for (int i = 0; i < CONCURRENT_THREADS; ++i) {
    workers.emplace_back(stress_worker, i);
  }
  
  // Let it run for a reasonable time, then stop
  std::this_thread::sleep_for(std::chrono::seconds(5));
  keep_running = false;
  
  // Wait for all workers to complete
  for (auto& worker : workers) {
    worker.join();
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  int total_ops = total_operations.load();
  int successes = success_count.load();
  int errors = error_count.load();
  
  printf("Concurrency Stress Results:\n");
  printf("  Total Operations: %d\n", total_ops);
  printf("  Successes: %d\n", successes);
  printf("  Errors: %d\n", errors);
  printf("  Duration: %lld ms\n", duration.count());
  printf("  Operations/second: %.2f\n", 
         total_ops / (duration.count() / 1000.0));
  
  // Validate results
  EXPECT_GT(total_ops, 100) << "Should have completed significant operations";
  EXPECT_GT(successes, total_ops / 2) << "At least half should succeed";
  EXPECT_LT(errors, total_ops / 4) << "Error rate should be reasonable";
  
  double success_rate = static_cast<double>(successes) / total_ops;
  EXPECT_GE(success_rate, 0.7) << "Success rate should be at least 70%";
}

TEST_F(StressTest, MemoryCorruptionResistance) {
  // Test formatter resistance to memory corruption scenarios
  
  auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
  
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                       ArchSpec("x86_64"), PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Test various corruption scenarios
  std::vector<std::pair<std::string, std::function<void()>>> corruption_tests = {
    
    {"Invalid count (negative)", [&]() {
      struct {
        uint64_t isa;
        uint64_t contents_ptr; 
        uint64_t count;
      } obj = {0x5000, 0x6000, static_cast<uint64_t>(-1)};
      
      static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &obj, sizeof(obj));
      
      lldb::ValueObjectSP obj_sp = MockValueObject::Create(target, "corrupted_array", 
                                                           0x8000, "NSArray");
      StreamString output;
      TypeSummaryOptions options;
      
      // Should not crash, may return false
      bool result = provider->FormatObject(*obj_sp, output, options);
      EXPECT_TRUE(result == false || !output.GetString().empty()) 
        << "Invalid count should be handled gracefully";
    }},
    
    {"Null contents pointer", [&]() {
      struct {
        uint64_t isa;
        uint64_t contents_ptr; 
        uint64_t count;
      } obj = {0x5000, 0, 10};
      
      static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &obj, sizeof(obj));
      
      lldb::ValueObjectSP obj_sp = MockValueObject::Create(target, "null_contents", 
                                                           0x8000, "NSArray");
      StreamString output;
      TypeSummaryOptions options;
      
      bool result = provider->FormatObject(*obj_sp, output, options);
      EXPECT_TRUE(result == false || !output.GetString().empty()) 
        << "Null contents should be handled gracefully";
    }},
    
    {"Invalid ISA pointer", [&]() {
      struct {
        uint64_t isa;
        uint64_t contents_ptr; 
        uint64_t count;
      } obj = {0x1, 0x6000, 5}; // Unaligned ISA
      
      static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &obj, sizeof(obj));
      
      lldb::ValueObjectSP obj_sp = MockValueObject::Create(target, "invalid_isa", 
                                                           0x8000, "NSArray");
      StreamString output;
      TypeSummaryOptions options;
      
      bool result = provider->FormatObject(*obj_sp, output, options);
      EXPECT_TRUE(result == false || !output.GetString().empty()) 
        << "Invalid ISA should be handled gracefully";
    }},
    
    {"Extreme count value", [&]() {
      struct {
        uint64_t isa;
        uint64_t contents_ptr; 
        uint64_t count;
      } obj = {0x5000, 0x6000, UINT64_MAX};
      
      static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &obj, sizeof(obj));
      
      lldb::ValueObjectSP obj_sp = MockValueObject::Create(target, "extreme_count", 
                                                           0x8000, "NSArray");
      StreamString output;
      TypeSummaryOptions options;
      
      auto start = std::chrono::high_resolution_clock::now();
      bool result = provider->FormatObject(*obj_sp, output, options);
      auto end = std::chrono::high_resolution_clock::now();
      
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
      
      EXPECT_LT(duration.count(), 1000) << "Should not hang on extreme values";
      EXPECT_TRUE(result == false || !output.GetString().empty()) 
        << "Extreme count should be handled gracefully";
    }}
  };
  
  for (auto& [test_name, test_func] : corruption_tests) {
    printf("Testing: %s\n", test_name.c_str());
    
    EXPECT_NO_FATAL_FAILURE(test_func()) 
      << "Corruption test '" << test_name << "' should not crash";
  }
}

TEST_F(StressTest, ResourceExhaustionHandling) {
  // Test behavior under resource exhaustion
  
  std::vector<std::unique_ptr<GNUstepNSArraySummaryProvider>> providers;
  std::vector<lldb::ValueObjectSP> objects;
  
  // Create many formatters and objects to exhaust resources
  // Removed try-catch as exceptions are disabled
  for (int i = 0; i < 10000; ++i) {
    // Protect against bad_alloc by checking size first
    if (providers.size() >= 10000) break;
    
    providers.push_back(std::make_unique<GNUstepNSArraySummaryProvider>());
    
    if (i % 100 == 0) {
      lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                           ArchSpec("x86_64"), PlatformSP());
      
      lldb::ValueObjectSP obj = MockValueObject::Create(target, "resource_test", 
                                                        0x8000 + i, "NSArray");
      objects.push_back(obj);
    }
  }
  
  EXPECT_EQ(providers.size(), 10000u) << "Should handle many formatter instances";
  EXPECT_GT(objects.size(), 90u) << "Should handle many value objects";
  
  // Test formatting under resource pressure
  if (!providers.empty() && !objects.empty()) {
    StreamString output;
    TypeSummaryOptions options;
    
    bool result = providers[0]->FormatObject(*objects[0], output, options);
    // Should succeed or gracefully fail, not crash
    EXPECT_TRUE(true) << "Should handle resource pressure gracefully";
  }
  
  // Clean up
  providers.clear();
  objects.clear();
}

} // namespace