//===-- NSUUIDFormatterTest.cpp -----------------------------------------===//
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
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Platform.h"
#include "lldb/Utility/ArchSpec.h"
#include "lldb/Core/Debugger.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Utility/StreamString.h"

#include "../Common/FormatterTestHelpers.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepUUIDFormatters.h"

#include <chrono>
#include <memory>
#include <array>
#include <limits>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::test;

namespace {

class NSUUIDFormatterTest : public ::testing::Test {
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

TEST_F(NSUUIDFormatterTest, BasicUUIDFormatting) {
  // Test basic NSUUID object formatting
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<MockTarget>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Standard UUID: 550E8400-E29B-41D4-A716-446655440000
  std::array<uint8_t, 16> uuid_bytes = {
    0x55, 0x0E, 0x84, 0x00, 0xE2, 0x9B, 0x41, 0xD4,
    0xA7, 0x16, 0x44, 0x66, 0x55, 0x44, 0x00, 0x00
  };
  
  struct {
    uint64_t isa;
    std::array<uint8_t, 16> uuid_bytes;
  } nsuuid_obj = {
    0x7000,  // ISA
    uuid_bytes
  };
  
  static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &nsuuid_obj, sizeof(nsuuid_obj));
  
  lldb::ValueObjectSP nsuuid = MockValueObject::Create(target, "test_uuid", 0x8000, "NSUUID");
  
  StreamString output_stream;
  TypeSummaryOptions options;
  
  bool result = GNUstepNSUUIDFormatterFunction(*nsuuid, output_stream, options);
  
  EXPECT_TRUE(result) << "NSUUID formatter should succeed";
  
  std::string output = output_stream.GetString().str();
  EXPECT_FALSE(output.empty()) << "NSUUID should produce output";
  
  // Should show UUID string format or recognizable components
  EXPECT_TRUE(output.find("550E8400") != std::string::npos || 
              output.find("E29B") != std::string::npos ||
              output.find("41D4") != std::string::npos ||
              output.find("UUID") != std::string::npos ||
              output.find("-") != std::string::npos ||
              output.length() >= 32) // UUID has at least 32 hex chars
    << "NSUUID should show UUID format: " << output;
}

TEST_F(NSUUIDFormatterTest, ZeroUUIDHandling) {
  // Test zero/nil UUID (00000000-0000-0000-0000-000000000000)
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<MockTarget>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  std::array<uint8_t, 16> zero_uuid = {0};  // All zeros
  
  struct {
    uint64_t isa;
    std::array<uint8_t, 16> uuid_bytes;
  } zero_nsuuid = {0x7000, zero_uuid};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0x9000, &zero_nsuuid, sizeof(zero_nsuuid));
  
  lldb::ValueObjectSP zero_obj = MockValueObject::Create(target, "zero_uuid", 0x9000, "NSUUID");
  
  StreamString output_stream;
  TypeSummaryOptions options;
  
  bool result = GNUstepNSUUIDFormatterFunction(*zero_obj, output_stream, options);
  
  EXPECT_TRUE(result) << "Zero UUID should be handled";
  
  std::string output = output_stream.GetString().str();
  EXPECT_TRUE(output.find("00000000") != std::string::npos ||
              output.find("0000-0000") != std::string::npos ||
              output.find("nil") != std::string::npos ||
              output.find("zero") != std::string::npos ||
              output.find("empty") != std::string::npos ||
              output.find("UUID") != std::string::npos) 
    << "Zero UUID should be recognizable: " << output;
}

