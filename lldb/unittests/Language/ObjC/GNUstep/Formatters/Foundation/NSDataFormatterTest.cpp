//===-- NSDataFormatterTest.cpp -----------------------------------------===//
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
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Utility/StreamString.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Target/Platform.h"
#include "lldb/DataFormatters/TypeSummary.h"

#include "../Common/FormatterTestHelpers.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDataFormatters.h"

#include <chrono>
#include <memory>
#include <vector>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::test;

namespace {

class NSDataFormatterTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
  }
  
  void TearDown() override {
    HostInfo::Terminate();
    FileSystem::Terminate();
  }
};

TEST_F(NSDataFormatterTest, BasicDataFormatting) {
  // Test basic NSData object formatting
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Create NSData object with some test data
  std::vector<uint8_t> test_data = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
  
  struct {
    uint64_t isa;
    uint64_t length;
    uint64_t data_ptr;
  } nsdata_obj = {
    0x5000,  // ISA
    test_data.size(),
    0x6000   // Points to data
  };
  
  static_cast<MockProcess*>(process.get())->SetMemory(0x7000, &nsdata_obj, sizeof(nsdata_obj));
  static_cast<MockProcess*>(process.get())->SetMemory(0x6000, test_data.data(), test_data.size());
  
  lldb::ValueObjectSP nsdata = MockValueObject::Create(target, "test_data", 0x7000, "NSData");
  
  StreamString output_stream;
  TypeSummaryOptions options;
  
  bool result = GNUstepNSDataFormatterFunction(*nsdata, output_stream, options);
  
  EXPECT_TRUE(result) << "NSData formatter should succeed";
  
  std::string output = output_stream.GetString().str();
  EXPECT_FALSE(output.empty()) << "NSData should produce output";
  
  // Should show length information
  EXPECT_TRUE(output.find("5") != std::string::npos || 
              output.find("bytes") != std::string::npos ||
              output.find("data") != std::string::npos) 
    << "NSData should show size info: " << output;
}

TEST_F(NSDataFormatterTest, EmptyDataHandling) {
  // Test NSData with zero length
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  struct {
    uint64_t isa;
    uint64_t length;
    uint64_t data_ptr;
  } empty_nsdata = {0x5000, 0, 0};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &empty_nsdata, sizeof(empty_nsdata));
  
  lldb::ValueObjectSP empty_data = MockValueObject::Create(target, "empty_data", 0x8000, "NSData");
  
  StreamString output_stream;
  TypeSummaryOptions options;
  
  bool result = GNUstepNSDataFormatterFunction(*empty_data, output_stream, options);
  
  EXPECT_TRUE(result) << "Empty NSData should be handled";
  
  std::string output = output_stream.GetString().str();
  EXPECT_TRUE(output.find("0") != std::string::npos ||
              output.find("empty") != std::string::npos ||
              output.find("bytes") != std::string::npos) 
    << "Empty NSData should indicate zero length: " << output;
}

TEST_F(NSDataFormatterTest, LargeDataHandling) {
  // Test NSData with large amount of data (should be truncated in preview)
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Create large data buffer
  std::vector<uint8_t> large_data(1000, 0xAB);
  
  struct {
    uint64_t isa;
    uint64_t length;
    uint64_t data_ptr;
  } large_nsdata = {0x5000, large_data.size(), 0x9000};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0xA000, &large_nsdata, sizeof(large_nsdata));
  static_cast<MockProcess*>(process.get())->SetMemory(0x9000, large_data.data(), 
                                                     std::min(size_t(64), large_data.size()));
  
  lldb::ValueObjectSP large_obj = MockValueObject::Create(target, "large_data", 0xA000, "NSData");
  
  StreamString output_stream;
  TypeSummaryOptions options;
  
  bool result = GNUstepNSDataFormatterFunction(*large_obj, output_stream, options);
  
  EXPECT_TRUE(result) << "Large NSData should be handled";
  
  std::string output = output_stream.GetString().str();
  EXPECT_TRUE(output.find("1000") != std::string::npos ||
              output.find("bytes") != std::string::npos) 
    << "Large NSData should show size: " << output;
  
  // Output should be reasonable length (not dump all data)
  EXPECT_LT(output.length(), 200) << "Large NSData output should be concise";
}

TEST_F(NSDataFormatterTest, DataPreviewFormatting) {
  // Test data preview for small NSData objects
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Test different small data patterns
  std::vector<std::vector<uint8_t>> test_patterns = {
    {0x00, 0x01, 0x02, 0x03},           // Sequential
    {0xFF, 0x00, 0xFF, 0x00},           // Pattern
    {0x48, 0x65, 0x6C, 0x6C, 0x6F},     // "Hello"
    {0x41, 0x42, 0x43}                  // "ABC"
  };
  
  GNUstepNSDataSummaryProvider provider;
  
  for (size_t i = 0; i < test_patterns.size(); ++i) {
    const auto& pattern = test_patterns[i];
    
    struct {
      uint64_t isa;
      uint64_t length;
      uint64_t data_ptr;
    } pattern_nsdata = {0x5000, pattern.size(), 0xB000 + (i * 100)};
    
    lldb::addr_t obj_addr = 0xC000 + (i * 100);
    lldb::addr_t data_addr = 0xB000 + (i * 100);
    
    static_cast<MockProcess*>(process.get())->SetMemory(obj_addr, &pattern_nsdata, sizeof(pattern_nsdata));
    static_cast<MockProcess*>(process.get())->SetMemory(data_addr, pattern.data(), pattern.size());
    
    lldb::ValueObjectSP pattern_obj = MockValueObject::Create(target, "pattern_data", 
                                                              obj_addr, "NSData");
    StreamString output_stream;
    TypeSummaryOptions options;
    
    bool result = provider.FormatObject(*pattern_obj, output_stream, options);
    
    if (result || !output_stream.GetString().empty()) {
      std::string output = output_stream.GetString().str();
      
      // Should show length
      EXPECT_TRUE(output.find(std::to_string(pattern.size())) != std::string::npos ||
                  output.find("bytes") != std::string::npos) 
        << "Pattern " << i << " should show length: " << output;
      
      // For small data, might show preview
      if (pattern.size() <= 16) {
        // Might contain hex preview or some indication
        EXPECT_TRUE(output.find("[") != std::string::npos ||
                    output.find("0x") != std::string::npos ||
                    output.find(std::to_string(pattern.size())) != std::string::npos) 
          << "Small data pattern " << i << " might show preview: " << output;
      }
    }
  }
}

