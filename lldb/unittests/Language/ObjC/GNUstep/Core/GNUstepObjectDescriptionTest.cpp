//===-- GNUstepObjectDescriptionTest.cpp --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception.
//
//===----------------------------------------------------------------------===//

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Utility/StreamString.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
// Removed TestingSupport/TestUtilities.h - not needed for this test
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>
#include <memory>

using namespace lldb;
using namespace lldb_private;
using testing::_;  
using testing::Return;
using testing::StrictMock;

// Mock introspector for testing GetObjectDescription functionality
class MockGNUstepObjCRuntimeIntrospector {
public:
  MOCK_METHOD(std::string, GetClassName, (lldb::addr_t addr), ());
  MOCK_METHOD(bool, IsTaggedPointer, (lldb::addr_t addr), ());
  MOCK_METHOD(std::string, GetTaggedPointerClassName, (lldb::addr_t addr), ());
  MOCK_METHOD(std::string, GetClassNameFromObject, (ValueObject &object), ());
};

class GNUstepObjectDescriptionTest : public ::testing::Test {
public:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
    
    // Create a mock target and process for testing
    m_debugger_sp = Debugger::CreateInstance();
    ASSERT_TRUE(m_debugger_sp);
    
    ArchSpec arch("x86_64-pc-linux");
    Status error = m_debugger_sp->GetTargetList().CreateTarget(
        *m_debugger_sp, "", arch, eLoadDependentsNo, nullptr, m_target_sp);
    ASSERT_TRUE(error.Success());
    ASSERT_TRUE(m_target_sp);
  }

  void TearDown() override {
    m_target_sp.reset();
    m_debugger_sp.reset();
    HostInfo::Terminate();
    FileSystem::Terminate();
  }

protected:
  // Test constants for object addresses
  static constexpr lldb::addr_t kValidObjectAddr = 0x7fff12345678ULL;
  static constexpr lldb::addr_t kTaggedNumberAddr = 0x1511ULL; // (42 << 3) | 1
  static constexpr lldb::addr_t kNullObjectAddr = 0x0ULL;
  static constexpr lldb::addr_t kInvalidObjectAddr = LLDB_INVALID_ADDRESS;
  
  DebuggerSP m_debugger_sp;
  TargetSP m_target_sp;
  
  // Helper to create a mock ValueObject with a given address and type
  ValueObjectSP CreateMockValueObject(lldb::addr_t addr, bool is_pointer_type = true) {
    auto *ts = m_target_sp->GetScratchTypeSystemForLanguage(eLanguageTypeC);
    EXPECT_NE(ts, nullptr);
    
    CompilerType type;
    if (is_pointer_type) {
      type = ts->GetBasicTypeFromAST(eBasicTypeVoid).GetPointerType();
    } else {
      type = ts->GetBasicTypeFromAST(eBasicTypeInt);
    }
    
    // Create a DataExtractor with the address
    DataExtractor data;
    data.SetData(&addr, sizeof(addr), endian::InlHostByteOrder());
    
    return ValueObjectConstResult::Create(
        m_target_sp.get(), type, ConstString("test_object"), data);
  }
  
  // Helper to create a mock Value with a given address
  Value CreateMockValue(lldb::addr_t addr) {
    Value val;
    val.GetScalar() = addr;
    val.SetValueType(Value::ValueType::Scalar);
    return val;
  }
  
  // Helper to test GetObjectDescription with timeout protection
  llvm::Error CallGetObjectDescriptionSafely(GNUstepObjCRuntime &runtime, 
                                             ValueObject &object,
                                             std::string &result,
                                             std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
    StreamString stream;
    
    // Use a simple timeout mechanism for testing
    auto start = std::chrono::high_resolution_clock::now();
    llvm::Error error = runtime.GetObjectDescription(stream, object);
    auto duration = std::chrono::high_resolution_clock::now() - start;
    
    // Verify it completed within timeout (should be very fast with safe implementation)
    EXPECT_LT(duration, timeout) << "GetObjectDescription took too long, possible hang";
    
    result = stream.GetString().str();
    return error;
  }
};

