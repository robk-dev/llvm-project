# GNUstep LLDB Plugin - Testing Instructions

## Overview
This document provides comprehensive testing requirements and instructions for validating the GNUstep LLDB plugin functionality.

## Test Coverage Gaps Analysis

### Critical Missing Test Coverage

#### 1. Existing Formatters Without Tests (7 formatters)
These formatters are implemented and registered but lack test programs:
- NSAttributedString
- NSIndexPath  
- NSNull
- NSException
- NSNotification
- NSUUID
- NSData

#### 2. Unit Test Coverage (0% - CRITICAL)
No unit tests exist for:
- Memory access patterns
- String extraction logic
- ISA resolution
- Tagged pointer handling
- Buffer overflow prevention

#### 3. Integration Test Coverage (Minimal)
Limited coverage for:
- Formatter interactions
- Nested object display
- Large collection handling
- Performance validation

## Test Implementation Strategy

### Phase 1: Create Missing Test Programs (Priority 0)

#### Test Program Template
```objc
// test_[classname].m
#import <Foundation/Foundation.h>

void test_nil_object() {
    NSClassName *obj = nil;
    NSLog(@"Testing nil object");  // Breakpoint 1
}

void test_empty_object() {
    NSClassName *obj = [[NSClassName alloc] init];
    NSLog(@"Testing empty object"); // Breakpoint 2
}

void test_simple_object() {
    NSClassName *obj = [NSClassName simpleExample];
    NSLog(@"Testing simple object"); // Breakpoint 3
}

void test_complex_object() {
    NSClassName *obj = [NSClassName complexExample];
    NSLog(@"Testing complex object"); // Breakpoint 4
}

void test_edge_cases() {
    // Class-specific edge cases
    NSLog(@"Testing edge cases"); // Breakpoint 5
}

void test_performance() {
    // Large dataset if applicable
    NSLog(@"Testing performance"); // Breakpoint 6
}

int main(int argc, char *argv[]) {
    @autoreleasepool {
        test_nil_object();
        test_empty_object();
        test_simple_object();
        test_complex_object();
        test_edge_cases();
        test_performance();
        
        NSLog(@"All tests complete");
        return 0;
    }
}
```

#### Specific Test Programs Needed

##### test_attributedstring.m
```objc
#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Test 1: Plain string
        NSAttributedString *plain = [[NSAttributedString alloc] 
            initWithString:@"Hello World"];
        
        // Test 2: With attributes
        NSDictionary *attrs = @{
            NSFontAttributeName: @"Helvetica",
            NSForegroundColorAttributeName: @"Red"
        };
        NSAttributedString *styled = [[NSAttributedString alloc]
            initWithString:@"Styled Text" attributes:attrs];
        
        // Test 3: Mutable variant
        NSMutableAttributedString *mutable = [[NSMutableAttributedString alloc]
            initWithString:@"Mutable String"];
        [mutable addAttribute:NSFontAttributeName 
                        value:@"Bold"
                        range:NSMakeRange(0, 7)];
        
        // Test 4: Empty string
        NSAttributedString *empty = [[NSAttributedString alloc] initWithString:@""];
        
        // Test 5: Nil
        NSAttributedString *nil_str = nil;
        
        NSLog(@"Breakpoint here"); // Set breakpoint
        
        // Expected outputs:
        // plain = @"Hello World" (0 attributes)
        // styled = @"Styled Text" (2 attributes)
        // mutable = @"Mutable String" (1 attribute)
        // empty = @"" (0 attributes)
        // nil_str = nil
        
        return 0;
    }
}
```

##### test_indexpath.m
```objc
#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Test 1: Single index
        NSIndexPath *single = [NSIndexPath indexPathWithIndex:5];
        
        // Test 2: Two components (section/row)
        NSUInteger indexes[] = {0, 3};
        NSIndexPath *twoLevel = [NSIndexPath indexPathWithIndexes:indexes length:2];
        
        // Test 3: Multiple components
        NSUInteger multiIndexes[] = {1, 2, 3, 4};
        NSIndexPath *multi = [NSIndexPath indexPathWithIndexes:multiIndexes length:4];
        
        // Test 4: Empty
        NSIndexPath *empty = [[NSIndexPath alloc] init];
        
        // Test 5: Nil
        NSIndexPath *nil_path = nil;
        
        NSLog(@"Breakpoint here");
        
        // Expected outputs:
        // single = [5]
        // twoLevel = [0, 3]
        // multi = [1, 2, 3, 4]
        // empty = []
        // nil_path = nil
        
        return 0;
    }
}
```

