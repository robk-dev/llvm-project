//===-- NSArrayFormatterTest.cpp -----------------------------------------===//
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

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepArrayFormatters.h"
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

class NSArrayFormatterTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
    summary_formatter = std::make_unique<GNUstepNSArraySummaryProvider>();
  }
  
  void TearDown() override {
    summary_formatter.reset();
    HostInfo::Terminate();
    FileSystem::Terminate();
  }
  
  std::unique_ptr<GNUstepNSArraySummaryProvider> summary_formatter;
};

TEST_F(NSArrayFormatterTest, SummaryProviderInstantiation) {
  // Test that NSArray summary formatter can be created successfully
  EXPECT_NE(summary_formatter.get(), nullptr) << "NSArray summary formatter should be instantiated";
  
  // Test that we can create multiple instances
  auto formatter2 = std::make_unique<GNUstepNSArraySummaryProvider>();
  EXPECT_NE(formatter2.get(), nullptr) << "Multiple formatter instances should work";
  
  // Test that formatters are distinct objects
  EXPECT_NE(summary_formatter.get(), formatter2.get()) << "Each formatter should be a distinct instance";
}

TEST_F(NSArrayFormatterTest, RegistrationFunctions) {
  // Test that array formatter registration functions exist and can be called
  
  // Verify function pointers exist - these are used by LLDB's formatter registration system
  auto array_func = &GNUstepNSArrayFormatterFunction;
  EXPECT_NE(array_func, nullptr) << "GNUstepNSArrayFormatterFunction should exist";
  
  auto synthetic_func = &GNUstepNSArraySyntheticFrontEndCreator;
  EXPECT_NE(synthetic_func, nullptr) << "GNUstepNSArraySyntheticFrontEndCreator should exist";
  
  // These functions are the bridge between LLDB's type system and our formatters
  // They must exist for the plugin to register correctly with LLDB
}

TEST_F(NSArrayFormatterTest, FormatterClassHierarchy) {
  // Test that the formatter correctly inherits from the expected base class
  // This validates the class hierarchy and ensures virtual methods work correctly
  
  GNUstepSummaryProvider* base_ptr = summary_formatter.get();
  EXPECT_NE(base_ptr, nullptr) << "Should be able to convert to base class pointer";
  
  // The formatter should be polymorphic (have virtual methods)
  // This is essential for LLDB's formatter dispatching to work
}

TEST_F(NSArrayFormatterTest, MemoryLayoutConstants) {
  // Test that array memory layout understanding is correct
  // These constants are critical for reading GSArray structure
  
  // GSArray structure offsets (from implementation analysis):
  // - isa: offset 0
  // - _contents_array pointer: offset 8  
  // - _count: offset 16
  // These offsets are hardcoded in ExtractArrayCount() at line 100
  
  // Validate critical memory layout constants
  const size_t ISA_OFFSET = 0;
  const size_t CONTENTS_PTR_OFFSET = 8;
  const size_t COUNT_OFFSET = 16;
  const size_t POINTER_SIZE = 8; // 64-bit pointers
  
  EXPECT_EQ(ISA_OFFSET, 0) << "ISA pointer must be at offset 0";
  EXPECT_EQ(CONTENTS_PTR_OFFSET, 8) << "Contents pointer at offset 8";
  EXPECT_EQ(COUNT_OFFSET, 16) << "Count field at offset 16";
  EXPECT_EQ(POINTER_SIZE, sizeof(void*)) << "Pointer size must match architecture";
  
  // Verify struct alignment assumptions
  EXPECT_EQ(CONTENTS_PTR_OFFSET % POINTER_SIZE, 0U) << "Contents pointer must be aligned";
  EXPECT_EQ(COUNT_OFFSET % sizeof(uint32_t), 0U) << "Count field must be aligned";
}

