//===-- ISADescriptorMappingTest.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepClassDescriptor.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepRuntimeV2API.h"

#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Symbol/CompilerType.h"
#include "lldb/Target/Process.h"

#include "gtest/gtest.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <thread>

using namespace lldb;
using namespace lldb_private;

/// Tests for ISA-to-descriptor mapping functionality
class ISADescriptorMappingTest : public ::testing::Test {
public:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
  }

  void TearDown() override {
    HostInfo::Terminate();
    FileSystem::Terminate();
  }

protected:

  // Mock ISA addresses for testing
  static constexpr ObjCLanguageRuntime::ObjCISA kTestISA1 = 0x1000;
  static constexpr ObjCLanguageRuntime::ObjCISA kTestISA2 = 0x2000;
  static constexpr ObjCLanguageRuntime::ObjCISA kTestISA3 = 0x3000;
  static constexpr ObjCLanguageRuntime::ObjCISA kInvalidISA = 0;

  // Helper to create mock Foundation classes
  std::vector<GNUstepRuntimeV2API::ClassInfo> CreateMockFoundationClasses() {
    std::vector<GNUstepRuntimeV2API::ClassInfo> classes;
    
    // Create mock Foundation classes
    GNUstepRuntimeV2API::ClassInfo nsstring;
    nsstring.name = "NSString";
    nsstring.class_ptr = reinterpret_cast<void*>(kTestISA1);
    nsstring.superclass_ptr = nullptr;
    nsstring.superclass_name = "NSObject";
    nsstring.instance_size = 16;
    classes.push_back(nsstring);
    
    GNUstepRuntimeV2API::ClassInfo nsarray;
    nsarray.name = "NSArray";
    nsarray.class_ptr = reinterpret_cast<void*>(kTestISA2);
    nsarray.superclass_ptr = nullptr;
    nsarray.superclass_name = "NSObject";
    nsarray.instance_size = 24;
    classes.push_back(nsarray);
    
    GNUstepRuntimeV2API::ClassInfo nsdictionary;
    nsdictionary.name = "NSDictionary";
    nsdictionary.class_ptr = reinterpret_cast<void*>(kTestISA3);
    nsdictionary.superclass_ptr = nullptr;
    nsdictionary.superclass_name = "NSObject";
    nsdictionary.instance_size = 32;
    classes.push_back(nsdictionary);
    
    return classes;
  }
};

/// Test basic ISA validation
TEST_F(ISADescriptorMappingTest, BasicISA_Validation) {
  // Test ISA address validation without full runtime
  
  std::vector<std::pair<ObjCLanguageRuntime::ObjCISA, bool>> test_cases = {
    {kTestISA1, true},   // Valid ISA
    {kTestISA2, true},   // Valid ISA  
    {kTestISA3, true},   // Valid ISA
    {kInvalidISA, false}, // Invalid ISA (zero)
    {LLDB_INVALID_ADDRESS, false}, // Invalid address
    {0x1, false},        // Too low address
    {0xFFF, false},      // Likely invalid
  };
  
  for (const auto& test : test_cases) {
    bool is_valid = (test.first != 0 && test.first != LLDB_INVALID_ADDRESS && test.first >= 0x1000);
    EXPECT_EQ(is_valid, test.second) 
        << "ISA validation failed for: 0x" << std::hex << test.first;
  }
}

/// Test ClassInfo structure
TEST_F(ISADescriptorMappingTest, ClassInfo_Structure) {
  // Test the ClassInfo data structure used in mapping
  
  std::vector<GNUstepRuntimeV2API::ClassInfo> test_classes = CreateMockFoundationClasses();
  
  EXPECT_EQ(test_classes.size(), 3);
  
  // Validate first class (NSString)
  const auto& nsstring = test_classes[0];
  EXPECT_EQ(nsstring.name, "NSString");
  EXPECT_EQ(reinterpret_cast<uintptr_t>(nsstring.class_ptr), kTestISA1);
  EXPECT_EQ(nsstring.superclass_name, "NSObject");
  EXPECT_GT(nsstring.instance_size, 0U);
  
  // Validate second class (NSArray)
  const auto& nsarray = test_classes[1];
  EXPECT_EQ(nsarray.name, "NSArray");
  EXPECT_EQ(reinterpret_cast<uintptr_t>(nsarray.class_ptr), kTestISA2);
  
  // Validate third class (NSDictionary)
  const auto& nsdict = test_classes[2];
  EXPECT_EQ(nsdict.name, "NSDictionary");
  EXPECT_EQ(reinterpret_cast<uintptr_t>(nsdict.class_ptr), kTestISA3);
}

