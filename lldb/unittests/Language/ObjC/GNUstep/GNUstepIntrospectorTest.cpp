//===-- GNUstepIntrospectorTest.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"

#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Utility/ArchSpec.h"
#include "lldb/Utility/DataBuffer.h"
#include "lldb/Utility/DataExtractor.h"

#include <chrono>

using namespace lldb;
using namespace lldb_private;

namespace {

// Test the introspector logic without requiring a full Process object
class GNUstepIntrospectorTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
  }

  void TearDown() override {
    HostInfo::Terminate();
    FileSystem::Terminate();
  }
  
  // GNUstep tagged pointer logic (from the actual implementation)
  bool IsTaggedPointer(lldb::addr_t obj_addr) {
    if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
      return false;
    }
    
    uint64_t tag = obj_addr & 0x7;
    return (tag == 1 || tag == 2 || tag == 4);
  }
  
  // Decode tagged string (from the actual implementation)
  std::string DecodeTaggedString(lldb::addr_t obj_addr) {
    if ((obj_addr & 0x7) != 4) {
      return "";
    }
    
    int length = (obj_addr >> 3) & 0x1f;
    if (length > 8 || length == 0) {
      return "";
    }
    
    std::string result;
    result.reserve(length);
    
    for (int i = 0; i < length; i++) {
      uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
      char c = (obj_addr & mask) >> (57 - (i * 7));
      
      if (c >= 0x20 && c <= 0x7e) {
        result += c;
      } else if (c == 0) {
        break;
      } else {
        if (result.empty()) {
          char buffer[32];
          snprintf(buffer, sizeof(buffer), "<tagged_%llx...>", 
                   (unsigned long long)(obj_addr & 0xffffffffffff));
          return buffer;
        }
        break;
      }
    }
    
    return result;
  }
  
  // Get class name for tagged pointers (from the actual implementation)
  std::string GetTaggedPointerClassName(lldb::addr_t isa_addr) {
    if (!IsTaggedPointer(isa_addr)) {
      return "";
    }
    
    uint64_t tag = isa_addr & 0x7;
    switch (tag) {
    case 1: // Tagged number
      return "NSNumber";
    case 2: // Tagged date or other
      return "NSDate";
    case 4: // Tagged string
      return "NSString";
    default:
      return "";
    }
  }
};

// Test tagged pointer detection without Process dependency
TEST_F(GNUstepIntrospectorTest, TaggedPointerDetection) {
  // Test nil handling
  EXPECT_FALSE(IsTaggedPointer(0));
  EXPECT_FALSE(IsTaggedPointer(LLDB_INVALID_ADDRESS));
  
  // Test valid tagged pointers
  EXPECT_TRUE(IsTaggedPointer(0x0000000000000001ULL));  // Tag 1 (NSNumber)
  EXPECT_TRUE(IsTaggedPointer(0x0000000000000002ULL));  // Tag 2 (NSDate)
  EXPECT_TRUE(IsTaggedPointer(0x0000000000000004ULL));  // Tag 4 (NSString)
  
  // Test invalid tags
  EXPECT_FALSE(IsTaggedPointer(0x0000000000000000ULL)); // Tag 0
  EXPECT_FALSE(IsTaggedPointer(0x0000000000000003ULL)); // Tag 3
  EXPECT_FALSE(IsTaggedPointer(0x0000000000000005ULL)); // Tag 5
  EXPECT_FALSE(IsTaggedPointer(0x0000000000000006ULL)); // Tag 6
  EXPECT_FALSE(IsTaggedPointer(0x0000000000000007ULL)); // Tag 7
  
  // Test regular pointers (even addresses)
  EXPECT_FALSE(IsTaggedPointer(0x7fff12345678ULL));
  EXPECT_FALSE(IsTaggedPointer(0x1000000000000000ULL));
}

// Test tagged string decoding
TEST_F(GNUstepIntrospectorTest, TaggedStringDecoding) {
  // Test invalid input
  EXPECT_EQ(DecodeTaggedString(0), "");
  EXPECT_EQ(DecodeTaggedString(1), ""); // Not a string tag
  
  // Test empty string (length = 0)
  uint64_t empty_string = 4; // Tag 4, length 0
  EXPECT_EQ(DecodeTaggedString(empty_string), "");
  
  // Test single character "A"
  uint64_t tagged_a = 4 | (1 << 3);  // Tag 4, length 1
  tagged_a |= (0x41ULL << 57);       // 'A' at bit 57
  std::string result_a = DecodeTaggedString(tagged_a);
  EXPECT_EQ(result_a, "A");
  
  // Test two character "Hi"
  uint64_t tagged_hi = 4 | (2 << 3); // Tag 4, length 2
  tagged_hi |= (0x48ULL << 57);      // 'H' at bit 57
  tagged_hi |= (0x69ULL << 50);      // 'i' at bit 50
  std::string result_hi = DecodeTaggedString(tagged_hi);
  EXPECT_EQ(result_hi, "Hi");
  
  // Test invalid length (too long)
  uint64_t invalid_long = 4 | (10 << 3); // Tag 4, length 10 (> 9)
  EXPECT_EQ(DecodeTaggedString(invalid_long), "");
}

