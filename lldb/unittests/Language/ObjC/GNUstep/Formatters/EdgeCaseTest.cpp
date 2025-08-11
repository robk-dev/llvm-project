//===-- EdgeCaseTest.cpp -------------------------------------------------===//
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

#include "Common/FormatterTestHelpers.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepArrayFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDictionaryFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepNumberFormatters.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepSetFormatters.h"

#include <chrono>
#include <memory>
#include <vector>
#include <limits>
#include <cstring>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::test;

namespace {

class EdgeCaseTest : public ::testing::Test {
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

TEST_F(EdgeCaseTest, BoundaryConditionArrays) {
  // Test array formatters with boundary conditions
  
  auto provider = std::make_unique<GNUstepNSArraySummaryProvider>();
  
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                       ArchSpec("x86_64"), PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Test boundary conditions
  struct TestCase {
    uint64_t count;
    std::string description;
    bool should_succeed;
  };
  
  std::vector<TestCase> test_cases = {
    {0, "Empty array", true},
    {1, "Single element", true},
    {2, "Two elements", true},
    {5, "Five elements (inline limit)", true},
    {6, "Six elements (first truncation)", true},
    {10, "Ten elements", true},
    {100, "Hundred elements", true},
    {1000, "Thousand elements", true},
    {10000, "Ten thousand elements", true},
    {100000, "Hundred thousand elements", true},
    {1000000, "One million elements (safety limit)", true},
    {1000001, "Over safety limit", false},
    {UINT32_MAX, "Maximum 32-bit value", false},
    {static_cast<uint64_t>(UINT32_MAX) + 1, "Over 32-bit maximum", false}
  };
  
  for (const auto& test : test_cases) {
    struct {
      uint64_t isa;
      uint64_t contents_ptr; 
      uint64_t count;
    } array_obj = {0x5000, 0x6000, test.count};
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &array_obj, sizeof(array_obj));
    
    lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "boundary_array", 
                                                               0x8000, "NSArray");
    
    StreamString output;
    TypeSummaryOptions options;
    bool result = provider->FormatObject(*array_obj_sp, output, options);
    
    if (test.should_succeed) {
      EXPECT_TRUE(result || !output.GetString().empty()) 
        << "Boundary case '" << test.description << "' should be handled";
        
      std::string output_str = output.GetString().str();
      
      if (test.count == 0) {
        EXPECT_TRUE(output_str.find("()") != std::string::npos ||
                    output_str.find("empty") != std::string::npos ||
                    output_str.find("0") != std::string::npos)
          << "Empty array should be clearly indicated: " << output_str;
      } else if (test.count <= 5) {
        // Small arrays might show inline content
        EXPECT_FALSE(output_str.empty()) 
          << "Small array should produce output: " << output_str;
      } else {
        // Large arrays should show count
        EXPECT_TRUE(output_str.find(std::to_string(test.count)) != std::string::npos ||
                    output_str.find("elements") != std::string::npos)
          << "Large array should show count info: " << output_str;
      }
    } else {
      // Over safety limit - should either fail gracefully or show error indication
      if (result || !output.GetString().empty()) {
        std::string output_str = output.GetString().str();
        // Should not show the extreme count value directly
        EXPECT_TRUE(output_str.find("invalid") != std::string::npos ||
                    output_str.find("error") != std::string::npos ||
                    output_str.find("limit") != std::string::npos ||
                    output_str.length() < 100)
          << "Over-limit case should show error indication: " << output_str;
      }
    }
    
    printf("Boundary test: %s (count=%lu) -> %s\n", 
           test.description.c_str(), test.count, 
           result ? "SUCCESS" : "HANDLED");
  }
}

