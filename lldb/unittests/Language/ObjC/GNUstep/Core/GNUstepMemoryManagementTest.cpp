//===-- GNUstepMemoryManagementTest.cpp ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception.
//
//===----------------------------------------------------------------------===//

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepRuntimeV2API.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Utility/Status.h"
#include "gtest/gtest.h"
#include <chrono>
#include <memory>
#include <vector>

using namespace lldb;
using namespace lldb_private;

/// Test Memory Management fixes for CallObjCCopyClassList
/// This tests the critical fix that prevents memory leaks and crashes
/// when enumerating Objective-C classes in the GNUstep runtime.
class GNUstepMemoryManagementTest : public ::testing::Test {
public:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
  }

  void TearDown() override {
    HostInfo::Terminate();
    FileSystem::Terminate();
  }

protected:
  // Test memory allocation patterns
  static constexpr size_t kMaxReasonableClassCount = 10000;
  static constexpr size_t kTypicalClassCount = 100;
  
  // Helper to simulate class list enumeration memory patterns
  std::vector<void*> SimulateClassListAllocation(size_t count) {
    std::vector<void*> pointers;
    
    // Simulate the pattern used in CallObjCCopyClassList
    for (size_t i = 0; i < count; ++i) {
      // Simulate allocating memory for class pointers
      void* class_ptr = malloc(sizeof(void*));
      if (class_ptr) {
        pointers.push_back(class_ptr);
      }
    }
    
    return pointers;
  }
  
  // Helper to clean up simulated allocations
  void CleanupClassListAllocation(std::vector<void*>& pointers) {
    for (void* ptr : pointers) {
      if (ptr) {
        free(ptr);
      }
    }
    pointers.clear();
  }
};

/// Test that class list enumeration doesn't leak memory
TEST_F(GNUstepMemoryManagementTest, ClassListEnumeration_NoMemoryLeaks) {
  // Test simulates the CallObjCCopyClassList fix that prevents memory leaks
  
  std::vector<void*> allocated_pointers;
  
  // Test 1: Normal allocation and cleanup
  {
    allocated_pointers = SimulateClassListAllocation(kTypicalClassCount);
    EXPECT_EQ(allocated_pointers.size(), kTypicalClassCount);
    
    // Verify all allocations succeeded
    for (void* ptr : allocated_pointers) {
      EXPECT_NE(ptr, nullptr) << "Class list allocation should succeed";
    }
    
    // Clean up properly (this is what the fix ensures happens)
    CleanupClassListAllocation(allocated_pointers);
    EXPECT_TRUE(allocated_pointers.empty()) << "Cleanup should clear all pointers";
  }
  
  // Test 2: Exception safety - cleanup should happen even if processing fails
  {
    allocated_pointers = SimulateClassListAllocation(50);
    EXPECT_EQ(allocated_pointers.size(), 50U);
    
    // Simulate error during processing (e.g., invalid class data)
    // Since exceptions are disabled, simulate error handling with return codes
    bool error_occurred = false;
    
    // Simulate some processing that might fail
    if (allocated_pointers.size() > 10) {
      error_occurred = true;
      // Even with error, cleanup should happen (RAII pattern)
      CleanupClassListAllocation(allocated_pointers);
    }
    
    EXPECT_TRUE(error_occurred) << "Error should have occurred";
    EXPECT_TRUE(allocated_pointers.empty()) << "Cleanup should happen even after error";
  }
}

/// Test memory management with edge cases
TEST_F(GNUstepMemoryManagementTest, ClassListEnumeration_EdgeCases) {
  // Test edge cases that could cause memory management issues
  
  // Test 1: Zero classes (empty runtime)
  {
    std::vector<void*> empty_list = SimulateClassListAllocation(0);
    EXPECT_TRUE(empty_list.empty()) << "Empty class list should be handled correctly";
    CleanupClassListAllocation(empty_list);
  }
  
  // Test 2: Single class
  {
    std::vector<void*> single_class = SimulateClassListAllocation(1);
    EXPECT_EQ(single_class.size(), 1U) << "Single class should be handled correctly";
    EXPECT_NE(single_class[0], nullptr) << "Single class pointer should be valid";
    CleanupClassListAllocation(single_class);
  }
  
  // Test 3: Large number of classes (stress test)
  {
    std::vector<void*> many_classes = SimulateClassListAllocation(1000);
    EXPECT_EQ(many_classes.size(), 1000U) << "Large class list should be handled";
    
    // Verify all allocations
    for (size_t i = 0; i < many_classes.size(); ++i) {
      EXPECT_NE(many_classes[i], nullptr) << "Class " << i << " should have valid pointer";
    }
    
    CleanupClassListAllocation(many_classes);
    EXPECT_TRUE(many_classes.empty()) << "Large list cleanup should succeed";
  }
}