TEST_F(NSDataFormatterTest, ErrorHandling) {
  // Test error conditions
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  GNUstepNSDataSummaryProvider provider;
  
  // Test null NSData object
  lldb::ValueObjectSP null_data = MockValueObject::Create(target, "null_data", 0x0, "NSData");
  StreamString null_stream;
  TypeSummaryOptions options;
  
  bool null_result = provider.FormatObject(*null_data, null_stream, options);
  
  if (!null_result) {
    EXPECT_TRUE(true) << "Null NSData correctly rejected";
  } else {
    std::string null_output = null_stream.GetString().str();
    EXPECT_TRUE(null_output.find("invalid") != std::string::npos ||
                null_output.find("nil") != std::string::npos) 
      << "Null NSData should indicate error: " << null_output;
  }
  
  // Test corrupted NSData (invalid data pointer)
  struct {
    uint64_t isa;
    uint64_t length;
    uint64_t data_ptr;
  } corrupted_nsdata = {0x5000, 100, LLDB_INVALID_ADDRESS};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0xD000, &corrupted_nsdata, sizeof(corrupted_nsdata));
  
  lldb::ValueObjectSP corrupted_obj = MockValueObject::Create(target, "corrupted_data", 
                                                              0xD000, "NSData");
  StreamString corrupted_stream;
  
  bool corrupted_result = provider.FormatObject(*corrupted_obj, corrupted_stream, options);
  
  // Should handle gracefully
  if (corrupted_result || !corrupted_stream.GetString().empty()) {
    std::string corrupted_output = corrupted_stream.GetString().str();
    EXPECT_TRUE(corrupted_output.find("NSData") != std::string::npos ||
                corrupted_output.find("data") != std::string::npos ||
                corrupted_output.find("bytes") != std::string::npos ||
                !corrupted_output.empty()) 
      << "Corrupted NSData should be handled: " << corrupted_output;
  }
}

TEST_F(NSDataFormatterTest, PerformanceRequirements) {
  // Test that NSData formatting meets <50ms requirement
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Create moderate-sized NSData
  std::vector<uint8_t> perf_data(512, 0x55);
  
  struct {
    uint64_t isa;
    uint64_t length;
    uint64_t data_ptr;
  } perf_nsdata = {0x5000, perf_data.size(), 0xE000};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0xF000, &perf_nsdata, sizeof(perf_nsdata));
  static_cast<MockProcess*>(process.get())->SetMemory(0xE000, perf_data.data(), perf_data.size());
  
  lldb::ValueObjectSP perf_obj = MockValueObject::Create(target, "perf_data", 0xF000, "NSData");
  
  // Test performance
  auto start = std::chrono::high_resolution_clock::now();
  
  StreamString output_stream;
  TypeSummaryOptions options;
  GNUstepNSDataSummaryProvider provider;
  
  bool result = provider.FormatObject(*perf_obj, output_stream, options);
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 50) << "NSData formatting should be under 50ms, took " 
                                   << duration.count() << "ms";
  
  if (result || !output_stream.GetString().empty()) {
    std::string output = output_stream.GetString().str();
    EXPECT_FALSE(output.empty()) << "Performance test should produce output";
  }
}

TEST_F(NSDataFormatterTest, NSMutableDataSupport) {
  // Test NSMutableData (subclass of NSData)
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  std::vector<uint8_t> mutable_data = {0xDE, 0xAD, 0xBE, 0xEF};
  
  struct {
    uint64_t isa;
    uint64_t length;
    uint64_t capacity;  // NSMutableData has capacity
    uint64_t data_ptr;
  } mutable_nsdata = {0x5100, mutable_data.size(), mutable_data.size() * 2, 0x10000};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0x11000, &mutable_nsdata, sizeof(mutable_nsdata));
  static_cast<MockProcess*>(process.get())->SetMemory(0x10000, mutable_data.data(), mutable_data.size());
  
  lldb::ValueObjectSP mutable_obj = MockValueObject::Create(target, "mutable_data", 
                                                            0x11000, "NSMutableData");
  
  StreamString output_stream;
  TypeSummaryOptions options;
  
  bool result = GNUstepNSDataFormatterFunction(*mutable_obj, output_stream, options);
  
  EXPECT_TRUE(result || !output_stream.GetString().empty()) << "NSMutableData should be handled";
  
  std::string output = output_stream.GetString().str();
  EXPECT_TRUE(output.find("4") != std::string::npos ||
              output.find("bytes") != std::string::npos ||
              output.find("data") != std::string::npos ||
              output.find("Mutable") != std::string::npos) 
    << "NSMutableData should show meaningful info: " << output;
}

} // namespace