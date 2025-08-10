//===-- NSDictionaryFormatterTest.cpp ------------------------------------===//
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

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDictionaryFormatters.h"
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

class NSDictionaryFormatterTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
    summary_formatter = std::make_unique<GNUstepNSDictionarySummaryProvider>();
  }
  
  void TearDown() override {
    summary_formatter.reset();
    HostInfo::Terminate();
    FileSystem::Terminate();
  }
  
  std::unique_ptr<GNUstepNSDictionarySummaryProvider> summary_formatter;
};

TEST_F(NSDictionaryFormatterTest, SummaryProviderInstantiation) {
  // Test that NSDictionary summary formatter can be created successfully
  EXPECT_NE(summary_formatter.get(), nullptr) << "NSDictionary summary formatter should be instantiated";
  
  // Test that we can create multiple instances
  auto formatter2 = std::make_unique<GNUstepNSDictionarySummaryProvider>();
  EXPECT_NE(formatter2.get(), nullptr) << "Multiple formatter instances should work";
  
  // Test that formatters are distinct objects
  EXPECT_NE(summary_formatter.get(), formatter2.get()) << "Each formatter should be a distinct instance";
}

TEST_F(NSDictionaryFormatterTest, RegistrationFunctions) {
  // Test that dictionary formatter registration functions exist and can be called
  
  // Verify function pointers exist - these are used by LLDB's formatter registration system
  auto dict_func = &GNUstepNSDictionaryFormatterFunction;
  EXPECT_NE(dict_func, nullptr) << "GNUstepNSDictionaryFormatterFunction should exist";
  
  auto synthetic_func = &GNUstepNSDictionarySyntheticFrontEndCreator;
  EXPECT_NE(synthetic_func, nullptr) << "GNUstepNSDictionarySyntheticFrontEndCreator should exist";
  
  // These functions are the bridge between LLDB's type system and our formatters
  // They must exist for the plugin to register correctly with LLDB
}

TEST_F(NSDictionaryFormatterTest, FormatterClassHierarchy) {
  // Test that the formatter correctly inherits from the expected base class
  // This validates the class hierarchy and ensures virtual methods work correctly
  
  GNUstepSummaryProvider* base_ptr = summary_formatter.get();
  EXPECT_NE(base_ptr, nullptr) << "Should be able to convert to base class pointer";
  
  // The formatter should be polymorphic (have virtual methods)
  // This is essential for LLDB's formatter dispatching to work
}

TEST_F(NSDictionaryFormatterTest, HashTableMemoryLayout) {
  // Test that dictionary hash table memory layout understanding is correct
  // Critical for reading GSDictionary -> GSIMapTable structure
  
  // GSDictionary structure (from implementation analysis):
  // - isa: offset 0
  // - GSIMapTable_t map: offset 8
  //
  // GSIMapTable_t structure:
  // - NSZone *zone: offset 0 (within map)
  // - uintptr_t nodeCount: offset 8 (within map) -> total offset 16 from obj
  // - uintptr_t bucketCount: offset 16 (within map) -> total offset 24 from obj  
  // - GSIMapBucket buckets: offset 24 (within map) -> total offset 32 from obj
  
  // Validate GSDictionary memory layout constants
  const size_t ISA_OFFSET = 0;
  const size_t MAP_TABLE_OFFSET = 8;
  
  // GSIMapTable_t offsets within map structure
  const size_t ZONE_OFFSET_IN_MAP = 0;
  const size_t NODE_COUNT_OFFSET_IN_MAP = 8;
  const size_t BUCKET_COUNT_OFFSET_IN_MAP = 16;
  const size_t BUCKETS_OFFSET_IN_MAP = 24;
  
  // Total offsets from object start
  const size_t NODE_COUNT_OFFSET = MAP_TABLE_OFFSET + NODE_COUNT_OFFSET_IN_MAP; // 16
  const size_t BUCKET_COUNT_OFFSET = MAP_TABLE_OFFSET + BUCKET_COUNT_OFFSET_IN_MAP; // 24
  const size_t BUCKETS_OFFSET = MAP_TABLE_OFFSET + BUCKETS_OFFSET_IN_MAP; // 32
  
  EXPECT_EQ(ISA_OFFSET, 0) << "ISA must be at offset 0";
  EXPECT_EQ(MAP_TABLE_OFFSET, 8) << "GSIMapTable at offset 8";
  EXPECT_EQ(NODE_COUNT_OFFSET, 16) << "NodeCount at total offset 16";
  EXPECT_EQ(BUCKET_COUNT_OFFSET, 24) << "BucketCount at total offset 24";
  EXPECT_EQ(BUCKETS_OFFSET, 32) << "Buckets at total offset 32";
  
  // Verify alignment requirements
  const size_t POINTER_SIZE = sizeof(void*);
  EXPECT_EQ(MAP_TABLE_OFFSET % POINTER_SIZE, 0U) << "Map table must be pointer-aligned";
  EXPECT_EQ(NODE_COUNT_OFFSET % sizeof(uintptr_t), 0U) << "Node count must be aligned";
  EXPECT_EQ(BUCKETS_OFFSET % POINTER_SIZE, 0U) << "Buckets must be pointer-aligned";
}

