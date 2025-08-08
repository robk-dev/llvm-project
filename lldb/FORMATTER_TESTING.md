# GNUstep LLDB Formatter Testing Framework

This directory contains an automated test framework for validating GNUstep LLDB formatters.

## Quick Start

Run all formatter tests:
```bash
./run_formatter_tests.sh
```

Run specific formatter tests:
```bash
./run_formatter_tests.sh NSString
./run_formatter_tests.sh NSNumber
./run_formatter_tests.sh NSArray
```

Run with verbose output:
```bash
./run_formatter_tests.sh -v
```

## Test Framework Components

### 1. Test Runner Script (`test_formatters.py`)
The main Python test framework that:
- Builds test programs using the Makefile
- Runs LLDB with each test program
- Captures formatter output
- Validates output against expected values
- Reports pass/fail results with colored output

### 2. Shell Wrapper (`run_formatter_tests.sh`)
Convenience script that:
- Checks prerequisites (Python 3, LLDB, lldb-server)
- Sets up environment variables
- Provides user-friendly interface
- Shows colored status messages

### 3. Test Configuration (`test_formatter_config.json`)
JSON configuration defining:
- Expected output for each formatter
- Test cases with variables and expected values
- Regular expression patterns for flexible matching
- Test settings and behaviors

### 4. Test Programs
Comprehensive test programs for each formatter:

#### NSString Tests
- `test_nsstring_formatter.m` - Basic NSString formatter tests
  - Constant strings, mutable strings
  - UTF-8 and Unicode strings
  - Special characters and escape sequences
  - Format strings and substrings
  - nil handling

#### NSNumber Tests
- `test_nsnumber_formatter.m` - Basic NSNumber formatter tests
- `test_nsnumber_comprehensive.m` - Comprehensive NSNumber tests
  - All integer types (char, short, int, long, long long)
  - All unsigned types
  - Float and double values
  - Special values (NaN, Infinity)
  - Boolean values
  - Literal syntax

#### NSArray Tests
- `simple_array_test.m` - Simple NSArray tests
- `test_nsarray_comprehensive.m` - Comprehensive NSArray tests
  - Empty arrays
  - Single element arrays
  - String, number, and mixed type arrays
  - Nested arrays
  - Mutable arrays
  - Large arrays (1000+ elements)
  - Arrays with custom objects

## Writing New Tests

### 1. Create Test Program
Create a new `.m` file in `/home/robk/code/llvm-project/lldb/examples/`:
```objc
#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Create test objects
        NSYourType *testObj = ...;
        
        // Set breakpoint line
        printf("Break here\n");  // Document line number
        
        return 0;
    }
}
```

### 2. Add to Makefile
Add your test program to the `SOURCES` list in the Makefile.

### 3. Add Test Definition
In `test_formatters.py`, add a new `FormatterTest` to `get_all_tests()`:
```python
tests.append(FormatterTest(
    name="NSYourType Formatter",
    source_file="test_nsyourtype.m",
    executable="test_nsyourtype",
    breakpoint_line=50,  # Line with printf
    test_cases=[
        TestCase(
            name="Basic Test",
            variable="testObj",
            expected_output="expected output",
            description="Test description"
        ),
        # More test cases...
    ]
))
```

### 4. Run Your Test
```bash
./run_formatter_tests.sh NSYourType
```

## Test Output Format

### Success Output
```
✓ NSString Formatter: All 10 tests passed
```

### Failure Output
```
✗ NSNumber Formatter: 8 passed, 2 failed, 0 errors
  ✗ Boolean YES: Expected: YES
                  Actual: 1
  ✗ Float Number: Expected pattern: 3\.14\d*
                  Actual: 3.141590
```

## Advanced Usage

### JSON Output
Save test results to JSON for CI/CD integration:
```bash
./run_formatter_tests.sh --json results.json
```

### Custom Build Directory
Use a different LLVM build:
```bash
./run_formatter_tests.sh --build-dir /path/to/build --examples-dir /path/to/examples
```

### Python API
Use the test framework programmatically:
```python
from test_formatters import FormatterTestFramework

framework = FormatterTestFramework(
    build_dir="/path/to/build",
    examples_dir="/path/to/examples",
    verbose=True
)
framework.run_all_tests()
framework.print_summary()
```

## Troubleshooting

### Build Failures
If tests fail to build:
1. Check GNUstep is installed: `gnustep-config --base-libs`
2. Verify clang path in Makefile
3. Check library paths are correct

### LLDB Server Issues
If you see "unable to locate lldb-server":
```bash
cd /home/robk/code/llvm-project/build
ninja lldb-server
```

### Formatter Not Loading
Check plugin is loaded:
1. Run LLDB manually
2. Check for "GNUstepObjCRuntime" in debug output
3. Verify `-fobjc-runtime=gnustep-2.1` is used

## Test Coverage Goals

### Phase 1 (Current)
- [x] NSString basic formatting
- [x] NSNumber basic formatting  
- [x] NSArray basic formatting

### Phase 2 (In Progress)
- [ ] NSString edge cases and performance
- [ ] NSNumber special values
- [ ] NSArray large collections

### Phase 3 (Planned)
- [ ] NSDictionary formatter
- [ ] NSSet formatter
- [ ] NSDate formatter
- [ ] Custom object formatters

## Contributing

When adding new formatters:
1. Create comprehensive test cases
2. Test edge cases (nil, empty, large data)
3. Ensure tests pass before committing
4. Update this documentation

## CI Integration

The test framework is designed for CI/CD:
- Exit code 0 on success, 1 on failure
- JSON output for test reporting
- Configurable timeouts
- No interactive prompts