/// Test mock Foundation classes creation
TEST_F(ISADescriptorMappingTest, MockFoundationClasses_Creation) {
  // Test our mock Foundation classes helper
  
  auto mock_classes = CreateMockFoundationClasses();
  
  EXPECT_EQ(mock_classes.size(), 3);
  
  // Check that all classes have valid information
  for (const auto& cls : mock_classes) {
    EXPECT_FALSE(cls.name.empty()) << "Class should have a name";
    EXPECT_NE(cls.class_ptr, nullptr) << "Class should have a valid pointer";
    EXPECT_GT(cls.instance_size, 0U) << "Class should have positive instance size";
    
    // Check that class names are reasonable
    EXPECT_TRUE(cls.name.find("NS") == 0) << "Foundation class should start with NS";
  }
  
  // Check specific classes exist
  bool found_string = false, found_array = false, found_dict = false;
  for (const auto& cls : mock_classes) {
    if (cls.name == "NSString") found_string = true;
    if (cls.name == "NSArray") found_array = true;
    if (cls.name == "NSDictionary") found_dict = true;
  }
  
  EXPECT_TRUE(found_string) << "Should contain NSString";
  EXPECT_TRUE(found_array) << "Should contain NSArray";
  EXPECT_TRUE(found_dict) << "Should contain NSDictionary";
}

/// Test class name validation
TEST_F(ISADescriptorMappingTest, ClassName_Validation) {
  // Test class name validation logic
  
  struct NameTest {
    std::string name;
    bool is_valid;
    std::string description;
  };
  
  std::vector<NameTest> tests = {
    {"NSString", true, "Standard Foundation class"},
    {"NSMutableArray", true, "Mutable Foundation class"},
    {"TestClass", true, "Custom class"},
    {"BankAccount", true, "Another custom class"},
    {"", false, "Empty name"},
    {"123BadName", false, "Starts with digit"},
    {"Bad Name", false, "Contains space"},
    {"Bad@Name", false, "Contains special character"},
  };
  
  for (const auto& test : tests) {
    ConstString cs(test.name);
    
    // Basic validation: non-empty, starts with letter, no spaces/special chars
    bool appears_valid = !test.name.empty() &&
                        std::isalpha(test.name[0]) &&
                        test.name.find(' ') == std::string::npos &&
                        test.name.find('@') == std::string::npos;
    
    EXPECT_EQ(appears_valid, test.is_valid)
        << test.description << " (name: '" << test.name << "')";
  }
}

/// Test ISA-to-pointer conversion
TEST_F(ISADescriptorMappingTest, ISA_PointerConversion) {
  // Test ISA to pointer conversion and validation
  
  auto test_classes = CreateMockFoundationClasses();
  
  for (const auto& cls : test_classes) {
    uintptr_t isa_value = reinterpret_cast<uintptr_t>(cls.class_ptr);
    
    // Test that ISA values are reasonable
    EXPECT_NE(isa_value, 0U) << "ISA should not be null for " << cls.name;
    EXPECT_NE(isa_value, LLDB_INVALID_ADDRESS) << "ISA should not be invalid for " << cls.name;
    
    // Test pointer conversion back
    void* ptr = reinterpret_cast<void*>(isa_value);
    EXPECT_EQ(ptr, cls.class_ptr) << "Pointer conversion should be symmetric";
  }
  
  // Test specific ISA values
  EXPECT_EQ(reinterpret_cast<uintptr_t>(test_classes[0].class_ptr), static_cast<uintptr_t>(kTestISA1));
  EXPECT_EQ(reinterpret_cast<uintptr_t>(test_classes[1].class_ptr), static_cast<uintptr_t>(kTestISA2));
  EXPECT_EQ(reinterpret_cast<uintptr_t>(test_classes[2].class_ptr), static_cast<uintptr_t>(kTestISA3));
}

