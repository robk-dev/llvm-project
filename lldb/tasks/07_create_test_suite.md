# Task 07: Create Comprehensive Test Suite

## Problem Statement
The GNUstep LLDB integration needs a robust test suite to ensure reliability across platforms and prevent regressions. Current testing may be informal or incomplete, making it difficult to validate the MVP functionality.

## Testing Strategy Overview

### Test Categories
1. **Unit Tests**: Individual component functionality
2. **Integration Tests**: End-to-end expression evaluation  
3. **Platform Tests**: Windows-specific and cross-platform validation
4. **Regression Tests**: Prevent previously fixed issues from reoccurring
5. **Performance Tests**: Ensure no significant performance impact

### Test Environment Setup
- Windows x64 with GNUstep/libobjc2
- Linux with GNUstep development environment
- Test programs with various ObjC constructs
- LLDB configured with GNUstep runtime

## Implementation Plan

### Step 1: Unit Test Infrastructure

#### Test Runtime Detection
```cpp
// File: unittests/Language/ObjC/GNUstep/RuntimeDetectionTest.cpp
#include "gtest/gtest.h"
#include "TestingSupport/Host/NativeProcessTestUtils.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h"

class GNUstepRuntimeDetectionTest : public testing::Test {
public:
    void SetUp() override {
        // Create mock process with GNUstep symbols
    }
    
    void TearDown() override {
        // Cleanup
    }
};

TEST_F(GNUstepRuntimeDetectionTest, DetectsObjCMarkers) {
    // Test that CreateInstance succeeds when ObjC symbols are present
    auto runtime = GNUstepObjCRuntime::CreateInstance(process, eLanguageTypeObjC);
    ASSERT_NE(nullptr, runtime);
}

TEST_F(GNUstepRuntimeDetectionTest, RejectsNonObjCPrograms) {
    // Test that CreateInstance rejects programs without ObjC symbols
    auto runtime = GNUstepObjCRuntime::CreateInstance(pure_c_process, eLanguageTypeC);
    ASSERT_EQ(nullptr, runtime);
}

TEST_F(GNUstepRuntimeDetectionTest, AcceptsCInObjCContext) {
    // Test that C expressions are accepted when ObjC context exists
    auto objc_runtime = GNUstepObjCRuntime::CreateInstance(process, eLanguageTypeObjC);
    ASSERT_NE(nullptr, objc_runtime);
    
    auto c_runtime = GNUstepObjCRuntime::CreateInstance(process, eLanguageTypeC);
    ASSERT_NE(nullptr, c_runtime); // Should succeed when ObjC context exists
}
```

#### Test Symbol Resolution
```cpp
// File: unittests/Language/ObjC/GNUstep/SymbolResolutionTest.cpp
TEST_F(GNUstepSymbolResolutionTest, ResolvesDirectSymbols) {
    auto addr = runtime->ResolveRuntimeSymbol("objc_msgSend");
    EXPECT_NE(LLDB_INVALID_ADDRESS, addr);
}

TEST_F(GNUstepSymbolResolutionTest, ResolvesImportThunks) {
    // Test Windows __imp_ symbols
    auto addr = runtime->ResolveRuntimeSymbol("objc_getClass");
    EXPECT_NE(LLDB_INVALID_ADDRESS, addr);
}

TEST_F(GNUstepSymbolResolutionTest, CachesResults) {
    auto addr1 = runtime->ResolveRuntimeSymbol("sel_getUid");
    auto addr2 = runtime->ResolveRuntimeSymbol("sel_getUid");
    EXPECT_EQ(addr1, addr2);
    EXPECT_NE(LLDB_INVALID_ADDRESS, addr1);
}
```

#### Test Literals and Subscripting
```cpp
// File: unittests/Language/ObjC/GNUstep/LiteralsTest.cpp
TEST_F(GNUstepLiteralsTest, DetectsSubscriptingSupport) {
    bool has_literals = runtime->CalculateHasNewLiteralsAndIndexing();
    EXPECT_TRUE(has_literals); // Should detect modern GNUstep Base
}

TEST_F(GNUstepLiteralsTest, HandlesOlderGNUstepBase) {
    // Test with older GNUstep that lacks subscripting
    bool has_literals = old_runtime->CalculateHasNewLiteralsAndIndexing();
    EXPECT_FALSE(has_literals);
}
```

### Step 2: Integration Test Framework

#### Test Program Creation
```cpp
// File: test/API/lang/objc/gnustep/TestPrograms/simple_gnustep_program.m
#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Basic NSNumber operations
        NSNumber *num = [NSNumber numberWithInt:42];
        int value = [num intValue];
        
        // Basic NSString operations  
        NSString *str = @"Hello";
        NSUInteger length = [str length];
        
        // Basic NSArray operations
        NSArray *arr = [NSArray arrayWithObjects:@"a", @"b", nil];
        NSString *first = [arr objectAtIndex:0];
        
        // Modern subscripting (if supported)
        if ([arr respondsToSelector:@selector(objectAtIndexedSubscript:)]) {
            NSString *second = arr[1];
        }
        
        return 0; // Breakpoint here
    }
}
```