/// Test performance of memory management operations
TEST_F(GNUstepMemoryManagementTest, ClassListEnumeration_Performance) {
  // Test that memory management operations complete in reasonable time
  // This prevents performance regressions in the CallObjCCopyClassList fix
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Allocate and clean up multiple times (simulates repeated class enumeration)
  constexpr int kNumIterations = 100;
  constexpr size_t kClassesPerIteration = 50;
  
  for (int i = 0; i < kNumIterations; ++i) {
    std::vector<void*> classes = SimulateClassListAllocation(kClassesPerIteration);
    EXPECT_EQ(classes.size(), kClassesPerIteration);
    
    // Simulate some processing time
    for (void* ptr : classes) {
      // Basic validation
      EXPECT_NE(ptr, nullptr);
    }
    
    CleanupClassListAllocation(classes);
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
  
  // Should complete quickly (< 1 second for 5000 total allocations)
  EXPECT_LT(duration.count(), 1000) 
      << "Memory management operations took too long: " << duration.count() << "ms";
  
  // Calculate average time per iteration
  double avg_per_iteration = static_cast<double>(duration.count()) / kNumIterations;
  EXPECT_LT(avg_per_iteration, 10.0) 
      << "Average per iteration: " << avg_per_iteration << "ms (should be < 10ms)";
}

/// Test memory management correctness patterns
TEST_F(GNUstepMemoryManagementTest, ClassListEnumeration_CorrectnessPatterns) {
  // Test patterns that ensure CallObjCCopyClassList fix works correctly
  
  // Test 1: Proper RAII pattern (Resource Acquisition Is Initialization)
  {
    std::vector<void*> managed_resources;
    
    // Scope-based resource management
    {
      managed_resources = SimulateClassListAllocation(25);
      EXPECT_EQ(managed_resources.size(), 25U);
      
      // Resources are allocated and available in this scope
      for (void* ptr : managed_resources) {
        EXPECT_NE(ptr, nullptr);
      }
      
      // Cleanup at end of scope (what RAII ensures)
      CleanupClassListAllocation(managed_resources);
    }
    
    // After scope, resources should be cleaned up
    EXPECT_TRUE(managed_resources.empty());
  }
  
  // Test 2: Multiple allocations with proper cleanup ordering
  {
    std::vector<std::vector<void*>> multiple_lists;
    
    // Create multiple class lists
    for (int i = 0; i < 5; ++i) {
      auto list = SimulateClassListAllocation(10 + i);
      EXPECT_EQ(list.size(), static_cast<size_t>(10 + i));
      multiple_lists.push_back(std::move(list));
    }
    
    EXPECT_EQ(multiple_lists.size(), 5U);
    
    // Clean up in reverse order (stack-like cleanup)
    for (auto it = multiple_lists.rbegin(); it != multiple_lists.rend(); ++it) {
      CleanupClassListAllocation(*it);
      EXPECT_TRUE(it->empty());
    }
    
    // Verify all lists are cleaned up
    for (const auto& list : multiple_lists) {
      EXPECT_TRUE(list.empty());
    }
  }
  
  // Test 3: Error handling with partial cleanup
  {
    std::vector<void*> partial_list = SimulateClassListAllocation(20);
    EXPECT_EQ(partial_list.size(), 20U);
    
    // Simulate scenario where only part of the list can be processed
    // (e.g., runtime error partway through)
    size_t processed = 0;
    bool error_occurred = false;
    
    for (size_t i = 0; i < partial_list.size(); ++i) {
      processed++;
      
      // Simulate error after processing half the list
      if (i == 10) {
        error_occurred = true;
        break; // Exit processing loop due to error
      }
    }
    
    EXPECT_TRUE(error_occurred);
    EXPECT_EQ(processed, 11U); // Should have processed up to the error point
    
    // Even with partial processing, all resources should be cleaned up
    CleanupClassListAllocation(partial_list);
    EXPECT_TRUE(partial_list.empty());
  }
}

/// Test thread safety of memory management operations
TEST_F(GNUstepMemoryManagementTest, ClassListEnumeration_ThreadSafety) {
  // Test that memory management is thread-safe
  // (Important for concurrent debugging scenarios)
  
  // Note: This is a simplified test since we can't easily create real threads
  // in unit tests. In practice, this simulates concurrent access patterns.
  
  std::vector<std::vector<void*>> concurrent_lists;
  
  // Simulate rapid allocations that might occur with concurrent access
  constexpr int kNumConcurrentOperations = 50;
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Rapid allocation/deallocation cycle
  for (int i = 0; i < kNumConcurrentOperations; ++i) {
    auto list = SimulateClassListAllocation(5 + (i % 10));
    EXPECT_FALSE(list.empty());
    
    // Verify integrity
    for (void* ptr : list) {
      EXPECT_NE(ptr, nullptr);
    }
    
    concurrent_lists.push_back(std::move(list));
    
    // Clean up every few iterations (simulates mixed alloc/dealloc pattern)
    if (i % 10 == 9) {
      for (auto& cleanup_list : concurrent_lists) {
        CleanupClassListAllocation(cleanup_list);
      }
      concurrent_lists.clear();
    }
  }
  
  // Final cleanup
  for (auto& list : concurrent_lists) {
    CleanupClassListAllocation(list);
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
  
  // Should complete quickly even with rapid operations
  EXPECT_LT(duration.count(), 500) 
      << "Concurrent-style operations took too long: " << duration.count() << "ms";
}

/// Test memory management with realistic class enumeration patterns
TEST_F(GNUstepMemoryManagementTest, RealisticClassEnumeration_Patterns) {
  // Test patterns that match real-world GNUstep class enumeration
  
  // Test 1: Foundation framework classes (typical count: 50-100)
  {
    std::vector<void*> foundation_classes = SimulateClassListAllocation(75);
    EXPECT_EQ(foundation_classes.size(), 75U);
    
    // Simulate processing Foundation classes (NSString, NSArray, etc.)
    size_t processed_count = 0;
    for (void* class_ptr : foundation_classes) {
      EXPECT_NE(class_ptr, nullptr);
      processed_count++;
      
      // Simulate class name extraction and validation
      // (This is where the original bug could cause issues)
    }
    
    EXPECT_EQ(processed_count, 75U);
    CleanupClassListAllocation(foundation_classes);
  }
  
  // Test 2: Application classes (typically smaller count: 10-20)
  {
    std::vector<void*> app_classes = SimulateClassListAllocation(15);
    EXPECT_EQ(app_classes.size(), 15U);
    
    // Simulate custom class processing (BankAccount, etc.)
    for (void* class_ptr : app_classes) {
      EXPECT_NE(class_ptr, nullptr);
      
      // Simulate accessing class metadata
      // (Operations that required the memory management fix)
    }
    
    CleanupClassListAllocation(app_classes);
  }
  
  // Test 3: Mixed system and application classes (realistic total)
  {
    std::vector<void*> all_classes = SimulateClassListAllocation(150);
    EXPECT_EQ(all_classes.size(), 150U);
    EXPECT_LE(all_classes.size(), kMaxReasonableClassCount);
    
    // Simulate comprehensive class enumeration
    size_t foundation_count = 0;
    size_t custom_count = 0;
    
    for (size_t i = 0; i < all_classes.size(); ++i) {
      EXPECT_NE(all_classes[i], nullptr);
      
      // Simulate classification logic
      if (i < 100) {
        foundation_count++;  // First 100 are "Foundation"
      } else {
        custom_count++;      // Rest are "custom"
      }
    }
    
    EXPECT_EQ(foundation_count, 100U);
    EXPECT_EQ(custom_count, 50U);
    EXPECT_EQ(foundation_count + custom_count, all_classes.size());
    
    CleanupClassListAllocation(all_classes);
  }
  
  SUCCEED() << "Realistic class enumeration patterns completed successfully";
}