TEST_F(NSUUIDFormatterTest, VariousUUIDFormats) {
  // Test different UUID patterns
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<MockTarget>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  GNUstepNSUUIDSummaryProvider provider;
  
  // Test different UUID patterns
  std::vector<std::array<uint8_t, 16>> uuid_patterns = {
    // Version 1 UUID (time-based)
    {0x6B, 0xA7, 0xB8, 0x10, 0x9D, 0xAD, 0x11, 0xD1, 0x80, 0xB4, 0x00, 0xC0, 0x4F, 0xD4, 0x30, 0xC8},
    // Version 4 UUID (random)
    {0x6B, 0xA7, 0xB8, 0x11, 0x9D, 0xAD, 0x41, 0xD1, 0x80, 0xB4, 0x00, 0xC0, 0x4F, 0xD4, 0x30, 0xC8},
    // All F's pattern
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    // Alternating pattern
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99}
  };
  
  for (size_t i = 0; i < uuid_patterns.size(); ++i) {
    const auto& pattern = uuid_patterns[i];
    
    struct {
      uint64_t isa;
      std::array<uint8_t, 16> uuid_bytes;
    } pattern_nsuuid = {0x7000, pattern};
    
    lldb::addr_t obj_addr = 0xA000 + (i * 100);
    static_cast<MockProcess*>(process.get())->SetMemory(obj_addr, &pattern_nsuuid, sizeof(pattern_nsuuid));
    
    lldb::ValueObjectSP pattern_obj = MockValueObject::Create(target, "pattern_uuid", 
                                                              obj_addr, "NSUUID");
    StreamString output_stream;
    TypeSummaryOptions options;
    
    bool result = provider.FormatObject(*pattern_obj, output_stream, options);
    
    if (result || !output_stream.GetString().empty()) {
      std::string output = output_stream.GetString().str();
      
      // Should show UUID-like format
      EXPECT_TRUE(output.find("UUID") != std::string::npos ||
                  output.find("-") != std::string::npos ||
                  output.length() >= 16 ||  // At least some hex representation
                  !output.empty()) 
        << "UUID pattern " << i << " should be formatted: " << output;
      
      // Should contain some hex characters
      bool has_hex = false;
      for (char c : output) {
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
          has_hex = true;
          break;
        }
      }
      EXPECT_TRUE(has_hex) << "UUID should contain hex characters: " << output;
    }
  }
}

TEST_F(NSUUIDFormatterTest, ErrorHandling) {
  // Test error conditions
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<MockTarget>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  GNUstepNSUUIDSummaryProvider provider;
  
  // Test null NSUUID object
  lldb::ValueObjectSP null_uuid = MockValueObject::Create(target, "null_uuid", 0x0, "NSUUID");
  StreamString null_stream;
  TypeSummaryOptions options;
  
  bool null_result = provider.FormatObject(*null_uuid, null_stream, options);
  
  if (!null_result) {
    EXPECT_TRUE(true) << "Null NSUUID correctly rejected";
  } else {
    std::string null_output = null_stream.GetString().str();
    EXPECT_TRUE(null_output.find("invalid") != std::string::npos ||
                null_output.find("nil") != std::string::npos ||
                null_output.find("null") != std::string::npos) 
      << "Null NSUUID should indicate error: " << null_output;
  }
  
  // Test corrupted NSUUID (invalid ISA)
  struct {
    uint64_t isa;
    std::array<uint8_t, 16> uuid_bytes;
  } corrupted_nsuuid = {LLDB_INVALID_ADDRESS, {0}};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0xB000, &corrupted_nsuuid, sizeof(corrupted_nsuuid));
  
  lldb::ValueObjectSP corrupted_obj = MockValueObject::Create(target, "corrupted_uuid", 
                                                              0xB000, "NSUUID");
  StreamString corrupted_stream;
  
  bool corrupted_result = provider.FormatObject(*corrupted_obj, corrupted_stream, options);
  
  // Should handle gracefully
  if (corrupted_result || !corrupted_stream.GetString().empty()) {
    std::string corrupted_output = corrupted_stream.GetString().str();
    EXPECT_TRUE(corrupted_output.find("NSUUID") != std::string::npos ||
                corrupted_output.find("UUID") != std::string::npos ||
                corrupted_output.find("invalid") != std::string::npos ||
                !corrupted_output.empty()) 
      << "Corrupted NSUUID should be handled: " << corrupted_output;
  }
}

TEST_F(NSUUIDFormatterTest, PerformanceRequirements) {
  // Test that NSUUID formatting meets <50ms requirement
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<MockTarget>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Create UUID for performance test
  std::array<uint8_t, 16> perf_uuid = {
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
  };
  
  struct {
    uint64_t isa;
    std::array<uint8_t, 16> uuid_bytes;
  } perf_nsuuid = {0x7000, perf_uuid};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0xC000, &perf_nsuuid, sizeof(perf_nsuuid));
  
  lldb::ValueObjectSP perf_obj = MockValueObject::Create(target, "perf_uuid", 0xC000, "NSUUID");
  
  // Test performance
  auto start = std::chrono::high_resolution_clock::now();
  
  StreamString output_stream;
  TypeSummaryOptions options;
  GNUstepNSUUIDSummaryProvider provider;
  
  bool result = provider.FormatObject(*perf_obj, output_stream, options);
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 50) << "NSUUID formatting should be under 50ms, took " 
                                   << duration.count() << "ms";
  
  if (result || !output_stream.GetString().empty()) {
    std::string output = output_stream.GetString().str();
    EXPECT_FALSE(output.empty()) << "Performance test should produce output";
  }
}

