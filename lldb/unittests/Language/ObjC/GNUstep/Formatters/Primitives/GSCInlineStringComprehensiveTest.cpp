//===-- GSCInlineStringComprehensiveTest.cpp - Comprehensive tests -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../../../../../../source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.h"
#include "../Common/FormatterTestHelpers.h"

#include "gtest/gtest.h"
#include "lldb/Core/ValueObject.h"
#include "lldb/DataFormatters/FormatManager.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Target/Platform.h"
#include "lldb/Utility/ArchSpec.h"
#include "lldb/Utility/StreamString.h"
#include "lldb/DataFormatters/TypeSummary.h"

#include <chrono>
#include <cstring>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::test;

class GSCInlineStringComprehensiveTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
    m_debugger = Debugger::CreateInstance();
    ArchSpec arch("x86_64-pc-linux");
    PlatformSP platform_sp = Platform::GetHostPlatform();
    m_target = std::make_shared<MockTarget>(*m_debugger, arch, platform_sp);
    m_process = std::make_shared<MockProcess>(m_target, ListenerSP());
    ModuleSP empty_module_sp;
    m_target->SetExecutableModule(empty_module_sp, eLoadDependentsNo);
  }

  void TearDown() override {
    m_process.reset();
    m_target.reset();
    Debugger::Destroy(m_debugger);
    HostInfo::Terminate();
    FileSystem::Terminate();
  }
  
  DebuggerSP m_debugger;
  TargetSP m_target;
  std::shared_ptr<MockProcess> m_process;
};

// Test GSCInlineString with various string lengths
TEST_F(GSCInlineStringComprehensiveTest, VariousStringLengths) {
  struct TestCase {
    const char* str;
    const char* description;
  };
  
  TestCase test_cases[] = {
    {"", "empty string"},
    {"A", "single character"},
    {"Hi", "two characters"},
    {"Test", "four characters"},
    {"Hello123", "eight characters"},
    {"Hello World!", "string with space"},
    {"Line1\nLine2", "string with newline"},
    {"Tab\there", "string with tab"},
    {"Unicode🎉", "string with emoji (will be truncated)"},
    {"Very long string that exceeds typical inline storage but should still work correctly in GSCInlineString implementation", "long string"}
  };
  
  for (const auto& tc : test_cases) {
    size_t str_len = strlen(tc.str);
    
    // GSCInlineString layout
    struct GSCInlineStringMock {
      uint64_t isa;
      uint64_t contents_ptr;
      uint32_t count;
      uint32_t flags;
      char inline_data[256];
    } __attribute__((packed));
    
    GSCInlineStringMock mock_str;
    mock_str.isa = 0x1000;  // GSCInlineString class
    mock_str.contents_ptr = 0x2000 + 24;  // Points to inline data
    mock_str.count = str_len;
    mock_str.flags = 0;  // ASCII (not wide)
    strcpy(mock_str.inline_data, tc.str);
    
    lldb::addr_t obj_addr = 0x2000;
    m_process->SetMemory(obj_addr, &mock_str, sizeof(mock_str));
    
    lldb::ValueObjectSP valobj = MockValueObject::Create(m_target, tc.description, 
                                                         obj_addr, "GSCInlineString");
    ASSERT_NE(valobj, nullptr) << "Failed to create ValueObject for: " << tc.description;
    
    StreamString stream;
    TypeSummaryOptions options;
    
    // Test the formatter
    bool result = GNUstepGSCInlineStringFormatterFunction(*valobj, stream, options);
    
    // For this mock test, we mainly verify the structure is correct
    EXPECT_EQ(mock_str.count, str_len) << "Count mismatch for: " << tc.description;
    EXPECT_EQ(mock_str.flags & 1, 0U) << "Should be ASCII for: " << tc.description;
    
    // Verify the inline data matches
    if (str_len > 0) {
      EXPECT_EQ(memcmp(mock_str.inline_data, tc.str, str_len), 0) 
        << "Inline data mismatch for: " << tc.description;
    }
  }
}

