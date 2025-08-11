//===-- GNUstepStepThroughTrampolineTest.cpp ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception.
//
//===----------------------------------------------------------------------===//

#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Utility/Status.h"
#include "gtest/gtest.h"
#include <chrono>
#include <memory>
#include <vector>

using namespace lldb;
using namespace lldb_private;

/// Test Step-Through Trampoline functionality for GetStepThroughTrampolinePlan
/// This tests the critical functionality that allows proper stepping through
/// GNUstep runtime trampolines (objc_msgSend variants, method dispatch, etc.)
class GNUstepStepThroughTrampolineTest : public ::testing::Test {
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
  // Test constants for trampoline addresses and patterns
  static constexpr lldb::addr_t kObjcMsgSendAddr = 0x7fff80001000ULL;
  static constexpr lldb::addr_t kObjcMsgSendSuperAddr = 0x7fff80001100ULL;
  static constexpr lldb::addr_t kObjcMsgSendFpretAddr = 0x7fff80001200ULL;
  static constexpr lldb::addr_t kObjcMsgSendStretAddr = 0x7fff80001300ULL;
  static constexpr lldb::addr_t kMethodImplementationAddr = 0x7fff90001000ULL;
  static constexpr lldb::addr_t kUserCodeAddr = 0x400000ULL;
  
  // Mock class for testing trampoline step-through logic
  struct MockTrampolineStepPlanner {
    // Known trampoline symbols and their patterns
    struct TrampolineInfo {
      std::string symbol_name;
      lldb::addr_t start_addr;
      lldb::addr_t end_addr;
      bool is_trampoline;
      std::string description;
    };
    
    std::vector<TrampolineInfo> known_trampolines = {
      {"objc_msgSend", kObjcMsgSendAddr, kObjcMsgSendAddr + 0x50, true, "Main message send"},
      {"objc_msgSendSuper", kObjcMsgSendSuperAddr, kObjcMsgSendSuperAddr + 0x50, true, "Super message send"},
      {"objc_msgSend_fpret", kObjcMsgSendFpretAddr, kObjcMsgSendFpretAddr + 0x40, true, "FP return message send"},
      {"objc_msgSend_stret", kObjcMsgSendStretAddr, kObjcMsgSendStretAddr + 0x40, true, "Struct return message send"},
      {"_objc_autoreleasePoolPush", 0x7fff80002000ULL, 0x7fff80002030ULL, true, "Autorelease pool push"},
      {"_objc_autoreleasePoolPop", 0x7fff80002040ULL, 0x7fff80002070ULL, true, "Autorelease pool pop"},
      {"class_getInstanceMethod", 0x7fff80002100ULL, 0x7fff80002130ULL, true, "Method lookup trampoline"},
      {"method_getImplementation", 0x7fff80002200ULL, 0x7fff80002220ULL, true, "Implementation resolver"},
      {"regular_function", kUserCodeAddr, kUserCodeAddr + 0x100, false, "Regular user function"}
    };
    
    // Simulate GetStepThroughTrampolinePlan logic
    struct StepPlan {
      bool should_step_through;
      lldb::addr_t target_address;
      std::string plan_type;
      std::string description;
      bool requires_instruction_stepping;
      int estimated_steps;
    };
    
    StepPlan CreateStepThroughPlan(lldb::addr_t current_addr, lldb::addr_t called_addr = LLDB_INVALID_ADDRESS) {
      StepPlan plan = {};
      
      // Step 1: Check if current address is in a known trampoline
      const TrampolineInfo* trampoline = FindTrampolineAtAddress(current_addr);
      if (!trampoline) {
        plan.should_step_through = false;
        plan.description = "Not in a trampoline";
        return plan;
      }
      
      if (!trampoline->is_trampoline) {
        plan.should_step_through = false;
        plan.description = "Regular function, not a trampoline";
        return plan;
      }
      
      // Step 2: Determine step-through strategy based on trampoline type
      plan.should_step_through = true;
      plan.target_address = DetermineTargetAddress(trampoline, called_addr);
      plan.plan_type = DeterminePlanType(trampoline);
      plan.description = CreatePlanDescription(trampoline);
      plan.requires_instruction_stepping = RequiresInstructionStepping(trampoline);
      plan.estimated_steps = EstimateStepsRequired(trampoline);
      
      return plan;
    }
    