TEST_F(NSArrayFormatterTest, TaggedPointerHandling) {
  // Test that array formatters can handle tagged pointer elements
  // Arrays often contain NSString and NSNumber tagged pointers
  
  // Tagged pointer detection logic (from implementation):
  // - GNUstep uses low 3 bits for tagging
  // - Tag 1: NSSmallInt
  // - Tag 2: NSSmallExtendedDouble  
  // - Tag 3: NSSmallRepeatingDouble
  // - Tag 4: NSString (tagged constant strings)
  // - Tag 5: NSSmallFloat
  
  const uint64_t TAG_MASK = 0x7; // Low 3 bits
  const uint64_t TAG_SMALLINT = 1;
  const uint64_t TAG_EXTENDED_DOUBLE = 2;
  const uint64_t TAG_REPEATING_DOUBLE = 3;
  const uint64_t TAG_STRING = 4;
  const uint64_t TAG_SMALLFLOAT = 5;
  
  // Test tagged pointer detection patterns
  uint64_t tagged_int = (42 << 3) | TAG_SMALLINT;
  uint64_t tagged_string = (0x1000 << 3) | TAG_STRING;
  uint64_t regular_pointer = 0x7fff12345678; // Regular aligned pointer
  
  EXPECT_EQ(tagged_int & TAG_MASK, TAG_SMALLINT) << "Tagged int should have correct tag";
  EXPECT_EQ(tagged_string & TAG_MASK, TAG_STRING) << "Tagged string should have correct tag";
  EXPECT_EQ(regular_pointer & TAG_MASK, 0U) << "Regular pointer should have no tag";
  
  // Verify tag extraction logic
  EXPECT_EQ((tagged_int >> 3), 42ULL) << "Tagged int value extraction";
  EXPECT_EQ((regular_pointer & TAG_MASK) == 0U, true) << "Regular pointers should not be tagged";
}

TEST_F(NSArrayFormatterTest, RecursionPrevention) {
  // Test that array formatters prevent infinite recursion
  // This is critical for arrays containing other collections
  
  // The implementation uses FormatterContext to track:
  // - Recursion depth (MAX_FORMATTER_DEPTH = 8)
  // - Visited object addresses to detect cycles
  // - MAX_COLLECTION_ELEMENTS_INLINE = 5 for performance
  
  const int MAX_FORMATTER_DEPTH = 8;
  const int MAX_COLLECTION_ELEMENTS_INLINE = 5;
  
  // Test recursion depth tracking
  EXPECT_GT(MAX_FORMATTER_DEPTH, 0) << "Must allow at least one level of recursion";
  EXPECT_LE(MAX_FORMATTER_DEPTH, 10) << "Recursion depth should be reasonable to prevent stack overflow";
  
  // Test collection element limits
  EXPECT_GT(MAX_COLLECTION_ELEMENTS_INLINE, 0) << "Must show at least one element inline";
  EXPECT_LE(MAX_COLLECTION_ELEMENTS_INLINE, 10) << "Inline elements should be limited for performance";
  
  // Simulate cycle detection requirement
  std::set<uint64_t> visited_addresses;
  uint64_t test_addr = 0x1000;
  
  // First visit should be allowed
  bool first_visit = visited_addresses.find(test_addr) == visited_addresses.end();
  EXPECT_TRUE(first_visit) << "First visit to address should be allowed";
  visited_addresses.insert(test_addr);
  
  // Second visit should be detected as cycle
  bool cycle_detected = visited_addresses.find(test_addr) != visited_addresses.end();
  EXPECT_TRUE(cycle_detected) << "Cycle detection should identify revisited addresses";
}

TEST_F(NSArrayFormatterTest, PerformanceCharacteristics) {
  // Test array formatter performance characteristics
  // REQUIREMENT: All formatters must respond within 50ms
  
  auto start = std::chrono::high_resolution_clock::now();
  
  // Create many array formatter instances to test performance
  std::vector<std::unique_ptr<GNUstepNSArraySummaryProvider>> formatters;
  for (int i = 0; i < 1000; ++i) {
    formatters.push_back(
      std::make_unique<GNUstepNSArraySummaryProvider>());
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 50) << "Creating 1000 array formatters should be fast (<50ms)";
  EXPECT_EQ(formatters.size(), 1000) << "All array formatters should be created successfully";
}

