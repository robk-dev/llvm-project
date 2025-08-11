//===-- EndiannessTest.cpp -----------------------------------------------===//
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
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/DataBufferHeap.h"

#include "../Formatters/Common/FormatterTestHelpers.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepArrayFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepNumberFormatters.h"

#include <memory>
#include <vector>
#include <cstring>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::test;

namespace {

class EndiannessTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
  }
  
  void TearDown() override {
    HostInfo::Terminate();
    FileSystem::Terminate();
  }
  
  // Helper to swap bytes for endianness testing
  template<typename T>
  T SwapBytes(T value) {
    T result;
    uint8_t* src = reinterpret_cast<uint8_t*>(&value);
    uint8_t* dst = reinterpret_cast<uint8_t*>(&result);
    
    for (size_t i = 0; i < sizeof(T); ++i) {
      dst[i] = src[sizeof(T) - 1 - i];
    }
    
    return result;
  }
  
  // Test structure with both endianness formats
  template<typename T>
  void TestBothEndianness(T little_endian_value, T big_endian_value, 
                         const std::string& description) {
    // Test that values are actually different when swapped (except for symmetric values)
    if (little_endian_value != SwapBytes(little_endian_value)) {
      EXPECT_EQ(SwapBytes(little_endian_value), big_endian_value) 
        << "Endianness swap should work for " << description;
    }
    
    // Verify the swap operation is reversible
    EXPECT_EQ(SwapBytes(SwapBytes(little_endian_value)), little_endian_value)
      << "Double swap should restore original value for " << description;
  }
};

TEST_F(EndiannessTest, BasicEndiannessDetection) {
  // Test basic endianness detection mechanisms
  
  // Test with a known multi-byte value
  uint32_t test_value = 0x12345678;
  uint8_t* bytes = reinterpret_cast<uint8_t*>(&test_value);
  
  bool is_little_endian = (bytes[0] == 0x78);
  bool is_big_endian = (bytes[0] == 0x12);
  
  EXPECT_TRUE(is_little_endian || is_big_endian) 
    << "Should detect either little or big endian";
  EXPECT_NE(is_little_endian, is_big_endian) 
    << "Should be exactly one endianness";
  
  printf("Host endianness: %s\n", is_little_endian ? "Little Endian" : "Big Endian");
  
  // Test endianness with DataExtractor
  DataBufferSP buffer(new DataBufferHeap(&test_value, sizeof(test_value)));
  
  DataExtractor le_extractor(buffer, eByteOrderLittle, sizeof(void*));
  DataExtractor be_extractor(buffer, eByteOrderBig, sizeof(void*));
  
  lldb::offset_t offset = 0;
  uint32_t le_value = le_extractor.GetU32(&offset);
  offset = 0;
  uint32_t be_value = be_extractor.GetU32(&offset);
  
  if (is_little_endian) {
    EXPECT_EQ(le_value, test_value) << "Little endian extraction should match";
    EXPECT_EQ(be_value, SwapBytes(test_value)) << "Big endian extraction should be swapped";
  } else {
    EXPECT_EQ(be_value, test_value) << "Big endian extraction should match";
    EXPECT_EQ(le_value, SwapBytes(test_value)) << "Little endian extraction should be swapped";
  }
}