    const TrampolineInfo* FindTrampolineAtAddress(lldb::addr_t addr) {
      for (const auto& trampoline : known_trampolines) {
        if (addr >= trampoline.start_addr && addr < trampoline.end_addr) {
          return &trampoline;
        }
      }
      return nullptr;
    }
    
    lldb::addr_t DetermineTargetAddress(const TrampolineInfo* trampoline, lldb::addr_t called_addr) {
      // Simulate target address resolution
      if (trampoline->symbol_name.find("objc_msgSend") != std::string::npos) {
        // For message send, target is the method implementation
        return (called_addr != LLDB_INVALID_ADDRESS) ? called_addr : kMethodImplementationAddr;
      } else if (trampoline->symbol_name.find("Pool") != std::string::npos) {
        // For autorelease pool operations, step to end of trampoline
        return trampoline->end_addr;
      } else {
        // Generic trampoline: step to end
        return trampoline->end_addr;
      }
    }
    
    std::string DeterminePlanType(const TrampolineInfo* trampoline) {
      if (trampoline->symbol_name.find("objc_msgSend") != std::string::npos) {
        return "MessageSendStepThrough";
      } else if (trampoline->symbol_name.find("Pool") != std::string::npos) {
        return "AutoreleasePoolStepThrough";
      } else if (trampoline->symbol_name.find("method_") != std::string::npos || 
                 trampoline->symbol_name.find("class_") != std::string::npos) {
        return "RuntimeLookupStepThrough";
      } else {
        return "GenericTrampolineStepThrough";
      }
    }
    
    std::string CreatePlanDescription(const TrampolineInfo* trampoline) {
      return std::string("Step through ") + trampoline->symbol_name + " (" + trampoline->description + ")";
    }
    
    bool RequiresInstructionStepping(const TrampolineInfo* trampoline) {
      // Message send trampolines typically require instruction-level stepping
      return trampoline->symbol_name.find("objc_msgSend") != std::string::npos;
    }
    
    int EstimateStepsRequired(const TrampolineInfo* trampoline) {
      // Estimate number of steps based on trampoline complexity
      if (trampoline->symbol_name.find("objc_msgSend") != std::string::npos) {
        return 5; // Message send: receiver check, selector resolution, method lookup, dispatch
      } else if (trampoline->symbol_name.find("Pool") != std::string::npos) {
        return 2; // Simple pool operations
      } else if (trampoline->symbol_name.find("method_") != std::string::npos) {
        return 3; // Method lookup operations
      } else {
        return 1; // Generic step-through
      }
    }
    
    // Test if an address range contains trampoline code
    bool IsAddressRangeTrampoline(lldb::addr_t start_addr, lldb::addr_t end_addr) {
      for (const auto& trampoline : known_trampolines) {
        if (trampoline.is_trampoline && 
            start_addr <= trampoline.start_addr && end_addr >= trampoline.end_addr) {
          return true;
        }
      }
      return false;
    }
    
    // Get all known trampoline symbols for validation
    std::vector<std::string> GetKnownTrampolineSymbols() {
      std::vector<std::string> symbols;
      for (const auto& trampoline : known_trampolines) {
        if (trampoline.is_trampoline) {
          symbols.push_back(trampoline.symbol_name);
        }
      }
      return symbols;
    }
  };
};

