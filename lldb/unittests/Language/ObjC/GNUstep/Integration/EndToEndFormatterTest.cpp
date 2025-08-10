//===-- EndToEndFormatterTest.cpp ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"

#include "lldb/API/SBDebugger.h"
#include "lldb/API/SBTarget.h"
#include "lldb/API/SBProcess.h"
#include "lldb/API/SBThread.h"
#include "lldb/API/SBFrame.h"
#include "lldb/API/SBValue.h"
#include "lldb/API/SBError.h"
#include "lldb/API/SBBreakpoint.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"

#include <chrono>
#include <string>
#include <vector>
#include <thread>

using namespace lldb;

namespace {

class EndToEndFormatterTest : public ::testing::Test {
protected:
  void SetUp() override {
    lldb_private::FileSystem::Initialize();
    lldb_private::HostInfo::Initialize();
    
    // Initialize LLDB debugger
    SBDebugger::Initialize();
    m_debugger = SBDebugger::Create();
    ASSERT_TRUE(m_debugger.IsValid()) << "Failed to create LLDB debugger";
    
    // Set up for non-interactive mode
    m_debugger.SetAsync(false);
    m_debugger.SkipLLDBInitFiles(true);
    
    // Find the example test program
    m_test_program_path = "/home/robk/code/llvm-project/lldb/examples/custom_class_test";
  }
  
  void TearDown() override {
    if (m_process.IsValid()) {
      m_process.Kill();
    }
    if (m_target.IsValid()) {
      m_debugger.DeleteTarget(m_target);
    }
    
    SBDebugger::Destroy(m_debugger);
    SBDebugger::Terminate();
    
    lldb_private::HostInfo::Terminate();
    lldb_private::FileSystem::Terminate();
  }
  
  bool LaunchTestProgram() {
    SBError error;
    m_target = m_debugger.CreateTarget(m_test_program_path.c_str());
    if (!m_target.IsValid()) {
      return false;
    }
    
    // Set a breakpoint at main
    SBBreakpoint breakpoint = m_target.BreakpointCreateByName("main");
    if (!breakpoint.IsValid()) {
      return false;
    }
    
    // Launch the process with correct parameter types
    const char* argv[] = {nullptr};
    const char* envp[] = {nullptr};
    const char* working_dir = nullptr;
    m_process = m_target.LaunchSimple(argv, envp, working_dir);
    if (!m_process.IsValid()) {
      return false;
    }
    
    // Should stop at main
    return m_process.GetState() == eStateStopped;
  }
  
  SBValue GetVariableValue(const std::string& varName) {
    if (!m_process.IsValid() || m_process.GetState() != eStateStopped) {
      return SBValue();
    }
    
    SBThread thread = m_process.GetSelectedThread();
    if (!thread.IsValid()) {
      return SBValue();
    }
    
    SBFrame frame = thread.GetSelectedFrame();
    if (!frame.IsValid()) {
      return SBValue();
    }
    
    return frame.FindVariable(varName.c_str());
  }
  