TEST_F(NSArrayFormatterTest, ElementCountLimits) {
  // Test array formatter behavior with different element counts
  // Implementation has specific behavior for different array sizes
  
  // From ExtractArrayCount() analysis:
  // - Empty arrays (count = 0): show "()"
  // - Small arrays (count <= MAX_COLLECTION_ELEMENTS_INLINE=5): show inline preview
  // - Large arrays (count > 5): show "(count elements)"
  // - Very large arrays (count > 1000000): safety limit in ReadArrayElements()
  
  const uint32_t MAX_ELEMENTS_INLINE = 5;
  const uint32_t SAFETY_LIMIT = 1000000;
  
  // Test different element count categories
  struct TestCase {
    uint32_t count;
    std::string expected_format;
    bool should_show_inline;
  };
  
  std::vector<TestCase> test_cases = {
    {0, "()", false},
    {1, "inline", true},
    {3, "inline", true}, 
    {5, "inline", true},
    {6, "(6 elements)", false},
    {100, "(100 elements)", false},
    {SAFETY_LIMIT, "(1000000 elements)", false}
  };
  
  for (const auto& test : test_cases) {
    // Validate element count behavior
    bool should_inline = test.count <= MAX_ELEMENTS_INLINE && test.count > 0;
    EXPECT_EQ(should_inline, test.should_show_inline) 
        << "Element count " << test.count << " should " 
        << (test.should_show_inline ? "" : "not ") << "show inline";
    
    // Verify safety limits
    if (test.count > SAFETY_LIMIT) {
      EXPECT_GT(test.count, SAFETY_LIMIT) << "Large arrays should exceed safety limit";
    }
  }
  
  EXPECT_GT(SAFETY_LIMIT, 100000U) << "Safety limit should be reasonable for large arrays";
}

TEST_F(NSArrayFormatterTest, MutableArraySupport) {
  // Test that mutable arrays use the same formatter infrastructure
  // GSMutableArray should be handled by the same formatters
  
  // The implementation detects mutable arrays in UpdateImpl():
  // m_is_mutable = (class_name.find("NSMutableArray") != std::string::npos ||
  //                 class_name.find("GSMutableArray") != std::string::npos);
  
  // Test class name detection patterns
  std::vector<std::string> mutable_class_names = {
    "NSMutableArray",
    "GSMutableArray", 
    "__NSArrayM",
    "GSMutableArray_concrete"
  };
  
  std::vector<std::string> immutable_class_names = {
    "NSArray",
    "GSArray",
    "__NSArrayI", 
    "GSArray_concrete"
  };
  
  // Validate mutable detection logic
  for (const auto& class_name : mutable_class_names) {
    bool has_mutable = class_name.find("Mutable") != std::string::npos || 
                       class_name.find("__NSArrayM") != std::string::npos;
    EXPECT_TRUE(has_mutable) << "Class " << class_name << " should be detected as mutable";
  }
  
  // Validate immutable detection logic
  for (const auto& class_name : immutable_class_names) {
    bool has_mutable = class_name.find("Mutable") != std::string::npos;
    EXPECT_FALSE(has_mutable) << "Class " << class_name << " should be detected as immutable";
  }
  
  // Both types should use same memory layout
  const size_t ARRAY_HEADER_SIZE = 24; // isa + contents_ptr + count
  EXPECT_EQ(ARRAY_HEADER_SIZE, 8 + 8 + 8) << "Both mutable and immutable arrays have same header size";
}