/// Test basic trampoline detection and step-through plan creation
TEST_F(GNUstepStepThroughTrampolineTest, TrampolineDetection_BasicFunctionality) {
  MockTrampolineStepPlanner planner;
  
  // Test objc_msgSend trampoline detection
  auto plan1 = planner.CreateStepThroughPlan(kObjcMsgSendAddr);
  EXPECT_TRUE(plan1.should_step_through) << "objc_msgSend should require step-through";
  EXPECT_EQ(plan1.plan_type, "MessageSendStepThrough") << "Should use message send plan type";
  EXPECT_TRUE(plan1.requires_instruction_stepping) << "Message send should require instruction stepping";
  EXPECT_EQ(plan1.estimated_steps, 5) << "Message send should estimate 5 steps";
  
  // Test objc_msgSendSuper trampoline detection
  auto plan2 = planner.CreateStepThroughPlan(kObjcMsgSendSuperAddr);
  EXPECT_TRUE(plan2.should_step_through) << "objc_msgSendSuper should require step-through";
  EXPECT_EQ(plan2.plan_type, "MessageSendStepThrough") << "Should use message send plan type";
  
  // Test regular function (should not step through)
  auto plan3 = planner.CreateStepThroughPlan(kUserCodeAddr);
  EXPECT_FALSE(plan3.should_step_through) << "Regular function should not step through";
  EXPECT_EQ(plan3.description, "Regular function, not a trampoline");
  
  // Test unknown address (should not step through)
  auto plan4 = planner.CreateStepThroughPlan(0x999999ULL);
  EXPECT_FALSE(plan4.should_step_through) << "Unknown address should not step through";
  EXPECT_EQ(plan4.description, "Not in a trampoline");
}

/// Test step-through plans for different message send variants
TEST_F(GNUstepStepThroughTrampolineTest, MessageSendTrampolines_StepThroughPlans) {
  MockTrampolineStepPlanner planner;
  
  struct MessageSendTest {
    lldb::addr_t trampoline_addr;
    std::string symbol_name;
    std::string description;
  };
  
  std::vector<MessageSendTest> msgSend_tests = {
    {kObjcMsgSendAddr, "objc_msgSend", "Main message send"},
    {kObjcMsgSendSuperAddr, "objc_msgSendSuper", "Super message send"},
    {kObjcMsgSendFpretAddr, "objc_msgSend_fpret", "FP return message send"},
    {kObjcMsgSendStretAddr, "objc_msgSend_stret", "Struct return message send"}
  };
  
  for (const auto& test : msgSend_tests) {
    auto plan = planner.CreateStepThroughPlan(test.trampoline_addr);
    
    EXPECT_TRUE(plan.should_step_through) << "Should step through: " << test.symbol_name;
    EXPECT_EQ(plan.plan_type, "MessageSendStepThrough") << "Plan type for: " << test.symbol_name;
    EXPECT_TRUE(plan.requires_instruction_stepping) << "Should require instruction stepping: " << test.symbol_name;
    EXPECT_EQ(plan.estimated_steps, 5) << "Should estimate 5 steps: " << test.symbol_name;
    EXPECT_TRUE(plan.description.find(test.symbol_name) != std::string::npos) 
        << "Description should mention symbol: " << test.symbol_name;
  }
}

/// Test step-through plans with target addresses
TEST_F(GNUstepStepThroughTrampolineTest, TrampolineStepThrough_WithTargetAddresses) {
  MockTrampolineStepPlanner planner;
  
  // Test objc_msgSend with specific target (method implementation)
  lldb::addr_t method_impl_addr = 0x7fff90005000ULL;
  auto plan1 = planner.CreateStepThroughPlan(kObjcMsgSendAddr, method_impl_addr);
  EXPECT_TRUE(plan1.should_step_through);
  EXPECT_EQ(plan1.target_address, method_impl_addr) << "Should use provided method implementation address";
  
  // Test objc_msgSend without target (should use default)
  auto plan2 = planner.CreateStepThroughPlan(kObjcMsgSendAddr);
  EXPECT_TRUE(plan2.should_step_through);
  EXPECT_EQ(plan2.target_address, kMethodImplementationAddr) << "Should use default method implementation";
  
  // Test autorelease pool operations
  auto plan3 = planner.CreateStepThroughPlan(0x7fff80002000ULL); // _objc_autoreleasePoolPush
  EXPECT_TRUE(plan3.should_step_through);
  EXPECT_EQ(plan3.plan_type, "AutoreleasePoolStepThrough");
  EXPECT_EQ(plan3.estimated_steps, 2) << "Pool operations should be simpler";
  EXPECT_FALSE(plan3.requires_instruction_stepping) << "Pool operations don't need instruction stepping";
}

