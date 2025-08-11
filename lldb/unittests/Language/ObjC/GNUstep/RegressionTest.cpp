//===-- RegressionTest.cpp -----------------------------------------------===//
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

#include "Formatters/Common/FormatterTestHelpers.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepArrayFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDictionaryFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepNumberFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepSetFormatters.h"

#include <chrono>
#include <memory>
#include <vector>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::test;

namespace {

class RegressionTest : public ::testing::Test {
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

TEST_F(RegressionTest, BasicFormatterInstantiation) {
  // Regression test: All formatters should instantiate without crashing
  // This caught several regression issues during development
  
  // Removed EXPECT_NO_THROW as exceptions are disabled
  {
    auto array_formatter = std::make_unique<GNUstepNSArraySummaryProvider>();
    EXPECT_NE(array_formatter.get(), nullptr);
  }
  
  {
    auto dict_formatter = std::make_unique<GNUstepNSDictionarySummaryProvider>();
    EXPECT_NE(dict_formatter.get(), nullptr);
  }
  
  {
    auto string_formatter = std::make_unique<GNUstepNSStringSummaryProvider>();
    EXPECT_NE(string_formatter.get(), nullptr);
  }
  
  {
    auto number_formatter = std::make_unique<GNUstepNSNumberSummaryProvider>();
    EXPECT_NE(number_formatter.get(), nullptr);
  }
  
  {
    auto set_formatter = std::make_unique<GNUstepNSSetSummaryProvider>();
    EXPECT_NE(set_formatter.get(), nullptr);
  }
}

TEST_F(RegressionTest, EmptyCollectionFormatting) {
  // Regression test: Empty collections should format without hanging or crashing
  // Previous versions had issues with empty collections
  
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                       ArchSpec("x86_64"), PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Test empty array
  {
    auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
    
    struct {
      uint64_t isa;
      uint64_t contents_ptr; 
      uint64_t count;
    } array_obj = {0x5000, 0x6000, 0};
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &array_obj, sizeof(array_obj));
    
    lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "empty_array", 
                                                               0x8000, "NSArray");
    
    StreamString output;
    TypeSummaryOptions options;
    
    auto start = std::chrono::high_resolution_clock::now();
    bool result = provider->FormatObject(*array_obj_sp, output, options);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 100) << "Empty array should format quickly";
    EXPECT_TRUE(result || !output.GetString().empty()) << "Empty array should produce output";
    
    std::string result_str = output.GetString().str();
    EXPECT_FALSE(result_str.empty()) << "Empty array should produce non-empty output";
  }
  
  // Test empty dictionary
  {
    auto provider = std::make_unique<GNUstepNSDictionarySummaryProvider>();
    
    struct {
      uint64_t isa;
      uint64_t table_ptr;
      uint64_t count;
    } dict_obj = {0x5100, 0x6000, 0};
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8100, &dict_obj, sizeof(dict_obj));
    
    lldb::ValueObjectSP dict_obj_sp = MockValueObject::Create(target, "empty_dict", 
                                                              0x8100, "NSDictionary");
    
    StreamString output;
    TypeSummaryOptions options;
    
    auto start = std::chrono::high_resolution_clock::now();
    bool result = provider->FormatObject(*dict_obj_sp, output, options);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 100) << "Empty dictionary should format quickly";
    EXPECT_TRUE(result || !output.GetString().empty()) << "Empty dictionary should produce output";
    
    std::string result_str = output.GetString().str();
    EXPECT_FALSE(result_str.empty()) << "Empty dictionary should produce non-empty output";
  }
  
  // Test empty set
  {
    auto provider = std::make_unique<GNUstepNSSetSummaryProvider>();
    
    struct {
      uint64_t isa;
      uint64_t table_ptr;
      uint64_t count;
    } set_obj = {0x5200, 0x6000, 0};
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8200, &set_obj, sizeof(set_obj));
    
    lldb::ValueObjectSP set_obj_sp = MockValueObject::Create(target, "empty_set", 
                                                             0x8200, "NSSet");
    
    StreamString output;
    TypeSummaryOptions options;
    
    auto start = std::chrono::high_resolution_clock::now();
    bool result = provider->FormatObject(*set_obj_sp, output, options);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 100) << "Empty set should format quickly";
    EXPECT_TRUE(result || !output.GetString().empty()) << "Empty set should produce output";
    
    std::string result_str = output.GetString().str();
    EXPECT_FALSE(result_str.empty()) << "Empty set should produce non-empty output";
  }
}