#### Expression Evaluation Tests
```python
# File: test/API/lang/objc/gnustep/TestGNUstepExpressions.py
import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
import lldbsuite.test.lldbutil as lldbutil

class TestGNUstepExpressions(TestBase):
    
    @skipUnlessGNUstep
    @skipUnlessWindows
    def test_basic_number_operations(self):
        """Test NSNumber creation and value extraction"""
        self.build()
        
        # Set breakpoint and run
        breakpoint = self.target().BreakpointCreateByLocation("simple_gnustep_program.m", 20)
        self.assertTrue(breakpoint.IsValid())
        
        process = self.target().LaunchSimple(None, None, self.get_process_working_directory())
        self.assertState(process.GetState(), lldb.eStateStopped)
        
        # Test NSNumber creation
        result = self.frame().EvaluateExpression("(id)[NSNumber numberWithInt:7]")
        self.assertTrue(result.IsValid())
        self.assertFalse(result.GetError().Fail())
        
        # Test value extraction
        result = self.frame().EvaluateExpression("(int)[(id)[NSNumber numberWithInt:7] intValue]")
        self.assertTrue(result.IsValid())
        self.assertEqual(result.GetValueAsSigned(), 7)
    
    @skipUnlessGNUstep  
    @skipUnlessWindows
    def test_string_literals(self):
        """Test @"" string literal support"""
        # Test basic string literal
        result = self.frame().EvaluateExpression('@"hello"')
        self.assertTrue(result.IsValid())
        self.assertFalse(result.GetError().Fail())
        
        # Test string length
        result = self.frame().EvaluateExpression('[@"test" length]')
        self.assertTrue(result.IsValid())
        self.assertEqual(result.GetValueAsUnsigned(), 4)
    
    @skipUnlessGNUstep
    def test_array_subscripting(self):
        """Test array subscripting if supported"""
        # Create array
        result = self.frame().EvaluateExpression('id arr = [NSArray arrayWithObjects:@"a", @"b", nil]')
        self.assertTrue(result.IsValid())
        
        # Test subscripting if available
        if self.runtime_supports_subscripting():
            result = self.frame().EvaluateExpression('(id)arr[0]')
            self.assertTrue(result.IsValid())
            # Verify it's the correct string
    
    def runtime_supports_subscripting(self):
        """Check if runtime supports modern subscripting"""
        result = self.frame().EvaluateExpression('[NSArray instancesRespondToSelector:@selector(objectAtIndexedSubscript:)]')
        return result.IsValid() and result.GetValueAsSigned() != 0
```

### Step 3: Windows-Specific Tests

#### Calling Convention Tests
```python
# File: test/API/lang/objc/gnustep/TestWindowsCallingConvention.py
class TestWindowsCallingConvention(TestBase):
    
    @skipUnlessGNUstep
    @skipUnlessWindows
    def test_objc_msgSend_calling_convention(self):
        """Verify objc_msgSend uses correct Win64 calling convention"""
        # This test verifies that we don't get access violations
        result = self.frame().EvaluateExpression("(id)[NSNumber numberWithDouble:3.14159]")
        self.assertTrue(result.IsValid())
        self.assertFalse(result.GetError().Fail())
        
        # Test that return values are handled correctly
        result = self.frame().EvaluateExpression("(double)[(id)[NSNumber numberWithDouble:3.14159] doubleValue]")
        self.assertTrue(result.IsValid())
        self.assertAlmostEqual(result.GetValueAsDouble(), 3.14159, places=5)
```

#### Symbol Resolution Tests
```python
# File: test/API/lang/objc/gnustep/TestSymbolResolution.py
class TestSymbolResolution(TestBase):
    
    @skipUnlessGNUstep
    @skipUnlessWindows
    def test_import_thunk_resolution(self):
        """Test that Windows DLL import thunks are resolved"""
        # Verify core runtime symbols are found
        result = self.frame().EvaluateExpression("(void*)objc_msgSend")
        self.assertTrue(result.IsValid())
        self.assertNotEqual(result.GetValueAsAddress(), 0)
        
        result = self.frame().EvaluateExpression("(void*)objc_getClass")
        self.assertTrue(result.IsValid())
        self.assertNotEqual(result.GetValueAsAddress(), 0)
```

### Step 4: Performance Tests