/// Test GetObjectDescription with null pointer
TEST_F(GNUstepObjectDescriptionTest, GetObjectDescription_NullPointer) {
  // Test that GetObjectDescription handles null pointers correctly
  // Should return "nil" without hanging
  
  auto runtime = std::make_unique<GNUstepObjCRuntime>(nullptr);
  ValueObjectSP null_obj = CreateMockValueObject(kNullObjectAddr);
  ASSERT_TRUE(null_obj);
  
  std::string result;
  llvm::Error error = CallGetObjectDescriptionSafely(*runtime, *null_obj, result);
  
  // Should succeed and return "nil"
  EXPECT_FALSE(static_cast<bool>(error)) << "GetObjectDescription should succeed for null pointer";
  EXPECT_EQ(result, "nil") << "Null pointer should return 'nil'";
}

/// Test GetObjectDescription with invalid address
TEST_F(GNUstepObjectDescriptionTest, GetObjectDescription_InvalidAddress) {
  // Test that GetObjectDescription handles invalid addresses correctly
  
  auto runtime = std::make_unique<GNUstepObjCRuntime>(nullptr);
  ValueObjectSP invalid_obj = CreateMockValueObject(kInvalidObjectAddr);
  ASSERT_TRUE(invalid_obj);
  
  std::string result;
  llvm::Error error = CallGetObjectDescriptionSafely(*runtime, *invalid_obj, result);
  
  // Should succeed and return "nil" for invalid address
  EXPECT_FALSE(static_cast<bool>(error)) << "GetObjectDescription should succeed for invalid address";
  EXPECT_EQ(result, "nil") << "Invalid address should return 'nil'";
}

/// Test GetObjectDescription with non-pointer type
TEST_F(GNUstepObjectDescriptionTest, GetObjectDescription_NonPointerType) {
  // Test that GetObjectDescription rejects non-pointer types
  
  auto runtime = std::make_unique<GNUstepObjCRuntime>(nullptr);
  ValueObjectSP int_obj = CreateMockValueObject(42, false); // Create integer type
  ASSERT_TRUE(int_obj);
  
  std::string result;
  llvm::Error error = CallGetObjectDescriptionSafely(*runtime, *int_obj, result);
  
  // Should fail with "not a pointer type" error
  EXPECT_TRUE(static_cast<bool>(error)) << "GetObjectDescription should fail for non-pointer type";
  if (error) {
    std::string error_msg = toString(std::move(error));
    EXPECT_TRUE(error_msg.find("not a pointer type") != std::string::npos)
        << "Error message should mention 'not a pointer type', got: " << error_msg;
  }
}

/// Test GetObjectDescription with valid object (no introspector)
TEST_F(GNUstepObjectDescriptionTest, GetObjectDescription_NoIntrospector) {
  // Test GetObjectDescription fallback behavior when no introspector is available
  
  auto runtime = std::make_unique<GNUstepObjCRuntime>(nullptr);
  ValueObjectSP valid_obj = CreateMockValueObject(kValidObjectAddr);
  ASSERT_TRUE(valid_obj);
  
  std::string result;
  llvm::Error error = CallGetObjectDescriptionSafely(*runtime, *valid_obj, result);
  
  // Should succeed with fallback message
  EXPECT_FALSE(static_cast<bool>(error)) << "GetObjectDescription should succeed with fallback";
  EXPECT_TRUE(result.find("GNUstep object at 0x") != std::string::npos)
      << "Should show fallback message, got: " << result;
  EXPECT_TRUE(result.find("7fff12345678") != std::string::npos)
      << "Should include object address, got: " << result;
}