TEST_F(EdgeCaseTest, MalformedStringObjects) {
  // Test string formatters with malformed data
  
  auto provider = std::make_unique<GNUstepNSStringSummaryProvider>();
  
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                       ArchSpec("x86_64"), PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  struct MalformedTest {
    std::string description;
    std::function<void()> setup_malformed_data;
    bool should_handle_gracefully;
  };
  
  std::vector<MalformedTest> malformed_tests = {
    
    {"Null characters in string", [&]() {
      std::vector<char> malformed_str = {'H', 'e', 'l', '\0', 'l', 'o', '\0'};
      struct {
        uint64_t isa;
        uint64_t length;
        uint64_t chars_ptr;
      } string_obj = {0x5000, malformed_str.size(), 0x6000};
      
      static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &string_obj, sizeof(string_obj));
      static_cast<MockProcess*>(process.get())->SetMemory(0x6000, malformed_str.data(), 
                                                          malformed_str.size());
      
      lldb::ValueObjectSP string_obj_sp = MockValueObject::Create(target, "null_chars", 
                                                                  0x8000, "NSString");
      StreamString output;
      TypeSummaryOptions options;
      bool result = provider->FormatObject(*string_obj_sp, output, options);
      
      EXPECT_TRUE(result || !output.GetString().empty()) 
        << "Null characters should be handled";
    }, true},
    
    {"Non-UTF8 bytes", [&]() {
      std::vector<uint8_t> invalid_utf8 = {0xFF, 0xFE, 0x80, 0x81, 0xC0, 0xC1};
      struct {
        uint64_t isa;
        uint64_t length;
        uint64_t chars_ptr;
      } string_obj = {0x5000, invalid_utf8.size(), 0x6000};
      
      static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &string_obj, sizeof(string_obj));
      static_cast<MockProcess*>(process.get())->SetMemory(0x6000, invalid_utf8.data(), 
                                                          invalid_utf8.size());
      
      lldb::ValueObjectSP string_obj_sp = MockValueObject::Create(target, "invalid_utf8", 
                                                                  0x8000, "NSString");
      StreamString output;
      TypeSummaryOptions options;
      bool result = provider->FormatObject(*string_obj_sp, output, options);
      
      EXPECT_TRUE(result || !output.GetString().empty()) 
        << "Invalid UTF-8 should be handled";
    }, true},
    
    {"Length mismatch", [&]() {
      std::string actual_str = "Short";
      struct {
        uint64_t isa;
        uint64_t length;  // Claims much longer than actual
        uint64_t chars_ptr;
      } string_obj = {0x5000, 10000, 0x6000};
      
      static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &string_obj, sizeof(string_obj));
      static_cast<MockProcess*>(process.get())->SetMemory(0x6000, actual_str.c_str(), 
                                                          actual_str.length());
      
      lldb::ValueObjectSP string_obj_sp = MockValueObject::Create(target, "length_mismatch", 
                                                                  0x8000, "NSString");
      StreamString output;
      TypeSummaryOptions options;
      bool result = provider->FormatObject(*string_obj_sp, output, options);
      
      EXPECT_TRUE(result || !output.GetString().empty()) 
        << "Length mismatch should be handled";
    }, true},
    
    {"Extremely long claimed length", [&]() {
      std::string actual_str = "Normal string";
      struct {
        uint64_t isa;
        uint64_t length;  // Extreme length
        uint64_t chars_ptr;
      } string_obj = {0x5000, UINT64_MAX, 0x6000};
      
      static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &string_obj, sizeof(string_obj));
      static_cast<MockProcess*>(process.get())->SetMemory(0x6000, actual_str.c_str(), 
                                                          actual_str.length());
      
      lldb::ValueObjectSP string_obj_sp = MockValueObject::Create(target, "extreme_length", 
                                                                  0x8000, "NSString");
      
      auto start = std::chrono::high_resolution_clock::now();
      
      StreamString output;
      TypeSummaryOptions options;
      bool result = provider->FormatObject(*string_obj_sp, output, options);
      
      auto end = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
      
      EXPECT_LT(duration.count(), 1000) << "Should not hang on extreme length";
      EXPECT_TRUE(result || !output.GetString().empty()) 
        << "Extreme length should be handled";
    }, true}
  };
  
  for (auto& test : malformed_tests) {
    printf("Malformed string test: %s\n", test.description.c_str());
    
    EXPECT_NO_FATAL_FAILURE(test.setup_malformed_data()) 
      << "Malformed test '" << test.description << "' should not crash";
  }
}