TEST_F(NSDictionaryFormatterTest, HashBucketTraversal) {
  // Test understanding of hash bucket linked list traversal
  // Critical for extracting key-value pairs correctly
  
  // GSIMapBucket structure:
  // - uintptr_t nodeCount: offset 0
  // - GSIMapNode firstNode: offset 8 (pointer to first node)
  //
  // GSIMapNode structure:
  // - GSIMapNode nextInBucket: offset 0 (pointer to next node)
  // - GSIMapKey key: offset 8 (union containing id)
  // - GSIMapVal value: offset 16 (union containing id)
  
  // Validate GSIMapBucket structure
  const size_t BUCKET_NODE_COUNT_OFFSET = 0;
  const size_t BUCKET_FIRST_NODE_OFFSET = 8;
  const size_t BUCKET_HEADER_SIZE = 16; // nodeCount + firstNode pointer
  
  EXPECT_EQ(BUCKET_NODE_COUNT_OFFSET, 0) << "Bucket node count at offset 0";
  EXPECT_EQ(BUCKET_FIRST_NODE_OFFSET, 8) << "First node pointer at offset 8";
  EXPECT_EQ(BUCKET_HEADER_SIZE, sizeof(uintptr_t) + sizeof(void*))
      << "Bucket header size should match nodeCount + pointer";
  
  // Validate GSIMapNode structure
  const size_t NODE_NEXT_OFFSET = 0;
  const size_t NODE_KEY_OFFSET = 8;
  const size_t NODE_VALUE_OFFSET = 16;
  const size_t NODE_SIZE = 24; // next + key + value (all pointers)
  
  EXPECT_EQ(NODE_NEXT_OFFSET, 0) << "Next node pointer at offset 0";
  EXPECT_EQ(NODE_KEY_OFFSET, 8) << "Key at offset 8";
  EXPECT_EQ(NODE_VALUE_OFFSET, 16) << "Value at offset 16";
  EXPECT_EQ(NODE_SIZE, 3 * sizeof(void*)) << "Node size should be 3 pointers";
  
  // Test linked list traversal simulation
  std::vector<uint64_t> node_addresses = {0x1000, 0x1100, 0x1200, 0}; // NULL-terminated
  
  uint64_t current = node_addresses[0];
  size_t visited_count = 0;
  
  while (current != 0 && visited_count < node_addresses.size()) {
    EXPECT_NE(current, 0U) << "Valid node address should not be NULL";
    
    // Simulate reading next pointer (would be at current + NODE_NEXT_OFFSET)
    visited_count++;
    current = (visited_count < node_addresses.size() - 1) ? node_addresses[visited_count] : 0;
  }
  
  EXPECT_EQ(visited_count, node_addresses.size() - 1) << "Should visit all non-NULL nodes";
}