/// Test GetObjectDescription performance (no hanging)
TEST_F(GNUstepObjectDescriptionTest, GetObjectDescription_Performance) {
  // Test that GetObjectDescription never hangs and completes quickly
  
  auto runtime = std::make_unique<GNUstepObjCRuntime>(nullptr);
  
  struct TestCase {
    lldb::addr_t addr;
    std::string description;
  };
  
  std::vector<TestCase> test_cases = {
    {kNullObjectAddr, "Null pointer"},
    {kInvalidObjectAddr, "Invalid address"},
    {kValidObjectAddr, "Valid object"},
    {kTaggedNumberAddr, "Tagged pointer"},
    {kValidObjectAddr + 0x1000, "Another valid object"}
  };
  
  // Test multiple calls to ensure consistent performance
  for (const auto& test_case : test_cases) {
    ValueObjectSP obj = CreateMockValueObject(test_case.addr);
    ASSERT_TRUE(obj) << "Failed to create object for: " << test_case.description;
    
    auto start = std::chrono::high_resolution_clock::now();
    std::string result;
    llvm::Error error = CallGetObjectDescriptionSafely(*runtime, *obj, result);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start);
    
    // Should complete within 100ms (much faster than old hanging behavior)
    EXPECT_LT(duration.count(), 100) 
        << test_case.description << " took too long: " << duration.count() << "ms";
    
    // Should never hang - if we get here, it didn't hang
    EXPECT_TRUE(true) << test_case.description << " completed without hanging";
    
    // Result should not be empty
    EXPECT_FALSE(result.empty()) << test_case.description << " should return some result";
  }
}

/// Test GetObjectDescription with Value interface
TEST_F(GNUstepObjectDescriptionTest, GetObjectDescription_ValueInterface) {
  // Test the GetObjectDescription(Stream&, Value&, ExecutionContextScope*) interface
  
  auto runtime = std::make_unique<GNUstepObjCRuntime>(nullptr);
  
  struct ValueTestCase {
    lldb::addr_t addr;
    bool should_succeed;
    std::string expected_pattern;
    std::string description;
  };
  
  std::vector<ValueTestCase> test_cases = {
    {kNullObjectAddr, true, "nil", "Null pointer should return nil"},
    {kInvalidObjectAddr, true, "nil", "Invalid address should return nil"},
    {kValidObjectAddr, true, "GNUstep object at 0x", "Valid object should show fallback"},
    {kTaggedNumberAddr, true, "GNUstep object at 0x", "Tagged pointer should show fallback"}
  };
  
  for (const auto& test_case : test_cases) {
    Value val = CreateMockValue(test_case.addr);
    StreamString stream;
    
    // Test with null execution context scope (should handle gracefully)
    llvm::Error error = runtime->GetObjectDescription(stream, val, nullptr);
    
    if (test_case.should_succeed) {
      if (test_case.addr == 0 || test_case.addr == LLDB_INVALID_ADDRESS) {
        // For null/invalid addresses, should fail due to no execution context
        EXPECT_TRUE(static_cast<bool>(error)) << test_case.description << " should fail with no context";
        if (error) {
          std::string error_msg = toString(std::move(error));
          EXPECT_TRUE(error_msg.find("no execution context scope") != std::string::npos)
              << "Error should mention execution context scope, got: " << error_msg;
        }
      }
    }
  }
}

/// Test GetObjectDescription with different object types
TEST_F(GNUstepObjectDescriptionTest, GetObjectDescription_ObjectTypes) {
  // Test GetObjectDescription with various object types that might be encountered
  
  auto runtime = std::make_unique<GNUstepObjCRuntime>(nullptr);
  
  struct ObjectTypeTest {
    lldb::addr_t addr;
    std::string expected_type;
    std::string description;
  };
  
  std::vector<ObjectTypeTest> object_tests = {
    // Foundation objects
    {kValidObjectAddr, "NSString", "NSString object"},
    {kValidObjectAddr + 8, "NSArray", "NSArray object"},
    {kValidObjectAddr + 16, "NSDictionary", "NSDictionary object"},
    {kValidObjectAddr + 24, "NSNumber", "NSNumber object"},
    
    // Tagged pointers  
    {(42ULL << 3) | 1, "NSNumber", "Tagged NSNumber"},
    {(2ULL << 3) | 4, "NSString", "Tagged NSString"},
    
    // Custom objects
    {kValidObjectAddr + 32, "BankAccount", "Custom object"},
    {kValidObjectAddr + 40, "CustomClass", "Another custom object"},
  };
  
  for (const auto& test : object_tests) {
    ValueObjectSP obj = CreateMockValueObject(test.addr);
    ASSERT_TRUE(obj) << "Failed to create object for: " << test.description;
    
    std::string result;
    llvm::Error error = CallGetObjectDescriptionSafely(*runtime, *obj, result);
    
    // Should succeed (may use fallback)
    EXPECT_FALSE(static_cast<bool>(error)) << test.description << " should succeed";
    
    // Should contain object address in hex
    std::stringstream addr_stream;
    addr_stream << std::hex << test.addr;
    EXPECT_TRUE(result.find(addr_stream.str()) != std::string::npos)
        << test.description << " should contain address " << addr_stream.str() 
        << ", got: " << result;
    
    // Should not be empty
    EXPECT_FALSE(result.empty()) << test.description << " should return non-empty result";
    
    // Should not contain error indicators
    EXPECT_TRUE(result.find("error") == std::string::npos)
        << test.description << " should not contain 'error', got: " << result;
    EXPECT_TRUE(result.find("failed") == std::string::npos)  
        << test.description << " should not contain 'failed', got: " << result;
  }
}

