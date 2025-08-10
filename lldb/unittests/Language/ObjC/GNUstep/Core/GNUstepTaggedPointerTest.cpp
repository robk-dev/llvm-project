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
  
  // GNUstep tagged pointer format (based on actual implementation):
  // Lower 3 bits are tag:
  //   1: Small integers (NSNumber)
  //   2: Dates or other small objects
  //   4: Tiny strings (up to 8 characters)
  // Other values (0, 3, 5, 6, 7) are not valid tagged pointers
  
  static constexpr uint64_t TAG_MASK = 0x07ULL;
  static constexpr uint64_t TAG_NUMBER = 1ULL;
  static constexpr uint64_t TAG_DATE = 2ULL;
  static constexpr uint64_t TAG_STRING = 4ULL;
  
  // Compile-time validation
  static_assert((TAG_NUMBER & TAG_MASK) == TAG_NUMBER, "TAG_NUMBER must fit in mask");
  static_assert((TAG_DATE & TAG_MASK) == TAG_DATE, "TAG_DATE must fit in mask");
  static_assert((TAG_STRING & TAG_MASK) == TAG_STRING, "TAG_STRING must fit in mask");
  
  static constexpr bool IsTaggedPointer(uint64_t ptr) noexcept {
    if (ptr == 0 || ptr == LLDB_INVALID_ADDRESS) {
      return false;
    }
    const uint64_t tag = ptr & TAG_MASK;
    return (tag == TAG_NUMBER || tag == TAG_DATE || tag == TAG_STRING);
  }
  
  static constexpr uint64_t GetTag(uint64_t ptr) noexcept {
    return ptr & TAG_MASK;
  }
  
  // Decode tagged string based on GNUstep implementation
  std::string DecodeTaggedString(uint64_t obj_addr) {
    if ((obj_addr & TAG_MASK) != TAG_STRING) {
      return "";
    }
    
    // Extract length from bits 3-7
    int length = (obj_addr >> 3) & 0x1f;
    if (length > 8 || length == 0) {
      return "";
    }
    
    // Decode characters - each uses 7 bits, stored from bit 57 downward
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
        // Non-printable character
        if (result.empty()) {
          return "<invalid_tagged_string>";
        }
        break;
      }
    }
    
    return result;
  }
};

// Test tagged pointer detection
TEST_F(GNUstepTaggedPointerTest, TaggedPointerDetection) {
  // Regular pointer (not tagged)
  uint64_t regular_ptr = 0x7fff12345678;
  EXPECT_FALSE(IsTaggedPointer(regular_ptr));
  
  // Nil pointer
  EXPECT_FALSE(IsTaggedPointer(0));
  EXPECT_FALSE(IsTaggedPointer(LLDB_INVALID_ADDRESS));
  
  // Tagged number
  uint64_t tagged_num = (42ULL << 3) | TAG_NUMBER;
  EXPECT_TRUE(IsTaggedPointer(tagged_num));
  EXPECT_EQ(GetTag(tagged_num), TAG_NUMBER);
  
  // Tagged string
  uint64_t tagged_str = 0x0000000000006548ULL | TAG_STRING;
  EXPECT_TRUE(IsTaggedPointer(tagged_str));
  EXPECT_EQ(GetTag(tagged_str), TAG_STRING);
  
  // Tagged date
  uint64_t tagged_date = (693874800ULL << 3) | TAG_DATE;
  EXPECT_TRUE(IsTaggedPointer(tagged_date));
  EXPECT_EQ(GetTag(tagged_date), TAG_DATE);
  
  // Invalid tags should not be recognized as tagged pointers
  uint64_t invalid_tag_0 = 0x1234567800000000ULL | 0;
  uint64_t invalid_tag_3 = 0x1234567800000000ULL | 3;
  uint64_t invalid_tag_5 = 0x1234567800000000ULL | 5;
  
  EXPECT_FALSE(IsTaggedPointer(invalid_tag_0));
  EXPECT_FALSE(IsTaggedPointer(invalid_tag_3));
  EXPECT_FALSE(IsTaggedPointer(invalid_tag_5));
}

