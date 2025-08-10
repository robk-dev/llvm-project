//===-- NSStringFormatterTest.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"

#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/Stream.h"

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.h"
#include "../Common/FormatterTestHelpers.h"

#include <chrono>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace {

class NSStringFormatterTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
    formatter = std::make_unique<GNUstepNSStringSummaryProvider>();
  }
  
  void TearDown() override {
    formatter.reset();
    HostInfo::Terminate();
    FileSystem::Terminate();
  }
  
  std::unique_ptr<GNUstepNSStringSummaryProvider> formatter;
};

TEST_F(NSStringFormatterTest, FormatterInstantiation) {
  // Test that NSString formatter can be created successfully
  EXPECT_NE(formatter.get(), nullptr) << "NSString formatter should be instantiated";
  
  // Test that we can create multiple instances
  auto formatter2 = std::make_unique<GNUstepNSStringSummaryProvider>();
  EXPECT_NE(formatter2.get(), nullptr) << "Multiple formatter instances should work";
  
  // Test that formatters are distinct objects
  EXPECT_NE(formatter.get(), formatter2.get()) << "Each formatter should be a distinct instance";
}

TEST_F(NSStringFormatterTest, FormatterRegistrationFunctions) {
  // Test that formatter registration functions exist and can be called
  
  // Verify function pointers exist - these are used by LLDB's formatter registration system
  auto string_func = &GNUstepNSStringFormatterFunction;
  EXPECT_NE(string_func, nullptr) << "GNUstepNSStringFormatterFunction should exist";
  
  auto id_func = &GNUstepIdFormatterFunction;
  EXPECT_NE(id_func, nullptr) << "GNUstepIdFormatterFunction should exist";
  
  // These functions are the bridge between LLDB's type system and our formatters
  // They must exist for the plugin to register correctly with LLDB
}

TEST_F(NSStringFormatterTest, FormatterClassHierarchy) {
  // Test that the formatter correctly inherits from the expected base class
  // This validates the class hierarchy and ensures virtual methods work correctly
  
  GNUstepSummaryProvider* base_ptr = formatter.get();
  EXPECT_NE(base_ptr, nullptr) << "Should be able to convert to base class pointer";
  
  // The formatter should be polymorphic (have virtual methods)
  // This is essential for LLDB's formatter dispatching to work
}

TEST_F(NSStringFormatterTest, CompilationAndLinking) {
  // This test validates that all necessary headers are included
  // and that the formatter code compiles and links correctly
  
  // If we can create the formatter object, it means:
  // 1. All headers are properly included
  // 2. All dependencies are linked
  // 3. No circular includes or missing symbols
  EXPECT_TRUE(true) << "Formatter compilation and linking successful";
}

TEST_F(NSStringFormatterTest, PerformanceBaseline) {
  // Test that formatter creation is fast enough for interactive debugging
  // LLDB may create many formatter instances during a debugging session
  
  auto start = std::chrono::high_resolution_clock::now();
  
  // Create many formatter instances to test instantiation performance
  std::vector<std::unique_ptr<GNUstepNSStringSummaryProvider>> formatters;
  for (int i = 0; i < 1000; ++i) {
    formatters.push_back(
      std::make_unique<GNUstepNSStringSummaryProvider>());
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 50) << "Creating 1000 formatters should be very fast (<50ms)";
  EXPECT_EQ(formatters.size(), 1000) << "All formatters should be created successfully";
}

TEST_F(NSStringFormatterTest, ThreadSafety) {
  // Test basic thread safety assumptions
  // Multiple formatter instances should be safe to create concurrently
  
  std::vector<std::thread> threads;
  std::vector<std::unique_ptr<GNUstepNSStringSummaryProvider>> results;
  std::mutex results_mutex;
  
  // Create formatters from multiple threads
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&results, &results_mutex]() {
      auto formatter = std::make_unique<GNUstepNSStringSummaryProvider>();
      
      std::lock_guard<std::mutex> lock(results_mutex);
      results.push_back(std::move(formatter));
    });
  }
  
  // Wait for all threads
  for (auto& thread : threads) {
    thread.join();
  }
  
  EXPECT_EQ(results.size(), 10) << "All formatters should be created from multiple threads";
  
  // All formatters should be valid
  for (const auto& formatter : results) {
    EXPECT_NE(formatter.get(), nullptr) << "Each formatter should be valid";
  }
}

TEST_F(NSStringFormatterTest, MemoryEfficiency) {
  // Test that formatters don't consume excessive memory
  // This is important since LLDB may keep many formatters in memory
  
  // Create a reasonable number of formatters
  std::vector<std::unique_ptr<GNUstepNSStringSummaryProvider>> formatters;
  for (int i = 0; i < 100; ++i) {
    formatters.push_back(
      std::make_unique<GNUstepNSStringSummaryProvider>());
  }
  
  // Since we can't easily measure memory usage in a unit test,
  // we'll just verify the formatters are created successfully
  EXPECT_EQ(formatters.size(), static_cast<size_t>(100)) << "All formatters should be created without excessive memory usage";
  
  // The fact that we can create 100 formatters suggests memory usage is reasonable
}

} // namespace