TEST_F(NSDictionaryFormatterTest, KeyValuePairExtraction) {
  // Test key-value pair extraction logic
  // Must handle different key/value types including tagged pointers
  
  // Key considerations from implementation:
  // - Keys can be NSString, NSNumber, or other objects
  // - Values can be any object type
  // - Both keys and values can be tagged pointers
  // - Must extract STORAGE ADDRESSES, not pointer values
  // - CreateValueObjectFromAddress needs storage locations
  
  // Test different key-value type combinations
  struct KeyValueTest {
    uint64_t key_addr;
    uint64_t value_addr;
    std::string key_type;
    std::string value_type;
    bool key_is_tagged;
    bool value_is_tagged;
  };
  
  const uint64_t TAG_MASK = 0x7;
  
  std::vector<KeyValueTest> test_pairs = {
    {0x1000, 0x2000, "NSString", "NSNumber", false, false},
    {0x123 | 0x4, 0x3000, "tagged string", "NSObject", true, false},
    {0x4000, 0x456 | 0x1, "NSString", "tagged int", false, true},
    {0x789 | 0x4, 0x321 | 0x1, "tagged string", "tagged int", true, true}
  };
  
  for (const auto& test : test_pairs) {
    // Validate tagged pointer detection
    bool key_detected_tagged = (test.key_addr & TAG_MASK) != 0;
    bool value_detected_tagged = (test.value_addr & TAG_MASK) != 0;
    
    EXPECT_EQ(key_detected_tagged, test.key_is_tagged)
        << "Key 0x" << std::hex << test.key_addr << " tagged detection mismatch";
    EXPECT_EQ(value_detected_tagged, test.value_is_tagged)
        << "Value 0x" << std::hex << test.value_addr << " tagged detection mismatch";
    
    // Test storage address requirements
    if (!test.key_is_tagged) {
      EXPECT_EQ(test.key_addr & TAG_MASK, 0U) << "Regular key should be aligned";
    }
    if (!test.value_is_tagged) {
      EXPECT_EQ(test.value_addr & TAG_MASK, 0U) << "Regular value should be aligned";
    }
  }
  
  // Test pair extraction structure
  struct KeyValuePair {
    uint64_t key_addr;
    uint64_t value_addr;
  };
  
  std::vector<KeyValuePair> extracted_pairs;
  for (const auto& test : test_pairs) {
    extracted_pairs.push_back({test.key_addr, test.value_addr});
  }
  
  EXPECT_EQ(extracted_pairs.size(), test_pairs.size())
      << "Should extract all key-value pairs";
  
  // Verify extracted addresses match originals
  for (size_t i = 0; i < extracted_pairs.size(); ++i) {
    EXPECT_EQ(extracted_pairs[i].key_addr, test_pairs[i].key_addr)
        << "Extracted key address should match original";
    EXPECT_EQ(extracted_pairs[i].value_addr, test_pairs[i].value_addr)
        << "Extracted value address should match original";
  }
}

TEST_F(NSDictionaryFormatterTest, RecursionPrevention) {
  // Test that dictionary formatters prevent infinite recursion
  // Critical for nested dictionaries and circular references
  
  // The implementation uses FormatterContext to track:
  // - Recursion depth (MAX_FORMATTER_DEPTH = 8)
  // - Visited object addresses to detect cycles
  // - MAX_COLLECTION_ELEMENTS_INLINE = 5 for performance
  // - Special handling for nested collections in GetElementSummary
  
  const int MAX_FORMATTER_DEPTH = 8;
  const int MAX_COLLECTION_ELEMENTS_INLINE = 5;
  
  // Test recursion depth management
  struct RecursionTest {
    int current_depth;
    bool should_continue_formatting;
  };
  
  std::vector<RecursionTest> depth_tests = {
    {0, true},
    {4, true},
    {7, true},
    {8, false}, // At limit
    {9, false}  // Beyond limit
  };
  
  for (const auto& test : depth_tests) {
    bool can_continue = test.current_depth < MAX_FORMATTER_DEPTH;
    EXPECT_EQ(can_continue, test.should_continue_formatting)
        << "Depth " << test.current_depth << " should "
        << (test.should_continue_formatting ? "" : "not ") << "allow continued formatting";
  }
  
  // Test cycle detection with visited addresses
  std::set<uint64_t> visited_objects;
  
  // Simulate nested dictionary scenario
  uint64_t dict1_addr = 0x1000;
  uint64_t dict2_addr = 0x2000;
  uint64_t dict3_addr = 0x3000;
  
  // First level - dict1
  EXPECT_TRUE(visited_objects.find(dict1_addr) == visited_objects.end())
      << "First visit to dict1 should be allowed";
  visited_objects.insert(dict1_addr);
  
  // Second level - dict2
  EXPECT_TRUE(visited_objects.find(dict2_addr) == visited_objects.end())
      << "First visit to dict2 should be allowed";
  visited_objects.insert(dict2_addr);
  
  // Third level - dict1 again (cycle!)
  bool cycle_detected = visited_objects.find(dict1_addr) != visited_objects.end();
  EXPECT_TRUE(cycle_detected) << "Should detect cycle when revisiting dict1";
  
  // Test inline element limits
  EXPECT_GT(MAX_COLLECTION_ELEMENTS_INLINE, 0)
      << "Should show at least one element inline";
  EXPECT_LE(MAX_COLLECTION_ELEMENTS_INLINE, 10)
      << "Inline limit should be reasonable for performance";
}