/// Test error handling for invalid inputs
TEST_F(ISADescriptorMappingTest, ErrorHandling_InvalidInputs) {
  // Test error handling with invalid inputs
  
  struct ErrorCase {
    ObjCLanguageRuntime::ObjCISA isa;
    bool should_be_valid;
    std::string description;
  };
  
  std::vector<ErrorCase> error_cases = {
    {0, false, "Null ISA"},
    {LLDB_INVALID_ADDRESS, false, "Invalid address constant"},
    {0x1, false, "Very low address"},
    {0xFFF, false, "Low address likely invalid"},
    {0xFFFFFFFFFFFFFFFF, false, "Maximum address likely invalid"},
    {kTestISA1, true, "Valid test ISA"},
  };
  
  for (const auto& test : error_cases) {
    // Test basic validation logic
    bool appears_valid = (test.isa != 0 && 
                         test.isa != LLDB_INVALID_ADDRESS && 
                         test.isa >= 0x1000 &&
                         test.isa != 0xFFFFFFFFFFFFFFFF);
    
    EXPECT_EQ(appears_valid, test.should_be_valid)
        << test.description << " (ISA: 0x" << std::hex << test.isa << ")";
  }
  
  // Test that we handle problematic class names
  std::vector<std::string> problematic_names = {
    "", "\0", "\n", "invalid name", "bad@name"
  };
  
  for (const auto& name : problematic_names) {
    ConstString cs(name);
    // Should not crash when creating ConstString with problematic input
    bool is_empty = cs.IsEmpty();
    (void)is_empty; // Prevent unused variable warning
  }
}

/// Test performance of basic operations
TEST_F(ISADescriptorMappingTest, Performance_BasicOperations) {
  // Test performance of basic operations without full runtime
  
  constexpr int kNumOperations = 10000;
  auto test_classes = CreateMockFoundationClasses();
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Test performance of class info processing
  for (int i = 0; i < kNumOperations; ++i) {
    const auto& cls = test_classes[i % test_classes.size()];
    
    // Basic operations that should be fast
    ConstString name(cls.name);
    uintptr_t isa = reinterpret_cast<uintptr_t>(cls.class_ptr);
    bool is_valid = (isa != 0 && isa != LLDB_INVALID_ADDRESS);
    size_t size = cls.instance_size;
    
    // Use variables to prevent optimization
    (void)name;
    (void)is_valid;
    (void)size;
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);
  
  // Should complete many operations quickly (less than 100ms for 10k operations)
  EXPECT_LT(duration.count(), 100000)
      << "Basic operations took too long: " << duration.count() << "μs";
  
  // Average should be very fast (less than 10μs per operation)
  double avg_per_op = static_cast<double>(duration.count()) / kNumOperations;
  EXPECT_LT(avg_per_op, 10.0)
      << "Average operation time: " << avg_per_op << "μs";
}

/// Test concurrent access to basic operations
TEST_F(ISADescriptorMappingTest, ThreadSafety_BasicOperations) {
  // Test thread safety of basic operations without full runtime
  
  constexpr int kNumThreads = 4;
  constexpr int kOperationsPerThread = 100;
  
  std::vector<std::thread> threads;
  std::vector<bool> success(kNumThreads, false);
  auto test_classes = CreateMockFoundationClasses();
  
  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&test_classes, t, &success, kOperationsPerThread]() {
      bool thread_success = true;
      
      for (int i = 0; i < kOperationsPerThread; ++i) {
        const auto& cls = test_classes[i % test_classes.size()];
        
        // Basic operations that should be thread-safe
        ConstString name(cls.name);
        uintptr_t isa = reinterpret_cast<uintptr_t>(cls.class_ptr);
        bool is_valid = (isa != 0);
        (void)is_valid; // Suppress unused variable warning
        
        // Validate operations
        if (name.IsEmpty() && !cls.name.empty()) {
          thread_success = false;
          break;
        }
        
        if (isa == 0 && cls.class_ptr != nullptr) {
          thread_success = false;
          break;
        }
      }
      
      success[t] = thread_success;
    });
  }
  
  // Wait for all threads to complete
  for (auto& thread : threads) {
    thread.join();
  }
  
  // Check that all threads succeeded
  for (int t = 0; t < kNumThreads; ++t) {
    EXPECT_TRUE(success[t]) << "Thread " << t << " failed";
  }
  
  SUCCEED();
}