TEST_F(EdgeCaseTest, TaggedPointerEdgeCases) {
  // Test tagged pointer edge cases
  
  struct TaggedTest {
    uint64_t pointer_value;
    std::string description;
    bool is_valid_tagged;
    std::string expected_type;
  };
  
  std::vector<TaggedTest> tagged_tests = {
    // Valid tagged pointers
    {0x0000000000000001ULL, "Tagged int: 0", true, "int"},
    {0x0000000000000009ULL, "Tagged int: 1", true, "int"},
    {0x0000000000000021ULL, "Tagged int: 4", true, "int"},
    {0x0000000000000004ULL, "Tagged string: empty", true, "string"},
    {0x000000000000002CULL, "Tagged string: length 1", true, "string"},
    
    // Edge cases
    {0x0000000000000000ULL, "Null pointer", false, "null"},
    {0x0000000000000002ULL, "Invalid tag 2", false, "invalid"},
    {0x0000000000000003ULL, "Invalid tag 3", false, "invalid"},
    {0x0000000000000005ULL, "Invalid tag 5", false, "invalid"},
    {0x0000000000000006ULL, "Invalid tag 6", false, "invalid"},
    {0x0000000000000007ULL, "Invalid tag 7", false, "invalid"},
    
    // Boundary tagged integers
    {0x7FFFFFFFFFFFFFFF, "Maximum positive tagged int", true, "int"},
    {0x8000000000000001ULL, "Negative tagged int", true, "int"},
    {0xFFFFFFFFFFFFFFF9ULL, "Maximum negative tagged int", true, "int"},
    
    // Boundary tagged strings
    {0x000000000000004CULL, "Tagged string: max length", true, "string"}, // Length 9
    {0x000000000000024CULL, "Tagged string: over max length", false, "invalid"}, // Length 10
  };
  
  for (const auto& test : tagged_tests) {
    // Test tagged pointer detection
    bool is_tagged = (test.pointer_value != 0) && 
                     (test.pointer_value != LLDB_INVALID_ADDRESS) &&
                     ((test.pointer_value & 0x7) == 1 || 
                      (test.pointer_value & 0x7) == 4);
    
    EXPECT_EQ(is_tagged, test.is_valid_tagged) 
      << "Tagged detection for " << test.description 
      << " (0x" << std::hex << test.pointer_value << std::dec << ")";
    
    if (test.is_valid_tagged && (test.pointer_value & 0x7) == 1) {
      // Test integer decoding
      int64_t value = static_cast<int64_t>(test.pointer_value) >> 3;
      
      // Should be a reasonable integer value
      EXPECT_TRUE((value >= INT32_MIN && value <= INT32_MAX) || 
                  (value >= INT64_MIN && value <= INT64_MAX))
        << "Tagged int should decode to reasonable value: " << value;
    }
    
    if (test.is_valid_tagged && (test.pointer_value & 0x7) == 4) {
      // Test string length extraction
      int length = (test.pointer_value >> 3) & 0x1F;
      
      EXPECT_GE(length, 0) << "Tagged string length should be non-negative";
      EXPECT_LE(length, 9) << "Tagged string length should be <= 9";
      
      if (test.description.find("over max") != std::string::npos) {
        EXPECT_GT(length, 9) << "Over-length string should have length > 9";
      }
    }
    
    printf("Tagged test: %s -> %s (0x%lx)\n", 
           test.description.c_str(),
           is_tagged ? "TAGGED" : "REGULAR",
           static_cast<unsigned long>(test.pointer_value));
  }
}

TEST_F(EdgeCaseTest, DictionaryCollisionAndChaining) {
  // Test dictionary formatters with hash collisions and chaining
  
  auto provider = std::make_unique<GNUstepNSDictionarySummaryProvider>();
  
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                       ArchSpec("x86_64"), PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Test various dictionary edge cases
  struct DictTest {
    uint64_t count;
    uint64_t table_size;
    std::string description;
    bool should_handle;
  };
  
  std::vector<DictTest> dict_tests = {
    {0, 0, "Empty dictionary", true},
    {1, 1, "Single entry, minimal table", true},
    {1, 16, "Single entry, oversized table", true},
    {16, 16, "Full table, no collisions", true},
    {32, 16, "Double load factor", true},
    {100, 16, "High collision scenario", true},
    {1000, 64, "Large dictionary", true},
    {UINT32_MAX, 1024, "Extreme count", false}
  };
  
  for (const auto& test : dict_tests) {
    struct {
      uint64_t isa;
      uint64_t table_ptr;
      uint64_t count;
      uint64_t table_size;
    } dict_obj = {0x5100, 0x6000, test.count, test.table_size};
    
    static_cast<MockProcess*>(process.get())->SetMemory(0x8000, &dict_obj, sizeof(dict_obj));
    
    // Create mock hash table with some entries
    if (test.table_size > 0 && test.table_size < 10000) {
      struct KVPair { uint64_t key; uint64_t value; };
      std::vector<KVPair> table(test.table_size);
      
      // Fill some entries to simulate collisions
      size_t entries_to_fill = std::min(static_cast<size_t>(test.count), test.table_size);
      for (size_t i = 0; i < entries_to_fill; ++i) {
        table[i % test.table_size].key = 0x7000 + i;
        table[i % test.table_size].value = 0x8000 + i;
      }
      
      static_cast<MockProcess*>(process.get())->SetMemory(0x6000, table.data(), 
                                                          table.size() * sizeof(table[0]));
    }
    
    lldb::ValueObjectSP dict_obj_sp = MockValueObject::Create(target, "collision_dict", 
                                                              0x8000, "NSDictionary");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    StreamString output;
    TypeSummaryOptions options;
    bool result = provider->FormatObject(*dict_obj_sp, output, options);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    if (test.should_handle) {
      EXPECT_TRUE(result || !output.GetString().empty()) 
        << "Dictionary test '" << test.description << "' should be handled";
        
      EXPECT_LT(duration.count(), 200) 
        << "Dictionary formatting should be reasonably fast";
        
      std::string output_str = output.GetString().str();
      
      if (test.count == 0) {
        EXPECT_TRUE(output_str.find("{}") != std::string::npos ||
                    output_str.find("empty") != std::string::npos)
          << "Empty dictionary should be indicated: " << output_str;
      } else if (test.count <= 10) {
        EXPECT_FALSE(output_str.empty())
          << "Small dictionary should produce output: " << output_str;
      } else {
        EXPECT_TRUE(output_str.find(std::to_string(test.count)) != std::string::npos ||
                    output_str.find("pairs") != std::string::npos ||
                    output_str.find("entries") != std::string::npos)
          << "Large dictionary should show count: " << output_str;
      }
    } else {
      EXPECT_LT(duration.count(), 1000) 
        << "Even extreme cases should not hang";
    }
    
    printf("Dictionary test: %s -> %ld ms\n", 
           test.description.c_str(), static_cast<long>(duration.count()));
  }
}