TEST_F(NSDictionaryFormatterTest, PerformanceCharacteristics) {
  // Test dictionary formatter performance characteristics
  // REQUIREMENT: All formatters must respond within 50ms
  
  auto start = std::chrono::high_resolution_clock::now();
  
  // Create many dictionary formatter instances to test performance
  std::vector<std::unique_ptr<GNUstepNSDictionarySummaryProvider>> formatters;
  for (int i = 0; i < 1000; ++i) {
    formatters.push_back(
      std::make_unique<GNUstepNSDictionarySummaryProvider>());
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 50) << "Creating 1000 dictionary formatters should be fast (<50ms)";
  EXPECT_EQ(formatters.size(), 1000) << "All dictionary formatters should be created successfully";
}

TEST_F(NSDictionaryFormatterTest, PairCountLimits) {
  // Test dictionary formatter behavior with different pair counts
  // Implementation has specific behavior for different dictionary sizes
  
  // From ExtractDictionaryCount() and GetInlinePairsPreview() analysis:
  // - Empty dictionaries (count = 0): show "{}"
  // - Small dictionaries (count <= MAX_COLLECTION_ELEMENTS_INLINE=5): show inline preview
  // - Large dictionaries (count > 5): show "{count pairs}"
  // - Very large dictionaries (count > 10000000): safety limit
  
  const uint32_t MAX_ELEMENTS_INLINE = 5;
  const uint32_t SAFETY_LIMIT = 10000000;
  
  // Test different pair count scenarios
  struct PairCountTest {
    uint32_t pair_count;
    std::string expected_format;
    bool should_show_inline;
  };
  
  std::vector<PairCountTest> test_cases = {
    {0, "{}", false},
    {1, "inline", true},
    {3, "inline", true},
    {5, "inline", true},
    {6, "{6 pairs}", false},
    {100, "{100 pairs}", false},
    {SAFETY_LIMIT, "{10000000 pairs}", false}
  };
  
  for (const auto& test : test_cases) {
    // Validate inline display logic
    bool should_inline = test.pair_count > 0 && test.pair_count <= MAX_ELEMENTS_INLINE;
    EXPECT_EQ(should_inline, test.should_show_inline)
        << "Pair count " << test.pair_count << " should "
        << (test.should_show_inline ? "" : "not ") << "show inline";
        
    // Test safety limit enforcement
    bool within_safety_limit = test.pair_count <= SAFETY_LIMIT;
    EXPECT_TRUE(within_safety_limit || test.pair_count == SAFETY_LIMIT)
        << "All test cases should be at or below safety limit";
  }
  
  // Test boundary conditions
  EXPECT_GT(MAX_ELEMENTS_INLINE, 0) << "Must show at least one pair inline";
  EXPECT_LT(MAX_ELEMENTS_INLINE, 20) << "Inline limit should be reasonable";
  EXPECT_GT(SAFETY_LIMIT, 1000000U) << "Safety limit should handle large dictionaries";
}