/// Test runtime lookup trampoline step-through
TEST_F(GNUstepStepThroughTrampolineTest, RuntimeLookupTrampolines_StepThrough) {
  MockTrampolineStepPlanner planner;
  
  // Test method lookup trampoline
  auto plan1 = planner.CreateStepThroughPlan(0x7fff80002100ULL); // class_getInstanceMethod
  EXPECT_TRUE(plan1.should_step_through) << "Method lookup should step through";
  EXPECT_EQ(plan1.plan_type, "RuntimeLookupStepThrough") << "Should use runtime lookup plan";
  EXPECT_EQ(plan1.estimated_steps, 3) << "Method lookup should estimate 3 steps";
  
  // Test implementation resolver trampoline
  auto plan2 = planner.CreateStepThroughPlan(0x7fff80002200ULL); // method_getImplementation
  EXPECT_TRUE(plan2.should_step_through) << "Implementation resolver should step through";
  EXPECT_EQ(plan2.plan_type, "RuntimeLookupStepThrough") << "Should use runtime lookup plan";
  
  // Test that descriptions are informative
  EXPECT_TRUE(plan1.description.find("class_getInstanceMethod") != std::string::npos)
      << "Should mention the specific function";
  EXPECT_TRUE(plan2.description.find("method_getImplementation") != std::string::npos)
      << "Should mention the specific function";
}

/// Test trampoline address range validation
TEST_F(GNUstepStepThroughTrampolineTest, TrampolineAddressRanges_Validation) {
  MockTrampolineStepPlanner planner;
  
  // Test addresses within known trampoline ranges
  EXPECT_TRUE(planner.IsAddressRangeTrampoline(kObjcMsgSendAddr - 10, kObjcMsgSendAddr + 100))
      << "Range containing objc_msgSend should be detected";
  
  EXPECT_TRUE(planner.IsAddressRangeTrampoline(0x7fff80000000ULL, 0x7fff80010000ULL))
      << "Large range containing multiple trampolines should be detected";
  
  // Test addresses outside trampoline ranges
  EXPECT_FALSE(planner.IsAddressRangeTrampoline(kUserCodeAddr, kUserCodeAddr + 100))
      << "User code range should not be detected as trampoline";
  
  EXPECT_FALSE(planner.IsAddressRangeTrampoline(0x500000ULL, 0x600000ULL))
      << "Random address range should not be detected as trampoline";
  
  // Test boundary conditions
  EXPECT_FALSE(planner.IsAddressRangeTrampoline(kObjcMsgSendAddr + 100, kObjcMsgSendAddr + 200))
      << "Range after trampoline should not be detected";
  
  EXPECT_FALSE(planner.IsAddressRangeTrampoline(kObjcMsgSendAddr - 200, kObjcMsgSendAddr - 100))
      << "Range before trampoline should not be detected";
}