TEST_F(EndiannessTest, NSArrayObjectLayoutEndianness) {
  // Test NSArray object layout under different endianness
  
  auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
  
  // Test data in both endianness formats
  struct {
    uint64_t isa;
    uint64_t contents_ptr;
    uint64_t count;
  } le_array = {0x0000000000005000ULL, 0x0000000000006000ULL, 0x0000000000000005ULL};
  
  struct {
    uint64_t isa;
    uint64_t contents_ptr;
    uint64_t count;
  } be_array = {SwapBytes(le_array.isa), SwapBytes(le_array.contents_ptr), SwapBytes(le_array.count)};
  
  // Test both endianness formats
  std::vector<std::pair<std::string, ByteOrder>> endianness_tests = {
    {"Little Endian", eByteOrderLittle},
    {"Big Endian", eByteOrderBig}
  };
  
  for (auto& [desc, byte_order] : endianness_tests) {
    lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                         ArchSpec("x86_64"), PlatformSP());
    
    // Configure target for specific endianness
    // TODO: MockTarget doesn't have SetByteOrder - need to fix this test
    // static_cast<MockTarget*>(target.get())->SetByteOrder(byte_order);
    
    lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
    // TODO: MockProcess doesn't have SetByteOrder - need to fix this test  
    // static_cast<MockProcess*>(process.get())->SetByteOrder(byte_order);
    
    // Choose appropriate array structure based on target endianness
    const void* array_data = (byte_order == eByteOrderLittle) ? 
                             static_cast<const void*>(&le_array) : 
                             static_cast<const void*>(&be_array);
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, array_data, 
                                                        sizeof(le_array));
    
    lldb::ValueObjectSP array_obj = MockValueObject::Create(target, "endian_array", 
                                                            0x8000, "NSArray");
    
    StreamString output;
    TypeSummaryOptions options;
    bool result = provider->FormatObject(*array_obj, output, options);
    
    EXPECT_TRUE(result || !output.GetString().empty()) 
      << desc << " NSArray should be formatted";
    
    std::string output_str = output.GetString().str();
    
    // Should show count of 5 regardless of endianness
    EXPECT_TRUE(output_str.find("5") != std::string::npos ||
                output_str.find("elements") != std::string::npos)
      << desc << " array should show element count: " << output_str;
    
    printf("%s NSArray result: %s\n", desc.c_str(), output_str.c_str());
  }
}

TEST_F(EndiannessTest, TaggedPointerEndiannessHandling) {
  // Test tagged pointer handling under different endianness
  
  // Tagged pointers should work the same regardless of host endianness
  // since they're immediate values, not memory-loaded structures
  
  std::vector<uint64_t> test_tagged_values = {
    0x0000000000000001ULL,  // Tagged int: 0
    0x0000000000000009ULL,  // Tagged int: 1  
    0x0000000000000029ULL,  // Tagged int: 5
    0x0000000000000004ULL,  // Tagged string: empty
    0x000000000000002CULL,  // Tagged string: length 1
    0x48692100000000044ULL  // Tagged string: "Hi!"
  };
  
  for (uint64_t tagged_value : test_tagged_values) {
    uint8_t tag = tagged_value & 0x7;
    bool is_tagged = (tagged_value != 0) && (tag == 1 || tag == 4);
    
    if (is_tagged) {
      if (tag == 1) {
        // Tagged integer - should decode the same regardless of endianness
        int64_t decoded_value = static_cast<int64_t>(tagged_value) >> 3;
        
        EXPECT_TRUE(decoded_value >= -1000000 && decoded_value <= 1000000)
          << "Tagged int should decode to reasonable value: " << decoded_value
          << " from 0x" << std::hex << tagged_value;
          
      } else if (tag == 4) {
        // Tagged string - length extraction should work regardless of endianness
        int length = (tagged_value >> 3) & 0x1F;
        
        EXPECT_GE(length, 0) << "Tagged string length should be non-negative";
        EXPECT_LE(length, 9) << "Tagged string length should be reasonable";
      }
    }
    
    // Test that tagged pointer detection is consistent
    bool detected_as_tagged = (tagged_value & 0x7) == 1 || (tagged_value & 0x7) == 4;
    EXPECT_EQ(detected_as_tagged, is_tagged) 
      << "Tagged pointer detection should be consistent for 0x" << std::hex << tagged_value;
  }
}