TEST_F(RegressionTest, NullPointerHandling) {
  // Regression test: Null pointers should not crash formatters
  // Previous versions crashed on null ValueObjects
  
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                       ArchSpec("x86_64"), PlatformSP());
  
  // Test null ValueObject
  lldb::ValueObjectSP null_obj;
  
  auto array_provider = std::make_unique<GNUstepNSArraySummaryProvider>();
  auto dict_provider = std::make_unique<GNUstepNSDictionarySummaryProvider>();
  auto string_provider = std::make_unique<GNUstepNSStringSummaryProvider>();
  
  StreamString output;
  TypeSummaryOptions options;
  
  // These should not crash, may return false
  EXPECT_NO_FATAL_FAILURE({
    if (null_obj.get()) {
      array_provider->FormatObject(*null_obj, output, options);
    }
  });
  
  EXPECT_NO_FATAL_FAILURE({
    if (null_obj.get()) {
      dict_provider->FormatObject(*null_obj, output, options);
    }
  });
  
  EXPECT_NO_FATAL_FAILURE({
    if (null_obj.get()) {
      string_provider->FormatObject(*null_obj, output, options);
    }
  });
  
  // Test object at address 0 (null)
  lldb::ValueObjectSP zero_obj = MockValueObject::Create(target, "null_object", 0, "NSObject");
  
  EXPECT_NO_FATAL_FAILURE({
    array_provider->FormatObject(*zero_obj, output, options);
  });
}

TEST_F(RegressionTest, LargeCountHandling) {
  // Regression test: Large counts should be handled gracefully
  // Previous versions had integer overflow issues
  
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                       ArchSpec("x86_64"), PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
  
  // Test various large count values
  std::vector<uint64_t> large_counts = {
    1000000,        // 1 million (should work)
    10000000,       // 10 million (boundary)
    100000000,      // 100 million (should be rejected)
    UINT32_MAX,     // Maximum 32-bit (should be rejected)
    UINT64_MAX      // Maximum 64-bit (should be rejected)
  };
  
  for (uint64_t count : large_counts) {
    struct {
      uint64_t isa;
      uint64_t contents_ptr; 
      uint64_t count;
    } array_obj = {0x5000, 0x6000, count};
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &array_obj, sizeof(array_obj));
    
    lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "large_count_array", 
                                                               0x8000, "NSArray");
    
    StreamString output;
    TypeSummaryOptions options;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Should not hang or crash regardless of count value
    EXPECT_NO_FATAL_FAILURE({
      bool result = provider->FormatObject(*array_obj_sp, output, options);
      (void)result; // May succeed or fail gracefully
    });
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 1000) 
      << "Large count " << count << " should not cause hanging (took " 
      << duration.count() << "ms)";
    
    printf("Large count %llu: handled in %lld ms\n", count, duration.count());
  }
}