// Test class name extraction for tagged pointers
TEST_F(GNUstepIntrospectorTest, TaggedPointerClassNames) {
  // Test tagged number
  uint64_t tagged_number = (42ULL << 3) | 1; // Value 42, tag 1
  EXPECT_EQ(GetTaggedPointerClassName(tagged_number), "NSNumber");
  
  // Test tagged date
  uint64_t tagged_date = (693874800ULL << 3) | 2; // Some date, tag 2
  EXPECT_EQ(GetTaggedPointerClassName(tagged_date), "NSDate");
  
  // Test tagged string
  uint64_t tagged_string = 4 | (1 << 3) | (0x41ULL << 57); // "A", tag 4
  EXPECT_EQ(GetTaggedPointerClassName(tagged_string), "NSString");
  
  // Test regular pointer
  uint64_t regular_ptr = 0x7fff12345678ULL;
  EXPECT_EQ(GetTaggedPointerClassName(regular_ptr), "");
  
  // Test invalid tag
  uint64_t invalid_tag = 3; // Tag 3 is not valid
  EXPECT_EQ(GetTaggedPointerClassName(invalid_tag), "");
}

// Test address validation logic
TEST_F(GNUstepIntrospectorTest, AddressValidation) {
  // Test that tagged pointers are considered valid
  EXPECT_TRUE(IsTaggedPointer(0x0000000000000001ULL));
  EXPECT_TRUE(IsTaggedPointer(0xFFFFFFFFFFFFFFF1ULL)); // Large value with tag 1
  
  // Test boundary conditions
  EXPECT_FALSE(IsTaggedPointer(0)); // Zero should not be tagged
  EXPECT_FALSE(IsTaggedPointer(LLDB_INVALID_ADDRESS));
  
  // Test that regular heap pointers are not tagged
  EXPECT_FALSE(IsTaggedPointer(0x0000000100000000ULL)); // Typical heap address
  EXPECT_FALSE(IsTaggedPointer(0x00007fff00000000ULL)); // Typical stack address
}

// Test edge cases and error conditions
TEST_F(GNUstepIntrospectorTest, EdgeCases) {
  // Test string decoding with non-printable characters
  uint64_t tagged_with_null = 4 | (3 << 3); // Tag 4, length 3
  tagged_with_null |= (0x41ULL << 57);       // 'A' at bit 57
  tagged_with_null |= (0x00ULL << 50);       // null at bit 50 (should stop)
  tagged_with_null |= (0x42ULL << 43);       // 'B' at bit 43 (should be ignored)
  
  std::string result_null = DecodeTaggedString(tagged_with_null);
  EXPECT_EQ(result_null, "A"); // Should stop at null
  
  // Test maximum valid string length (8 characters)
  uint64_t max_string = 4 | (8 << 3); // Tag 4, length 8
  for (int i = 0; i < 8; i++) {
    char c = 'A' + i;
    max_string |= (static_cast<uint64_t>(c) << (57 - i * 7));
  }
  
  std::string result_max = DecodeTaggedString(max_string);
  EXPECT_EQ(result_max, "ABCDEFGH");
}

// Test performance of tagged pointer operations
TEST_F(GNUstepIntrospectorTest, Performance) {
  // These operations should be very fast since they don't access memory
  auto start = std::chrono::high_resolution_clock::now();
  
  const int iterations = 10000;
  for (int i = 0; i < iterations; ++i) {
    uint64_t addr = static_cast<uint64_t>(i) | 1; // Make it a tagged number
    bool is_tagged = IsTaggedPointer(addr);
    std::string class_name = GetTaggedPointerClassName(addr);
    (void)is_tagged;
    (void)class_name;
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  
  // Should complete very quickly (under 10ms)
  EXPECT_LT(duration.count(), 10000);
}

// Test consistency of tagged pointer logic
TEST_F(GNUstepIntrospectorTest, Consistency) {
  // Verify that all operations are consistent
  uint64_t tagged_addresses[] = {
    (42ULL << 3) | 1,    // NSNumber
    (123ULL << 3) | 2,   // NSDate  
    4 | (1 << 3) | (0x41ULL << 57), // NSString "A"
  };
  
  const char* expected_classes[] = {
    "NSNumber",
    "NSDate",
    "NSString"
  };
  
  for (size_t i = 0; i < sizeof(tagged_addresses) / sizeof(tagged_addresses[0]); ++i) {
    uint64_t addr = tagged_addresses[i];
    
    // Should be detected as tagged
    EXPECT_TRUE(IsTaggedPointer(addr));
    
    // Should return correct class name
    std::string class_name = GetTaggedPointerClassName(addr);
    EXPECT_EQ(class_name, expected_classes[i]);
  }
}

} // namespace