// Test tagged integer decoding
TEST_F(GNUstepTaggedPointerTest, TaggedIntegerDecoding) {
  // Positive integer
  int64_t pos_value = 42;
  uint64_t tagged_pos = (static_cast<uint64_t>(pos_value) << 3) | TAG_NUMBER;
  int64_t decoded_pos = static_cast<int64_t>(tagged_pos) >> 3;
  EXPECT_EQ(decoded_pos, pos_value);
  EXPECT_TRUE(IsTaggedPointer(tagged_pos));
  EXPECT_EQ(GetTag(tagged_pos), TAG_NUMBER);
  
  // Negative integer - sign extension is important
  int64_t neg_value = -17;
  uint64_t tagged_neg = (static_cast<uint64_t>(neg_value) << 3) | TAG_NUMBER;
  // Must sign-extend when decoding
  int64_t decoded_neg = static_cast<int64_t>(tagged_neg) >> 3;
  EXPECT_EQ(decoded_neg, neg_value);
  EXPECT_TRUE(IsTaggedPointer(tagged_neg));
  
  // Zero
  int64_t zero_value = 0;
  uint64_t tagged_zero = (static_cast<uint64_t>(zero_value) << 3) | TAG_NUMBER;
  int64_t decoded_zero = static_cast<int64_t>(tagged_zero) >> 3;
  EXPECT_EQ(decoded_zero, zero_value);
  EXPECT_TRUE(IsTaggedPointer(tagged_zero));
  
  // Maximum tagged integer (61 bits)
  int64_t max_value = (1LL << 60) - 1;
  uint64_t tagged_max = (static_cast<uint64_t>(max_value) << 3) | TAG_NUMBER;
  int64_t decoded_max = static_cast<int64_t>(tagged_max) >> 3;
  EXPECT_EQ(decoded_max, max_value);
  EXPECT_TRUE(IsTaggedPointer(tagged_max));
}

// Test tagged string decoding
TEST_F(GNUstepTaggedPointerTest, TaggedStringDecoding) {
  // Test basic string decoding using the GNUstep format
  // Create a tagged string "Hi" (2 characters)
  // Length = 2 (stored in bits 3-7), characters stored from bit 57 downward
  uint64_t tagged_hi = TAG_STRING | (2 << 3);  // Length = 2
  // Encode 'H' (0x48) at bit 57, 'i' (0x69) at bit 50
  tagged_hi |= (0x48ULL << 57) | (0x69ULL << 50);
  
  std::string decoded_hi = DecodeTaggedString(tagged_hi);
  EXPECT_EQ(decoded_hi, "Hi");
  EXPECT_TRUE(IsTaggedPointer(tagged_hi));
  EXPECT_EQ(GetTag(tagged_hi), TAG_STRING);
  
  // Test single character string "A"
  uint64_t tagged_a = TAG_STRING | (1 << 3);  // Length = 1
  tagged_a |= (0x41ULL << 57);  // 'A' at bit 57
  
  std::string decoded_a = DecodeTaggedString(tagged_a);
  EXPECT_EQ(decoded_a, "A");
  
  // Test empty string (length = 0)
  uint64_t tagged_empty = TAG_STRING | (0 << 3);
  std::string decoded_empty = DecodeTaggedString(tagged_empty);
  EXPECT_EQ(decoded_empty, "");
  
  // Test invalid tag (not a string)
  uint64_t not_string = TAG_NUMBER | (2 << 3);
  std::string decoded_not_string = DecodeTaggedString(not_string);
  EXPECT_EQ(decoded_not_string, "");
  
  // Test maximum length string (8 characters) - "ABCDEFGH"
  uint64_t tagged_long = TAG_STRING | (8 << 3);
  for (int i = 0; i < 8; i++) {
    char c = 'A' + i;
    tagged_long |= (static_cast<uint64_t>(c) << (57 - i * 7));
  }
  
  std::string decoded_long = DecodeTaggedString(tagged_long);
  EXPECT_EQ(decoded_long, "ABCDEFGH");
}

