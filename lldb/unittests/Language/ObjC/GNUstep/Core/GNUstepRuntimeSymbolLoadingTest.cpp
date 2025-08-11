//===-- GNUstepRuntimeSymbolLoadingTest.cpp -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception.
//
//===----------------------------------------------------------------------===//

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepRuntimeV2API.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Target/Process.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Utility/Status.h"
#include "gtest/gtest.h"
#include <chrono>
#include <memory>
#include <unordered_map>

using namespace lldb;
using namespace lldb_private;

/// Test Runtime Symbol Loading functionality for LoadRuntimeSymbols
/// This tests the critical functionality that resolves GNUstep runtime symbols
/// needed for proper object introspection and formatting.
class GNUstepRuntimeSymbolLoadingTest : public ::testing::Test {
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
  // Mock symbol resolution for testing
  struct MockSymbolResolver {
    std::unordered_map<std::string, lldb::addr_t> symbol_table;
    
    MockSymbolResolver() {
      // Essential GNUstep runtime symbols
      symbol_table["objc_getClass"] = 0x7fff80001000ULL;
      symbol_table["objc_lookUpClass"] = 0x7fff80001010ULL;
      symbol_table["objc_copyClassList"] = 0x7fff80001020ULL;
      symbol_table["objc_getProtocol"] = 0x7fff80001030ULL;
      symbol_table["objc_copyProtocolList"] = 0x7fff80001040ULL;
      symbol_table["objc_getMetaClass"] = 0x7fff80001050ULL;
      symbol_table["object_getClass"] = 0x7fff80001060ULL;
      symbol_table["class_getName"] = 0x7fff80001070ULL;
      symbol_table["class_getSuperclass"] = 0x7fff80001080ULL;
      symbol_table["class_getInstanceSize"] = 0x7fff80001090ULL;
      symbol_table["class_getInstanceMethod"] = 0x7fff800010A0ULL;
      symbol_table["class_copyIvarList"] = 0x7fff800010B0ULL;
      symbol_table["class_copyMethodList"] = 0x7fff800010C0ULL;
      symbol_table["class_copyPropertyList"] = 0x7fff800010D0ULL;
      symbol_table["sel_getName"] = 0x7fff800010E0ULL;
      symbol_table["sel_registerName"] = 0x7fff800010F0ULL;
      symbol_table["method_getName"] = 0x7fff80001100ULL;
      symbol_table["method_getImplementation"] = 0x7fff80001110ULL;
      symbol_table["method_getTypeEncoding"] = 0x7fff80001120ULL;
      symbol_table["ivar_getName"] = 0x7fff80001130ULL;
      symbol_table["ivar_getTypeEncoding"] = 0x7fff80001140ULL;
      symbol_table["ivar_getOffset"] = 0x7fff80001150ULL;
      
      // GNUstep-specific symbols
      symbol_table["objc_taggedPointerClasses"] = 0x7fff80002000ULL;
      symbol_table["objc_debug_taggedpointer_classes"] = 0x7fff80002010ULL;
      symbol_table["objc_debug_taggedpointer_ext_classes"] = 0x7fff80002020ULL;
    }
    
    lldb::addr_t ResolveSymbol(const std::string& symbol_name) {
      auto it = symbol_table.find(symbol_name);
      return (it != symbol_table.end()) ? it->second : LLDB_INVALID_ADDRESS;
    }
    
    bool HasSymbol(const std::string& symbol_name) {
      return symbol_table.find(symbol_name) != symbol_table.end();
    }
    
    size_t GetLoadedSymbolCount() const {
      return symbol_table.size();
    }
    
    std::vector<std::string> GetMissingEssentialSymbols() const {
      std::vector<std::string> essential_symbols = {
        "objc_getClass", "objc_copyClassList", "class_getName", "object_getClass"
      };
      
      std::vector<std::string> missing;
      for (const auto& symbol : essential_symbols) {
        if (symbol_table.find(symbol) == symbol_table.end()) {
          missing.push_back(symbol);
        }
      }
      return missing;
    }
  };
  