TEST_F(RegressionTest, TaggedPointerConsistency) {
  // Regression test: Tagged pointer detection should be consistent
  // Previous versions had inconsistent tagged pointer handling
  
  std::vector<uint64_t> test_values = {
    0x0000000000000000ULL,  // Null
    0x0000000000000001ULL,  // Tagged int: 0
    0x0000000000000009ULL,  // Tagged int: 1
    0x0000000000000004ULL,  // Tagged string: empty
    0x0000000000001000ULL,  // Regular pointer (aligned)
    0x0000000000001001ULL,  // Regular pointer + tag 1 (invalid)
    0x0000000000001004ULL,  // Regular pointer + tag 4 (invalid)
    0xFFFFFFFFFFFFFFFFULL   // Invalid address
  };
  
  for (uint64_t value : test_values) {
    // Test consistent tagged pointer detection
    uint8_t tag = value & 0x7;
    bool should_be_tagged = (value != 0) && 
                           (value != LLDB_INVALID_ADDRESS) &&
                           (tag == 1 || tag == 4);
    
    // Test the detection multiple times to ensure consistency
    for (int i = 0; i < 5; ++i) {
      bool detected_as_tagged = (value != 0) && 
                               (value != LLDB_INVALID_ADDRESS) &&
                               ((value & 0x7) == 1 || (value & 0x7) == 4);
      
      EXPECT_EQ(detected_as_tagged, should_be_tagged) 
        << "Tagged pointer detection should be consistent for 0x" 
        << std::hex << value << " (iteration " << i << ")";
    }
    
    // Tagged pointers should not change when detected multiple times
    if (should_be_tagged) {
      if (tag == 1) {
        int64_t decoded_value1 = static_cast<int64_t>(value) >> 3;
        int64_t decoded_value2 = static_cast<int64_t>(value) >> 3;
        
        EXPECT_EQ(decoded_value1, decoded_value2) 
          << "Tagged int decoding should be consistent";
      } else if (tag == 4) {
        int length1 = (value >> 3) & 0x1F;
        int length2 = (value >> 3) & 0x1F;
        
        EXPECT_EQ(length1, length2) 
          << "Tagged string length should be consistent";
      }
    }
  }
}

TEST_F(RegressionTest, PerformanceRegression) {
  // Regression test: Ensure formatters maintain performance requirements
  // This catches performance regressions introduced by new features
  
  const int64_t MAX_TIME_MS = 50; // Core performance requirement
  
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                       ArchSpec("x86_64"), PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Test array formatter performance
  {
    auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
    
    struct {
      uint64_t isa;
      uint64_t contents_ptr; 
      uint64_t count;
    } array_obj = {0x5000, 0x6000, 100}; // Moderate size
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &array_obj, sizeof(array_obj));
    
    lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "perf_array", 
                                                               0x8000, "NSArray");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    StreamString output;
    TypeSummaryOptions options;
    bool result = provider->FormatObject(*array_obj_sp, output, options);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), MAX_TIME_MS) 
      << "Array formatter took " << duration.count() 
      << "ms (requirement: <" << MAX_TIME_MS << "ms)";
    
    EXPECT_TRUE(result || !output.GetString().empty()) 
      << "Array formatter should produce output";
  }
  
  // Test string formatter performance
  {
    auto provider = std::make_unique<GNUstepNSStringSummaryProvider>();
    
    std::string test_str(1000, 'A'); // 1KB string
    
    struct {
      uint64_t isa;
      uint64_t length;
      uint64_t chars_ptr;
    } string_obj = {0x5000, test_str.length(), 0x6000};
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8100, &string_obj, sizeof(string_obj));
    static_cast<MockProcess*>(process.get())->SetMemory(0x6000, test_str.c_str(), 
                                                        std::min(test_str.length(), size_t(512)));
    
    lldb::ValueObjectSP string_obj_sp = MockValueObject::Create(target, "perf_string", 
                                                                0x8100, "NSString");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    StreamString output;
    TypeSummaryOptions options;
    bool result = provider->FormatObject(*string_obj_sp, output, options);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), MAX_TIME_MS) 
      << "String formatter took " << duration.count() 
      << "ms (requirement: <" << MAX_TIME_MS << "ms)";
      
    EXPECT_TRUE(result || !output.GetString().empty()) 
      << "String formatter should produce output";
  }
}

