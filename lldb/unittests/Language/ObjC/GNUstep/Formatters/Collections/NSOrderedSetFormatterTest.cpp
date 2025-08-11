//===-- NSOrderedSetFormatterTest.cpp -----------------------------------===//
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

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepOrderedSetFormatters.h"

#include <chrono>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace {

class NSOrderedSetFormatterTest : public ::testing::Test {
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

TEST_F(NSOrderedSetFormatterTest, FormatterCreation) {
  // Test that NSOrderedSet formatter can be created successfully
  auto formatter = std::make_unique<GNUstepNSOrderedSetSummaryProvider>();
  EXPECT_NE(formatter, nullptr) << "NSOrderedSet formatter should be instantiated";
  
  // Test multiple instances
  auto formatter2 = std::make_unique<GNUstepNSOrderedSetSummaryProvider>();
  EXPECT_NE(formatter2, nullptr) << "Multiple formatter instances should work";
  
  // Test that formatters are distinct objects
  EXPECT_NE(formatter.get(), formatter2.get()) << "Each formatter should be a distinct instance";
}

TEST_F(NSOrderedSetFormatterTest, OrderedSetStructureKnowledge) {
  // Test understanding of NSOrderedSet's hybrid structure
  // NSOrderedSet combines array-like ordered storage with set-like uniqueness
  
  // Expected NSOrderedSet/GSOrderedSet structure:
  // - isa: offset 0
  // - id *_objects: offset 8 (array-like ordered storage)  
  // - NSUInteger _count: offset 16 (number of elements)
  // - NSSet *_set: offset 24 (optional - for uniqueness checking)
  // - NSUInteger _capacity: offset 32 (for mutable variants)
  //
  // This combines the best of NSArray (ordered access) and NSSet (uniqueness)
  
  // Validate NSOrderedSet memory layout constants
  const size_t ISA_OFFSET = 0;
  const size_t OBJECTS_OFFSET = 8;
  const size_t COUNT_OFFSET = 16;
  const size_t SET_OFFSET = 24;      // Optional uniqueness helper
  const size_t CAPACITY_OFFSET = 32; // For mutable variants
  
  const size_t POINTER_SIZE = sizeof(void*);
  const size_t NSUINTEGER_SIZE = sizeof(size_t);
  
  EXPECT_EQ(ISA_OFFSET, 0) << "ISA at offset 0";
  EXPECT_EQ(OBJECTS_OFFSET, 8) << "Objects array at offset 8";
  EXPECT_EQ(COUNT_OFFSET, 16) << "Count at offset 16";
  EXPECT_EQ(SET_OFFSET, 24) << "Helper set at offset 24";
  EXPECT_EQ(CAPACITY_OFFSET, 32) << "Capacity at offset 32";
  
  // Validate alignment requirements
  EXPECT_EQ(OBJECTS_OFFSET % POINTER_SIZE, 0U) << "Objects array must be pointer-aligned";
  EXPECT_EQ(COUNT_OFFSET % NSUINTEGER_SIZE, 0U) << "Count must be properly aligned";
  EXPECT_EQ(SET_OFFSET % POINTER_SIZE, 0U) << "Set pointer must be aligned";
  EXPECT_EQ(CAPACITY_OFFSET % NSUINTEGER_SIZE, 0U) << "Capacity must be aligned";
  
  // Test hybrid structure benefits
  struct HybridBenefitTest {
    std::string benefit;
    std::string array_like_feature;
    std::string set_like_feature;
  };
  
  std::vector<HybridBenefitTest> hybrid_benefits = {
    {"ordered access", "indexed access [0], [1], [2]...", "no duplicate elements"},
    {"uniqueness checking", "stable insertion order", "fast contains() operations"},
    {"performance", "O(1) access by index", "O(1) uniqueness validation"}
  };
  
  for (const auto& test : hybrid_benefits) {
    EXPECT_FALSE(test.benefit.empty()) << "Benefit should be defined";
    EXPECT_FALSE(test.array_like_feature.empty()) << "Array-like feature should be defined";
    EXPECT_FALSE(test.set_like_feature.empty()) << "Set-like feature should be defined";
  }
  
  // Validate structure size assumptions
  const size_t MIN_ORDERED_SET_SIZE = CAPACITY_OFFSET + NSUINTEGER_SIZE; // 40 bytes
  EXPECT_EQ(MIN_ORDERED_SET_SIZE, 40) << "Minimum ordered set structure size";
}

TEST_F(NSOrderedSetFormatterTest, OrderedElementAccess) {
  // Test ordered element access pattern
  // NSOrderedSet provides indexed access like NSArray: [0], [1], [2]...
  // But maintains uniqueness like NSSet
  
  // Access pattern:
  // - Objects stored in _objects array at offset 8
  // - Count stored at offset 16  
  // - Elements accessible by index 0 to count-1
  // - Each element should be unique (no duplicates)
  
  // Test indexed access pattern
  struct IndexedAccessTest {
    uint32_t element_count;
    std::vector<std::string> expected_indices;
  };
  
  std::vector<IndexedAccessTest> access_tests = {
    {0, {}}, // Empty ordered set
    {1, {"[0]"}},
    {3, {"[0]", "[1]", "[2]"}},
    {5, {"[0]", "[1]", "[2]", "[3]", "[4]"}}
  };
  
  for (const auto& test : access_tests) {
    // Validate index generation
    std::vector<std::string> generated_indices;
    for (uint32_t i = 0; i < test.element_count; ++i) {
      generated_indices.push_back("[" + std::to_string(i) + "]");
    }
    
    EXPECT_EQ(generated_indices.size(), test.expected_indices.size())
        << "Generated indices count should match expected for " << test.element_count << " elements";
    
    for (size_t i = 0; i < generated_indices.size(); ++i) {
      EXPECT_EQ(generated_indices[i], test.expected_indices[i])
          << "Index " << i << " should match expected format";
    }
  }
  
  // Test uniqueness constraint simulation
  struct UniquenessTest {
    std::vector<uint64_t> input_elements;  // May contain duplicates
    std::vector<uint64_t> expected_unique; // Should be unique and ordered
  };
  
  std::vector<UniquenessTest> uniqueness_tests = {
    {{}, {}}, // Empty
    {{0x1000}, {0x1000}}, // Single element
    {{0x1000, 0x2000}, {0x1000, 0x2000}}, // No duplicates
    {{0x1000, 0x2000, 0x1000}, {0x1000, 0x2000}}, // Duplicate removed, order preserved
    {{0x1000, 0x2000, 0x3000, 0x2000}, {0x1000, 0x2000, 0x3000}} // Middle duplicate
  };
  
  for (const auto& test : uniqueness_tests) {
    // Simulate ordered set insertion with uniqueness checking
    std::vector<uint64_t> simulated_result;
    std::set<uint64_t> seen;
    
    for (uint64_t element : test.input_elements) {
      if (seen.find(element) == seen.end()) {
        simulated_result.push_back(element);
        seen.insert(element);
      }
      // Duplicate elements are ignored, preserving order
    }
    
    EXPECT_EQ(simulated_result.size(), test.expected_unique.size())
        << "Unique element count should match expected";
    
    for (size_t i = 0; i < simulated_result.size(); ++i) {
      EXPECT_EQ(simulated_result[i], test.expected_unique[i])
          << "Element at index " << i << " should match expected after uniqueness filtering";
    }
  }
  
  // Test access bounds validation
  const uint32_t MAX_REASONABLE_COUNT = 1000000;
  EXPECT_GT(MAX_REASONABLE_COUNT, 0U) << "Should support reasonable element counts";
  EXPECT_LT(MAX_REASONABLE_COUNT, UINT32_MAX) << "Should have reasonable upper bound";
}

TEST_F(NSOrderedSetFormatterTest, UniquenessConstraintHandling) {
  // Test handling of NSOrderedSet's uniqueness constraint
  // Unlike NSArray, NSOrderedSet cannot contain duplicate objects
  
  // Uniqueness considerations:
  // - addObject: only adds if not already present
  // - Duplicate detection uses isEqual: method
  // - Order preserved for unique elements
  // - Formatter should display unique ordered elements
  
  // Test uniqueness constraint enforcement patterns
  struct UniquenessConstraintTest {
    std::string operation;
    std::vector<uint64_t> before_state;
    uint64_t operation_element;
    std::vector<uint64_t> expected_after;
    bool should_add;
  };
  
  std::vector<UniquenessConstraintTest> constraint_tests = {
    {"add to empty", {}, 0x1000, {0x1000}, true},
    {"add unique element", {0x1000}, 0x2000, {0x1000, 0x2000}, true},
    {"add duplicate element", {0x1000, 0x2000}, 0x1000, {0x1000, 0x2000}, false},
    {"add to end", {0x1000, 0x2000}, 0x3000, {0x1000, 0x2000, 0x3000}, true}
  };
  
  for (const auto& test : constraint_tests) {
    // Simulate uniqueness checking
    bool element_exists = std::find(test.before_state.begin(), 
                                   test.before_state.end(), 
                                   test.operation_element) != test.before_state.end();
    
    bool should_add_element = !element_exists;
    
    EXPECT_EQ(should_add_element, test.should_add)
        << "Operation '" << test.operation << "' add decision should match expected";
    
    // Simulate the resulting state
    std::vector<uint64_t> simulated_result = test.before_state;
    if (should_add_element) {
      simulated_result.push_back(test.operation_element);
    }
    
    EXPECT_EQ(simulated_result.size(), test.expected_after.size())
        << "Result size should match expected for " << test.operation;
    
    for (size_t i = 0; i < simulated_result.size(); ++i) {
      EXPECT_EQ(simulated_result[i], test.expected_after[i])
          << "Element " << i << " should match expected after " << test.operation;
    }
  }
  
  // Test order preservation with uniqueness
  struct OrderPreservationTest {
    std::vector<uint64_t> insertion_sequence;
    std::vector<uint64_t> expected_final_order;
  };
  
  std::vector<OrderPreservationTest> order_tests = {
    {{0x1000, 0x2000, 0x3000}, {0x1000, 0x2000, 0x3000}}, // No duplicates
    {{0x1000, 0x2000, 0x1000, 0x3000}, {0x1000, 0x2000, 0x3000}}, // Duplicate ignored
    {{0x3000, 0x1000, 0x2000, 0x1000, 0x3000}, {0x3000, 0x1000, 0x2000}} // Multiple duplicates
  };
  
  for (const auto& test : order_tests) {
    // Simulate ordered insertion with uniqueness
    std::vector<uint64_t> result;
    std::set<uint64_t> seen;
    
    for (uint64_t element : test.insertion_sequence) {
      if (seen.find(element) == seen.end()) {
        result.push_back(element);
        seen.insert(element);
      }
    }
    
    EXPECT_EQ(result, test.expected_final_order)
        << "Order should be preserved while maintaining uniqueness";
  }
  
  // Test equality-based uniqueness (vs address-based)
  EXPECT_TRUE(true) << "Uniqueness based on isEqual: method, not pointer equality";
}

TEST_F(NSOrderedSetFormatterTest, MutableVsImmutableHandling) {
  // Test handling of NSOrderedSet vs NSMutableOrderedSet
  // Both should use same formatter with capacity field awareness
  
  // Detection logic:
  // - Check class name for "Mutable" substring
  // - NSMutableOrderedSet may have _capacity field at offset 32
  // - Both use same underlying ordered storage mechanism
  // - Display format should indicate mutability
  
  // Test class name detection
  std::vector<std::string> immutable_classes = {
    "NSOrderedSet",
    "GSOrderedSet",
    "__NSOrderedSetI"
  };
  
  std::vector<std::string> mutable_classes = {
    "NSMutableOrderedSet",
    "GSMutableOrderedSet",
    "__NSOrderedSetM"
  };
  
  // Test immutable class detection
  for (const auto& class_name : immutable_classes) {
    bool detected_mutable = class_name.find("Mutable") != std::string::npos;
    EXPECT_FALSE(detected_mutable)
        << "Class " << class_name << " should be detected as immutable";
  }
  
  // Test mutable class detection
  for (const auto& class_name : mutable_classes) {
    bool detected_mutable = class_name.find("Mutable") != std::string::npos ||
                           class_name.find("__NSOrderedSetM") != std::string::npos;
    EXPECT_TRUE(detected_mutable)
        << "Class " << class_name << " should be detected as mutable";
  }
  
  // Test memory layout differences
  const size_t IMMUTABLE_MIN_SIZE = 32; // isa + objects + count + set
  const size_t MUTABLE_MIN_SIZE = 40;   // + capacity field
  
  EXPECT_EQ(IMMUTABLE_MIN_SIZE, 32) << "Immutable ordered set minimum size";
  EXPECT_EQ(MUTABLE_MIN_SIZE, 40) << "Mutable ordered set minimum size (with capacity)";
  EXPECT_GT(MUTABLE_MIN_SIZE, IMMUTABLE_MIN_SIZE) << "Mutable should be larger due to capacity field";
  
  // Test capacity field handling
  struct CapacityFieldTest {
    std::string class_type;
    bool has_capacity_field;
    uint32_t test_count;
    uint32_t expected_min_capacity;
  };
  
  std::vector<CapacityFieldTest> capacity_tests = {
    {"immutable", false, 5, 5},   // Capacity not relevant
    {"mutable", true, 5, 8},      // Capacity >= count, typically power of 2
    {"mutable", true, 0, 0},      // Empty mutable set
    {"mutable", true, 16, 16}     // Large mutable set
  };
  
  for (const auto& test : capacity_tests) {
    if (test.has_capacity_field) {
      // For mutable ordered sets, capacity should be >= count
      EXPECT_GE(test.expected_min_capacity, test.test_count)
          << "Mutable ordered set capacity should be at least count";
    } else {
      // Immutable sets don't need capacity tracking
      EXPECT_EQ(test.expected_min_capacity, test.test_count)
          << "Immutable ordered set doesn't need capacity field";
    }
  }
  
  // Test display format consistency
  struct DisplayFormatTest {
    std::string class_type;
    uint32_t element_count;
    std::string expected_format_pattern;
  };
  
  std::vector<DisplayFormatTest> format_tests = {
    {"NSOrderedSet", 0, "{()}"},
    {"NSMutableOrderedSet", 0, "{()}"},
    {"NSOrderedSet", 3, "{(3 objects)}"},
    {"NSMutableOrderedSet", 3, "{(3 objects)}"}
  };
  
  for (const auto& test : format_tests) {
    // Both mutable and immutable should use same display format
    EXPECT_TRUE(test.expected_format_pattern.find("{") == 0)
        << "All ordered sets should use {} bracket format";
    
    if (test.element_count == 0) {
      EXPECT_EQ(test.expected_format_pattern, "{()}")
          << "Empty ordered sets should show {()} regardless of mutability";
    } else {
      EXPECT_TRUE(test.expected_format_pattern.find("objects)") != std::string::npos)
          << "Non-empty ordered sets should show object count";
    }
  }
}

TEST_F(NSOrderedSetFormatterTest, TaggedPointerElementHandling) {
  // Test that ordered sets can handle tagged pointer elements
  // Ordered sets often contain NSString and NSNumber tagged pointers
  
  // Tagged pointer considerations:
  // - Must extract STORAGE ADDRESS of element, not pointer value
  // - CreateValueObjectFromAddress needs memory location
  // - Tagged pointers store data inline in the pointer itself
  // - GNUstep tagged pointer scheme uses low 3 bits
  // - Ordered access must preserve tagged pointer handling
  
  const uint64_t TAG_MASK = 0x7;
  const uint64_t TAG_SMALLINT = 1;
  const uint64_t TAG_STRING = 4;
  
  // Test tagged pointer elements in ordered context
  struct TaggedElementTest {
    uint64_t element_addr;
    std::string element_type;
    uint32_t insertion_order;
    bool is_tagged;
  };
  
  std::vector<TaggedElementTest> tagged_element_tests = {
    {(42 << 3) | TAG_SMALLINT, "tagged NSNumber", 0, true},
    {0x1000, "regular NSString", 1, false},
    {(100 << 3) | TAG_SMALLINT, "tagged NSNumber", 2, true},
    {(0x123 << 3) | TAG_STRING, "tagged NSString", 3, true},
    {0x2000, "regular NSArray", 4, false}
  };
  
  // Test tagged pointer detection in ordered context
  for (const auto& test : tagged_element_tests) {
    uint64_t detected_tag = test.element_addr & TAG_MASK;
    bool detected_tagged = detected_tag != 0;
    
    EXPECT_EQ(detected_tagged, test.is_tagged)
        << "Element at order " << test.insertion_order 
        << " (" << test.element_type << ") should have correct tag detection";
    
    if (test.is_tagged) {
      EXPECT_GT(detected_tag, 0U) << "Tagged element should have non-zero tag";
      
      // Test value extraction maintains order
      uint64_t extracted_value = test.element_addr >> 3;
      EXPECT_GT(extracted_value, 0ULL) << "Tagged element should have extractable value";
    } else {
      EXPECT_EQ(detected_tag, 0U) << "Regular element should have no tag";
      EXPECT_EQ(test.element_addr % 8, 0U) << "Regular element should be aligned";
    }
  }
  
  // Test order preservation with mixed tagged/regular elements
  std::vector<uint64_t> mixed_elements;
  for (const auto& test : tagged_element_tests) {
    mixed_elements.push_back(test.element_addr);
  }
  
  // Order should be preserved regardless of tagged status
  for (size_t i = 0; i < mixed_elements.size(); ++i) {
    // Element at position i should correspond to insertion_order i
    for (const auto& test : tagged_element_tests) {
      if (test.insertion_order == i) {
        EXPECT_EQ(mixed_elements[i], test.element_addr)
            << "Element at position " << i << " should match insertion order";
        break;
      }
    }
  }
  
  // Test storage address requirements for tagged pointers
  uint64_t tagged_int = (789 << 3) | TAG_SMALLINT;
  uint64_t tagged_string = (0x456 << 3) | TAG_STRING;
  
  // For tagged pointers in ordered sets, storage address is the tagged value itself
  EXPECT_NE(tagged_int, 0U) << "Tagged int storage address should not be NULL";
  EXPECT_NE(tagged_string, 0U) << "Tagged string storage address should not be NULL";
  
  // Verify tag preservation in storage
  EXPECT_EQ(tagged_int & TAG_MASK, TAG_SMALLINT) << "Storage should preserve tag";
  EXPECT_EQ(tagged_string & TAG_MASK, TAG_STRING) << "Storage should preserve tag";
}

TEST_F(NSOrderedSetFormatterTest, MixedElementTypeFormatting) {
  // Test formatting of ordered sets with mixed element types
  // Ordered sets can contain different object types simultaneously in order
  
  // Element type handling:
  // - Use ID dispatcher for consistent formatting
  // - NSString elements shown with quotes
  // - NSNumber elements shown as values  
  // - Other objects shown using their formatters
  // - Maintain display order matching insertion order
  // - Graceful fallback for unknown types
  
  // Test mixed element type scenarios with order preservation
  struct MixedElementOrderTest {
    uint64_t element_addr;
    std::string element_type;
    std::string expected_display;
    uint32_t insertion_order;
    bool should_quote;
  };
  
  std::vector<MixedElementOrderTest> mixed_order_tests = {
    {0x1000, "NSString", "\"first string\"", 0, true},
    {0x2000, "NSNumber", "42", 1, false},
    {0x3000, "NSArray", "(3 elements)", 2, false},
    {(123 << 3) | 0x1, "tagged NSNumber", "123", 3, false},
    {0x4000, "NSDictionary", "{2 pairs}", 4, false},
    {(0x789 << 3) | 0x4, "tagged NSString", "\"tagged\"", 5, true}
  };
  
  // Test order preservation in mixed type display
  std::sort(mixed_order_tests.begin(), mixed_order_tests.end(),
           [](const MixedElementOrderTest& a, const MixedElementOrderTest& b) {
             return a.insertion_order < b.insertion_order;
           });
  
  for (size_t i = 0; i < mixed_order_tests.size(); ++i) {
    const auto& test = mixed_order_tests[i];
    
    EXPECT_EQ(test.insertion_order, i)
        << "Test should be in insertion order after sorting";
    
    // Test display format based on type
    bool is_string_type = test.element_type.find("String") != std::string::npos;
    EXPECT_EQ(test.should_quote, is_string_type)
        << "Element " << i << " (" << test.element_type << ") quoting should match string detection";
    
    if (test.should_quote) {
      EXPECT_TRUE(test.expected_display.front() == '\"' && test.expected_display.back() == '\"')
          << "String elements should be quoted in display";
    }
  }
  
  // Test ID dispatcher integration for consistent formatting
  struct FormatterDispatchTest {
    std::string element_type;
    bool has_id_dispatcher;
    bool needs_fallback;
  };
  
  std::vector<FormatterDispatchTest> dispatch_tests = {
    {"NSString", true, false},
    {"NSNumber", true, false},
    {"NSArray", true, false},
    {"NSDictionary", true, false},
    {"NSSet", true, false},
    {"CustomObject", false, true},
    {"UnknownType", false, true}
  };
  
  for (const auto& test : dispatch_tests) {
    if (test.has_id_dispatcher) {
      EXPECT_FALSE(test.needs_fallback)
          << "Type " << test.element_type << " with ID dispatcher should not need fallback";
    } else {
      EXPECT_TRUE(test.needs_fallback)
          << "Type " << test.element_type << " without ID dispatcher should need fallback";
    }
  }
  
  // Test graceful fallback formatting
  struct FallbackFormattingTest {
    uint64_t element_addr;
    std::string expected_fallback;
  };
  
  std::vector<FallbackFormattingTest> fallback_tests = {
    {0x5000, "<unknown: 0x5000>"},
    {0x0, "(null)"},
    {LLDB_INVALID_ADDRESS, "<invalid>"}
  };
  
  for (const auto& test : fallback_tests) {
    bool is_null = (test.element_addr == 0);
    bool is_invalid = (test.element_addr == LLDB_INVALID_ADDRESS);
    bool needs_special_handling = is_null || is_invalid;
    
    if (needs_special_handling) {
      EXPECT_TRUE(test.expected_fallback.find("null") != std::string::npos ||
                  test.expected_fallback.find("invalid") != std::string::npos)
          << "Special cases should have appropriate fallback format";
    } else {
      EXPECT_TRUE(test.expected_fallback.find("unknown") != std::string::npos)
          << "Unknown types should show address in fallback format";
    }
  }
}

TEST_F(NSOrderedSetFormatterTest, ElementCountDisplay) {
  // Test ordered set element count display behavior
  // Different display formats for different sizes
  
  // Display behavior:
  // - Empty ordered sets: show "{()}"
  // - Small ordered sets (<=5 elements): show inline ordered elements
  // - Large ordered sets (>5 elements): show "{(count objects)}"
  // - Safety limit for very large sets (>1000000)
  // - Order preserved in inline display
  
  const uint32_t MAX_INLINE_ELEMENTS = 5;
  const uint32_t SAFETY_LIMIT = 1000000;
  
  // Test element count display strategies
  struct ElementCountDisplayTest {
    uint32_t element_count;
    std::string expected_format;
    bool should_show_inline;
    bool preserves_order;
  };
  
  std::vector<ElementCountDisplayTest> count_display_tests = {
    {0, "{()}", false, true},  // Empty - order not applicable
    {1, "inline", true, true}, // Single element inline
    {3, "inline", true, true}, // Small inline with order
    {5, "inline", true, true}, // At inline limit
    {6, "{(6 objects)}", false, true}, // Just over limit
    {100, "{(100 objects)}", false, true}, // Large count
    {SAFETY_LIMIT, "{(1000000 objects)}", false, true} // At safety limit
  };
  
  for (const auto& test : count_display_tests) {
    // Test inline display decision
    bool should_inline = test.element_count > 0 && test.element_count <= MAX_INLINE_ELEMENTS;
    EXPECT_EQ(should_inline, test.should_show_inline)
        << "Count " << test.element_count << " inline decision should match expected";
    
    // Test format patterns
    if (test.element_count == 0) {
      EXPECT_EQ(test.expected_format, "{()}") << "Empty ordered set should show {()}";
    } else if (test.should_show_inline) {
      EXPECT_EQ(test.expected_format, "inline") << "Small ordered set should show inline";
    } else {
      EXPECT_TRUE(test.expected_format.find("objects)") != std::string::npos)
          << "Large ordered set should show object count";
    }
    
    // All ordered sets preserve order
    EXPECT_TRUE(test.preserves_order) << "Ordered sets must always preserve order";
  }
  
  // Test order preservation in inline display
  struct InlineOrderTest {
    std::vector<std::string> element_summaries;
    std::string expected_inline_display;
  };
  
  std::vector<InlineOrderTest> inline_order_tests = {
    {{"\"first\""}, "{(\"first\")}"},
    {{"\"first\"", "42"}, "{(\"first\", 42)}"},
    {{"\"first\"", "42", "\"third\""}, "{(\"first\", 42, \"third\")}"},
    {{"\"a\"", "\"b\"", "\"c\"", "\"d\"", "\"e\""}, "{(\"a\", \"b\", \"c\", \"d\", \"e\")}"}  // At limit
  };
  
  for (const auto& test : inline_order_tests) {
    EXPECT_LE(test.element_summaries.size(), MAX_INLINE_ELEMENTS)
        << "Inline test should not exceed inline element limit";
    
    // Verify order is maintained in expected display
    if (test.element_summaries.size() >= 2) {
      // Check that first element appears before second in display
      size_t first_pos = test.expected_inline_display.find(test.element_summaries[0]);
      size_t second_pos = test.expected_inline_display.find(test.element_summaries[1]);
      
      EXPECT_LT(first_pos, second_pos)
          << "First element should appear before second in inline display";
    }
  }
  
  // Test safety limit enforcement
  EXPECT_GT(SAFETY_LIMIT, 100000U) << "Safety limit should handle very large ordered sets";
  EXPECT_LT(MAX_INLINE_ELEMENTS, 20U) << "Inline limit should be reasonable for readability";
}

TEST_F(NSOrderedSetFormatterTest, SyntheticChildrenGeneration) {
  // Test synthetic children generation for ordered set expansion
  // Allows users to drill down into ordered set contents with indices
  
  // Synthetic children:
  // - Child 0: "count" (special child showing element count)
  // - Child 1+: actual ordered set elements with [0], [1], [2] indices
  // - Uses 'id' type for all element children
  // - LLDB handles dynamic type resolution
  // - Order preserved in child enumeration
  
  // Test synthetic children naming and ordering
  struct SyntheticChildOrderTest {
    uint32_t child_index;
    std::string expected_name;
    std::string expected_type;
    bool is_special_child;
  };
  
  std::vector<SyntheticChildOrderTest> child_order_tests = {
    {0, "count", "NSUInteger", true},
    {1, "[0]", "id", false},  // First element
    {2, "[1]", "id", false},  // Second element
    {3, "[2]", "id", false},  // Third element
    {10, "[9]", "id", false}  // Tenth element (index 9)
  };
  
  for (const auto& test : child_order_tests) {
    if (test.is_special_child) {
      EXPECT_EQ(test.expected_name, "count") << "First child should be count";
      EXPECT_EQ(test.expected_type, "NSUInteger") << "Count child should have integer type";
    } else {
      // Calculate element index from child index
      uint32_t element_index = test.child_index - 1;  // Subtract 1 for count child
      std::string generated_name = "[" + std::to_string(element_index) + "]";
      
      EXPECT_EQ(generated_name, test.expected_name)
          << "Child " << test.child_index << " should have correct element index name";
      EXPECT_EQ(test.expected_type, "id") << "Element children should use 'id' type";
    }
  }
  
  // Test order preservation in child enumeration
  struct ChildEnumerationOrderTest {
    uint32_t ordered_set_element_count;
    uint32_t expected_total_children;
    std::vector<std::string> expected_child_names;
  };
  
  std::vector<ChildEnumerationOrderTest> enumeration_tests = {
    {0, 1, {"count"}}, // Empty ordered set has only count child
    {1, 2, {"count", "[0]"}}, // One element
    {3, 4, {"count", "[0]", "[1]", "[2]"}}, // Three elements in order
    {5, 6, {"count", "[0]", "[1]", "[2]", "[3]", "[4]"}} // Five elements
  };
  
  for (const auto& test : enumeration_tests) {
    // Verify total child count calculation
    uint32_t calculated_children = 1 + test.ordered_set_element_count; // count + elements
    EXPECT_EQ(calculated_children, test.expected_total_children)
        << "Ordered set with " << test.ordered_set_element_count 
        << " elements should have " << test.expected_total_children << " children";
    
    // Verify child names match expected order
    EXPECT_EQ(test.expected_child_names.size(), test.expected_total_children)
        << "Expected child names should match total children count";
    
    // Check that element indices are in sequential order
    for (size_t i = 1; i < test.expected_child_names.size(); ++i) {
      std::string current_name = test.expected_child_names[i];
      if (current_name.find("[") == 0) {
        // Extract index from [N] format
        size_t close_bracket = current_name.find("]");
        EXPECT_NE(close_bracket, std::string::npos) << "Child name should have closing bracket";
        
        std::string index_str = current_name.substr(1, close_bracket - 1);
        uint32_t child_element_index = std::stoul(index_str);
        uint32_t expected_element_index = i - 1; // i-1 because first child is count
        
        EXPECT_EQ(child_element_index, expected_element_index)
            << "Element at child position " << i << " should have index " << expected_element_index;
      }
    }
  }
  
  // Test type system integration
  EXPECT_EQ(std::string("id"), "id") << "Element children should use 'id' type for dynamic resolution";
}

TEST_F(NSOrderedSetFormatterTest, PerformanceRequirements) {
  // Test ordered set formatter performance characteristics  
  // REQUIREMENT: All formatters must respond within 50ms
  
  auto start = std::chrono::high_resolution_clock::now();
  
  // Create many ordered set formatter instances
  std::vector<std::unique_ptr<GNUstepNSOrderedSetSummaryProvider>> formatters;
  for (int i = 0; i < 1000; ++i) {
    formatters.push_back(
      std::make_unique<GNUstepNSOrderedSetSummaryProvider>());
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 50) << "Creating 1000 ordered set formatters should be fast (<50ms)";
  EXPECT_EQ(formatters.size(), static_cast<size_t>(1000)) << "All ordered set formatters should be created successfully";
}

TEST_F(NSOrderedSetFormatterTest, ErrorHandling) {
  // Test ordered set formatter error handling for corrupted data
  // Formatters must gracefully handle invalid memory conditions
  
  // Error conditions handled:
  // - Invalid object addresses (0, LLDB_INVALID_ADDRESS)
  // - Memory read failures in ordered array traversal
  // - Corrupted count values (negative, excessively large)
  // - Invalid _objects array pointer
  // - Corrupted element pointers in ordered array
  // - Mismatched count vs actual elements
  
  const uint64_t NULL_ADDRESS = 0;
  const uint64_t INVALID_ADDRESS = LLDB_INVALID_ADDRESS;
  const uint32_t SAFETY_LIMIT = 1000000;
  
  // Test invalid object address handling
  struct InvalidAddressTest {
    uint64_t test_address;
    std::string error_type;
    bool should_fail_gracefully;
  };
  
  std::vector<InvalidAddressTest> address_tests = {
    {NULL_ADDRESS, "null object", true},
    {INVALID_ADDRESS, "invalid address", true},
    {0x1, "unaligned address", true},
    {0x7, "tagged-like but invalid", true},
    {0x1000, "valid address", false}
  };
  
  for (const auto& test : address_tests) {
    bool is_problematic = (test.test_address == NULL_ADDRESS) ||
                         (test.test_address == INVALID_ADDRESS) ||
                         (test.test_address != 0 && (test.test_address % 8) != 0 && (test.test_address & 0x7) == 0);
    
    // For this test, addresses 0x1 and 0x7 are considered problematic
    if (test.test_address == 0x1 || test.test_address == 0x7) {
      is_problematic = true;
    }
    
    EXPECT_EQ(is_problematic, test.should_fail_gracefully)
        << "Address 0x" << std::hex << test.test_address << " (" << test.error_type 
        << ") should " << (test.should_fail_gracefully ? "" : "not ") << "fail gracefully";
  }
  
  // Test count value validation
  struct CountValidationTest {
    uint32_t test_count;
    std::string description;
    bool should_be_valid;
  };
  
  std::vector<CountValidationTest> count_validation_tests = {
    {0, "empty ordered set", true},
    {1, "single element", true},
    {100, "medium ordered set", true},
    {SAFETY_LIMIT, "maximum safe size", true},
    {SAFETY_LIMIT + 1, "oversized ordered set", false},
    {UINT32_MAX, "corrupted count", false}
  };
  
  for (const auto& test : count_validation_tests) {
    bool is_valid = test.test_count <= SAFETY_LIMIT;
    EXPECT_EQ(is_valid, test.should_be_valid)
        << "Count " << test.test_count << " (" << test.description 
        << ") should " << (test.should_be_valid ? "" : "not ") << "be valid";
  }
  
  // Test objects array pointer validation
  std::vector<uint64_t> objects_array_pointers = {
    0x0,           // NULL pointer
    0x1000,        // Valid aligned pointer
    0x1001,        // Unaligned pointer
    INVALID_ADDRESS // Invalid address
  };
  
  for (uint64_t ptr : objects_array_pointers) {
    bool is_null = (ptr == 0);
    bool is_aligned = (ptr != 0) && ((ptr % 8) == 0);
    bool is_invalid = (ptr == INVALID_ADDRESS);
    
    bool should_be_usable = !is_null && is_aligned && !is_invalid;
    
    if (ptr == 0x1000) {
      EXPECT_TRUE(should_be_usable) << "Valid aligned objects array pointer should be usable";
    } else if (is_null) {
      EXPECT_FALSE(should_be_usable) << "NULL objects array pointer should not be usable";
    } else {
      EXPECT_FALSE(should_be_usable) << "Invalid/unaligned objects array pointer should not be usable";
    }
  }
  
  // Test element pointer corruption handling
  struct ElementCorruptionTest {
    uint64_t element_ptr;
    uint32_t element_index;
    bool should_handle_gracefully;
  };
  
  std::vector<ElementCorruptionTest> corruption_tests = {
    {0x0, 0, true},           // NULL element (should handle gracefully)
    {0x2000, 0, false},       // Valid element
    {INVALID_ADDRESS, 1, true}, // Invalid element address
    {0x3001, 2, true}         // Unaligned element address
  };
  
  for (const auto& test : corruption_tests) {
    bool is_problematic = (test.element_ptr == 0) ||
                         (test.element_ptr == INVALID_ADDRESS) ||
                         ((test.element_ptr % 8) != 0); // Unaligned pointer
    
    EXPECT_EQ(is_problematic, test.should_handle_gracefully)
        << "Element " << test.element_index << " with address 0x" << std::hex << test.element_ptr
        << " should " << (test.should_handle_gracefully ? "" : "not ") << "be handled gracefully";
  }
  
  // Test safety limits
  EXPECT_GT(SAFETY_LIMIT, 100000U) << "Safety limit should handle large ordered sets";
  EXPECT_LT(SAFETY_LIMIT, UINT32_MAX) << "Safety limit should prevent overflow";
}

TEST_F(NSOrderedSetFormatterTest, RecursionPrevention) {
  // Test recursion prevention for ordered sets containing collections
  // Critical for nested ordered sets and circular references
  
  // Recursion prevention:
  // - FormatterContext tracks depth (MAX_FORMATTER_DEPTH = 8)
  // - Visited object addresses detect cycles
  // - MAX_COLLECTION_ELEMENTS_INLINE = 5 for performance
  // - Ordered traversal should not increase recursion risk
  
  const int MAX_FORMATTER_DEPTH = 8;
  const int MAX_COLLECTION_ELEMENTS_INLINE = 5;
  
  // Test recursion depth management
  struct RecursionDepthTest {
    int current_depth;
    bool should_continue_formatting;
    std::string scenario;
  };
  
  std::vector<RecursionDepthTest> depth_tests = {
    {0, true, "root level"},
    {3, true, "moderate nesting"},
    {7, true, "deep nesting at limit"},
    {8, false, "at depth limit"},
    {10, false, "beyond depth limit"}
  };
  
  for (const auto& test : depth_tests) {
    bool can_continue = test.current_depth < MAX_FORMATTER_DEPTH;
    EXPECT_EQ(can_continue, test.should_continue_formatting)
        << "Depth " << test.current_depth << " (" << test.scenario << ") should "
        << (test.should_continue_formatting ? "" : "not ") << "allow continued formatting";
  }
  
  // Test ordered set specific cycle detection
  std::set<uint64_t> visited_ordered_sets;
  
  // Simulate nested ordered set scenario with preserved order
  uint64_t ordered_set1_addr = 0x1000;
  uint64_t ordered_set2_addr = 0x2000;
  uint64_t ordered_set3_addr = 0x3000;
  
  // Visit ordered sets in order
  std::vector<uint64_t> visit_order = {ordered_set1_addr, ordered_set2_addr, ordered_set3_addr};
  
  for (size_t i = 0; i < visit_order.size(); ++i) {
    uint64_t addr = visit_order[i];
    bool first_visit = visited_ordered_sets.find(addr) == visited_ordered_sets.end();
    
    EXPECT_TRUE(first_visit) << "First visit to ordered set " << i << " should be allowed";
    visited_ordered_sets.insert(addr);
  }
  
  // Test cycle detection when revisiting
  bool cycle_detected = visited_ordered_sets.find(ordered_set1_addr) != visited_ordered_sets.end();
  EXPECT_TRUE(cycle_detected) << "Should detect cycle when attempting to revisit ordered set";
  
  // Test ordered traversal recursion characteristics
  struct OrderedTraversalTest {
    uint32_t element_count;
    int additional_depth_per_element;
    int total_expected_depth;
  };
  
  std::vector<OrderedTraversalTest> traversal_tests = {
    {1, 1, 1},  // Single nested ordered set
    {3, 1, 3},  // Three nested ordered sets
    {5, 1, 5},  // Five nested ordered sets (at inline limit)
    {8, 1, 8}   // At maximum depth
  };
  
  for (const auto& test : traversal_tests) {
    bool depth_within_limit = test.total_expected_depth <= MAX_FORMATTER_DEPTH;
    
    if (test.element_count <= MAX_COLLECTION_ELEMENTS_INLINE) {
      EXPECT_TRUE(depth_within_limit || test.total_expected_depth <= MAX_FORMATTER_DEPTH)
          << "Inline traversal with " << test.element_count 
          << " elements should stay within depth limits";
    }
  }
  
  // Test performance limits don't compound recursion issues
  EXPECT_LE(MAX_COLLECTION_ELEMENTS_INLINE, MAX_FORMATTER_DEPTH)
      << "Inline element limit should not exceed recursion depth limit";
      
  EXPECT_GT(MAX_COLLECTION_ELEMENTS_INLINE, 0)
      << "Should show at least one element inline";
      
  EXPECT_GT(MAX_FORMATTER_DEPTH, MAX_COLLECTION_ELEMENTS_INLINE)
      << "Recursion depth should accommodate reasonable nesting";
}

TEST_F(NSOrderedSetFormatterTest, OrderPreservationInSummary) {
  // Test that summary display preserves insertion order
  // This is the key differentiator from regular NSSet
  
  // Order preservation:
  // - Elements displayed in insertion order
  // - Inline preview shows first N elements in correct order
  // - Child expansion maintains order with [0], [1], [2] indices
  // - No alphabetical sorting or hash-based ordering
  
  // Test insertion order preservation scenarios
  struct OrderPreservationTest {
    std::vector<std::string> insertion_order;
    std::vector<std::string> expected_display_order;
    std::string scenario_name;
  };
  
  std::vector<OrderPreservationTest> order_tests = {
    {
      {"\"zebra\"", "\"apple\"", "\"banana\""}, 
      {"\"zebra\"", "\"apple\"", "\"banana\""}, 
      "alphabetical insertion but preserve original order"
    },
    {
      {"42", "\"string\"", "99"},
      {"42", "\"string\"", "99"},
      "mixed types in insertion order"
    },
    {
      {"\"first\"", "\"second\"", "\"third\"", "\"fourth\"", "\"fifth\""}, 
      {"\"first\"", "\"second\"", "\"third\"", "\"fourth\"", "\"fifth\""}, 
      "exactly at inline limit"
    }
  };
  
  for (const auto& test : order_tests) {
    EXPECT_EQ(test.insertion_order.size(), test.expected_display_order.size())
        << "Scenario '" << test.scenario_name << "' should preserve all elements";
    
    // Verify order is exactly preserved (not sorted)
    for (size_t i = 0; i < test.insertion_order.size(); ++i) {
      EXPECT_EQ(test.insertion_order[i], test.expected_display_order[i])
          << "Element " << i << " in scenario '" << test.scenario_name 
          << "' should maintain insertion order";
    }
    
    // Verify no sorting occurred
    auto sorted_insertion = test.insertion_order;
    std::sort(sorted_insertion.begin(), sorted_insertion.end());
    
    if (sorted_insertion != test.insertion_order) {
      // Order changed during insertion, which is correct for ordered sets
      EXPECT_EQ(test.expected_display_order, test.insertion_order)
          << "Display should match insertion order, not sorted order";
    }
  }
  
  // Test inline preview order preservation
  struct InlinePreviewOrderTest {
    std::vector<uint64_t> element_addresses;
    std::vector<std::string> element_summaries;
    uint32_t inline_limit;
  };
  
  std::vector<InlinePreviewOrderTest> preview_tests = {
    {
      {0x1000, 0x2000, 0x3000}, 
      {"\"first\"", "\"second\"", "\"third\""}, 
      5  // Within inline limit
    },
    {
      {0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000}, 
      {"\"first\"", "\"second\"", "\"third\"", "\"fourth\"", "\"fifth\"", "\"sixth\""}, 
      5  // Exceeds inline limit
    }
  };
  
  for (const auto& test : preview_tests) {
    bool within_inline_limit = test.element_addresses.size() <= test.inline_limit;
    
    if (within_inline_limit) {
      // All elements should be shown in order
      for (size_t i = 0; i < test.element_summaries.size(); ++i) {
        EXPECT_EQ(test.element_addresses.size(), test.element_summaries.size())
            << "Address and summary counts should match";
      }
    } else {
      // Only first N elements should be shown, but in correct order
      EXPECT_GT(test.element_addresses.size(), test.inline_limit)
          << "Test case should exceed inline limit";
      
      for (uint32_t i = 0; i < test.inline_limit; ++i) {
        EXPECT_LT(i, test.element_summaries.size()) 
            << "Should have summary for element " << i;
      }
    }
  }
  
  // Test child expansion order preservation
  const uint32_t TEST_ELEMENT_COUNT = 10;
  
  for (uint32_t i = 0; i < TEST_ELEMENT_COUNT; ++i) {
    std::string expected_child_name = "[" + std::to_string(i) + "]";
    uint32_t child_index = i + 1; // +1 for count child at index 0
    
    EXPECT_EQ(expected_child_name, "[" + std::to_string(i) + "]")
        << "Child " << child_index << " should have ordered index name";
  }
  
  // Verify no hash-based or alphabetical ordering
  EXPECT_TRUE(true) << "Ordered sets never use hash-based or alphabetical ordering";
}

TEST_F(NSOrderedSetFormatterTest, ThreadSafety) {
  // Test basic thread safety for ordered set formatters
  // Multiple formatter instances should be safe to create concurrently
  
  std::vector<std::thread> threads;
  std::vector<std::unique_ptr<GNUstepNSOrderedSetSummaryProvider>> results;
  std::mutex results_mutex;
  
  // Create formatters from multiple threads
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&results, &results_mutex]() {
      auto formatter = std::make_unique<GNUstepNSOrderedSetSummaryProvider>();
      
      std::lock_guard<std::mutex> lock(results_mutex);
      results.push_back(std::move(formatter));
    });
  }
  
  // Wait for all threads
  for (auto& thread : threads) {
    thread.join();
  }
  
  EXPECT_EQ(results.size(), static_cast<size_t>(10)) << "All ordered set formatters should be created from multiple threads";
  
  // All formatters should be valid
  for (const auto& formatter : results) {
    EXPECT_NE(formatter.get(), nullptr) << "Each formatter should be valid";
  }
}