### Phase 2: Unit Test Implementation

#### Unit Test Structure
```cpp
// lldb/unittests/Language/ObjC/GNUstep/GNUstepFormatterUnitTests.cpp

#include "gtest/gtest.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepFormattersBase.h"
#include "lldb/Core/ValueObject.h"
#include "lldb/Target/Process.h"

class GNUstepFormatterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup mock process and memory
        m_process = CreateMockProcess();
        m_memory = std::make_unique<MockMemory>();
    }
    
    void TearDown() override {
        // Cleanup
    }
    
    MockProcess *m_process;
    std::unique_ptr<MockMemory> m_memory;
};

// Test memory access patterns
TEST_F(GNUstepFormatterTest, SafeMemoryRead) {
    // Test valid memory read
    uint8_t buffer[256];
    Status error = SafeReadMemory(0x1000, buffer, 256);
    EXPECT_TRUE(error.Success());
    
    // Test invalid address
    error = SafeReadMemory(0x0, buffer, 256);
    EXPECT_FALSE(error.Success());
    
    // Test bounds checking
    error = SafeReadMemory(0xFFFFFFFF, buffer, 256);
    EXPECT_FALSE(error.Success());
}

// Test string extraction
TEST_F(GNUstepFormatterTest, ExtractNSString) {
    // Setup mock NSString in memory
    m_memory->SetupNSString(0x1000, "Test String");
    
    std::string result;
    bool success = ExtractNSStringContents(0x1000, result);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(result, "Test String");
}

// Test buffer overflow prevention
TEST_F(GNUstepFormatterTest, PreventBufferOverflow) {
    // Create string longer than buffer
    std::string long_string(10000, 'A');
    m_memory->SetupNSString(0x1000, long_string);
    
    std::string result;
    bool success = ExtractNSStringContents(0x1000, result);
    
    EXPECT_TRUE(success);
    EXPECT_LE(result.length(), 1024); // Should be truncated
}

// Test ISA resolution
TEST_F(GNUstepFormatterTest, ResolveISA) {
    // Setup mock class structure
    m_memory->SetupClass(0x2000, "NSString");
    m_memory->SetupObject(0x3000, 0x2000); // Object with ISA
    
    ConstString class_name = GetClassNameFromISA(0x2000);
    EXPECT_EQ(class_name.GetStringRef(), "NSString");
}

// Test tagged pointer handling
TEST_F(GNUstepFormatterTest, TaggedPointers) {
    // Test NSNumber tagged pointer
    uint64_t tagged_number = MakeTaggedNumber(42);
    EXPECT_TRUE(IsTaggedPointer(tagged_number));
    
    int64_t value = ExtractTaggedNumberValue(tagged_number);
    EXPECT_EQ(value, 42);
    
    // Test NSString tagged pointer
    uint64_t tagged_string = MakeTaggedString("Hi");
    EXPECT_TRUE(IsTaggedPointer(tagged_string));
    
    std::string str = ExtractTaggedStringValue(tagged_string);
    EXPECT_EQ(str, "Hi");
}

// Test performance
TEST_F(GNUstepFormatterTest, PerformanceUnder50ms) {
    // Setup complex object
    m_memory->SetupLargeDictionary(0x4000, 1000); // 1000 key-value pairs
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string summary = GetDictionarySummary(0x4000);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 50);
}
```

### Phase 3: Integration Tests

