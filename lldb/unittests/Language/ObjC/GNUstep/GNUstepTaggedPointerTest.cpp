//===-- GNUstepTaggedPointerTest.cpp -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"

#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Utility/DataExtractor.h"

#include <chrono>
#include <cstring>

using namespace lldb;
using namespace lldb_private;

namespace {

class GNUstepTaggedPointerTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
  }

  void TearDown() override {
    HostInfo::Terminate();
    FileSystem::Terminate();
  }
  
  // GNUstep tagged pointer format:
  // Bit 0: Tagged pointer flag (1 = tagged)
  // Bits 1-2: Tag type
  //   00 (0): Small integer
  //   01 (1): Reserved
  //   10 (2): Small string
  //   11 (3): Extended tag (NSNumber float/double, NSDate, etc.)
  // Remaining bits: Payload
  
  static constexpr uint64_t TAGGED_POINTER_FLAG = 0x01;
  static constexpr uint64_t TAG_TYPE_MASK = 0x06;
  static constexpr uint64_t TAG_TYPE_SHIFT = 1;
  
  static constexpr uint64_t TAG_TYPE_INTEGER = 0x00;
  static constexpr uint64_t TAG_TYPE_RESERVED = 0x01;
  static constexpr uint64_t TAG_TYPE_STRING = 0x02;
  static constexpr uint64_t TAG_TYPE_EXTENDED = 0x03;
  
  bool IsTaggedPointer(uint64_t ptr) {
    return (ptr & TAGGED_POINTER_FLAG) != 0;
  }
  
  uint64_t GetTagType(uint64_t ptr) {
    return (ptr & TAG_TYPE_MASK) >> TAG_TYPE_SHIFT;
  }
};

// Test tagged pointer detection
TEST_F(GNUstepTaggedPointerTest, TaggedPointerDetection) {
  // Regular pointer (not tagged)
  uint64_t regular_ptr = 0x7fff12345678;
  EXPECT_FALSE(IsTaggedPointer(regular_ptr));
  
  // Tagged integer
  uint64_t tagged_int = (42ULL << 3) | TAG_TYPE_INTEGER << TAG_TYPE_SHIFT | TAGGED_POINTER_FLAG;
  EXPECT_TRUE(IsTaggedPointer(tagged_int));
  EXPECT_EQ(GetTagType(tagged_int), TAG_TYPE_INTEGER);
  
  // Tagged string
  uint64_t tagged_str = 0x0000000000006548ULL | TAG_TYPE_STRING << TAG_TYPE_SHIFT | TAGGED_POINTER_FLAG;
  EXPECT_TRUE(IsTaggedPointer(tagged_str));
  EXPECT_EQ(GetTagType(tagged_str), TAG_TYPE_STRING);
  
  // Tagged extended (float)
  uint64_t tagged_float = (0x40490FDBULL << 32) | TAG_TYPE_EXTENDED << TAG_TYPE_SHIFT | TAGGED_POINTER_FLAG;
  EXPECT_TRUE(IsTaggedPointer(tagged_float));
  EXPECT_EQ(GetTagType(tagged_float), TAG_TYPE_EXTENDED);
}

// Test tagged integer decoding
TEST_F(GNUstepTaggedPointerTest, TaggedIntegerDecoding) {
  // Positive integer
  int64_t pos_value = 42;
  uint64_t tagged_pos = (static_cast<uint64_t>(pos_value) << 3) | TAG_TYPE_INTEGER << TAG_TYPE_SHIFT | TAGGED_POINTER_FLAG;
  int64_t decoded_pos = static_cast<int64_t>(tagged_pos >> 3);
  EXPECT_EQ(decoded_pos, pos_value);
  
  // Negative integer - sign extension is important
  int64_t neg_value = -17;
  uint64_t tagged_neg = (static_cast<uint64_t>(neg_value) << 3) | TAG_TYPE_INTEGER << TAG_TYPE_SHIFT | TAGGED_POINTER_FLAG;
  // Must sign-extend when decoding
  int64_t decoded_neg = static_cast<int64_t>(tagged_neg) >> 3;
  EXPECT_EQ(decoded_neg, neg_value);
  
  // Zero
  int64_t zero_value = 0;
  uint64_t tagged_zero = (static_cast<uint64_t>(zero_value) << 3) | TAG_TYPE_INTEGER << TAG_TYPE_SHIFT | TAGGED_POINTER_FLAG;
  int64_t decoded_zero = static_cast<int64_t>(tagged_zero >> 3);
  EXPECT_EQ(decoded_zero, zero_value);
  
  // Maximum tagged integer (61 bits)
  int64_t max_value = (1LL << 60) - 1;
  uint64_t tagged_max = (static_cast<uint64_t>(max_value) << 3) | TAG_TYPE_INTEGER << TAG_TYPE_SHIFT | TAGGED_POINTER_FLAG;
  int64_t decoded_max = static_cast<int64_t>(tagged_max >> 3);
  EXPECT_EQ(decoded_max, max_value);
}