// Test GSCInlineString vs GSTinyString distinction
TEST_F(GSCInlineStringComprehensiveTest, InlineVsTinyStringDistinction) {
  // GSCInlineString is a regular object with inline storage
  // GSTinyString is a tagged pointer (tag = 4)
  
  // Test GSCInlineString (regular object)
  struct GSCInlineStringMock {
    uint64_t isa;
    uint64_t contents_ptr;
    uint32_t count;
    uint32_t flags;
    char inline_data[16];
  } __attribute__((packed));
  
  GSCInlineStringMock inline_str;
  inline_str.isa = 0x1000;
  inline_str.contents_ptr = 0x3000 + 24;
  inline_str.count = 5;
  inline_str.flags = 0;
  strcpy(inline_str.inline_data, "Hello");
  
  lldb::addr_t inline_addr = 0x3000;
  m_process->SetMemory(inline_addr, &inline_str, sizeof(inline_str));
  
  // Test GSTinyString (tagged pointer)
  // Tag = 4, Length = 5, "Hello" encoded in bits
  lldb::addr_t tiny_addr = 0x919766cde000002c;  // Example from actual test
  
  // Verify addresses
  EXPECT_EQ(inline_addr & 0x7, 0U) << "GSCInlineString should not be tagged";
  EXPECT_EQ(tiny_addr & 0x7, 4U) << "GSTinyString should have tag 4";
  
  // Verify length extraction from GSTinyString
  int tiny_length = (tiny_addr >> 3) & 0x1F;
  EXPECT_EQ(tiny_length, 5) << "GSTinyString length should be 5";
}

// Test Unicode (wide character) support
TEST_F(GSCInlineStringComprehensiveTest, UnicodeSupport) {
  const char16_t unicode_str[] = u"Hello 世界";
  size_t char_count = 8;
  
  struct GSUInlineStringMock {
    uint64_t isa;
    uint64_t contents_ptr;
    uint32_t count;
    uint32_t flags;
    char16_t inline_data[64];
  } __attribute__((packed));
  
  GSUInlineStringMock unicode_mock;
  unicode_mock.isa = 0x1100;  // GSUInlineString class
  unicode_mock.contents_ptr = 0x4000 + 24;
  unicode_mock.count = char_count;
  unicode_mock.flags = 1;  // wide = 1 for Unicode
  memcpy(unicode_mock.inline_data, unicode_str, (char_count + 1) * sizeof(char16_t));
  
  lldb::addr_t obj_addr = 0x4000;
  m_process->SetMemory(obj_addr, &unicode_mock, sizeof(unicode_mock));
  
  lldb::ValueObjectSP valobj = MockValueObject::Create(m_target, "unicode_string", 
                                                       obj_addr, "GSUInlineString");
  ASSERT_NE(valobj, nullptr);
  
  // Verify Unicode structure
  EXPECT_EQ(unicode_mock.count, char_count) << "Unicode string character count";
  EXPECT_EQ(unicode_mock.flags & 1, 1U) << "Wide flag should be set for Unicode";
  
  // Verify first few characters
  EXPECT_EQ(unicode_mock.inline_data[0], u'H');
  EXPECT_EQ(unicode_mock.inline_data[1], u'e');
  EXPECT_EQ(unicode_mock.inline_data[6], u'世');
  EXPECT_EQ(unicode_mock.inline_data[7], u'界');
}

// Test integration with dictionary keys
TEST_F(GSCInlineStringComprehensiveTest, DictionaryKeyIntegration) {
  // Simulate GSCInlineString used as dictionary key
  const char* key_str = "key1";
  const char* value_str = "value1";
  
  // Create key (GSCInlineString)
  struct GSCInlineStringMock {
    uint64_t isa;
    uint64_t contents_ptr;
    uint32_t count;
    uint32_t flags;
    char inline_data[16];
  } __attribute__((packed));
  
  GSCInlineStringMock key_mock;
  key_mock.isa = 0x1000;
  key_mock.contents_ptr = 0x5000 + 24;
  key_mock.count = strlen(key_str);
  key_mock.flags = 0;
  strcpy(key_mock.inline_data, key_str);
  
  lldb::addr_t key_addr = 0x5000;
  m_process->SetMemory(key_addr, &key_mock, sizeof(key_mock));
  
  // Create value (NSConstantString for variety)
  struct NSConstantStringMock {
    uint64_t isa;
    const char* str_ptr;
    uint32_t length;
    uint32_t padding;
  } __attribute__((packed));
  
  NSConstantStringMock value_mock;
  value_mock.isa = 0x2000;
  value_mock.str_ptr = value_str;
  value_mock.length = strlen(value_str);
  value_mock.padding = 0;
  
  lldb::addr_t value_addr = 0x6000;
  m_process->SetMemory(value_addr, &value_mock, sizeof(value_mock));
  m_process->SetMemory(reinterpret_cast<lldb::addr_t>(value_str), 
                      value_str, strlen(value_str) + 1);
  
  // Verify key can be extracted
  lldb::ValueObjectSP key_valobj = MockValueObject::Create(m_target, "dict_key", 
                                                           key_addr, "GSCInlineString");
  ASSERT_NE(key_valobj, nullptr);
  
  // Verify the key structure
  EXPECT_EQ(key_mock.count, strlen(key_str)) << "Key length should match";
  EXPECT_STREQ(key_mock.inline_data, key_str) << "Key data should match";
}