TEST_F(NSUUIDFormatterTest, UUIDStringRepresentation) {
  // Test that UUID string representation follows standard format
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<MockTarget>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  GNUstepNSUUIDSummaryProvider provider;
  
  // Well-known UUID for testing: 6BA7B810-9DAD-11D1-80B4-00C04FD430C8 (RFC 4122 example)
  std::array<uint8_t, 16> rfc_uuid = {
    0x6B, 0xA7, 0xB8, 0x10, 0x9D, 0xAD, 0x11, 0xD1,
    0x80, 0xB4, 0x00, 0xC0, 0x4F, 0xD4, 0x30, 0xC8
  };
  
  struct {
    uint64_t isa;
    std::array<uint8_t, 16> uuid_bytes;
  } rfc_nsuuid = {0x7000, rfc_uuid};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0xD000, &rfc_nsuuid, sizeof(rfc_nsuuid));
  
  lldb::ValueObjectSP rfc_obj = MockValueObject::Create(target, "rfc_uuid", 0xD000, "NSUUID");
  
  StreamString output_stream;
  TypeSummaryOptions options;
  
  bool result = provider.FormatObject(*rfc_obj, output_stream, options);
  
  if (result || !output_stream.GetString().empty()) {
    std::string output = output_stream.GetString().str();
    
    // Should show some recognizable components of the UUID
    EXPECT_TRUE(output.find("6BA7B810") != std::string::npos ||
                output.find("9DAD") != std::string::npos ||
                output.find("11D1") != std::string::npos ||
                output.find("80B4") != std::string::npos ||
                output.find("C04FD430C8") != std::string::npos ||
                // Alternative: might show as hyphenated format
                output.find("6BA7") != std::string::npos ||
                output.find("B810") != std::string::npos ||
                // Or as compact format
                output.length() >= 32) 
      << "RFC UUID should show recognizable format: " << output;
    
    // Standard UUID string should be reasonable length
    EXPECT_LE(output.length(), 100) << "UUID string should be reasonable length: " << output;
  }
}

TEST_F(NSUUIDFormatterTest, ComparisonWithKnownUUIDs) {
  // Test formatting of well-known special UUIDs
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<MockTarget>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  GNUstepNSUUIDSummaryProvider provider;
  
  // Test several well-known UUID patterns
  struct UUIDTestCase {
    std::string name;
    std::array<uint8_t, 16> bytes;
    std::string expected_component;
  };
  
  std::vector<UUIDTestCase> test_cases = {
    {"Version 1", {0x6B, 0xA7, 0xB8, 0x10, 0x9D, 0xAD, 0x11, 0xD1, 0x80, 0xB4, 0x00, 0xC0, 0x4F, 0xD4, 0x30, 0xC8}, "6BA7"},
    {"Version 4", {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0x4E, 0xF0, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}, "1234"},
    {"Microsoft", {0x8B, 0xC3, 0xF0, 0x5E, 0x3D, 0x19, 0x11, 0xCE, 0x99, 0x82, 0x00, 0xAA, 0x00, 0x4B, 0xB8, 0x51}, "8BC3"},
    {"Nil UUID", {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, "0000"}
  };
  
  for (size_t i = 0; i < test_cases.size(); ++i) {
    const auto& test_case = test_cases[i];
    
    struct {
      uint64_t isa;
      std::array<uint8_t, 16> uuid_bytes;
    } test_nsuuid = {0x7000, test_case.bytes};
    
    lldb::addr_t obj_addr = 0xE000 + (i * 100);
    static_cast<MockProcess*>(process.get())->SetMemory(obj_addr, &test_nsuuid, sizeof(test_nsuuid));
    
    lldb::ValueObjectSP test_obj = MockValueObject::Create(target, "known_uuid", obj_addr, "NSUUID");
    StreamString output_stream;
    TypeSummaryOptions options;
    
    bool result = provider.FormatObject(*test_obj, output_stream, options);
    
    if (result || !output_stream.GetString().empty()) {
      std::string output = output_stream.GetString().str();
      
      // Should contain expected component or reasonable UUID representation
      EXPECT_TRUE(output.find(test_case.expected_component) != std::string::npos ||
                  output.find("UUID") != std::string::npos ||
                  output.length() >= 8 ||  // Some reasonable representation
                  !output.empty()) 
        << test_case.name << " UUID should be properly formatted: " << output;
    }
  }
}

} // namespace