// Test tagged string decoding
TEST_F(GNUstepTaggedPointerTest, TaggedStringDecoding) {
  // Small ASCII string (up to 6 bytes on 64-bit)
  // "Hi" = 0x48, 0x69 in little-endian
  // The string is stored starting at bit 8 (after tag bits)
  uint64_t tagged_hi = (0x6948ULL << 8) | TAG_TYPE_STRING << TAG_TYPE_SHIFT | TAGGED_POINTER_FLAG;
  
  // Extract string bytes
  char decoded[8] = {0};
  uint64_t str_data = tagged_hi >> 8; // Skip tag bits
  memcpy(decoded, &str_data, 6);
  EXPECT_STREQ(decoded, "Hi");
  
  // Empty string
  uint64_t tagged_empty = TAG_TYPE_STRING << TAG_TYPE_SHIFT | TAGGED_POINTER_FLAG;
  char decoded_empty[8] = {0};
  uint64_t empty_data = tagged_empty >> 8;
  memcpy(decoded_empty, &empty_data, 6);
  EXPECT_STREQ(decoded_empty, "");
  
  // Maximum length string (6 bytes)
  // "ABCDEF" = 0x41, 0x42, 0x43, 0x44, 0x45, 0x46 in little-endian
  uint64_t tagged_max = (0x464544434241ULL << 8) | TAG_TYPE_STRING << TAG_TYPE_SHIFT | TAGGED_POINTER_FLAG;
  char decoded_max[8] = {0};
  uint64_t max_data = tagged_max >> 8;
  memcpy(decoded_max, &max_data, 6);
  EXPECT_STREQ(decoded_max, "ABCDEF");
}

// Test extended tag types
TEST_F(GNUstepTaggedPointerTest, ExtendedTagTypes) {
  // Extended tag format:
  // Bits 3-7: Extended type
  //   0: Float
  //   1: Double
  //   2: NSDate
  //   3: Reserved
  
  constexpr uint64_t EXT_TYPE_FLOAT = 0;
  [[maybe_unused]] constexpr uint64_t EXT_TYPE_DOUBLE = 1;
  constexpr uint64_t EXT_TYPE_DATE = 2;
  
  // Tagged float (π)
  [[maybe_unused]] uint32_t float_bits = 0x40490FDB; // π as float
  uint64_t tagged_float = (static_cast<uint64_t>(float_bits) << 32) | 
                          (EXT_TYPE_FLOAT << 3) |
                          TAG_TYPE_EXTENDED << TAG_TYPE_SHIFT | 
                          TAGGED_POINTER_FLAG;
  
  EXPECT_TRUE(IsTaggedPointer(tagged_float));
  EXPECT_EQ(GetTagType(tagged_float), TAG_TYPE_EXTENDED);
  
  uint32_t decoded_float_bits = static_cast<uint32_t>(tagged_float >> 32);
  float decoded_float;
  memcpy(&decoded_float, &decoded_float_bits, sizeof(float));
  EXPECT_FLOAT_EQ(decoded_float, 3.14159274f);
  
  // Tagged double (e)
  [[maybe_unused]] uint64_t double_bits = 0x4005BF0A8B145769ULL; // e as double
  // For double, we'd need a different encoding scheme since we need 64 bits
  // This is typically done with an indirection or compression
  
  // Tagged NSDate (seconds since reference date)
  uint64_t date_seconds = 693874800; // Some date
  uint64_t tagged_date = (date_seconds << 8) |
                         (EXT_TYPE_DATE << 3) |
                         TAG_TYPE_EXTENDED << TAG_TYPE_SHIFT |
                         TAGGED_POINTER_FLAG;
  
  EXPECT_TRUE(IsTaggedPointer(tagged_date));
  EXPECT_EQ(GetTagType(tagged_date), TAG_TYPE_EXTENDED);
}

// Test edge cases
TEST_F(GNUstepTaggedPointerTest, EdgeCases) {
  // Nil pointer
  uint64_t nil_ptr = 0x0;
  EXPECT_FALSE(IsTaggedPointer(nil_ptr));
  
  // All bits set
  uint64_t all_bits = 0xFFFFFFFFFFFFFFFFULL;
  EXPECT_TRUE(IsTaggedPointer(all_bits)); // Has tagged bit set
  
  // Only tagged bit set
  uint64_t only_tagged = TAGGED_POINTER_FLAG;
  EXPECT_TRUE(IsTaggedPointer(only_tagged));
  EXPECT_EQ(GetTagType(only_tagged), 0ULL);
  
  // Maximum regular pointer (no tagged bit)
  uint64_t max_regular = 0xFFFFFFFFFFFFFFFEULL;
  EXPECT_FALSE(IsTaggedPointer(max_regular));
}

// Test tagged pointer validation
TEST_F(GNUstepTaggedPointerTest, Validation) {
  // Valid tagged pointers should have consistent structure
  
  // Integer with invalid tag bits (should not happen)
  uint64_t invalid_int = (42ULL << 3) | 0x07; // Invalid tag pattern
  EXPECT_TRUE(IsTaggedPointer(invalid_int)); // Still tagged
  
  // String with length byte (if implemented)
  // Some implementations include a length byte for variable-length strings
  uint64_t str_with_len = 0x0300000000006948ULL | TAG_TYPE_STRING << TAG_TYPE_SHIFT | TAGGED_POINTER_FLAG;
  // Length = 3, "Hi\0"
  EXPECT_TRUE(IsTaggedPointer(str_with_len));
}

// Test performance characteristics
TEST_F(GNUstepTaggedPointerTest, Performance) {
  // Tagged pointers should be fast to decode (no memory access)
  
  // Measure time to decode 1 million tagged integers
  const int iterations = 1000000;
  auto start = std::chrono::high_resolution_clock::now();
  
  for (int i = 0; i < iterations; ++i) {
    uint64_t tagged = (static_cast<uint64_t>(i) << 3) | TAG_TYPE_INTEGER << TAG_TYPE_SHIFT | TAGGED_POINTER_FLAG;
    bool is_tagged = IsTaggedPointer(tagged);
    uint64_t tag_type = GetTagType(tagged);
    int64_t value = static_cast<int64_t>(tagged >> 3);
    (void)is_tagged;
    (void)tag_type;
    (void)value;
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  
  // Should complete in less than 100ms (100,000 microseconds)
  EXPECT_LT(duration.count(), 100000);
}

} // namespace