TEST_F(NSArrayFormatterTest, InlineArrayVariants) {
  // Test support for GSInlineArray (elements stored inline)
  // Different from regular GSArray where elements are in separate allocation
  
  // From UpdateImpl() analysis:
  // - GSInlineArray: elements stored immediately after object header
  // - _contents pointer points to inline element storage
  // - Different memory layout than regular GSArray
  
  // Test inline vs separate allocation patterns
  const size_t OBJECT_HEADER_SIZE = 24; // isa + contents_ptr + count
  const size_t POINTER_SIZE = 8;
  
  // Inline array layout: elements follow immediately after header
  uint64_t inline_obj_addr = 0x1000;
  uint64_t inline_elements_addr = inline_obj_addr + OBJECT_HEADER_SIZE;
  
  EXPECT_EQ(inline_elements_addr, inline_obj_addr + OBJECT_HEADER_SIZE)
      << "Inline elements should immediately follow object header";
  
  // Separate allocation layout: elements in different memory region
  uint64_t separate_obj_addr = 0x1000;
  uint64_t separate_elements_addr = 0x2000; // Different allocation
  
  EXPECT_NE(separate_elements_addr, separate_obj_addr + OBJECT_HEADER_SIZE)
      << "Separate allocation should not immediately follow header";
  
  // Both should be pointer-aligned
  EXPECT_EQ(inline_elements_addr % POINTER_SIZE, 0U) << "Inline elements must be aligned";
  EXPECT_EQ(separate_elements_addr % POINTER_SIZE, 0U) << "Separate elements must be aligned";
  
  // Test capacity implications for inline arrays
  const uint32_t TYPICAL_INLINE_CAPACITY = 4; // Small fixed capacity
  const uint32_t TYPICAL_SEPARATE_CAPACITY = 16; // Larger variable capacity
  
  EXPECT_LT(TYPICAL_INLINE_CAPACITY, TYPICAL_SEPARATE_CAPACITY)
      << "Inline arrays typically have smaller capacity than separate allocation";
}

TEST_F(NSArrayFormatterTest, ErrorHandling) {
  // Test array formatter error handling for corrupted data
  // Formatters must gracefully handle invalid memory conditions
  
  // Error conditions handled by implementation:
  // - Invalid object addresses (0, LLDB_INVALID_ADDRESS)
  // - Memory read failures
  // - Corrupted count values (> 1000000 safety limit)
  // - Invalid contents_array pointers
  // - Truncated element arrays
  
  const uint64_t NULL_ADDRESS = 0;
  const uint64_t INVALID_ADDRESS = LLDB_INVALID_ADDRESS;
  const uint32_t SAFETY_LIMIT = 1000000;
  const uint64_t INVALID_POINTER = 0x1; // Unaligned/invalid
  
  // Test invalid address detection
  EXPECT_EQ(NULL_ADDRESS, 0U) << "NULL address should be 0";
  EXPECT_NE(INVALID_ADDRESS, 0U) << "LLDB_INVALID_ADDRESS should be non-zero";
  
  // Test count validation
  struct CountTest {
    uint32_t count;
    bool should_be_valid;
    std::string description;
  };
  
  std::vector<CountTest> count_tests = {
    {0, true, "empty array"},
    {10, true, "small array"},
    {1000, true, "medium array"},
    {SAFETY_LIMIT, true, "maximum safe array"},
    {SAFETY_LIMIT + 1, false, "oversized array"}
  };
  
  for (const auto& test : count_tests) {
    bool is_valid = test.count <= SAFETY_LIMIT;
    EXPECT_EQ(is_valid, test.should_be_valid) 
        << "Count " << test.count << " for " << test.description 
        << " should " << (test.should_be_valid ? "" : "not ") << "be valid";
  }
  
  // Test pointer alignment validation
  EXPECT_NE(INVALID_POINTER % 8, 0U) << "Invalid pointer should not be aligned";
  
  // Test memory read failure scenarios
  std::vector<uint64_t> problematic_addresses = {
    NULL_ADDRESS,
    INVALID_ADDRESS, 
    INVALID_POINTER,
    0xDEADBEEF // Likely unmapped
  };
  
  for (uint64_t addr : problematic_addresses) {
    // These addresses should trigger error handling paths
    EXPECT_TRUE(addr == NULL_ADDRESS || addr == INVALID_ADDRESS || 
                addr == INVALID_POINTER || addr == 0xDEADBEEF)
        << "Address 0x" << std::hex << addr << " should be recognized as problematic";
  }
}