#### LLDB Script Tests
```python
# lldb/test/API/lang/objc/gnustep/TestGNUstepFormatters.py

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *

class TestGNUstepFormatters(TestBase):
    
    @no_debug_info_test
    def test_string_formatter(self):
        """Test NSString formatter output"""
        self.build()
        self.runCmd("file test_strings")
        self.runCmd("b main")
        self.runCmd("run")
        
        # Test constant string
        self.expect("po constant_str",
                   substrs=['@"Hello World"'])
        
        # Test mutable string
        self.expect("po mutable_str",
                   substrs=['@"Mutable String"'])
        
        # Test nil
        self.expect("po nil_str",
                   substrs=['nil'])
    
    def test_array_formatter(self):
        """Test NSArray formatter output"""
        self.build()
        self.runCmd("file test_arrays")
        self.runCmd("b main")
        self.runCmd("run")
        
        # Test array display
        self.expect("po fruits",
                   substrs=['3 objects', 'Apple', 'Banana', 'Cherry'])
        
        # Test empty array
        self.expect("po empty_array",
                   substrs=['0 objects'])
    
    def test_dictionary_formatter(self):
        """Test NSDictionary formatter output"""
        self.build()
        self.runCmd("file test_dictionaries")
        self.runCmd("b main")
        self.runCmd("run")
        
        # Test dictionary display
        self.expect("po person",
                   substrs=['2 key/value pairs', 'name', 'John', 'age', '42'])
        
        # Verify format is key = value, not [0].key
        self.expect("po person",
                   patterns=['name = @"John"'],
                   matching=True)
    
    def test_custom_class_formatter(self):
        """Test custom class introspection"""
        self.build()
        self.runCmd("file test_custom_class")
        self.runCmd("b main")
        self.runCmd("run")
        
        # Test custom class display
        self.expect("po account",
                   substrs=['BankAccount', 'accountNumber', '12345', 
                           'owner', 'John Doe'])
        
        # Verify ISA resolution
        self.expect("expr -d run -- account",
                   substrs=['BankAccount *'])
    
    def test_performance(self):
        """Test formatter performance with large collections"""
        self.build()
        self.runCmd("file test_performance")
        self.runCmd("b main")
        self.runCmd("run")
        
        import time
        
        # Test large array (10000 elements)
        start = time.time()
        self.runCmd("po large_array")
        duration = time.time() - start
        
        self.assertLess(duration, 0.05, "Formatter took too long")
```

#### Interactive Test Scripts
```bash
# test_formatters.lldb
# Run with: lldb -s test_formatters.lldb test_program

# Setup
file test_program
b main
run

# Test all formatters
po string_obj
po number_obj
po array_obj
po dict_obj
po set_obj
po date_obj
po url_obj
po error_obj
po data_obj
po uuid_obj
po null_obj
po exception_obj
po attributed_str

# Test custom classes
po custom_obj
expr -d run -- custom_obj

# Test performance
script import time
script start = time.time()
po large_collection
script print(f"Duration: {time.time() - start:.3f}s")

# Verify memory usage
statistics dump

quit
```

### Phase 4: Regression Test Suite

#### Automated Regression Tests
```bash
#!/bin/bash
# run_regression_tests.sh

# Build test programs
cd /home/robk/code/llvm-project/lldb/examples
make clean
make all

# Run formatter tests
for test in test_*; do
    echo "Testing $test..."
    lldb -b -s test_${test}.lldb $test > ${test}.out 2>&1
    
    # Check for crashes
    if grep -q "CRASHED" ${test}.out; then
        echo "FAIL: $test crashed"
        exit 1
    fi
    
    # Check for errors
    if grep -q "error:" ${test}.out; then
        echo "FAIL: $test had errors"
        exit 1
    fi
    
    # Verify expected output
    if ! grep -q "PASS" ${test}.out; then
        echo "FAIL: $test missing expected output"
        exit 1
    fi
    
    echo "PASS: $test"
done

echo "All regression tests passed"
```

#### Performance Benchmarks
```cpp
// benchmark_formatters.cpp
#include <benchmark/benchmark.h>

static void BM_StringFormatter(benchmark::State& state) {
    // Setup
    NSString *str = @"Benchmark String";
    
    for (auto _ : state) {
        std::string summary = GetStringSummary(str);
        benchmark::DoNotOptimize(summary);
    }
}
BENCHMARK(BM_StringFormatter);

static void BM_ArrayFormatter(benchmark::State& state) {
    // Setup array with N elements
    size_t n = state.range(0);
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:n];
    for (size_t i = 0; i < n; i++) {
        [array addObject:@(i)];
    }
    
    for (auto _ : state) {
        std::string summary = GetArraySummary(array);
        benchmark::DoNotOptimize(summary);
    }
}
BENCHMARK(BM_ArrayFormatter)->Range(1, 10000);

static void BM_DictionaryFormatter(benchmark::State& state) {
    // Setup dictionary with N pairs
    size_t n = state.range(0);
    NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity:n];
    for (size_t i = 0; i < n; i++) {
        dict[@(i)] = @(i * 2);
    }
    
    for (auto _ : state) {
        std::string summary = GetDictionarySummary(dict);
        benchmark::DoNotOptimize(summary);
    }
}
BENCHMARK(BM_DictionaryFormatter)->Range(1, 1000);

BENCHMARK_MAIN();
```