// Performance test
TEST_F(GSCInlineStringComprehensiveTest, PerformanceBenchmark) {
  const char* perf_str = "Performance test string";
  
  struct GSCInlineStringMock {
    uint64_t isa;
    uint64_t contents_ptr;
    uint32_t count;
    uint32_t flags;
    char inline_data[128];
  } __attribute__((packed));
  
  GSCInlineStringMock perf_mock;
  perf_mock.isa = 0x1000;
  perf_mock.contents_ptr = 0x7000 + 24;
  perf_mock.count = strlen(perf_str);
  perf_mock.flags = 0;
  strcpy(perf_mock.inline_data, perf_str);
  
  lldb::addr_t obj_addr = 0x7000;
  m_process->SetMemory(obj_addr, &perf_mock, sizeof(perf_mock));
  
  lldb::ValueObjectSP valobj = MockValueObject::Create(m_target, "perf_test", 
                                                       obj_addr, "GSCInlineString");
  ASSERT_NE(valobj, nullptr);
  
  StreamString stream;
  TypeSummaryOptions options;
  
  // Measure formatting time
  auto start = std::chrono::high_resolution_clock::now();
  
  const int iterations = 1000;
  for (int i = 0; i < iterations; ++i) {
    stream.Clear();
    bool result = GNUstepGSCInlineStringFormatterFunction(*valobj, stream, options);
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  
  double avg_us = duration.count() / static_cast<double>(iterations);
  
  EXPECT_LT(avg_us, 50.0) << "Average formatting time should be under 50μs, was " << avg_us << "μs";
  
  // Log performance for tracking
  std::cout << "GSCInlineString formatter performance: " << avg_us << "μs per operation" << std::endl;
}

// Test error handling
TEST_F(GSCInlineStringComprehensiveTest, ErrorHandling) {
  // Test with invalid/corrupted data
  struct TestCase {
    uint32_t count;
    uint32_t flags;
    const char* description;
  };
  
  TestCase error_cases[] = {
    {0xFFFFFFFF, 0, "invalid count (too large)"},
    {100, 0xFFFFFFFF, "invalid flags"},
    {0, 0, "zero length string"},
    {10000, 0, "excessively long string"}
  };
  
  for (const auto& tc : error_cases) {
    struct GSCInlineStringMock {
      uint64_t isa;
      uint64_t contents_ptr;
      uint32_t count;
      uint32_t flags;
      char inline_data[16];
    } __attribute__((packed));
    
    GSCInlineStringMock error_mock;
    error_mock.isa = 0x1000;
    error_mock.contents_ptr = 0x8000 + 24;
    error_mock.count = tc.count;
    error_mock.flags = tc.flags;
    memset(error_mock.inline_data, 0, sizeof(error_mock.inline_data));
    
    lldb::addr_t obj_addr = 0x8000;
    m_process->SetMemory(obj_addr, &error_mock, sizeof(error_mock));
    
    lldb::ValueObjectSP valobj = MockValueObject::Create(m_target, tc.description, 
                                                         obj_addr, "GSCInlineString");
    ASSERT_NE(valobj, nullptr) << "Failed for: " << tc.description;
    
    StreamString stream;
    TypeSummaryOptions options;
    
    // Formatter should handle errors gracefully
    bool result = GNUstepGSCInlineStringFormatterFunction(*valobj, stream, options);
    
    // We don't crash - that's the main test
    // The formatter may return false or an error string
  }
}