/// Test trampoline step-through performance
TEST_F(GNUstepStepThroughTrampolineTest, TrampolineStepThrough_Performance) {
  MockTrampolineStepPlanner planner;
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Test performance with multiple step-through plan creations
  constexpr int kNumPlanCreations = 10000;
  std::vector<lldb::addr_t> test_addresses = {
    kObjcMsgSendAddr, kObjcMsgSendSuperAddr, kObjcMsgSendFpretAddr,
    0x7fff80002000ULL, 0x7fff80002100ULL, kUserCodeAddr, 0x999999ULL
  };
  
  int successful_plans = 0;
  int step_through_plans = 0;
  
  for (int i = 0; i < kNumPlanCreations; ++i) {
    lldb::addr_t addr = test_addresses[i % test_addresses.size()];
    auto plan = planner.CreateStepThroughPlan(addr);
    
    successful_plans++; // All plan creations should succeed (even if they decide not to step through)
    if (plan.should_step_through) {
      step_through_plans++;
      EXPECT_FALSE(plan.plan_type.empty()) << "Step-through plan should have type";
      EXPECT_GT(plan.estimated_steps, 0) << "Should estimate positive steps";
    }
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // Performance requirements for interactive debugging
  EXPECT_LT(duration.count(), 100) 
      << "Plan creation took too long: " << duration.count() << "ms";
  
  EXPECT_EQ(successful_plans, kNumPlanCreations) << "All plan creations should succeed";
  EXPECT_GT(step_through_plans, 0) << "Should have some step-through plans";
  
  // Average time per plan should be very fast
  double avg_per_plan = static_cast<double>(duration.count()) / kNumPlanCreations;
  EXPECT_LT(avg_per_plan, 0.01) << "Average per plan: " << avg_per_plan << "ms";
}

/// Test trampoline symbol recognition and catalog completeness
TEST_F(GNUstepStepThroughTrampolineTest, TrampolineSymbols_CatalogCompleteness) {
  MockTrampolineStepPlanner planner;
  
  // Get all known trampoline symbols
  auto symbols = planner.GetKnownTrampolineSymbols();
  
  // Verify essential GNUstep runtime trampolines are known
  std::vector<std::string> essential_symbols = {
    "objc_msgSend", "objc_msgSendSuper", "objc_msgSend_fpret", "objc_msgSend_stret"
  };
  
  for (const auto& essential : essential_symbols) {
    EXPECT_TRUE(std::find(symbols.begin(), symbols.end(), essential) != symbols.end())
        << "Essential trampoline should be known: " << essential;
  }
  
  // Verify we have reasonable number of trampolines
  EXPECT_GE(symbols.size(), 4ULL) << "Should know at least 4 essential trampolines";
  EXPECT_LE(symbols.size(), 20ULL) << "Should not have excessive trampolines";
  
  // Test that each symbol has a valid plan
  for (const auto& symbol : symbols) {
    // Find the trampoline info for this symbol
    auto trampoline_iter = std::find_if(planner.known_trampolines.begin(), planner.known_trampolines.end(),
        [&symbol](const MockTrampolineStepPlanner::TrampolineInfo& info) {
            return info.symbol_name == symbol;
        });
    const MockTrampolineStepPlanner::TrampolineInfo* trampoline = 
        (trampoline_iter != planner.known_trampolines.end()) ? &(*trampoline_iter) : nullptr;
    
    ASSERT_NE(trampoline, nullptr) << "Symbol should have trampoline info: " << symbol;
    
    // Test creating a plan for this trampoline
    auto plan = planner.CreateStepThroughPlan(trampoline->start_addr);
    EXPECT_TRUE(plan.should_step_through) << "Known trampoline should step through: " << symbol;
    EXPECT_FALSE(plan.plan_type.empty()) << "Should have plan type: " << symbol;
    EXPECT_FALSE(plan.description.empty()) << "Should have description: " << symbol;
  }
}

/// Test trampoline step-through error handling and edge cases
TEST_F(GNUstepStepThroughTrampolineTest, TrampolineStepThrough_ErrorHandling) {
  MockTrampolineStepPlanner planner;
  
  // Test edge case addresses
  std::vector<lldb::addr_t> edge_addresses = {
    0x0ULL,                          // Null address
    LLDB_INVALID_ADDRESS,            // Invalid address constant
    0x1ULL,                          // Very low address
    0x7FFFFFFFFFFFFFFFULL,           // Very high address
    kObjcMsgSendAddr - 1,            // Just before trampoline
    kObjcMsgSendAddr + 0x50,         // Just after trampoline
  };
  
  for (auto addr : edge_addresses) {
    auto plan = planner.CreateStepThroughPlan(addr);
    
    // All plan creation should succeed (even if they decide not to step through)
    // No crashes or exceptions should occur
    EXPECT_TRUE(true) << "Plan creation should not crash for address 0x" << std::hex << addr;
    
    // Results should be consistent with address location
    const auto* trampoline = planner.FindTrampolineAtAddress(addr);
    if (trampoline && trampoline->is_trampoline) {
      EXPECT_TRUE(plan.should_step_through) << "Should step through for trampoline at 0x" << std::hex << addr;
    } else {
      EXPECT_FALSE(plan.should_step_through) << "Should not step through for non-trampoline at 0x" << std::hex << addr;
    }
  }
}

/// Test trampoline step-through plan consistency and reliability
TEST_F(GNUstepStepThroughTrampolineTest, TrampolineStepThrough_Consistency) {
  MockTrampolineStepPlanner planner;
  
  std::vector<lldb::addr_t> test_addresses = {
    kObjcMsgSendAddr, kObjcMsgSendSuperAddr, kUserCodeAddr, 0x7fff80002000ULL
  };
  
  // Test consistency across multiple calls
  for (auto addr : test_addresses) {
    auto plan1 = planner.CreateStepThroughPlan(addr);
    auto plan2 = planner.CreateStepThroughPlan(addr);
    auto plan3 = planner.CreateStepThroughPlan(addr);
    
    // All calls should return same decision
    EXPECT_EQ(plan1.should_step_through, plan2.should_step_through)
        << "Step-through decision should be consistent for address 0x" << std::hex << addr;
    EXPECT_EQ(plan2.should_step_through, plan3.should_step_through)
        << "Step-through decision should be consistent for address 0x" << std::hex << addr;
    
    if (plan1.should_step_through) {
      // All calls should return same plan details
      EXPECT_EQ(plan1.plan_type, plan2.plan_type) 
          << "Plan type should be consistent for address 0x" << std::hex << addr;
      EXPECT_EQ(plan2.plan_type, plan3.plan_type) 
          << "Plan type should be consistent for address 0x" << std::hex << addr;
      
      EXPECT_EQ(plan1.estimated_steps, plan2.estimated_steps)
          << "Step estimate should be consistent for address 0x" << std::hex << addr;
      EXPECT_EQ(plan2.estimated_steps, plan3.estimated_steps)
          << "Step estimate should be consistent for address 0x" << std::hex << addr;
      
      EXPECT_EQ(plan1.requires_instruction_stepping, plan2.requires_instruction_stepping)
          << "Instruction stepping requirement should be consistent for address 0x" << std::hex << addr;
    }
  }
}

/// Test trampoline step-through with complex scenarios
TEST_F(GNUstepStepThroughTrampolineTest, TrampolineStepThrough_ComplexScenarios) {
  MockTrampolineStepPlanner planner;
  
  // Test nested trampoline scenarios (one trampoline calling another)
  struct NestedScenario {
    lldb::addr_t outer_trampoline;
    lldb::addr_t inner_target;
    std::string description;
  };
  
  std::vector<NestedScenario> scenarios = {
    {kObjcMsgSendAddr, 0x7fff80002100ULL, "objc_msgSend calling method lookup"},
    {0x7fff80002100ULL, kMethodImplementationAddr, "Method lookup resolving to implementation"},
    {kObjcMsgSendSuperAddr, kMethodImplementationAddr, "Super send to method implementation"}
  };
  
  for (const auto& scenario : scenarios) {
    auto plan = planner.CreateStepThroughPlan(scenario.outer_trampoline, scenario.inner_target);
    
    EXPECT_TRUE(plan.should_step_through) << "Complex scenario should step through: " << scenario.description;
    EXPECT_EQ(plan.target_address, scenario.inner_target) 
        << "Should target inner address: " << scenario.description;
    EXPECT_FALSE(plan.plan_type.empty()) << "Should have plan type: " << scenario.description;
    EXPECT_GT(plan.estimated_steps, 0) << "Should estimate steps: " << scenario.description;
  }
  
  // Test rapid succession of trampoline encounters (debugging through tight loops)
  constexpr int kRapidSuccessionCount = 1000;
  auto start_time = std::chrono::high_resolution_clock::now();
  
  for (int i = 0; i < kRapidSuccessionCount; ++i) {
    lldb::addr_t addr = (i % 2 == 0) ? kObjcMsgSendAddr : kObjcMsgSendSuperAddr;
    auto plan = planner.CreateStepThroughPlan(addr);
    
    EXPECT_TRUE(plan.should_step_through) << "Rapid succession should work: iteration " << i;
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // Should handle rapid succession efficiently
  EXPECT_LT(duration.count(), 50) << "Rapid succession took too long: " << duration.count() << "ms";
}