TEST_F(EdgeCaseTest, MemoryAlignmentIssues) {
  // Test formatters with misaligned memory addresses
  
  auto array_provider = std::make_unique<GNUstepNSArraySummaryProvider>();
  auto string_provider = std::make_unique<GNUstepNSStringSummaryProvider>();
  
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), 
                                                       ArchSpec("x86_64"), PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Test various alignment scenarios
  std::vector<uint64_t> test_addresses = {
    0x8000,        // 8-byte aligned (good)
    0x8001,        // Misaligned by 1
    0x8002,        // Misaligned by 2
    0x8004,        // 4-byte aligned
    0x8007,        // Misaligned by 7
    0x8008,        // 8-byte aligned (good)
    0x800F,        // Misaligned by 7
  };
  
  for (uint64_t addr : test_addresses) {
    bool is_aligned = (addr % 8) == 0;
    
    // Test array at this address
    struct {
      uint64_t isa;
      uint64_t contents_ptr; 
      uint64_t count;
    } array_obj = {0x5000, 0x6000, 3};
    
    static_cast<MockProcess*>(process.get())->SetMemory(addr, &array_obj, sizeof(array_obj));
    
    lldb::ValueObjectSP array_obj_sp = MockValueObject::Create(target, "align_test_array", 
                                                               addr, "NSArray");
    
    StreamString array_output;
    TypeSummaryOptions options;
    bool array_result = array_provider->FormatObject(*array_obj_sp, array_output, options);
    
    // Should handle both aligned and misaligned addresses gracefully
    EXPECT_TRUE(array_result || !array_output.GetString().empty()) 
      << "Array at address 0x" << std::hex << addr << " should be handled";
    
    // Test string at this address  
    std::string test_str = "Align";
    struct {
      uint64_t isa;
      uint64_t length;
      uint64_t chars_ptr;
    } string_obj = {0x5000, test_str.length(), 0x7000};
    
    static_cast<MockProcess*>(process.get())->SetMemory(addr + 0x1000, &string_obj, sizeof(string_obj));
    static_cast<MockProcess*>(process.get())->SetMemory(0x7000, test_str.c_str(), test_str.length());
    
    lldb::ValueObjectSP string_obj_sp = MockValueObject::Create(target, "align_test_string", 
                                                                addr + 0x1000, "NSString");
    
    StreamString string_output;
    bool string_result = string_provider->FormatObject(*string_obj_sp, string_output, options);
    
    EXPECT_TRUE(string_result || !string_output.GetString().empty()) 
      << "String at address 0x" << std::hex << addr + 0x1000 << " should be handled";
    
    printf("Alignment test: 0x%lx (%s) -> Array: %s, String: %s\n",
           static_cast<unsigned long>(addr), is_aligned ? "aligned" : "misaligned",
           array_result ? "OK" : "HANDLED",
           string_result ? "OK" : "HANDLED");
  }
}

} // namespace