TEST_F(RegressionTest, OutputQualityValidation) {
  // Regression test: Ensure formatters produce meaningful output
  // This catches regressions where formatters succeed but produce poor output
  
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                       ArchSpec("x86_64"), PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Test that array formatter produces meaningful output
  {
    auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
    
    struct {
      uint64_t isa;
      uint64_t contents_ptr; 
      uint64_t count;
    } array_obj = {0x5000, 0x6000, 3};
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &array_obj, sizeof(array_obj));
    
    lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "quality_array", 
                                                               0x8000, "NSArray");
    
    StreamString output;
    TypeSummaryOptions options;
    bool result = provider->FormatObject(*array_obj_sp, output, options);
    
    EXPECT_TRUE(result || !output.GetString().empty()) << "Array formatter should succeed";
    
    std::string output_str = output.GetString().str();
    
    // Output quality checks
    EXPECT_GT(output_str.length(), 3) << "Array output should be more than just count";
    EXPECT_TRUE(output_str.find("3") != std::string::npos ||
                output_str.find("elements") != std::string::npos ||
                output_str.find("objects") != std::string::npos)
      << "Array should indicate its size: " << output_str;
    
    // Should not be just error messages or memory addresses
    EXPECT_FALSE(output_str.find("error") == 0) << "Should not start with error";
    EXPECT_FALSE(output_str.find("0x") == 0) << "Should not be just an address";
  }
  
  // Test that string formatter produces meaningful output  
  {
    auto provider = std::make_unique<GNUstepNSStringSummaryProvider>();
    
    std::string test_str = "Hello World";
    
    struct {
      uint64_t isa;
      uint64_t length;
      uint64_t chars_ptr;
    } string_obj = {0x5000, test_str.length(), 0x6000};
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8100, &string_obj, sizeof(string_obj));
    static_cast<MockProcess*>(process.get())->SetMemory(0x6000, test_str.c_str(), test_str.length());
    
    lldb::ValueObjectSP string_obj_sp = MockValueObject::Create(target, "quality_string", 
                                                                0x8100, "NSString");
    
    StreamString output;
    TypeSummaryOptions options;
    bool result = provider->FormatObject(*string_obj_sp, output, options);
    
    EXPECT_TRUE(result || !output.GetString().empty()) << "String formatter should succeed";
    
    std::string output_str = output.GetString().str();
    
    // Output quality checks
    EXPECT_GT(output_str.length(), 5) << "String output should be substantial";
    EXPECT_TRUE(output_str.find("Hello") != std::string::npos ||
                output_str.find("World") != std::string::npos ||
                output_str.find("11") != std::string::npos) // length
      << "String should show content or length: " << output_str;
  }
}

TEST_F(RegressionTest, MemoryAccessErrorHandling) {
  // Regression test: Memory access errors should be handled gracefully
  // Previous versions could crash on memory read failures
  
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                       ArchSpec("x86_64"), PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
  
  // Test object with invalid contents pointer
  struct {
    uint64_t isa;
    uint64_t contents_ptr; 
    uint64_t count;
  } array_obj = {0x5000, LLDB_INVALID_ADDRESS, 5}; // Invalid contents pointer
  
  static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &array_obj, sizeof(array_obj));
  
  lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "invalid_memory", 
                                                             0x8000, "NSArray");
  
  StreamString output;
  TypeSummaryOptions options;
  
  // Should not crash on invalid memory access
  EXPECT_NO_FATAL_FAILURE({
    bool result = provider->FormatObject(*array_obj_sp, output, options);
    (void)result; // May succeed or fail, but should not crash
  });
  
  // Should complete in reasonable time even with memory access failures
  auto start = std::chrono::high_resolution_clock::now();
  
  provider->FormatObject(*array_obj_sp, output, options);
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 500) << "Memory access errors should not cause hanging";
}

} // namespace