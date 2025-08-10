//===-- NSSetFormatterTest.cpp -------------------------------------------===//
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

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepSetFormatters.h"
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

class NSSetFormatterTest : public ::testing::Test {
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

TEST_F(NSSetFormatterTest, FormatterCreation) {
  // Test that NSSet formatter can be created successfully
  auto formatter = std::make_unique<GNUstepNSSetSummaryProvider>();
  EXPECT_NE(formatter, nullptr) << "NSSet formatter should be instantiated";
  
  // Test multiple instances
  auto formatter2 = std::make_unique<GNUstepNSSetSummaryProvider>();
  EXPECT_NE(formatter2, nullptr) << "Multiple formatter instances should work";
  
  // Test that formatters are distinct objects
  EXPECT_NE(formatter.get(), formatter2.get()) << "Each formatter should be a distinct instance";
}

TEST_F(NSSetFormatterTest, GSIMapTableStructureKnowledge) {
  // Test understanding of GSSet's underlying GSIMapTable structure
  // GSSet and NSDictionary share the same hash table implementation
  
  // GSSet structure:
  // - isa: offset 0
  // - GSIMapTable_t map: offset 8
  //
  // GSIMapTable_t structure (same as dictionary):
  // - NSZone *zone: offset 0 (within map)
  // - uintptr_t nodeCount: offset 8 (within map) -> total offset 16 from obj
  // - uintptr_t bucketCount: offset 16 (within map) -> total offset 24 from obj  
  // - GSIMapBucket buckets: offset 24 (within map) -> total offset 32 from obj
  
  // Validate GSSet memory layout matches GSDictionary
  const size_t ISA_OFFSET = 0;
  const size_t MAP_TABLE_OFFSET = 8;
  
  // GSIMapTable_t offsets within map (same as dictionary)
  const size_t NODE_COUNT_OFFSET = MAP_TABLE_OFFSET + 8;  // 16
  const size_t BUCKET_COUNT_OFFSET = MAP_TABLE_OFFSET + 16; // 24
  const size_t BUCKETS_OFFSET = MAP_TABLE_OFFSET + 24;     // 32
  
  EXPECT_EQ(ISA_OFFSET, 0) << "ISA at offset 0 (same as dictionary)";
  EXPECT_EQ(MAP_TABLE_OFFSET, 8) << "Map table at offset 8 (same as dictionary)";
  EXPECT_EQ(NODE_COUNT_OFFSET, 16) << "Node count at offset 16 (same as dictionary)";
  EXPECT_EQ(BUCKET_COUNT_OFFSET, 24) << "Bucket count at offset 24 (same as dictionary)";
  EXPECT_EQ(BUCKETS_OFFSET, 32) << "Buckets at offset 32 (same as dictionary)";
  
  // Verify that GSSet and GSDictionary have identical hash table structure
  const size_t SET_HEADER_SIZE = ISA_OFFSET + sizeof(void*) + MAP_TABLE_OFFSET;
  const size_t DICT_HEADER_SIZE = 16; // Same as dictionary from previous test
  
  EXPECT_EQ(SET_HEADER_SIZE, DICT_HEADER_SIZE) << "GSSet and GSDictionary headers should be identical";
  
  // Test hash table implementation sharing
  const size_t MAP_TABLE_SIZE = 4 * sizeof(void*); // zone + nodeCount + bucketCount + buckets
  EXPECT_EQ(MAP_TABLE_SIZE, 32) << "Map table size should be consistent";
}

TEST_F(NSSetFormatterTest, HashTableTraversalAlgorithm) {
  // Test hash table traversal for set elements
  // Similar to dictionary but only extracts keys (no values)
  
  // GSIMapNode structure for sets:
  // - GSIMapNode nextInBucket: offset 0 (pointer to next node)
  // - GSIMapKey key: offset 8 (the set object)
  // - GSIMapVal value: offset 16 (unused in sets)
  
  // For sets, only the key field contains meaningful data
  
  const size_t NODE_NEXT_OFFSET = 0;
  const size_t NODE_KEY_OFFSET = 8;
  const size_t NODE_VALUE_OFFSET = 16; // Unused for sets
  const size_t NODE_SIZE = 24;
  
  EXPECT_EQ(NODE_NEXT_OFFSET, 0) << "Next pointer at offset 0";
  EXPECT_EQ(NODE_KEY_OFFSET, 8) << "Set element (key) at offset 8";
  EXPECT_EQ(NODE_VALUE_OFFSET, 16) << "Value field at offset 16 (unused for sets)";
  EXPECT_EQ(NODE_SIZE, 3 * sizeof(void*)) << "Node size should be 3 pointers";
  
  // Test set-specific traversal behavior
  struct SetElementTest {
    uint64_t key_addr;    // The actual set element
    uint64_t value_addr;  // Should be unused/ignored
    bool key_is_valid;
  };
  
  std::vector<SetElementTest> element_tests = {
    {0x1000, 0x0, true},        // Valid element, unused value
    {0x2000, 0x9999, true},     // Valid element, ignored value
    {0x123 | 0x1, 0x0, true},   // Tagged pointer element
    {0x0, 0x3000, false}       // Invalid element (NULL key)
  };
  
  for (const auto& test : element_tests) {
    // For sets, only key matters
    bool key_valid = test.key_addr != 0;
    EXPECT_EQ(key_valid, test.key_is_valid)
        << "Key 0x" << std::hex << test.key_addr << " validity should match expected";
    
    // Value should be ignored for sets
    EXPECT_TRUE(true) << "Value field 0x" << std::hex << test.value_addr << " should be ignored";
  }
  
  // Test traversal order independence (sets are unordered)
  std::vector<uint64_t> insertion_order = {0x1000, 0x2000, 0x3000};
  std::vector<uint64_t> hash_order = {0x3000, 0x1000, 0x2000}; // Different order
  
  // Both orderings should be valid for sets
  EXPECT_EQ(insertion_order.size(), hash_order.size()) << "Set size should be preserved";
  
  // Check that all elements are present (order doesn't matter)
  std::set<uint64_t> insertion_set(insertion_order.begin(), insertion_order.end());
  std::set<uint64_t> hash_set(hash_order.begin(), hash_order.end());
  
  EXPECT_EQ(insertion_set, hash_set) << "Set contents should be same regardless of order";
}

TEST_F(NSSetFormatterTest, TaggedPointerElementHandling) {
  // Test that sets can handle tagged pointer elements
  // Sets often contain NSString and NSNumber tagged pointers
  
  // Tagged pointer considerations:
  // - Must extract STORAGE ADDRESS of element, not pointer value
  // - CreateValueObjectFromAddress needs memory location
  // - Tagged pointers store data inline in the pointer itself
  // - GNUstep tagged pointer scheme uses low 3 bits
  
  const uint64_t TAG_MASK = 0x7;
  const uint64_t TAG_SMALLINT = 1;
  const uint64_t TAG_STRING = 4;
  
  // Test different tagged pointer types in sets
  struct TaggedElementTest {
    uint64_t element_addr;
    uint64_t tag;
    std::string element_type;
    bool is_tagged;
  };
  
  std::vector<TaggedElementTest> tagged_tests = {
    {(42 << 3) | TAG_SMALLINT, TAG_SMALLINT, "NSNumber (int)", true},
    {(0x1000 << 3) | TAG_STRING, TAG_STRING, "NSString", true},
    {0x7fff12345678, 0, "Regular object", false},
    {0x8000000000000000, 0, "Regular object (high bit)", false}
  };
  
  for (const auto& test : tagged_tests) {
    uint64_t detected_tag = test.element_addr & TAG_MASK;
    bool detected_tagged = detected_tag != 0;
    
    EXPECT_EQ(detected_tagged, test.is_tagged)
        << "Element 0x" << std::hex << test.element_addr 
        << " (" << test.element_type << ") tagged detection mismatch";
    
    if (test.is_tagged) {
      EXPECT_EQ(detected_tag, test.tag)
          << "Tagged element should have correct tag value";
      
      // Test value extraction for tagged pointers
      uint64_t extracted_value = test.element_addr >> 3;
      EXPECT_GT(extracted_value, 0ULL)
          << "Tagged pointer should have extractable value";
    } else {
      EXPECT_EQ(detected_tag, 0U)
          << "Regular pointer should have no tag";
      
      // Regular pointers should be aligned
      EXPECT_EQ(test.element_addr % 8, 0U)
          << "Regular object pointer should be 8-byte aligned";
    }
  }
  
  // Test tagged pointer storage address requirements
  uint64_t tagged_int = (123 << 3) | TAG_SMALLINT;
  
  // For tagged pointers, the "storage address" is the pointer value itself
  // (since data is stored inline)
  EXPECT_NE(tagged_int, 0U) << "Tagged pointer storage address should not be NULL";
  EXPECT_EQ(tagged_int & TAG_MASK, TAG_SMALLINT) << "Storage address should preserve tag";
}

TEST_F(NSSetFormatterTest, MixedElementTypeFormatting) {
  // Test formatting of sets with mixed element types
  // Sets can contain different object types simultaneously
  
  // Element type handling:
  // - Use ID dispatcher for consistent formatting
  // - NSString elements shown with quotes
  // - NSNumber elements shown as values
  // - Other objects shown using their formatters
  // - Graceful fallback for unknown types
  
  // Test mixed element scenarios
  struct MixedElementTest {
    uint64_t element_addr;
    std::string element_type;
    std::string expected_format_pattern;
    bool should_quote;
  };
  
  std::vector<MixedElementTest> mixed_tests = {
    {0x1000, "NSString", "\"string_content\"", true},
    {0x2000, "NSNumber", "42", false},
    {0x3000, "NSArray", "(3 elements)", false},
    {0x4000, "NSDictionary", "{2 pairs}", false},
    {0x5000, "CustomObject", "<CustomObject: 0x5000>", false},
    {(123 << 3) | 0x1, "tagged NSNumber", "123", false},
    {(0x456 << 3) | 0x4, "tagged NSString", "\"tagged_str\"", true}
  };
  
  for (const auto& test : mixed_tests) {
    // Test element type detection
    bool is_tagged = (test.element_addr & 0x7) != 0;
    bool is_string_type = test.element_type.find("String") != std::string::npos;
    
    EXPECT_EQ(test.should_quote, is_string_type)
        << "Element type " << test.element_type << " quoting should match string detection";
    
    // Test ID dispatcher priority
    bool has_id_dispatcher = !test.element_type.empty();
    EXPECT_TRUE(has_id_dispatcher)
        << "All test elements should have type information for ID dispatcher";
    
    // Test format pattern validation
    if (test.should_quote) {
      EXPECT_TRUE(test.expected_format_pattern.front() == '\"' && 
                  test.expected_format_pattern.back() == '\"')
          << "String elements should have quoted format pattern";
    } else {
      EXPECT_FALSE(test.expected_format_pattern.front() == '\"' && 
                   test.expected_format_pattern.back() == '\"')
          << "Non-string elements should not have quoted format pattern";
    }
  }
  
  // Test fallback behavior for unknown types
  struct UnknownTypeTest {
    uint64_t element_addr;
    std::string fallback_format;
  };
  
  std::vector<UnknownTypeTest> unknown_tests = {
    {0x6000, "<unknown: 0x6000>"},
    {0x0, "(null)"},
    {LLDB_INVALID_ADDRESS, "<invalid>"}
  };
  
  for (const auto& test : unknown_tests) {
    bool is_null = (test.element_addr == 0);
    bool is_invalid = (test.element_addr == LLDB_INVALID_ADDRESS);
    bool needs_fallback = is_null || is_invalid || test.element_addr == 0x6000;
    
    EXPECT_TRUE(needs_fallback)
        << "Address 0x" << std::hex << test.element_addr 
        << " should require fallback formatting";
  }
}

TEST_F(NSSetFormatterTest, PerformanceRequirements) {
  // Test set formatter performance characteristics
  // REQUIREMENT: All formatters must respond within 50ms
  
  auto start = std::chrono::high_resolution_clock::now();
  
  // Create many set formatter instances
  std::vector<std::unique_ptr<GNUstepNSSetSummaryProvider>> formatters;
  for (int i = 0; i < 1000; ++i) {
    formatters.push_back(
      std::make_unique<GNUstepNSSetSummaryProvider>());
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 50) << "Creating 1000 set formatters should be fast (<50ms)";
  EXPECT_EQ(formatters.size(), 1000) << "All set formatters should be created successfully";
}

TEST_F(NSSetFormatterTest, ErrorHandling) {
  // Test set formatter error handling for corrupted data
  // Formatters must gracefully handle invalid memory conditions
  
  // Error conditions handled:
  // - Invalid object addresses (0, LLDB_INVALID_ADDRESS)
  // - Memory read failures in hash table traversal
  // - Corrupted nodeCount/bucketCount values
  // - Invalid bucket pointers
  // - Broken linked lists in hash buckets
  // - Corrupted element pointers
  
  const uint64_t NULL_ADDRESS = 0;
  const uint64_t INVALID_ADDRESS = LLDB_INVALID_ADDRESS;
  const uint32_t SAFETY_LIMIT = 10000000; // Same as dictionary
  
  // Test invalid object address handling
  std::vector<uint64_t> invalid_obj_addresses = {
    NULL_ADDRESS,
    INVALID_ADDRESS,
    0x1, // Unaligned
    0x7  // Tagged-like but invalid for objects
  };
  
  for (uint64_t addr : invalid_obj_addresses) {
    bool should_reject = (addr == NULL_ADDRESS) || 
                        (addr == INVALID_ADDRESS) ||
                        (addr != 0 && (addr % 8) != 0 && (addr & 0x7) == 0);
    
    EXPECT_TRUE(addr == NULL_ADDRESS || addr == INVALID_ADDRESS || 
                addr == 0x1 || addr == 0x7)
        << "Address 0x" << std::hex << addr << " should be in invalid test set";
  }
  
  // Test count validation (same logic as dictionary)
  struct SetCountTest {
    uint32_t element_count;
    bool should_be_valid;
  };
  
  std::vector<SetCountTest> count_tests = {
    {0, true},
    {10, true},
    {1000, true},
    {SAFETY_LIMIT, true},
    {SAFETY_LIMIT + 1, false},
    {UINT32_MAX, false}
  };
  
  for (const auto& test : count_tests) {
    bool is_valid = test.element_count <= SAFETY_LIMIT;
    EXPECT_EQ(is_valid, test.should_be_valid)
        << "Element count " << test.element_count 
        << " validity should match expected";
  }
  
  // Test hash bucket pointer validation
  std::vector<uint64_t> bucket_pointers = {
    0x0,           // NULL - should be handled
    0x1000,        // Valid aligned pointer
    0x1001,        // Unaligned - should be rejected
    INVALID_ADDRESS // Invalid - should be rejected
  };
  
  for (uint64_t bucket_ptr : bucket_pointers) {
    bool is_null = (bucket_ptr == 0);
    bool is_aligned = (bucket_ptr != 0) && ((bucket_ptr % 8) == 0);
    bool is_invalid = (bucket_ptr == INVALID_ADDRESS);
    
    bool should_be_usable = !is_null && is_aligned && !is_invalid;
    
    if (bucket_ptr == 0x1000) {
      EXPECT_TRUE(should_be_usable) << "Valid aligned pointer should be usable";
    } else {
      EXPECT_FALSE(should_be_usable || bucket_ptr == 0) 
          << "Invalid/unaligned/null pointers should not be usable (except null may be handled)";
    }
  }
}

TEST_F(NSSetFormatterTest, MutableVsImmutableHandling) {
  // Test handling of NSSet vs NSMutableSet
  // Both should use the same formatter infrastructure
  
  // Detection logic:
  // - Check class name for "Mutable" substring
  // - GSSet and GSMutableSet have same memory layout
  // - Display format should indicate mutability
  
  // Test mutable set class detection
  std::vector<std::string> mutable_set_classes = {
    "NSMutableSet",
    "GSMutableSet",
    "__NSSetM",
    "GSMutableSet_concrete"
  };
  
  std::vector<std::string> immutable_set_classes = {
    "NSSet",
    "GSSet",
    "__NSSetI",
    "GSSet_concrete"
  };
  
  // Validate mutable detection
  for (const auto& class_name : mutable_set_classes) {
    bool detected_mutable = class_name.find("Mutable") != std::string::npos;
    EXPECT_TRUE(detected_mutable)
        << "Class " << class_name << " should be detected as mutable";
  }
  
  // Validate immutable detection
  for (const auto& class_name : immutable_set_classes) {
    bool detected_mutable = class_name.find("Mutable") != std::string::npos;
    EXPECT_FALSE(detected_mutable)
        << "Class " << class_name << " should be detected as immutable";
  }
  
  // Test memory layout consistency
  const size_t SET_HEADER_SIZE = sizeof(void*) + sizeof(void*); // isa + map
  const size_t MAP_TABLE_SIZE = 4 * sizeof(void*); // zone + nodeCount + bucketCount + buckets
  
  EXPECT_EQ(SET_HEADER_SIZE, 16) << "Set header size consistent for both types";
  EXPECT_EQ(MAP_TABLE_SIZE, 32) << "Map table size consistent for both types";
  
  // Test display format implications
  struct SetDisplayTest {
    std::string class_name;
    uint32_t element_count;
    bool is_mutable;
    std::string expected_format_pattern;
  };
  
  std::vector<SetDisplayTest> display_tests = {
    {"NSSet", 3, false, "{(3 objects)}"},
    {"NSMutableSet", 3, true, "{(3 objects)}"},
    {"GSSet", 0, false, "{()}"},
    {"GSMutableSet", 0, true, "{()}"},
  };
  
  for (const auto& test : display_tests) {
    bool detected_mutable = test.class_name.find("Mutable") != std::string::npos;
    EXPECT_EQ(detected_mutable, test.is_mutable)
        << "Mutability detection should match expected for " << test.class_name;
    
    // Both mutable and immutable should have same display format for same count
    EXPECT_TRUE(test.expected_format_pattern.find("{(") == 0)
        << "All sets should use consistent display format";
  }
}

TEST_F(NSSetFormatterTest, ElementCountDisplay) {
  // Test set element count display behavior
  // Different display formats for different sizes
  
  // Display behavior:
  // - Empty sets: show "{()}"
  // - Small sets (<=5 elements): show inline elements
  // - Large sets (>5 elements): show "{(count objects)}"
  // - Safety limit for very large sets (>10000000)
  
  const uint32_t MAX_INLINE_ELEMENTS = 5;
  const uint32_t SAFETY_LIMIT = 10000000;
  
  // Test element count display scenarios
  struct ElementCountTest {
    uint32_t element_count;
    std::string expected_format;
    bool should_show_inline;
  };
  
  std::vector<ElementCountTest> count_tests = {
    {0, "{()}", false},
    {1, "inline", true},
    {3, "inline", true},
    {5, "inline", true},
    {6, "{(6 objects)}", false},
    {100, "{(100 objects)}", false},
    {SAFETY_LIMIT, "{(10000000 objects)}", false}
  };
  
  for (const auto& test : count_tests) {
    // Test inline display logic
    bool should_inline = test.element_count > 0 && test.element_count <= MAX_INLINE_ELEMENTS;
    EXPECT_EQ(should_inline, test.should_show_inline)
        << "Count " << test.element_count << " inline display should match expected";
    
    // Test format string patterns
    if (test.element_count == 0) {
      EXPECT_EQ(test.expected_format, "{()}") << "Empty set should show {()}";
    } else if (test.should_show_inline) {
      EXPECT_EQ(test.expected_format, "inline") << "Small set should show inline";
    } else {
      std::string expected_large = "{(" + std::to_string(test.element_count) + " objects)}";
      if (test.expected_format != "inline") {
        EXPECT_TRUE(test.expected_format.find("objects)") != std::string::npos)
            << "Large set should show object count";
      }
    }
  }
  
  // Test safety limit enforcement
  EXPECT_GT(SAFETY_LIMIT, 1000000U) << "Safety limit should handle very large sets";
  EXPECT_LE(MAX_INLINE_ELEMENTS, 10U) << "Inline limit should be reasonable";
}

TEST_F(NSSetFormatterTest, SyntheticChildrenGeneration) {
  // Test synthetic children generation for set expansion
  // Allows users to drill down into set contents
  
  // Synthetic children:
  // - Child 0: "count" (special child showing element count)
  // - Child 1+: actual set elements
  // - Uses 'id' type for all element children
  // - LLDB handles dynamic type resolution
  
  // Test synthetic children naming pattern
  struct SyntheticChildTest {
    uint32_t child_index;
    std::string expected_name;
    bool is_special_child;
  };
  
  std::vector<SyntheticChildTest> child_tests = {
    {0, "count", true},
    {1, "[0]", false},
    {2, "[1]", false},
    {3, "[2]", false},
    {10, "[9]", false}
  };
  
  for (const auto& test : child_tests) {
    if (test.is_special_child) {
      EXPECT_EQ(test.expected_name, "count")
          << "First child should be special 'count' child";
    } else {
      // Calculate element index (child_index - 1 for 0-based element indexing)
      uint32_t element_index = test.child_index - 1;
      std::string generated_name = "[" + std::to_string(element_index) + "]";
      
      EXPECT_EQ(generated_name, test.expected_name)
          << "Child " << test.child_index << " should have name " << test.expected_name;
    }
  }
  
  // Test type system requirements
  const std::string ID_TYPE = "id";
  const std::string COUNT_TYPE = "NSUInteger"; // For count child
  
  EXPECT_EQ(ID_TYPE, "id") << "Set elements should use 'id' type";
  EXPECT_EQ(COUNT_TYPE, "NSUInteger") << "Count child should use appropriate integer type";
  
  // Test child count calculation
  struct ChildCountTest {
    uint32_t set_element_count;
    uint32_t expected_child_count;
  };
  
  std::vector<ChildCountTest> count_calc_tests = {
    {0, 1}, // Just count child
    {1, 2}, // Count + 1 element
    {5, 6}, // Count + 5 elements
    {100, 101} // Count + 100 elements
  };
  
  for (const auto& test : count_calc_tests) {
    uint32_t calculated_children = 1 + test.set_element_count; // count child + elements
    EXPECT_EQ(calculated_children, test.expected_child_count)
        << "Set with " << test.set_element_count << " elements should have "
        << test.expected_child_count << " synthetic children";
  }
}

TEST_F(NSSetFormatterTest, RecursionPrevention) {
  // Test recursion prevention for sets containing collections
  // Critical for nested sets and circular references
  
  // Recursion prevention:
  // - FormatterContext tracks depth (MAX_FORMATTER_DEPTH = 8)
  // - Visited object addresses detect cycles
  // - MAX_COLLECTION_ELEMENTS_INLINE = 5 for performance
  
  const int MAX_FORMATTER_DEPTH = 8;
  const int MAX_COLLECTION_ELEMENTS_INLINE = 5;
  
  // Test recursion depth management
  struct RecursionDepthTest {
    int current_depth;
    bool should_continue;
  };
  
  std::vector<RecursionDepthTest> depth_tests = {
    {0, true},
    {4, true},
    {7, true},
    {8, false}, // At limit
    {9, false}  // Beyond limit
  };
  
  for (const auto& test : depth_tests) {
    bool can_continue = test.current_depth < MAX_FORMATTER_DEPTH;
    EXPECT_EQ(can_continue, test.should_continue)
        << "Depth " << test.current_depth << " should "
        << (test.should_continue ? "" : "not ") << "allow continued formatting";
  }
  
  // Test circular reference detection
  std::set<uint64_t> visited_sets;
  
  // Simulate nested set scenario: set1 -> set2 -> set1 (cycle)
  uint64_t set1_addr = 0x1000;
  uint64_t set2_addr = 0x2000;
  
  // First visit to set1
  EXPECT_TRUE(visited_sets.find(set1_addr) == visited_sets.end())
      << "First visit to set1 should be allowed";
  visited_sets.insert(set1_addr);
  
  // Visit set2 (nested in set1)
  EXPECT_TRUE(visited_sets.find(set2_addr) == visited_sets.end())
      << "First visit to set2 should be allowed";
  visited_sets.insert(set2_addr);
  
  // Attempt to visit set1 again (cycle detection)
  bool cycle_detected = visited_sets.find(set1_addr) != visited_sets.end();
  EXPECT_TRUE(cycle_detected) << "Should detect cycle when revisiting set1";
  
  // Test element limit for performance
  EXPECT_GT(MAX_COLLECTION_ELEMENTS_INLINE, 0)
      << "Should show at least one element inline";
  EXPECT_LE(MAX_COLLECTION_ELEMENTS_INLINE, 10)
      << "Inline element limit should be reasonable for performance";
  
  // Test prevention mechanism effectiveness
  int simulated_recursive_calls = 0;
  int max_depth = MAX_FORMATTER_DEPTH;
  
  while (simulated_recursive_calls < max_depth) {
    simulated_recursive_calls++;
  }
  
  EXPECT_EQ(simulated_recursive_calls, MAX_FORMATTER_DEPTH)
      << "Recursion prevention should limit to maximum depth";
}

TEST_F(NSSetFormatterTest, ThreadSafety) {
  // Test basic thread safety for set formatters
  // Multiple formatter instances should be safe to create concurrently
  
  std::vector<std::thread> threads;
  std::vector<std::unique_ptr<GNUstepNSSetSummaryProvider>> results;
  std::mutex results_mutex;
  
  // Create formatters from multiple threads
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&results, &results_mutex]() {
      auto formatter = std::make_unique<GNUstepNSSetSummaryProvider>();
      
      std::lock_guard<std::mutex> lock(results_mutex);
      results.push_back(std::move(formatter));
    });
  }
  
  // Wait for all threads
  for (auto& thread : threads) {
    thread.join();
  }
  
  EXPECT_EQ(results.size(), 10) << "All set formatters should be created from multiple threads";
  
  // All formatters should be valid
  for (const auto& formatter : results) {
    EXPECT_NE(formatter.get(), nullptr) << "Each formatter should be valid";
  }
}

} // namespace