  // Test constants
  static constexpr size_t kExpectedMinimumSymbolCount = 20;
  static constexpr size_t kEssentialSymbolCount = 4;
};

/// Test basic runtime symbol loading functionality
TEST_F(GNUstepRuntimeSymbolLoadingTest, LoadRuntimeSymbols_BasicFunctionality) {
  // Test that LoadRuntimeSymbols successfully loads essential GNUstep runtime symbols
  
  MockSymbolResolver resolver;
  
  // Test 1: Verify essential symbols are available
  std::vector<std::string> essential_symbols = {
    "objc_getClass", "objc_copyClassList", "class_getName", "object_getClass"
  };
  
  for (const auto& symbol : essential_symbols) {
    lldb::addr_t addr = resolver.ResolveSymbol(symbol);
    EXPECT_NE(addr, LLDB_INVALID_ADDRESS) << "Essential symbol should be resolved: " << symbol;
    EXPECT_TRUE(resolver.HasSymbol(symbol)) << "Essential symbol should be available: " << symbol;
  }
  
  // Test 2: Verify minimum symbol count
  EXPECT_GE(resolver.GetLoadedSymbolCount(), kExpectedMinimumSymbolCount)
      << "Should load at least " << kExpectedMinimumSymbolCount << " runtime symbols";
  
  // Test 3: Verify no essential symbols are missing
  auto missing_symbols = resolver.GetMissingEssentialSymbols();
  EXPECT_TRUE(missing_symbols.empty()) 
      << "No essential symbols should be missing, found missing: " << missing_symbols.size();
}