#### Startup Performance
```cpp
// File: unittests/Language/ObjC/GNUstep/PerformanceTest.cpp
TEST_F(GNUstepPerformanceTest, FastStartup) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto runtime = GNUstepObjCRuntime::CreateInstance(process, eLanguageTypeObjC);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Runtime creation should be fast (< 100ms)
    EXPECT_LT(duration.count(), 100);
}

TEST_F(GNUstepPerformanceTest, SymbolResolutionCaching) {
    // First resolution (may be slow)
    auto start1 = std::chrono::high_resolution_clock::now();
    auto addr1 = runtime->ResolveRuntimeSymbol("objc_msgSend");
    auto end1 = std::chrono::high_resolution_clock::now();
    
    // Second resolution (should be fast due to caching)
    auto start2 = std::chrono::high_resolution_clock::now();
    auto addr2 = runtime->ResolveRuntimeSymbol("objc_msgSend");
    auto end2 = std::chrono::high_resolution_clock::now();
    
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    
    EXPECT_EQ(addr1, addr2);
    EXPECT_LT(duration2.count(), duration1.count() / 10); // Should be 10x faster
}
```

### Step 5: Test Configuration and CI Integration

#### CMake Test Configuration
```cmake
# File: unittests/Language/ObjC/GNUstep/CMakeLists.txt
if(LLDB_TEST_OBJC_GNUSTEP)
    add_lldb_unittest(GNUstepObjCRuntimeTests
        RuntimeDetectionTest.cpp
        SymbolResolutionTest.cpp
        LiteralsTest.cpp
        PerformanceTest.cpp
        ${LLDB_TEST_COMMON_LIBS}
    )
    
    target_link_libraries(GNUstepObjCRuntimeTests
        lldbLanguageObjCGNUstepObjCRuntime
        lldbCore
        lldbHost
        lldbTarget
    )
endif()
```

#### Test Environment Setup
```bash
# File: test/Shell/helper/gnustep_test_setup.sh
#!/bin/bash
# Set up GNUstep environment for testing

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    # Windows/MSYS2 setup
    export GNUSTEP_SYSTEM_ROOT="/ucrt64"
    export PATH="/ucrt64/bin:$PATH"
    export LIBRARY_PATH="/ucrt64/lib:$LIBRARY_PATH"
else
    # Linux setup
    source /usr/share/GNUstep/Makefiles/GNUstep.sh
fi

# Verify GNUstep is available
if ! command -v gnustep-config &> /dev/null; then
    echo "GNUstep not found, skipping tests"
    exit 77  # Skip code for lit tests
fi
```

### Step 6: Regression Tests

#### Known Issue Prevention
```python
# File: test/API/lang/objc/gnustep/TestRegressions.py
class TestRegressions(TestBase):
    
    def test_no_infinite_recursion_in_formatters(self):
        """Regression test for formatter infinite recursion"""
        # This should not hang or crash
        result = self.frame().EvaluateExpression('@"test"', options=lldb.SBExpressionOptions())
        self.assertTrue(result.IsValid(), "String literal evaluation failed")
    
    def test_c_language_expression_handling(self):
        """Regression test for C language expression rejection"""
        # Ensure C expressions work when ObjC context is present
        result = self.frame().EvaluateExpression('1 + 1', lldb.SBExpressionOptions())
        self.assertTrue(result.IsValid())
        self.assertEqual(result.GetValueAsSigned(), 2)
```

## Files to Create/Modify
- `unittests/Language/ObjC/GNUstep/RuntimeDetectionTest.cpp`
- `unittests/Language/ObjC/GNUstep/SymbolResolutionTest.cpp`
- `unittests/Language/ObjC/GNUstep/LiteralsTest.cpp`
- `unittests/Language/ObjC/GNUstep/PerformanceTest.cpp`
- `test/API/lang/objc/gnustep/TestGNUstepExpressions.py`
- `test/API/lang/objc/gnustep/TestWindowsCallingConvention.py`
- `test/API/lang/objc/gnustep/TestSymbolResolution.py`
- `test/API/lang/objc/gnustep/TestRegressions.py`
- `test/API/lang/objc/gnustep/TestPrograms/simple_gnustep_program.m`

## Success Criteria
- [ ] All unit tests pass on development systems
- [ ] Integration tests validate MVP functionality
- [ ] Windows-specific tests pass on Windows x64
- [ ] Performance tests show acceptable overhead
- [ ] Regression tests prevent known issues
- [ ] Test suite can be run in CI environment

## Test Execution Strategy

### Local Development
```bash
# Run unit tests
ninja check-lldb-unit

# Run specific GNUstep tests
ninja check-lldb-api-lang-objc-gnustep

# Run performance tests
ninja check-lldb-unit-gnustep-performance
```

### CI Integration
- Enable `LLDB_TEST_OBJC_GNUSTEP` in CI environments with GNUstep
- Conditional test execution based on platform and availability
- Clear reporting of test results and failures

## Implementation Status
- [ ] Unit test framework created
- [ ] Integration tests implemented
- [ ] Platform-specific tests added
- [ ] Performance tests developed
- [ ] Regression tests created
- [ ] CI integration configured
- [ ] All tests passing
- [ ] Ready for review

## Dependencies
- Requires Tasks 01-06 to be completed for testing
- May uncover issues requiring fixes in earlier tasks