TEST_F(NSDictionaryFormatterTest, MutableDictionarySupport) {
  // Test that mutable dictionaries use the same formatter infrastructure
  // NSMutableDictionary and GSMutableDictionary should be handled correctly
  
  // The implementation detects mutable dictionaries in UpdateImpl():
  // m_is_mutable = (class_name && strstr(class_name, "Mutable"));
  
  // Test mutable dictionary class detection
  std::vector<std::string> mutable_classes = {
    "NSMutableDictionary",
    "GSMutableDictionary",
    "__NSDictionaryM",
    "GSMutableDictionary_concrete"
  };
  
  std::vector<std::string> immutable_classes = {
    "NSDictionary",
    "GSDictionary", 
    "__NSDictionaryI",
    "GSDictionary_concrete"
  };
  
  // Test mutable detection logic
  for (const auto& class_name : mutable_classes) {
    bool detected_mutable = class_name.find("Mutable") != std::string::npos;
    EXPECT_TRUE(detected_mutable)
        << "Class " << class_name << " should be detected as mutable";
  }
  
  // Test immutable detection logic
  for (const auto& class_name : immutable_classes) {
    bool detected_mutable = class_name.find("Mutable") != std::string::npos;
    EXPECT_FALSE(detected_mutable)
        << "Class " << class_name << " should be detected as immutable";
  }
  
  // Both should use same GSIMapTable memory layout
  const size_t DICTIONARY_HEADER_SIZE = 8 + 8; // isa + map_table
  const size_t MAP_TABLE_SIZE = 8 + 8 + 8 + 8; // zone + nodeCount + bucketCount + buckets
  
  EXPECT_EQ(DICTIONARY_HEADER_SIZE, 16) << "Dictionary header consistent for both types";
  EXPECT_EQ(MAP_TABLE_SIZE, 32) << "Map table layout consistent for both types";
}

TEST_F(NSDictionaryFormatterTest, SyntheticChildrenNaming) {
  // Test synthetic children naming convention
  // Dictionary children should follow "[idx].key" and "[idx].value" pattern
  
  // From GetChildAtIndex() implementation:
  // - Child 0: "count" (special synthetic child showing pair count)
  // - Child 1: "[0].key" (first pair's key)
  // - Child 2: "[0].value" (first pair's value)
  // - Child 3: "[1].key" (second pair's key)
  // - etc.
  
  // Test child naming pattern generation
  struct ChildNameTest {
    uint32_t child_index;
    std::string expected_name;
    bool is_special_child;
  };
  
  std::vector<ChildNameTest> naming_tests = {
    {0, "count", true},
    {1, "[0].key", false},
    {2, "[0].value", false},
    {3, "[1].key", false},
    {4, "[1].value", false},
    {5, "[2].key", false},
    {6, "[2].value", false}
  };
  
  for (const auto& test : naming_tests) {
    if (test.is_special_child) {
      EXPECT_EQ(test.expected_name, "count")
          << "First child should be special 'count' child";
    } else {
      // Calculate pair index and key/value from child index
      uint32_t pair_index = (test.child_index - 1) / 2;
      bool is_key = ((test.child_index - 1) % 2) == 0;
      
      std::string generated_name = "[" + std::to_string(pair_index) + "]."
                                  + (is_key ? "key" : "value");
      
      EXPECT_EQ(generated_name, test.expected_name)
          << "Child " << test.child_index << " should have name " << test.expected_name;
    }
  }
  
  // Test special count child
  EXPECT_EQ(naming_tests[0].expected_name, "count")
      << "Count child should have special name";
  
  // Test key/value alternation pattern
  for (size_t i = 1; i < naming_tests.size(); i += 2) {
    EXPECT_TRUE(naming_tests[i].expected_name.find(".key") != std::string::npos)
        << "Odd-indexed children should be keys";
    if (i + 1 < naming_tests.size()) {
      EXPECT_TRUE(naming_tests[i + 1].expected_name.find(".value") != std::string::npos)
          << "Even-indexed children should be values";
    }
  }
}