## Test Validation Criteria

### Formatter Test Checklist
For each formatter, verify:
- [ ] Nil object returns "nil"
- [ ] Empty object shows appropriate empty state
- [ ] Simple object displays correctly
- [ ] Complex object shows all relevant fields
- [ ] Edge cases handled gracefully
- [ ] No crashes or hangs
- [ ] Performance < 50ms
- [ ] Memory leaks checked with valgrind
- [ ] Thread safety verified

### Integration Test Checklist
- [ ] All formatters work together
- [ ] Nested objects display correctly
- [ ] Recursive structures don't cause infinite loops
- [ ] Large collections handled efficiently
- [ ] Cross-formatter references work
- [ ] Custom classes show properties
- [ ] ISA resolution works for all types

### Performance Test Checklist
- [ ] String formatter < 10ms
- [ ] Number formatter < 5ms
- [ ] Array formatter < 20ms for 100 elements
- [ ] Dictionary formatter < 30ms for 100 pairs
- [ ] Set formatter < 20ms for 100 objects
- [ ] Custom class formatter < 15ms
- [ ] Overall response time < 50ms

## Test Output Validation

### Expected Formatter Outputs

#### Strings
```
nil_string = nil
empty_string = @""
simple_string = @"Hello World"
unicode_string = @"Hello 世界 🌍"
long_string = @"This is a very long string that sho..."
```

#### Numbers
```
nil_number = nil
zero = 0
integer = 42
float_num = 3.14159
bool_yes = YES
bool_no = NO
```

#### Arrays
```
nil_array = nil
empty_array = @"0 objects"
simple_array = @"3 objects" {
  [0] = @"Apple"
  [1] = @"Banana"
  [2] = @"Cherry"
}
```

#### Dictionaries
```
nil_dict = nil
empty_dict = @"0 key/value pairs"
simple_dict = @"2 key/value pairs" {
  name = @"John"
  age = 42
}
```

#### Custom Classes
```
nil_obj = nil
simple_obj = <BankAccount: 0x1234> {
  accountNumber = 12345
  owner = @"John Doe"
  balance = 1000.00
}
```

## MCP Testing Integration

### Using MCP LLDB Tools for Testing
```python
# test_with_mcp.py
import mcp_lldb

# Start LLDB session
session = mcp_lldb.start_session()

# Load test program
session.load_program("test_formatters")

# Set breakpoint and run
session.set_breakpoint("main")
session.run()

# Test formatter outputs
result = session.print_object("string_obj")
assert "@\"Hello World\"" in result

result = session.print_object("array_obj")
assert "3 objects" in result

# Performance test
import time
start = time.time()
session.print_object("large_collection")
duration = time.time() - start
assert duration < 0.05  # 50ms limit

# Cleanup
session.terminate()
```

## Continuous Integration

### GitHub Actions Workflow
```yaml
# .github/workflows/test-gnustep-lldb.yml
name: Test GNUstep LLDB Plugin

on:
  push:
    paths:
      - 'lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/**'
  pull_request:
    paths:
      - 'lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/**'

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Build LLDB
      run: |
        mkdir build
        cd build
        cmake -G Ninja ../llvm \
          -DLLVM_ENABLE_PROJECTS="clang;lldb" \
          -DCMAKE_BUILD_TYPE=Debug
        ninja lldb
    
    - name: Build Test Programs
      run: |
        cd lldb/examples
        make all
    
    - name: Run Unit Tests
      run: |
        cd build
        ninja check-lldb-unit-gnustep
    
    - name: Run Integration Tests
      run: |
        cd lldb/test
        python dotest.py -p TestGNUstep
    
    - name: Run Regression Tests
      run: |
        cd lldb/examples
        ./run_regression_tests.sh
    
    - name: Performance Benchmarks
      run: |
        cd build
        ./benchmark_formatters --benchmark_format=json > benchmarks.json
        python check_benchmarks.py benchmarks.json
```

## Success Metrics

Testing is considered complete when:
1. 100% of implemented formatters have test programs
2. All unit tests pass (100% pass rate)
3. All integration tests pass
4. No regressions in nightly builds
5. Performance benchmarks meet targets
6. Zero crashes in 1000 test runs
7. Memory leak free (valgrind clean)
8. Thread sanitizer clean
9. Coverage > 80% for formatter code
10. All edge cases documented and tested