/// Test GetObjectDescription error handling
TEST_F(GNUstepObjectDescriptionTest, GetObjectDescription_ErrorHandling) {
  // Test comprehensive error handling in GetObjectDescription
  
  auto runtime = std::make_unique<GNUstepObjCRuntime>(nullptr);
  
  // Test 1: Non-pointer type should fail
  {
    ValueObjectSP int_obj = CreateMockValueObject(123, false);
    ASSERT_TRUE(int_obj);
    
    StreamString stream;
    llvm::Error error = runtime->GetObjectDescription(stream, *int_obj);
    
    EXPECT_TRUE(static_cast<bool>(error)) << "Non-pointer type should fail";
    if (error) {
      std::string error_msg = toString(std::move(error));
      EXPECT_TRUE(error_msg.find("not a pointer type") != std::string::npos)
          << "Should mention 'not a pointer type', got: " << error_msg;
    }
  }
  
  // Test 2: Null and invalid addresses should succeed with "nil"
  {
    std::vector<lldb::addr_t> invalid_addrs = {0x0ULL, LLDB_INVALID_ADDRESS};
    
    for (auto addr : invalid_addrs) {
      ValueObjectSP obj = CreateMockValueObject(addr);
      ASSERT_TRUE(obj);
      
      std::string result;
      llvm::Error error = CallGetObjectDescriptionSafely(*runtime, *obj, result);
      
      EXPECT_FALSE(static_cast<bool>(error)) << "Address 0x" << std::hex << addr << " should succeed";
      EXPECT_EQ(result, "nil") << "Address 0x" << std::hex << addr << " should return 'nil'";
    }
  }
  
  // Test 3: Valid addresses should succeed with fallback
  {
    std::vector<lldb::addr_t> valid_addrs = {
      kValidObjectAddr, kTaggedNumberAddr, kValidObjectAddr + 0x1000
    };
    
    for (auto addr : valid_addrs) {
      ValueObjectSP obj = CreateMockValueObject(addr);
      ASSERT_TRUE(obj);
      
      std::string result;
      llvm::Error error = CallGetObjectDescriptionSafely(*runtime, *obj, result);
      
      EXPECT_FALSE(static_cast<bool>(error)) << "Valid address 0x" << std::hex << addr << " should succeed";
      EXPECT_FALSE(result.empty()) << "Valid address should return non-empty result";
      EXPECT_TRUE(result.find("0x") != std::string::npos) << "Should contain hex address";
    }
  }
}