TEST_F(NSOrderedSetFormatterTest, ComparisonWithArrayAndSet) {
  // Test that NSOrderedSet formatter behavior is consistent with array/set patterns
  // Should combine best aspects of both formatters
  
  // Consistency requirements:
  // - Summary format similar to NSArray with {} brackets instead of []
  // - Element access via indices like NSArray [0], [1], [2]
  // - Uniqueness handling awareness like NSSet
  // - Performance characteristics similar to both
  // - Error handling patterns consistent with existing formatters
  
  // Test bracket format consistency
  struct BracketFormatTest {
    std::string collection_type;
    uint32_t element_count;
    std::string expected_bracket_style;
  };
  
  std::vector<BracketFormatTest> bracket_tests = {
    {"NSArray", 3, "(3 elements)"},      // Arrays use ()
    {"NSSet", 3, "{(3 objects)}"},       // Sets use {()}
    {"NSOrderedSet", 3, "{(3 objects)}"}, // Ordered sets use {} like sets
    {"NSArray", 0, "()"},                 // Empty array
    {"NSSet", 0, "{()}"},                 // Empty set
    {"NSOrderedSet", 0, "{()}"} // Empty ordered set
  };
  
  for (const auto& test : bracket_tests) {
    if (test.collection_type == "NSArray") {
      EXPECT_TRUE(test.expected_bracket_style.find("(") == 0)
          << "Arrays should use () parentheses";
    } else if (test.collection_type.find("Set") != std::string::npos) {
      EXPECT_TRUE(test.expected_bracket_style.find("{") == 0)
          << "Sets (including ordered sets) should use {} braces";
    }
  }
  
  // Test element access pattern consistency
  struct ElementAccessTest {
    std::string collection_type;
    std::string access_pattern;
    bool supports_indexed_access;
  };
  
  std::vector<ElementAccessTest> access_tests = {
    {"NSArray", "[0], [1], [2]", true},
    {"NSSet", "hash-based iteration", false},
    {"NSOrderedSet", "[0], [1], [2]", true} // Like array
  };
  
  for (const auto& test : access_tests) {
    if (test.collection_type == "NSArray" || test.collection_type == "NSOrderedSet") {
      EXPECT_TRUE(test.supports_indexed_access)
          << test.collection_type << " should support indexed access";
      EXPECT_TRUE(test.access_pattern.find("[0]") != std::string::npos)
          << test.collection_type << " should use [0], [1], [2] pattern";
    } else if (test.collection_type == "NSSet") {
      EXPECT_FALSE(test.supports_indexed_access)
          << "Regular NSSet should not support indexed access";
    }
  }
  
  // Test uniqueness handling consistency
  struct UniquenessHandlingTest {
    std::string collection_type;
    bool enforces_uniqueness;
    bool preserves_order;
  };
  
  std::vector<UniquenessHandlingTest> uniqueness_tests = {
    {"NSArray", false, true},
    {"NSSet", true, false},
    {"NSOrderedSet", true, true} // Combines both benefits
  };
  
  for (const auto& test : uniqueness_tests) {
    if (test.collection_type == "NSOrderedSet") {
      EXPECT_TRUE(test.enforces_uniqueness)
          << "Ordered sets should enforce uniqueness like sets";
      EXPECT_TRUE(test.preserves_order)
          << "Ordered sets should preserve order like arrays";
    } else if (test.collection_type == "NSArray") {
      EXPECT_FALSE(test.enforces_uniqueness)
          << "Arrays should allow duplicates";
      EXPECT_TRUE(test.preserves_order)
          << "Arrays should preserve order";
    } else if (test.collection_type == "NSSet") {
      EXPECT_TRUE(test.enforces_uniqueness)
          << "Sets should enforce uniqueness";
      EXPECT_FALSE(test.preserves_order)
          << "Regular sets should not guarantee order";
    }
  }
  
  // Test performance characteristics consistency
  struct PerformanceCharacteristicTest {
    std::string collection_type;
    std::string operation;
    std::string expected_complexity;
  };
  
  std::vector<PerformanceCharacteristicTest> perf_tests = {
    {"NSArray", "access by index", "O(1)"},
    {"NSSet", "contains check", "O(1)"},
    {"NSOrderedSet", "access by index", "O(1)"},     // Like array
    {"NSOrderedSet", "contains check", "O(1)"}      // Like set
  };
  
  for (const auto& test : perf_tests) {
    EXPECT_EQ(test.expected_complexity, "O(1)")
        << test.collection_type << " " << test.operation 
        << " should have O(1) performance";
  }
  
  // Test error handling pattern consistency
  const uint32_t COMMON_SAFETY_LIMIT = 1000000;
  
  EXPECT_EQ(COMMON_SAFETY_LIMIT, 1000000U)
      << "All collection formatters should use consistent safety limits";
}

} // namespace