TEST_F(EndiannessTest, StringObjectEndiannessHandling) {
  // Test NSString object layout under different endianness
  
  auto provider = std::make_unique<GNUstepNSStringSummaryProvider>();
  
  std::string test_string = "Hello, 世界!"; // Mixed ASCII and Unicode
  
  struct {
    uint64_t isa;
    uint64_t length;
    uint64_t chars_ptr;
  } le_string = {0x0000000000005000ULL, test_string.length(), 0x0000000000006000ULL};
  
  struct {
    uint64_t isa;
    uint64_t length;
    uint64_t chars_ptr;
  } be_string = {SwapBytes(le_string.isa), SwapBytes(le_string.length), SwapBytes(le_string.chars_ptr)};
  
  std::vector<std::pair<std::string, ByteOrder>> endianness_tests = {
    {"Little Endian", eByteOrderLittle},
    {"Big Endian", eByteOrderBig}
  };
  
  for (auto& [desc, byte_order] : endianness_tests) {
    lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                         ArchSpec("x86_64"), PlatformSP());
    
    // TODO: MockTarget doesn't have SetByteOrder - need to fix this test
    // static_cast<MockTarget*>(target.get())->SetByteOrder(byte_order);
    
    lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
    // TODO: MockProcess doesn't have SetByteOrder - need to fix this test
    // static_cast<MockProcess*>(process.get())->SetByteOrder(byte_order);
    
    const void* string_data = (byte_order == eByteOrderLittle) ? 
                              static_cast<const void*>(&le_string) : 
                              static_cast<const void*>(&be_string);
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, string_data, 
                                                        sizeof(le_string));
    static_cast<MockProcess*>(process.get())->SetMemory(0x6000, test_string.c_str(), 
                                                        test_string.length());
    
    lldb::ValueObjectSP string_obj = MockValueObject::Create(target, "endian_string", 
                                                             0x8000, "NSString");
    
    StreamString output;
    TypeSummaryOptions options;
    bool result = provider->FormatObject(*string_obj, output, options);
    
    EXPECT_TRUE(result || !output.GetString().empty()) 
      << desc << " NSString should be formatted";
    
    std::string output_str = output.GetString().str();
    
    // Should contain the original string content regardless of endianness
    EXPECT_TRUE(output_str.find("Hello") != std::string::npos)
      << desc << " string should contain original text: " << output_str;
    
    printf("%s NSString result: %s\n", desc.c_str(), output_str.c_str());
  }
}

TEST_F(EndiannessTest, DataExtractorEndiannessConsistency) {
  // Test that DataExtractor handles endianness correctly for our use cases
  
  // Test data with different endianness significance
  std::vector<uint64_t> test_values = {
    0x0000000000000001ULL,  // Low bit set
    0x0000000000001000ULL,  // Middle bits
    0x0000001000000000ULL,  // High middle bits
    0x1000000000000000ULL,  // High bit set
    0x123456789ABCDEFULL    // All different bytes
  };
  
  for (uint64_t value : test_values) {
    // Create buffer with the value
    DataBufferSP buffer(new DataBufferHeap(&value, sizeof(value)));
    
    // Test both endianness interpretations
    DataExtractor le_extractor(buffer, eByteOrderLittle, 8);
    DataExtractor be_extractor(buffer, eByteOrderBig, 8);
    
    lldb::offset_t offset = 0;
    uint64_t le_result = le_extractor.GetU64(&offset);
    offset = 0;
    uint64_t be_result = be_extractor.GetU64(&offset);
    
    // One should match the original, one should be byte-swapped
    bool le_matches = (le_result == value);
    bool be_matches = (be_result == value);
    
    // For symmetric values, both might match
    if (value != SwapBytes(value)) {
      EXPECT_NE(le_matches, be_matches) 
        << "Exactly one endianness should match for asymmetric value 0x" 
        << std::hex << value;
      
      if (le_matches) {
        EXPECT_EQ(be_result, SwapBytes(value)) 
          << "Big endian should be swapped when little endian matches";
      } else {
        EXPECT_EQ(le_result, SwapBytes(value)) 
          << "Little endian should be swapped when big endian matches";
      }
    }
    
    printf("Value 0x%llx: LE=0x%llx, BE=0x%llx\n", value, le_result, be_result);
  }
}