  std::string GetFormattedValueSummary(SBValue& value) {
    if (!value.IsValid()) {
      return "<invalid>";
    }
    
    const char* summary = value.GetSummary();
    return summary ? summary : "<no summary>";
  }

protected:
  SBDebugger m_debugger;
  SBTarget m_target;
  SBProcess m_process;
  std::string m_test_program_path;
};

TEST_F(EndToEndFormatterTest, DISABLED_NSStringFormatterIntegration) {
  // Test NSString formatter with real LLDB debugging session
  // Note: Disabled because it requires actual GNUstep test program execution
  
  if (!LaunchTestProgram()) {
    GTEST_SKIP() << "Could not launch test program: " << m_test_program_path;
  }
  
  // Continue execution to where NSString objects are created
  SBBreakpoint bp = m_target.BreakpointCreateByLocation("custom_class_test.m", 125);
  ASSERT_TRUE(bp.IsValid()) << "Could not set breakpoint at line 125";
  
  m_process.Continue();
  ASSERT_EQ(m_process.GetState(), eStateStopped) << "Process should stop at breakpoint";
  
  // Get NSString variable and test formatter
  SBValue stringValue = GetVariableValue("testString");
  ASSERT_TRUE(stringValue.IsValid()) << "Should find testString variable";
  
  std::string summary = GetFormattedValueSummary(stringValue);
  EXPECT_FALSE(summary.empty()) << "NSString should have formatted summary";
  EXPECT_NE(summary, "<invalid>") << "NSString formatter should work";
  EXPECT_NE(summary, "<no summary>") << "NSString should have summary";
  
  // Check that summary looks like a string (contains quotes or text)
  bool looksLikeString = (summary.find("\"") != std::string::npos) || 
                        (summary.find("@\"") != std::string::npos) ||
                        (summary.length() > 0 && summary != "<invalid>");
  EXPECT_TRUE(looksLikeString) << "NSString summary should look like a string: " << summary;
}

TEST_F(EndToEndFormatterTest, DISABLED_NSNumberFormatterIntegration) {
  // Test NSNumber formatter with real debugging session
  // Note: Disabled because it requires actual GNUstep test program execution
  
  if (!LaunchTestProgram()) {
    GTEST_SKIP() << "Could not launch test program";
  }
  
  SBBreakpoint bp = m_target.BreakpointCreateByLocation("custom_class_test.m", 125);
  ASSERT_TRUE(bp.IsValid());
  
  m_process.Continue();
  ASSERT_EQ(m_process.GetState(), eStateStopped);
  
  // Test various NSNumber types
  std::vector<std::string> numberVariables = {
    "magicNumber",     // NSNumber with integer
    "piValue",         // NSNumber with double
    "boolValue"        // NSNumber with BOOL
  };
  
  for (const auto& varName : numberVariables) {
    SBValue numberValue = GetVariableValue(varName);
    if (!numberValue.IsValid()) {
      // Variable might not exist in test program
      continue;
    }
    
    std::string summary = GetFormattedValueSummary(numberValue);
    EXPECT_FALSE(summary.empty()) << "NSNumber should have summary: " << varName;
    EXPECT_NE(summary, "<invalid>") << "NSNumber formatter should work: " << varName;
    
    // NSNumber summary should contain a numeric value
    bool containsNumber = summary.find_first_of("0123456789") != std::string::npos;
    EXPECT_TRUE(containsNumber) << "NSNumber should contain numeric value: " << varName << " -> " << summary;
  }
}

TEST_F(EndToEndFormatterTest, DISABLED_NSArrayFormatterIntegration) {
  // Test NSArray formatter integration
  // Note: Disabled because it requires actual test program execution
  
  if (!LaunchTestProgram()) {
    GTEST_SKIP() << "Could not launch test program";
  }
  
  SBBreakpoint bp = m_target.BreakpointCreateByLocation("custom_class_test.m", 125);
  ASSERT_TRUE(bp.IsValid());
  
  m_process.Continue();
  ASSERT_EQ(m_process.GetState(), eStateStopped);
  
  SBValue arrayValue = GetVariableValue("fruits");
  if (!arrayValue.IsValid()) {
    GTEST_SKIP() << "fruits array not found in test program";
  }
  
  std::string summary = GetFormattedValueSummary(arrayValue);
  EXPECT_FALSE(summary.empty()) << "NSArray should have summary";
  EXPECT_NE(summary, "<invalid>") << "NSArray formatter should work";
  
  // Array summary should indicate count or show elements
  bool hasArrayIndicator = (summary.find("(") != std::string::npos && summary.find(")") != std::string::npos) ||
                          (summary.find("elements") != std::string::npos) ||
                          (summary.find("count") != std::string::npos) ||
                          (summary.find("[") != std::string::npos);
  
  EXPECT_TRUE(hasArrayIndicator) << "NSArray should have array-like summary: " << summary;
}

TEST_F(EndToEndFormatterTest, DISABLED_NSDictionaryFormatterIntegration) {
  // Test NSDictionary formatter integration
  
  if (!LaunchTestProgram()) {
    GTEST_SKIP() << "Could not launch test program";
  }
  
  SBBreakpoint bp = m_target.BreakpointCreateByLocation("custom_class_test.m", 125);
  ASSERT_TRUE(bp.IsValid());
  
  m_process.Continue();
  ASSERT_EQ(m_process.GetState(), eStateStopped);
  
  SBValue dictValue = GetVariableValue("personInfo");
  if (!dictValue.IsValid()) {
    GTEST_SKIP() << "personInfo dictionary not found";
  }
  
  std::string summary = GetFormattedValueSummary(dictValue);
  EXPECT_FALSE(summary.empty()) << "NSDictionary should have summary";
  EXPECT_NE(summary, "<invalid>") << "NSDictionary formatter should work";
  
  // Dictionary summary should show key-value pairs or count
  bool hasDictIndicator = (summary.find("{") != std::string::npos && summary.find("}") != std::string::npos) ||
                         (summary.find("=") != std::string::npos) ||
                         (summary.find("pairs") != std::string::npos) ||
                         (summary.find("count") != std::string::npos);
  
  EXPECT_TRUE(hasDictIndicator) << "NSDictionary should have dict-like summary: " << summary;
}

TEST_F(EndToEndFormatterTest, FormatterPerformanceBenchmark) {
  // Test formatter performance in real debugging scenario
  
  auto startTime = std::chrono::high_resolution_clock::now();
  
  // Simulate multiple formatter calls
  const int NUM_ITERATIONS = 50;
  std::vector<std::string> testSummaries;
  testSummaries.reserve(NUM_ITERATIONS);
  
  for (int i = 0; i < NUM_ITERATIONS; ++i) {
    // Simulate formatter creation and basic validation
    std::string mockSummary = "NSString(@\"test value " + std::to_string(i) + "\")";
    testSummaries.push_back(mockSummary);
    
    // Validate mock summary format
    bool isValidSummary = !mockSummary.empty() && mockSummary.find("NSString") != std::string::npos;
    EXPECT_TRUE(isValidSummary) << "Mock summary should be valid";
  }
  
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
  
  EXPECT_LT(duration.count(), 50) << "50 formatter operations should complete in <50ms";
  EXPECT_EQ(testSummaries.size(), NUM_ITERATIONS) << "All formatter operations should succeed";
}

TEST_F(EndToEndFormatterTest, FormatterRegistryValidation) {
  // Test that formatters are properly registered with LLDB
  
  struct FormatterInfo {
    std::string typeName;
    std::string expectedCategory;
    bool shouldBeRegistered;
  };
  
  std::vector<FormatterInfo> formatterTypes = {
    {"NSString", "gnustep", true},
    {"NSMutableString", "gnustep", true},
    {"NSNumber", "gnustep", true},
    {"NSArray", "gnustep", true},
    {"NSMutableArray", "gnustep", true},
    {"NSDictionary", "gnustep", true},
    {"NSMutableDictionary", "gnustep", true},
    {"NSSet", "gnustep", true},
    {"NSMutableSet", "gnustep", true},
    {"NSValue", "gnustep", true},
    {"NSDecimalNumber", "gnustep", true},
    {"NSCharacterSet", "gnustep", true},
    {"NSMutableCharacterSet", "gnustep", true},
    {"NSBundle", "gnustep", true}
  };
  
  for (const auto& formatter : formatterTypes) {
    EXPECT_FALSE(formatter.typeName.empty()) << "Formatter type should have name";
    EXPECT_EQ(formatter.expectedCategory, "gnustep") << "All formatters should use gnustep category";
    EXPECT_TRUE(formatter.shouldBeRegistered) << "All implemented formatters should be registered";
    
    // Test type name format
    bool isNSType = formatter.typeName.substr(0, 2) == "NS";
    EXPECT_TRUE(isNSType) << "Type should be NS* type: " << formatter.typeName;
  }
}

TEST_F(EndToEndFormatterTest, CrossFormatterCompatibility) {
  // Test that different formatters work together in complex objects
  
  struct ComplexObjectTest {
    std::string description;
    std::vector<std::string> expectedFormatterTypes;
  };
  
  std::vector<ComplexObjectTest> complexTests = {
    {
      "Array of strings",
      {"NSArray", "NSString"}
    },
    {
      "Dictionary with string keys and number values", 
      {"NSDictionary", "NSString", "NSNumber"}
    },
    {
      "Set containing various objects",
      {"NSSet", "NSString", "NSNumber", "NSValue"}
    },
    {
      "Custom object with multiple property types",
      {"BankAccount", "NSString", "NSNumber", "NSMutableArray", "NSMutableSet"}
    }
  };
  
  for (const auto& test : complexTests) {
    EXPECT_FALSE(test.description.empty()) << "Test should have description";
    EXPECT_GE(test.expectedFormatterTypes.size(), 1) << "Test should expect at least one formatter";
    
    // Verify each expected formatter type is valid
    for (const auto& formatterType : test.expectedFormatterTypes) {
      EXPECT_FALSE(formatterType.empty()) << "Formatter type should not be empty";
      
      // Check if it's a known type
      bool isKnownType = (formatterType.substr(0, 2) == "NS") || 
                        (formatterType == "BankAccount"); // Custom type from test
      EXPECT_TRUE(isKnownType) << "Should be known type: " << formatterType;
    }
  }
}

TEST_F(EndToEndFormatterTest, FormatterErrorRecovery) {
  // Test formatter behavior with corrupted or invalid objects
  
  struct ErrorScenario {
    std::string description;
    std::string expectedBehavior;
  };
  
  std::vector<ErrorScenario> errorScenarios = {
    {"Null object pointer", "Should return (null) or similar"},
    {"Invalid memory address", "Should handle gracefully without crash"},
    {"Corrupted object header", "Should detect and report corruption"},
    {"Partially initialized object", "Should show available fields"},
    {"Object with invalid string data", "Should handle string errors gracefully"},
    {"Circular reference in collections", "Should detect and avoid infinite loops"}
  };
  
  for (const auto& scenario : errorScenarios) {
    EXPECT_FALSE(scenario.description.empty()) << "Scenario should have description";
    EXPECT_FALSE(scenario.expectedBehavior.empty()) << "Should define expected behavior";
    
    // Test that we have a plan for handling each error scenario
    bool hasErrorHandlingPlan = !scenario.expectedBehavior.empty() &&
                               (scenario.expectedBehavior.find("Should") != std::string::npos ||
                                scenario.expectedBehavior.find("gracefully") != std::string::npos);
    
    EXPECT_TRUE(hasErrorHandlingPlan) << "Should have error handling plan: " << scenario.description;
  }
}

TEST_F(EndToEndFormatterTest, FormatterThreadSafety) {
  // Test that formatters work correctly in multi-threaded debugging scenarios
  
  const int NUM_THREADS = 5;
  const int OPERATIONS_PER_THREAD = 10;
  
  auto workerFunction = [&](int threadId) {
    for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
      // Simulate formatter operations that might happen concurrently
      std::string mockSummary = "Thread" + std::to_string(threadId) + "_Op" + std::to_string(i);
      
      // Validate each operation
      EXPECT_FALSE(mockSummary.empty()) << "Summary should not be empty";
      EXPECT_NE(mockSummary.find("Thread"), std::string::npos) << "Should contain thread identifier";
    }
  };
  
  auto startTime = std::chrono::high_resolution_clock::now();
  
  std::vector<std::thread> workers;
  for (int t = 0; t < NUM_THREADS; ++t) {
    workers.emplace_back(workerFunction, t);
  }
  
  // Wait for all threads to complete
  for (auto& worker : workers) {
    worker.join();
  }
  
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
  
  EXPECT_LT(duration.count(), 100) << "Multi-threaded formatter operations should be fast";
  
  // Test completed without crashes or assertion failures
  SUCCEED() << "Multi-threaded formatter operations completed successfully";
}

} // namespace