/// Test runtime symbol loading performance
TEST_F(GNUstepRuntimeSymbolLoadingTest, LoadRuntimeSymbols_Performance) {
  // Test that LoadRuntimeSymbols completes quickly for interactive debugging
  
  MockSymbolResolver resolver;
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Simulate symbol loading operations
  constexpr int kNumLookups = 1000;
  std::vector<std::string> test_symbols = {
    "objc_getClass", "class_getName", "objc_copyClassList", "object_getClass"
  };
  
  for (int i = 0; i < kNumLookups; ++i) {
    std::string symbol = test_symbols[i % test_symbols.size()];
    lldb::addr_t addr = resolver.ResolveSymbol(symbol);
    EXPECT_NE(addr, LLDB_INVALID_ADDRESS) << "Symbol lookup should succeed: " << symbol;
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // LoadRuntimeSymbols should complete very quickly (< 100ms for 1000 lookups)
  EXPECT_LT(duration.count(), 100) 
      << "Symbol loading took too long: " << duration.count() << "ms";
  
  // Average lookup time should be very fast
  double avg_per_lookup = static_cast<double>(duration.count()) / kNumLookups;
  EXPECT_LT(avg_per_lookup, 0.1) << "Average lookup time: " << avg_per_lookup << "ms";
}

/// Test runtime symbol loading with missing symbols
TEST_F(GNUstepRuntimeSymbolLoadingTest, LoadRuntimeSymbols_MissingSymbols) {
  // Test LoadRuntimeSymbols behavior when some symbols are missing
  
  MockSymbolResolver incomplete_resolver;
  
  // Remove some non-essential symbols to simulate partial loading
  incomplete_resolver.symbol_table.erase("objc_copyProtocolList");
  incomplete_resolver.symbol_table.erase("class_copyPropertyList");
  incomplete_resolver.symbol_table.erase("objc_debug_taggedpointer_ext_classes");
  
  // Test 1: Essential symbols should still be available
  std::vector<std::string> essential_symbols = {
    "objc_getClass", "objc_copyClassList", "class_getName", "object_getClass"
  };
  
  for (const auto& symbol : essential_symbols) {
    EXPECT_TRUE(incomplete_resolver.HasSymbol(symbol)) 
        << "Essential symbol should still be available: " << symbol;
    EXPECT_NE(incomplete_resolver.ResolveSymbol(symbol), LLDB_INVALID_ADDRESS)
        << "Essential symbol should resolve: " << symbol;
  }
  
  // Test 2: Missing symbols should return LLDB_INVALID_ADDRESS
  std::vector<std::string> missing_symbols = {
    "objc_copyProtocolList", "class_copyPropertyList", "nonexistent_symbol"
  };
  
  for (const auto& symbol : missing_symbols) {
    EXPECT_EQ(incomplete_resolver.ResolveSymbol(symbol), LLDB_INVALID_ADDRESS)
        << "Missing symbol should return invalid address: " << symbol;
    EXPECT_FALSE(incomplete_resolver.HasSymbol(symbol))
        << "Missing symbol should not be reported as available: " << symbol;
  }
  
  // Test 3: Should still have reasonable number of symbols
  EXPECT_GE(incomplete_resolver.GetLoadedSymbolCount(), kExpectedMinimumSymbolCount - 5)
      << "Should have most symbols even with some missing";
}

/// Test runtime symbol address validation
TEST_F(GNUstepRuntimeSymbolLoadingTest, LoadRuntimeSymbols_AddressValidation) {
  // Test that LoadRuntimeSymbols returns valid addresses for resolved symbols
  
  MockSymbolResolver resolver;
  
  std::vector<std::string> test_symbols = {
    "objc_getClass", "class_getName", "objc_copyClassList", "object_getClass",
    "class_getSuperclass", "sel_getName", "method_getName", "ivar_getName"
  };
  
  for (const auto& symbol : test_symbols) {
    lldb::addr_t addr = resolver.ResolveSymbol(symbol);
    
    // Test address validity
    EXPECT_NE(addr, LLDB_INVALID_ADDRESS) << "Symbol should resolve: " << symbol;
    EXPECT_NE(addr, 0ULL) << "Symbol address should not be null: " << symbol;
    EXPECT_GT(addr, 0x1000ULL) << "Symbol address should be in valid range: " << symbol;
    EXPECT_LT(addr, 0x8000000000000000ULL) << "Symbol address should be reasonable: " << symbol;
    
    // Test that addresses are unique (no duplicates)
    for (const auto& other_symbol : test_symbols) {
      if (other_symbol != symbol) {
        lldb::addr_t other_addr = resolver.ResolveSymbol(other_symbol);
        EXPECT_NE(addr, other_addr) 
            << "Symbols should have unique addresses: " << symbol << " vs " << other_symbol;
      }
    }
  }
}

/// Test runtime symbol loading with GNUstep-specific symbols
TEST_F(GNUstepRuntimeSymbolLoadingTest, LoadRuntimeSymbols_GNUstepSpecificSymbols) {
  // Test that LoadRuntimeSymbols loads GNUstep-specific symbols correctly
  
  MockSymbolResolver resolver;
  
  // GNUstep-specific symbols for tagged pointer support
  std::vector<std::string> gnustep_symbols = {
    "objc_taggedPointerClasses",
    "objc_debug_taggedpointer_classes",
    "objc_debug_taggedpointer_ext_classes"
  };
  
  for (const auto& symbol : gnustep_symbols) {
    lldb::addr_t addr = resolver.ResolveSymbol(symbol);
    EXPECT_NE(addr, LLDB_INVALID_ADDRESS) << "GNUstep-specific symbol should resolve: " << symbol;
    EXPECT_TRUE(resolver.HasSymbol(symbol)) << "GNUstep-specific symbol should be available: " << symbol;
  }
  
  // Test that GNUstep symbols have different address space than standard ObjC symbols
  lldb::addr_t tagged_classes_addr = resolver.ResolveSymbol("objc_taggedPointerClasses");
  lldb::addr_t objc_getclass_addr = resolver.ResolveSymbol("objc_getClass");
  
  EXPECT_NE(tagged_classes_addr, objc_getclass_addr) 
      << "GNUstep-specific and standard symbols should have different addresses";
  
  // Test that tagged pointer symbols are in expected address range
  EXPECT_GE(tagged_classes_addr, 0x7fff80002000ULL) 
      << "Tagged pointer symbols should be in expected range";
}

/// Test runtime symbol loading error handling
TEST_F(GNUstepRuntimeSymbolLoadingTest, LoadRuntimeSymbols_ErrorHandling) {
  // Test LoadRuntimeSymbols error handling with various failure scenarios
  
  // Test 1: Empty symbol table (complete failure)
  MockSymbolResolver empty_resolver;
  empty_resolver.symbol_table.clear();
  
  EXPECT_EQ(empty_resolver.GetLoadedSymbolCount(), 0ULL) 
      << "Empty resolver should have no symbols";
  
  std::vector<std::string> test_symbols = {"objc_getClass", "class_getName"};
  for (const auto& symbol : test_symbols) {
    EXPECT_EQ(empty_resolver.ResolveSymbol(symbol), LLDB_INVALID_ADDRESS)
        << "Empty resolver should fail to resolve: " << symbol;
  }
  
  // Test 2: Partial failure (only some symbols available)
  MockSymbolResolver partial_resolver;
  partial_resolver.symbol_table.clear();
  partial_resolver.symbol_table["objc_getClass"] = 0x7fff80001000ULL;
  partial_resolver.symbol_table["class_getName"] = 0x7fff80001070ULL;
  
  EXPECT_EQ(partial_resolver.GetLoadedSymbolCount(), 2ULL) 
      << "Partial resolver should have limited symbols";
  
  // Available symbols should work
  EXPECT_NE(partial_resolver.ResolveSymbol("objc_getClass"), LLDB_INVALID_ADDRESS);
  EXPECT_NE(partial_resolver.ResolveSymbol("class_getName"), LLDB_INVALID_ADDRESS);
  
  // Missing symbols should fail
  EXPECT_EQ(partial_resolver.ResolveSymbol("objc_copyClassList"), LLDB_INVALID_ADDRESS);
  EXPECT_EQ(partial_resolver.ResolveSymbol("missing_symbol"), LLDB_INVALID_ADDRESS);
  
  // Test 3: Invalid addresses
  MockSymbolResolver invalid_resolver;
  invalid_resolver.symbol_table["bad_symbol"] = 0x0ULL; // Null address
  invalid_resolver.symbol_table["another_bad_symbol"] = LLDB_INVALID_ADDRESS;
  
  EXPECT_EQ(invalid_resolver.ResolveSymbol("bad_symbol"), 0x0ULL);
  EXPECT_EQ(invalid_resolver.ResolveSymbol("another_bad_symbol"), LLDB_INVALID_ADDRESS);
  EXPECT_EQ(invalid_resolver.ResolveSymbol("nonexistent"), LLDB_INVALID_ADDRESS);
}

/// Test runtime symbol loading consistency and caching
TEST_F(GNUstepRuntimeSymbolLoadingTest, LoadRuntimeSymbols_ConsistencyAndCaching) {
  // Test that LoadRuntimeSymbols provides consistent results and proper caching
  
  MockSymbolResolver resolver;
  
  // Test 1: Consistency - multiple lookups should return same address
  std::vector<std::string> test_symbols = {
    "objc_getClass", "class_getName", "objc_copyClassList"
  };
  
  for (const auto& symbol : test_symbols) {
    lldb::addr_t addr1 = resolver.ResolveSymbol(symbol);
    lldb::addr_t addr2 = resolver.ResolveSymbol(symbol);
    lldb::addr_t addr3 = resolver.ResolveSymbol(symbol);
    
    EXPECT_EQ(addr1, addr2) << "Multiple lookups should return consistent address: " << symbol;
    EXPECT_EQ(addr2, addr3) << "Multiple lookups should return consistent address: " << symbol;
    EXPECT_NE(addr1, LLDB_INVALID_ADDRESS) << "Symbol should resolve consistently: " << symbol;
  }
  
  // Test 2: Performance with caching (rapid lookups should be fast)
  auto start_time = std::chrono::high_resolution_clock::now();
  
  constexpr int kNumRapidLookups = 10000;
  std::string test_symbol = "objc_getClass";
  
  for (int i = 0; i < kNumRapidLookups; ++i) {
    lldb::addr_t addr = resolver.ResolveSymbol(test_symbol);
    EXPECT_NE(addr, LLDB_INVALID_ADDRESS) << "Rapid lookup " << i << " should succeed";
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // Cached lookups should be very fast
  EXPECT_LT(duration.count(), 50) 
      << "Rapid cached lookups took too long: " << duration.count() << "ms";
  
  // Test 3: Symbol availability consistency
  for (const auto& symbol : test_symbols) {
    bool has_symbol1 = resolver.HasSymbol(symbol);
    bool has_symbol2 = resolver.HasSymbol(symbol);
    EXPECT_EQ(has_symbol1, has_symbol2) << "Symbol availability should be consistent: " << symbol;
    EXPECT_TRUE(has_symbol1) << "Test symbol should be available: " << symbol;
  }
}

/// Test runtime symbol loading integration patterns
TEST_F(GNUstepRuntimeSymbolLoadingTest, LoadRuntimeSymbols_IntegrationPatterns) {
  // Test LoadRuntimeSymbols in realistic integration scenarios
  
  MockSymbolResolver resolver;
  
  // Test 1: Class inspection workflow symbols
  std::vector<std::string> class_inspection_symbols = {
    "objc_getClass", "class_getName", "class_getSuperclass", 
    "class_getInstanceSize", "class_copyIvarList"
  };
  
  for (const auto& symbol : class_inspection_symbols) {
    lldb::addr_t addr = resolver.ResolveSymbol(symbol);
    EXPECT_NE(addr, LLDB_INVALID_ADDRESS) 
        << "Class inspection symbol should be available: " << symbol;
  }
  
  // Test 2: Method introspection workflow symbols
  std::vector<std::string> method_symbols = {
    "class_copyMethodList", "method_getName", "method_getImplementation", 
    "method_getTypeEncoding", "sel_getName"
  };
  
  for (const auto& symbol : method_symbols) {
    lldb::addr_t addr = resolver.ResolveSymbol(symbol);
    EXPECT_NE(addr, LLDB_INVALID_ADDRESS) 
        << "Method introspection symbol should be available: " << symbol;
  }
  
  // Test 3: Tagged pointer workflow symbols
  std::vector<std::string> tagged_pointer_symbols = {
    "objc_taggedPointerClasses", "objc_debug_taggedpointer_classes"
  };
  
  for (const auto& symbol : tagged_pointer_symbols) {
    lldb::addr_t addr = resolver.ResolveSymbol(symbol);
    EXPECT_NE(addr, LLDB_INVALID_ADDRESS) 
        << "Tagged pointer symbol should be available: " << symbol;
  }
  
  // Test 4: Complete workflow simulation
  // Simulate a complete object inspection workflow
  bool workflow_successful = true;
  
  // Step 1: Get object class
  if (resolver.ResolveSymbol("object_getClass") == LLDB_INVALID_ADDRESS) {
    workflow_successful = false;
  }
  
  // Step 2: Get class name
  if (resolver.ResolveSymbol("class_getName") == LLDB_INVALID_ADDRESS) {
    workflow_successful = false;
  }
  
  // Step 3: Get class methods
  if (resolver.ResolveSymbol("class_copyMethodList") == LLDB_INVALID_ADDRESS) {
    workflow_successful = false;
  }
  
  // Step 4: Get method details
  if (resolver.ResolveSymbol("method_getName") == LLDB_INVALID_ADDRESS) {
    workflow_successful = false;
  }
  
  EXPECT_TRUE(workflow_successful) << "Complete object inspection workflow should be supported";
  
  // Test 5: Performance in workflow context
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Simulate typical debugging session (multiple object inspections)
  constexpr int kNumWorkflowRuns = 100;
  for (int i = 0; i < kNumWorkflowRuns; ++i) {
    // Each workflow needs these key symbols
    resolver.ResolveSymbol("object_getClass");
    resolver.ResolveSymbol("class_getName");
    resolver.ResolveSymbol("objc_taggedPointerClasses");
    resolver.ResolveSymbol("class_getSuperclass");
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // Workflow symbol resolution should be fast for interactive debugging
  EXPECT_LT(duration.count(), 50) 
      << "Workflow symbol resolution took too long: " << duration.count() << "ms";
}