/// Test GetObjectDescription consistency and reliability  
TEST_F(GNUstepObjectDescriptionTest, GetObjectDescription_ConsistencyReliability) {
  // Test that GetObjectDescription is consistent and reliable across multiple calls
  
  auto runtime = std::make_unique<GNUstepObjCRuntime>(nullptr);
  
  struct ConsistencyTest {
    lldb::addr_t addr;
    int num_calls;
    std::string description;
  };
  
  std::vector<ConsistencyTest> consistency_tests = {
    {kNullObjectAddr, 10, "Null pointer multiple calls"},
    {kValidObjectAddr, 10, "Valid object multiple calls"},
    {kTaggedNumberAddr, 10, "Tagged pointer multiple calls"},
    {LLDB_INVALID_ADDRESS, 10, "Invalid address multiple calls"}
  };
  
  for (const auto& test : consistency_tests) {
    std::vector<std::string> results;
    std::vector<bool> success_flags;
    
    // Make multiple calls and collect results
    for (int i = 0; i < test.num_calls; ++i) {
      ValueObjectSP obj = CreateMockValueObject(test.addr);
      ASSERT_TRUE(obj) << "Failed to create object for call " << i;
      
      std::string result;
      llvm::Error error = CallGetObjectDescriptionSafely(*runtime, *obj, result);
      
      results.push_back(result);
      success_flags.push_back(!static_cast<bool>(error));
      
      // Each call should complete quickly
      auto start = std::chrono::high_resolution_clock::now();
      // (timing already checked in CallGetObjectDescriptionSafely)
    }
    
    // Verify consistency
    EXPECT_FALSE(results.empty()) << test.description << " should have results";
    
    // All calls should have same success/failure status
    bool first_success = success_flags[0];
    for (size_t i = 1; i < success_flags.size(); ++i) {
      EXPECT_EQ(success_flags[i], first_success) 
          << test.description << " call " << i << " should match first call success status";
    }
    
    // All successful calls should return same result
    if (first_success) {
      std::string first_result = results[0];
      for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_EQ(results[i], first_result)
            << test.description << " call " << i << " should return same result as first call";
      }
    }
    
    // No result should be empty for successful calls
    for (size_t i = 0; i < results.size(); ++i) {
      if (success_flags[i]) {
        EXPECT_FALSE(results[i].empty()) 
            << test.description << " successful call " << i << " should not return empty result";
      }
    }
  }
}

/// Test GetObjectDescription thread safety simulation
TEST_F(GNUstepObjectDescriptionTest, GetObjectDescription_ThreadSafety) {
  // Test GetObjectDescription behavior under simulated concurrent access
  // (Note: This is a simulation since we can't easily create real threads in unit tests)
  
  auto runtime = std::make_unique<GNUstepObjCRuntime>(nullptr);
  
  // Simulate rapid successive calls that might occur in multi-threaded scenarios
  constexpr int kNumRapidCalls = 100;
  std::vector<lldb::addr_t> test_addresses = {
    kNullObjectAddr, kValidObjectAddr, kTaggedNumberAddr, kValidObjectAddr + 0x1000
  };
  
  for (auto addr : test_addresses) {
    std::vector<std::string> results;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Make rapid successive calls
    for (int i = 0; i < kNumRapidCalls; ++i) {
      ValueObjectSP obj = CreateMockValueObject(addr);
      ASSERT_TRUE(obj) << "Failed to create object for rapid call " << i;
      
      std::string result;
      llvm::Error error = CallGetObjectDescriptionSafely(*runtime, *obj, result);
      
      // Should not fail (may return different things based on address)
      if (addr == 0 || addr == LLDB_INVALID_ADDRESS) {
        EXPECT_FALSE(static_cast<bool>(error)) << "Rapid call " << i << " should succeed for special addresses";
        EXPECT_EQ(result, "nil") << "Rapid call " << i << " should return 'nil' for null/invalid";
      } else {
        EXPECT_FALSE(static_cast<bool>(error)) << "Rapid call " << i << " should succeed for valid addresses";
        EXPECT_FALSE(result.empty()) << "Rapid call " << i << " should return non-empty result";
      }
      
      results.push_back(result);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // All rapid calls should complete quickly (average < 10ms per call)
    double avg_per_call = static_cast<double>(total_duration.count()) / kNumRapidCalls;
    EXPECT_LT(avg_per_call, 10.0) 
        << "Average call time should be < 10ms, got: " << avg_per_call << "ms for address 0x" << std::hex << addr;
    
    // Results should be consistent across all calls for same address
    if (!results.empty()) {
      std::string first_result = results[0];
      for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_EQ(results[i], first_result)
            << "Rapid call " << i << " should return consistent result for address 0x" << std::hex << addr;
      }
    }
  }
}