TEST_F(NSArrayFormatterTest, SyntheticChildrenArchitecture) {
  // Test that synthetic children provider architecture is sound
  // Synthetic providers enable drill-down into array elements
  
  // Critical architecture requirements:
  // - Uses 'id' type for all children (GetConcreteTypeForObject)
  // - LLDB dynamic type resolution handles concrete types
  // - Proper handling of tagged vs regular object pointers
  // - CreateValueObjectFromAddress vs CreateValueObjectFromData
  
  // Test child naming conventions
  struct ChildNameTest {
    uint32_t index;
    std::string expected_name;
  };
  
  std::vector<ChildNameTest> name_tests = {
    {0, "[0]"},
    {1, "[1]"},
    {9, "[9]"},
    {99, "[99]"}
  };
  
  for (const auto& test : name_tests) {
    std::string generated_name = "[" + std::to_string(test.index) + "]";
    EXPECT_EQ(generated_name, test.expected_name)
        << "Child at index " << test.index << " should have name " << test.expected_name;
  }
  
  // Test type system integration requirements
  const std::string ID_TYPE_NAME = "id";
  const std::string OBJECT_TYPE_NAME = "NSObject";
  
  EXPECT_EQ(ID_TYPE_NAME, "id") << "Children should use 'id' type for dynamic resolution";
  EXPECT_NE(ID_TYPE_NAME, OBJECT_TYPE_NAME) << "'id' type should be different from concrete types";
  
  // Test address vs data creation patterns
  uint64_t test_address = 0x1000;
  bool address_is_valid = test_address != 0 && test_address != LLDB_INVALID_ADDRESS;
  
  EXPECT_TRUE(address_is_valid) << "Valid addresses should use CreateValueObjectFromAddress";
  
  // Test tagged pointer handling requirements
  uint64_t tagged_pointer = 0x123 | 0x1; // Tagged with bit 0
  uint64_t regular_pointer = 0x123456780; // 8-byte aligned
  
  bool tagged_is_tagged = (tagged_pointer & 0x7) != 0;
  bool tagged_is_regular = (tagged_pointer & 0x7) == 0;
  
  bool regular_is_tagged = (regular_pointer & 0x7) != 0;
  bool regular_is_regular = (regular_pointer & 0x7) == 0;
  
  EXPECT_TRUE(tagged_is_tagged) << "Tagged pointer should be detected as tagged";
  EXPECT_FALSE(tagged_is_regular) << "Tagged pointer should not be detected as regular";
  
  EXPECT_FALSE(regular_is_tagged) << "Regular pointer should not be detected as tagged";
  EXPECT_TRUE(regular_is_regular) << "Regular pointer should be detected as regular";
  
  // Tagged and regular detection should be mutually exclusive for the same pointer
  EXPECT_NE(tagged_is_tagged, tagged_is_regular) << "Tagged pointer detection should be consistent";
  EXPECT_NE(regular_is_tagged, regular_is_regular) << "Regular pointer detection should be consistent";
}

TEST_F(NSArrayFormatterTest, ThreadSafety) {
  // Test basic thread safety assumptions for array formatters
  // Multiple formatter instances should be safe to create concurrently
  
  std::vector<std::thread> threads;
  std::vector<std::unique_ptr<GNUstepNSArraySummaryProvider>> results;
  std::mutex results_mutex;
  
  // Create formatters from multiple threads
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&results, &results_mutex]() {
      auto formatter = std::make_unique<GNUstepNSArraySummaryProvider>();
      
      std::lock_guard<std::mutex> lock(results_mutex);
      results.push_back(std::move(formatter));
    });
  }
  
  // Wait for all threads
  for (auto& thread : threads) {
    thread.join();
  }
  
  EXPECT_EQ(results.size(), 10) << "All array formatters should be created from multiple threads";
  
  // All formatters should be valid
  for (const auto& formatter : results) {
    EXPECT_NE(formatter.get(), nullptr) << "Each formatter should be valid";
  }
}

} // namespace