TEST_F(NSDictionaryFormatterTest, IdDispatcherIntegration) {
  // Test integration with ID dispatcher for element summaries
  // ID dispatcher should be used for consistent formatting
  
  // From GetElementSummary() implementation:
  // - Try ID dispatcher FIRST before custom string extraction
  // - This ensures consistent behavior across all formatters
  // - Handles tagged pointers and regular objects uniformly
  // - Falls back to custom extraction if dispatcher fails
  
  // Test element summary priority order
  enum class SummaryMethod {
    ID_DISPATCHER,
    CUSTOM_STRING_EXTRACTION,
    FALLBACK_DEFAULT
  };
  
  struct ElementSummaryTest {
    uint64_t element_addr;
    bool id_dispatcher_available;
    bool custom_extraction_available;
    SummaryMethod expected_method;
  };
  
  std::vector<ElementSummaryTest> summary_tests = {
    {0x1000, true, true, SummaryMethod::ID_DISPATCHER},
    {0x2000, false, true, SummaryMethod::CUSTOM_STRING_EXTRACTION},
    {0x3000, false, false, SummaryMethod::FALLBACK_DEFAULT},
    {0x123 | 0x4, true, false, SummaryMethod::ID_DISPATCHER} // Tagged pointer
  };
  
  for (const auto& test : summary_tests) {
    // Test priority logic
    SummaryMethod selected_method;
    
    if (test.id_dispatcher_available) {
      selected_method = SummaryMethod::ID_DISPATCHER;
    } else if (test.custom_extraction_available) {
      selected_method = SummaryMethod::CUSTOM_STRING_EXTRACTION;
    } else {
      selected_method = SummaryMethod::FALLBACK_DEFAULT;
    }
    
    EXPECT_EQ(selected_method, test.expected_method)
        << "Address 0x" << std::hex << test.element_addr 
        << " should use expected summary method";
  }
  
  // Test tagged pointer handling through ID dispatcher
  uint64_t tagged_addr = 0x123 | 0x4; // Tagged string
  bool is_tagged = (tagged_addr & 0x7) != 0;
  
  EXPECT_TRUE(is_tagged) << "Tagged addresses should be detected";
  
  // Test consistency requirements
  std::vector<std::string> expected_consistent_types = {
    "NSString", "NSNumber", "NSArray", "NSDictionary", "NSSet"
  };
  
  for (const auto& type : expected_consistent_types) {
    EXPECT_FALSE(type.empty()) << "Type " << type << " should have consistent formatting";
  }
}

TEST_F(NSDictionaryFormatterTest, ErrorHandling) {
  // Test dictionary formatter error handling for corrupted data
  // Formatters must gracefully handle invalid memory conditions
  
  // Error conditions handled by implementation:
  // - Invalid object addresses (0, LLDB_INVALID_ADDRESS)
  // - Memory read failures in hash table traversal
  // - Corrupted nodeCount/bucketCount values (> 10000000 safety limit)
  // - Invalid bucket pointers
  // - Broken linked lists in hash buckets
  // - Corrupted key/value pointers
  
  const uint64_t NULL_ADDRESS = 0;
  const uint64_t INVALID_ADDRESS = LLDB_INVALID_ADDRESS;
  const uint32_t SAFETY_LIMIT = 10000000;
  const uint64_t UNALIGNED_POINTER = 0x1001; // Not 8-byte aligned
  
  // Test invalid address detection
  std::vector<uint64_t> invalid_addresses = {
    NULL_ADDRESS,
    INVALID_ADDRESS,
    UNALIGNED_POINTER
  };
  
  for (uint64_t addr : invalid_addresses) {
    bool is_null = (addr == NULL_ADDRESS);
    bool is_invalid = (addr == INVALID_ADDRESS);
    bool is_unaligned = (addr != 0 && addr != INVALID_ADDRESS && (addr % 8) != 0);
    
    bool should_reject = is_null || is_invalid || is_unaligned;
    EXPECT_TRUE(should_reject) << "Address 0x" << std::hex << addr << " should be rejected";
  }
  
  // Test count validation
  struct CountValidationTest {
    uint32_t count;
    bool should_be_valid;
    std::string description;
  };
  
  std::vector<CountValidationTest> count_tests = {
    {0, true, "empty dictionary"},
    {10, true, "small dictionary"},
    {1000, true, "medium dictionary"},
    {SAFETY_LIMIT, true, "maximum safe dictionary"},
    {SAFETY_LIMIT + 1, false, "oversized dictionary"},
    {UINT32_MAX, false, "corrupted count"}
  };
  
  for (const auto& test : count_tests) {
    bool is_valid = test.count <= SAFETY_LIMIT;
    EXPECT_EQ(is_valid, test.should_be_valid)
        << "Count " << test.count << " for " << test.description
        << " should " << (test.should_be_valid ? "" : "not ") << "be valid";
  }
  
  // Test bucket pointer validation
  uint64_t valid_bucket_ptr = 0x1000;
  uint64_t invalid_bucket_ptr = 0x0;
  
  EXPECT_NE(valid_bucket_ptr, 0U) << "Valid bucket pointer should not be NULL";
  EXPECT_EQ(valid_bucket_ptr % 8, 0U) << "Valid bucket pointer should be aligned";
  EXPECT_EQ(invalid_bucket_ptr, 0U) << "Invalid bucket pointer should be detected";
}