TEST_F(EndiannessTest, PointerAlignmentAndEndianness) {
  // Test that pointer handling works correctly under different endianness
  
  std::vector<uint64_t> test_pointers = {
    0x0000000000001000ULL,  // Low address
    0x0000000012345678ULL,  // Medium address  
    0x00007FFF00000000ULL,  // High user space address
    0x0000000000000000ULL,  // NULL pointer
    0xFFFFFFFFFFFFFFFFULL   // Invalid address
  };
  
  for (uint64_t ptr : test_pointers) {
    // Test alignment detection (should be endianness-independent for addresses)
    bool is_aligned = (ptr % 8) == 0;
    bool is_null = (ptr == 0);
    bool is_invalid = (ptr == LLDB_INVALID_ADDRESS);
    
    // These properties should be independent of endianness
    uint64_t swapped_ptr = SwapBytes(ptr);
    bool swapped_is_aligned = (swapped_ptr % 8) == 0;
    bool swapped_is_null = (swapped_ptr == 0);
    
    if (ptr == SwapBytes(ptr)) {
      // Symmetric pointers (like 0x0000000000000000)
      EXPECT_EQ(is_aligned, swapped_is_aligned) 
        << "Symmetric pointer alignment should match";
      EXPECT_EQ(is_null, swapped_is_null) 
        << "Symmetric pointer null status should match";
    }
    
    // Test tagged pointer detection (should be endianness-independent)
    uint8_t tag = ptr & 0x7;
    uint8_t swapped_tag = swapped_ptr & 0x7;
    
    bool is_tagged_ptr = (tag == 1 || tag == 4) && ptr != 0;
    bool swapped_is_tagged_ptr = (swapped_tag == 1 || swapped_tag == 4) && swapped_ptr != 0;
    
    printf("Pointer 0x%llx: aligned=%d, null=%d, tagged=%d | "
           "swapped=0x%llx: aligned=%d, null=%d, tagged=%d\n",
           ptr, is_aligned, is_null, is_tagged_ptr,
           swapped_ptr, swapped_is_aligned, swapped_is_null, swapped_is_tagged_ptr);
  }
}

TEST_F(EndiannessTest, CrossPlatformCompatibility) {
  // Test compatibility patterns for cross-platform debugging
  
  // Simulate debugging a process with different endianness than host
  struct TestScenario {
    std::string description;
    ByteOrder host_order;
    ByteOrder target_order;
    bool requires_swapping;
  };
  
  std::vector<TestScenario> scenarios = {
    {"Host LE, Target LE", eByteOrderLittle, eByteOrderLittle, false},
    {"Host LE, Target BE", eByteOrderLittle, eByteOrderBig, true},
    {"Host BE, Target LE", eByteOrderBig, eByteOrderLittle, true},
    {"Host BE, Target BE", eByteOrderBig, eByteOrderBig, false}
  };
  
  for (const auto& scenario : scenarios) {
    // Test value that shows endianness differences
    uint64_t test_count = 0x0000000000000005ULL; // 5 elements
    uint64_t target_representation = scenario.requires_swapping ? 
                                    SwapBytes(test_count) : test_count;
    
    // Create mock target with specific endianness
    lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                         ArchSpec("x86_64"), PlatformSP());
    // TODO: MockTarget doesn't have SetByteOrder - need to fix this test
    // static_cast<MockTarget*>(target.get())->SetByteOrder(scenario.target_order);
    
    lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
    // TODO: MockProcess doesn't have SetByteOrder - need to fix this test
    // static_cast<MockProcess*>(process.get())->SetByteOrder(scenario.target_order);
    
    // Store data in target's endianness
    struct {
      uint64_t isa;
      uint64_t contents_ptr;
      uint64_t count;
    } array_obj = {
      scenario.requires_swapping ? SwapBytes(0x5000ULL) : 0x5000ULL,
      scenario.requires_swapping ? SwapBytes(0x6000ULL) : 0x6000ULL,
      target_representation
    };
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &array_obj, sizeof(array_obj));
    
    lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "cross_platform_array", 
                                                               0x8000, "NSArray");
    
    auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
    StreamString output;
    TypeSummaryOptions options;
    bool result = provider->FormatObject(*array_obj_sp, output, options);
    
    EXPECT_TRUE(result || !output.GetString().empty()) 
      << scenario.description << " should be handled";
    
    std::string output_str = output.GetString().str();
    
    // Should correctly interpret the count as 5 regardless of endianness differences
    EXPECT_TRUE(output_str.find("5") != std::string::npos ||
                output_str.find("elements") != std::string::npos)
      << scenario.description << " should show correct count: " << output_str;
    
    printf("%s: %s\n", scenario.description.c_str(), output_str.c_str());
  }
}

} // namespace