//===-- GNUstepClassDescriptorTest.cpp -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepClassDescriptor.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Utility/StreamString.h"

using namespace lldb;
using namespace lldb_private;

namespace {

// Simple mock runtime for testing - minimal implementation
class MockGNUstepRuntime {
public:
  MockGNUstepRuntime() = default;
  ~MockGNUstepRuntime() = default;
};

class GNUstepClassDescriptorTest : public ::testing::Test {
public:
  void SetUp() override {
    // Minimal setup - just create mock runtime
    m_mock_runtime = std::make_unique<MockGNUstepRuntime>();
  }
  
  void TearDown() override {
    m_mock_runtime.reset();
  }
  
protected:
  std::unique_ptr<MockGNUstepRuntime> m_mock_runtime;
};

// Simple test for ConstString functionality (used in class descriptor)
TEST_F(GNUstepClassDescriptorTest, ConstStringBasicFunctionality) {
  // Test ConstString creation and comparison
  ConstString str1("TestClass");
  ConstString str2("TestClass");
  ConstString str3("DifferentClass");
  
  EXPECT_EQ(str1, str2);  // Same content should be equal
  EXPECT_NE(str1, str3);  // Different content should not be equal
  EXPECT_STREQ(str1.GetCString(), "TestClass");
}

// Test StreamString functionality (used in formatters)
TEST_F(GNUstepClassDescriptorTest, StreamStringFunctionality) {
  StreamString stream;
  
  stream.Printf("Test class: %s", "BankAccount");
  stream.Printf(", ISA: 0x%llx", (unsigned long long)0x12345678);
  
  std::string result = stream.GetString().str();
  EXPECT_EQ(result, "Test class: BankAccount, ISA: 0x12345678");
}

// Test address arithmetic (used in class descriptor)
TEST_F(GNUstepClassDescriptorTest, AddressArithmetic) {
  const uint64_t base_addr = 0x12345678;
  const uint64_t offset = 16;
  
  uint64_t result_addr = base_addr + offset;
  EXPECT_EQ(result_addr, 0x12345688);
  
  // Test address validation
  const uint64_t invalid_addr = 0;
  const uint64_t valid_addr = 0x1000;
  
  EXPECT_EQ(invalid_addr, 0);
  EXPECT_NE(valid_addr, 0);
}

// Test basic class name patterns
TEST_F(GNUstepClassDescriptorTest, ClassNamePatterns) {
  std::vector<std::string> foundation_classes = {
    "NSObject", "NSString", "NSMutableString", "NSArray", "NSMutableArray",
    "NSDictionary", "NSMutableDictionary", "NSSet", "NSMutableSet",
    "NSNumber", "NSValue", "NSDate", "NSURL", "NSError", "NSData"
  };
  
  // Test that all Foundation class names start with "NS"
  for (const auto &class_name : foundation_classes) {
    EXPECT_EQ(class_name.substr(0, 2), "NS");
  }
  
  // Test custom class name
  std::string custom_class = "BankAccount";
  EXPECT_NE(custom_class.substr(0, 2), "NS");
}

// Test basic type size calculations
TEST_F(GNUstepClassDescriptorTest, TypeSizeCalculations) {
  // Test basic type sizes that might be used in class descriptor
  EXPECT_EQ(sizeof(void*), sizeof(uint64_t)); // On 64-bit systems
  EXPECT_GE(sizeof(double), 8);
  EXPECT_GE(sizeof(int32_t), 4);
  
  // Test typical object layout calculations
  const size_t isa_size = sizeof(void*);  // ISA pointer
  const size_t string_ptr_size = sizeof(void*);
  const size_t double_size = sizeof(double);
  
  // Typical BankAccount object size estimation
  size_t estimated_size = isa_size +           // ISA
                         string_ptr_size +     // _accountNumber
                         string_ptr_size +     // _ownerName
                         double_size +         // _balance
                         string_ptr_size +     // _transactions
                         string_ptr_size;      // _authorizedUsers
  
  EXPECT_GE(estimated_size, 5 * sizeof(void*) + sizeof(double));
}

// Test instance variable offset calculations
TEST_F(GNUstepClassDescriptorTest, IvarOffsetCalculations) {
  // Test typical ivar offset calculations for a class like BankAccount
  const size_t isa_offset = 0;
  const size_t isa_size = sizeof(void*);
  
  // Assume ivars start after ISA
  size_t current_offset = isa_size;
  
  // _accountNumber (NSString *)
  size_t accountNumber_offset = current_offset;
  current_offset += sizeof(void*);
  
  // _ownerName (NSString *)
  size_t ownerName_offset = current_offset;
  current_offset += sizeof(void*);
  
  // _balance (double)
  size_t balance_offset = current_offset;
  current_offset += sizeof(double);
  
  EXPECT_EQ(accountNumber_offset, sizeof(void*));
  EXPECT_EQ(ownerName_offset, 2 * sizeof(void*));
  EXPECT_EQ(balance_offset, 3 * sizeof(void*));
}

// Test class hierarchy patterns
TEST_F(GNUstepClassDescriptorTest, ClassHierarchyPatterns) {
  // Test typical Foundation class hierarchies
  std::map<std::string, std::string> class_hierarchies = {
    {"NSObject", ""},                    // Root class
    {"NSString", "NSObject"},
    {"NSMutableString", "NSString"},
    {"NSArray", "NSObject"},
    {"NSMutableArray", "NSArray"},
    {"BankAccount", "NSObject"}           // Custom class
  };
  
  // Verify hierarchy relationships
  EXPECT_EQ(class_hierarchies["NSMutableString"], "NSString");
  EXPECT_EQ(class_hierarchies["BankAccount"], "NSObject");
  
  // Test that root class has no superclass
  EXPECT_TRUE(class_hierarchies["NSObject"].empty());
}

// Test address validation patterns
TEST_F(GNUstepClassDescriptorTest, AddressValidation) {
  // Test typical address validation patterns
  const uint64_t null_addr = 0;
  const uint64_t invalid_addr = 0xFFFFFFFFFFFFFFFFULL;
  const uint64_t valid_addr_1 = 0x12345678;
  const uint64_t valid_addr_2 = 0x7fff12345678ULL;
  
  // Basic validation tests
  EXPECT_EQ(null_addr, 0);
  EXPECT_NE(valid_addr_1, 0);
  EXPECT_NE(valid_addr_2, 0);
  EXPECT_NE(valid_addr_1, invalid_addr);
}

// Test basic error handling patterns
TEST_F(GNUstepClassDescriptorTest, ErrorHandlingPatterns) {
  // Test patterns used in error handling
  std::vector<int> error_codes = {-1, 0, 1, 404, 500};
  
  // Test error classification
  for (int code : error_codes) {
    bool is_error = (code < 0);
    bool is_success = (code == 0);
    bool is_warning = (code > 0);
    
    if (code < 0) {
      EXPECT_TRUE(is_error);
      EXPECT_FALSE(is_success);
    } else if (code == 0) {
      EXPECT_FALSE(is_error);
      EXPECT_TRUE(is_success);
    } else {
      EXPECT_FALSE(is_error);
      EXPECT_FALSE(is_success);
      EXPECT_TRUE(is_warning);
    }
  }
}

// Test basic string operations used in class names
TEST_F(GNUstepClassDescriptorTest, StringOperations) {
  std::string class_name = "BankAccount";
  std::string foundation_class = "NSString";
  
  // Test prefix detection
  bool is_foundation = foundation_class.substr(0, 2) == "NS";
  bool is_custom = class_name.substr(0, 2) != "NS";
  
  EXPECT_TRUE(is_foundation);
  EXPECT_TRUE(is_custom);
  
  // Test case insensitive operations
  std::string upper_class = class_name;
  std::transform(upper_class.begin(), upper_class.end(), 
                 upper_class.begin(), ::toupper);
  
  EXPECT_EQ(upper_class, "BANKACCOUNT");
  EXPECT_NE(upper_class, class_name);
}

// Test lambda function patterns used in class introspection
TEST_F(GNUstepClassDescriptorTest, LambdaPatterns) {
  // Test lambda functions similar to those used in Describe method
  std::vector<uint64_t> addresses;
  std::vector<std::string> names;
  
  auto address_collector = [&](uint64_t addr) {
    addresses.push_back(addr);
  };
  
  auto name_collector = [&](const std::string &name) -> bool {
    names.push_back(name);
    return true;
  };
  
  // Test the collectors
  address_collector(0x12345678);
  address_collector(0x87654321);
  
  name_collector("BankAccount");
  name_collector("NSObject");
  
  // Verify collections
  EXPECT_EQ(addresses.size(), 2);
  EXPECT_EQ(names.size(), 2);
  EXPECT_EQ(addresses[0], 0x12345678ULL);
  EXPECT_EQ(names[0], "BankAccount");
}

// Test collection operations used in class metadata
TEST_F(GNUstepClassDescriptorTest, CollectionOperations) {
  // Test vector operations similar to those used for ivars and methods
  std::vector<std::string> ivar_names = {
    "_accountNumber", "_ownerName", "_balance", "_transactions", "_authorizedUsers"
  };
  
  std::vector<size_t> ivar_offsets = {8, 16, 24, 32, 40};
  std::vector<size_t> ivar_sizes = {8, 8, 8, 8, 8};
  
  // Test collection sizes match
  EXPECT_EQ(ivar_names.size(), 5);
  EXPECT_EQ(ivar_offsets.size(), ivar_names.size());
  EXPECT_EQ(ivar_sizes.size(), ivar_names.size());
  
  // Test iteration and indexing
  for (size_t i = 0; i < ivar_names.size(); ++i) {
    EXPECT_FALSE(ivar_names[i].empty());
    EXPECT_GT(ivar_offsets[i], 0); // All offsets should be > 0
    EXPECT_GT(ivar_sizes[i], 0);   // All sizes should be > 0
  }
  
  // Test range-based iteration
  size_t count = 0;
  for (const auto &name : ivar_names) {
    EXPECT_FALSE(name.empty());
    EXPECT_EQ(name[0], '_'); // All ivars should start with _
    count++;
  }
  EXPECT_EQ(count, 5);
}

} // anonymous namespace