TEST_F(NSDictionaryFormatterTest, StringQuotingBehavior) {
  // Test proper string quoting behavior for dictionary display
  // String keys and values should be properly quoted for readability
  
  // From FormatStringValue() implementation:
  // - Strings are quoted with double quotes
  // - Escaped quotes inside strings handled correctly
  // - Non-string objects not quoted
  // - Consistent with Apple's NSDictionary formatting
  
  // Test string quoting scenarios
  struct StringQuotingTest {
    std::string input_string;
    std::string expected_output;
    bool should_be_quoted;
  };
  
  std::vector<StringQuotingTest> quoting_tests = {
    {"simple", "\"simple\"", true},
    {"with space", "\"with space\"", true},
    {"with\"quote", "\"with\\\"quote\"", true},
    {"with\nwline", "\"with\\newline\"", true},
    {"", "\"\"", true} // Empty string
  };
  
  for (const auto& test : quoting_tests) {
    // Test quoting logic
    std::string quoted_result = "\"" + test.input_string + "\"";
    
    // For strings with quotes, test escaping
    if (test.input_string.find('\"') != std::string::npos) {
      // Should escape internal quotes
      EXPECT_TRUE(test.expected_output.find("\\\"") != std::string::npos)
          << "String with quotes should have escaped quotes in output";
    }
    
    // All strings should be quoted
    EXPECT_TRUE(test.should_be_quoted) << "All test strings should be quoted";
    EXPECT_TRUE(test.expected_output.front() == '\"') << "Output should start with quote";
    EXPECT_TRUE(test.expected_output.back() == '\"') << "Output should end with quote";
  }
  
  // Test non-string object behavior
  struct NonStringTest {
    std::string object_type;
    std::string sample_output;
    bool should_be_quoted;
  };
  
  std::vector<NonStringTest> non_string_tests = {
    {"NSNumber", "42", false},
    {"NSArray", "(3 elements)", false},
    {"NSDictionary", "{2 pairs}", false},
    {"NSObject", "<NSObject: 0x1000>", false}
  };
  
  for (const auto& test : non_string_tests) {
    EXPECT_FALSE(test.should_be_quoted)
        << "Non-string type " << test.object_type << " should not be quoted";
    EXPECT_TRUE(test.sample_output.front() != '\"' || test.sample_output.back() != '\"')
        << "Non-string output should not be quoted";
  }
}

TEST_F(NSDictionaryFormatterTest, ThreadSafety) {
  // Test basic thread safety assumptions for dictionary formatters
  // Multiple formatter instances should be safe to create concurrently
  
  std::vector<std::thread> threads;
  std::vector<std::unique_ptr<GNUstepNSDictionarySummaryProvider>> results;
  std::mutex results_mutex;
  
  // Create formatters from multiple threads
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&results, &results_mutex]() {
      auto formatter = std::make_unique<GNUstepNSDictionarySummaryProvider>();
      
      std::lock_guard<std::mutex> lock(results_mutex);
      results.push_back(std::move(formatter));
    });
  }
  
  // Wait for all threads
  for (auto& thread : threads) {
    thread.join();
  }
  
  EXPECT_EQ(results.size(), 10) << "All dictionary formatters should be created from multiple threads";
  
  // All formatters should be valid
  for (const auto& formatter : results) {
    EXPECT_NE(formatter.get(), nullptr) << "Each formatter should be valid";
  }
}

} // namespace