// Test date tag type
TEST_F(GNUstepTaggedPointerTest, TaggedDates) {
  // Test tagged date (tag = 2)
  // Store date as seconds since reference date
  uint64_t date_seconds = 693874800; // Some date
  uint64_t tagged_date = (date_seconds << 3) | TAG_DATE;
  
  EXPECT_TRUE(IsTaggedPointer(tagged_date));
  EXPECT_EQ(GetTag(tagged_date), TAG_DATE);
  
  // Decode the date value
  uint64_t decoded_seconds = tagged_date >> 3;
  EXPECT_EQ(decoded_seconds, date_seconds);
  
  // Test zero date
  uint64_t tagged_zero_date = (0ULL << 3) | TAG_DATE;
  EXPECT_TRUE(IsTaggedPointer(tagged_zero_date));
  EXPECT_EQ(GetTag(tagged_zero_date), TAG_DATE);
  
  uint64_t decoded_zero = tagged_zero_date >> 3;
  EXPECT_EQ(decoded_zero, 0ULL);
}

// Test edge cases
TEST_F(GNUstepTaggedPointerTest, EdgeCases) {
  // Nil pointer
  uint64_t nil_ptr = 0x0;
  EXPECT_FALSE(IsTaggedPointer(nil_ptr));
  
  // All bits set with valid tag
  uint64_t all_bits_valid_tag = 0xFFFFFFFFFFFFFFFFULL & ~TAG_MASK;
  all_bits_valid_tag |= TAG_NUMBER;
  EXPECT_TRUE(IsTaggedPointer(all_bits_valid_tag));
  
  // All bits set with invalid tag
  uint64_t all_bits_invalid_tag = 0xFFFFFFFFFFFFFFFFULL & ~TAG_MASK;
  all_bits_invalid_tag |= 3; // Invalid tag
  EXPECT_FALSE(IsTaggedPointer(all_bits_invalid_tag));
  
  // Maximum regular pointer (even number, no valid tag)
  uint64_t max_regular = 0xFFFFFFFFFFFFFFFEULL;
  EXPECT_FALSE(IsTaggedPointer(max_regular));
  
  // Test boundary values for string length  
  uint64_t tagged_max_len = TAG_STRING | (10 << 3);  // Length > 9 (invalid)
  std::string decoded_invalid = DecodeTaggedString(tagged_max_len);
  EXPECT_EQ(decoded_invalid, "");  // Should return empty for invalid length
  
  // Test length = 9 (also invalid, max is 8)
  uint64_t tagged_nine_len = TAG_STRING | (9 << 3);
  std::string decoded_nine = DecodeTaggedString(tagged_nine_len);
  EXPECT_EQ(decoded_nine, "");  // Should return empty for length 9
}

// Test tagged pointer validation
TEST_F(GNUstepTaggedPointerTest, Validation) {
  // Valid tagged pointers should have consistent structure
  
  // Integer with invalid tag bits (should be rejected)
  uint64_t invalid_tag = (42ULL << 3) | 0x07; // Invalid tag pattern
  EXPECT_FALSE(IsTaggedPointer(invalid_tag)); // Should not be tagged
  
  // Valid string with proper encoding
  uint64_t valid_string = TAG_STRING | (3 << 3);  // Length = 3
  valid_string |= (0x48ULL << 57) | (0x69ULL << 50) | (0x21ULL << 43); // "Hi!"
  EXPECT_TRUE(IsTaggedPointer(valid_string));
  std::string decoded = DecodeTaggedString(valid_string);
  EXPECT_EQ(decoded, "Hi!");
}

// Test performance characteristics
TEST_F(GNUstepTaggedPointerTest, Performance) {
  // Tagged pointers should be fast to decode (no memory access)
  
  // Measure time to decode 100,000 tagged integers (reduced for CI)
  const int iterations = 100000;
  auto start = std::chrono::high_resolution_clock::now();
  
  for (int i = 0; i < iterations; ++i) {
    uint64_t tagged = (static_cast<uint64_t>(i & 0xFFFFFF) << 3) | TAG_NUMBER;
    bool is_tagged = IsTaggedPointer(tagged);
    uint64_t tag = GetTag(tagged);
    int64_t value = static_cast<int64_t>(tagged) >> 3;
    (void)is_tagged;
    (void)tag;
    (void)value;
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  
  // Should complete in less than 50ms (50,000 microseconds)
  EXPECT_LT(duration.count(), 50000);
}

} // namespace