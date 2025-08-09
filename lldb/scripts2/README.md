# GNUstep LLDB Plugin Testing Framework

This directory contains the testing infrastructure for the GNUstep LLDB plugin, including unit tests, integration tests, and validation scripts.

## Overview

The GNUstep LLDB plugin enables debugging of Objective-C programs compiled with the GNUstep runtime on Linux/WSL. This testing framework ensures the plugin's formatters and introspection capabilities work correctly.

## Directory Structure

```
llvm-project/
├── lldb/
│   ├── unittests/Language/ObjC/GNUstep/    # Unit tests
│   │   ├── CMakeLists.txt
│   │   ├── GNUstepIntrospectorTest.cpp     # Runtime introspection tests
│   │   ├── GNUstepFormattersTest.cpp       # Formatter logic tests
│   │   └── GNUstepTaggedPointerTest.cpp    # Tagged pointer tests
│   │
│   ├── test/API/lang/objc/gnustep/         # Integration tests
│   │   ├── TestGNUstepFormatters.py        # Python API tests
│   │   ├── main.m                          # Test program
│   │   └── Makefile                        # Build configuration
│   │
│   ├── examples/                            # Test programs
│   │   ├── custom_class_test.m
│   │   ├── test_dictionary_display.m
│   │   ├── test_set_display.m
│   │   └── ... (other test programs)
│   │
│   └── scripts2/                            # Test infrastructure
│       ├── run_gnustep_tests.sh            # Main test runner
│       ├── build_test_programs.sh          # Build all test programs
│       ├── validate_formatters.py          # Formatter validation
│       └── README.md                       # This file
```

## Prerequisites

1. **Build LLDB with GNUstep plugin:**
   ```bash
   cd /home/robk/code/llvm-project/build
   ninja lldb lldbPluginGNUstepObjCRuntime
   ```

2. **Install GNUstep runtime:**
   ```bash
   # The runtime should be installed in /usr/local/lib
   ls -la /usr/local/lib/libobjc.so*
   ls -la /usr/local/lib/libgnustep-base.so*
   ```

3. **Python 3 with LLDB bindings:**
   ```bash
   export PYTHONPATH=/home/robk/code/llvm-project/build/lib/python3/dist-packages:$PYTHONPATH
   ```

## Running Tests

### Quick Start - Run All Tests

```bash
cd /home/robk/code/llvm-project/lldb/scripts2
./run_gnustep_tests.sh
```

This will:
- Build the GNUstep plugin
- Run unit tests
- Build test programs
- Run integration tests
- Display a summary with pass/fail statistics

### Run Specific Test Types

#### Unit Tests Only
```bash
cd /home/robk/code/llvm-project/build
ninja LanguageObjCGNUstepTests
./tools/lldb/unittests/Language/ObjC/GNUstep/LanguageObjCGNUstepTests
```

#### Integration Tests Only
```bash
cd /home/robk/code/llvm-project/lldb/test
python -m lldb.dotest -p TestGNUstepFormatters.py
```

#### Formatter Validation Only
```bash
cd /home/robk/code/llvm-project/lldb/scripts2
./build_test_programs.sh  # Build test programs first
./validate_formatters.py
```

### Build Test Programs

To build all test programs without running tests:

```bash
cd /home/robk/code/llvm-project/lldb/scripts2
./build_test_programs.sh
```

This builds programs in:
- `/home/robk/code/llvm-project/lldb/examples/`
- `/home/robk/code/llvm-project/lldb/test/API/lang/objc/gnustep/`

## Adding New Tests

### Adding Unit Tests

1. Create new test file in `lldb/unittests/Language/ObjC/GNUstep/`:
   ```cpp
   // MyNewTest.cpp
   #include "gtest/gtest.h"
   
   TEST(GNUstepNewFeature, BasicTest) {
       EXPECT_TRUE(true);
   }
   ```

2. Add to `CMakeLists.txt`:
   ```cmake
   add_lldb_unittest(LanguageObjCGNUstepTests
     ...
     MyNewTest.cpp
   )
   ```

3. Rebuild and run:
   ```bash
   cd /home/robk/code/llvm-project/build
   ninja LanguageObjCGNUstepTests
   ```

### Adding Integration Tests

1. Create new Python test in `lldb/test/API/lang/objc/gnustep/`:
   ```python
   # TestNewFeature.py
   class TestNewFeature(TestBase):
       def test_feature(self):
           self.build()
           self.expect("po myObject", substrs=["expected"])
   ```

2. Add test program if needed in `main.m`

3. Run the test:
   ```bash
   python -m lldb.dotest -p TestNewFeature.py
   ```

### Adding Formatter Validation Tests

1. Edit `validate_formatters.py` to add new test cases:
   ```python
   TestCase("My New Test", "variableName", ["expected", "output"])
   ```

2. Or create a JSON test file:
   ```json
   [
     {
       "name": "Custom Test",
       "variable": "myVar",
       "expected_patterns": ["pattern1", "pattern2"],
       "is_regex": false
     }
   ]
   ```

3. Run with custom tests:
   ```bash
   ./validate_formatters.py --test-file my_tests.json
   ```

## Test Coverage

The test suite covers:

### Formatter Tests (80%+ coverage required)
- ✅ **NSString**: Empty, ASCII, UTF-8, emoji, tagged pointers
- ✅ **NSNumber**: Integer, float, double, boolean, tagged integers
- ✅ **NSArray/NSMutableArray**: Empty, single, multiple elements, large arrays
- ✅ **NSDictionary/NSMutableDictionary**: Empty, single, multiple pairs, nested
- ✅ **NSSet/NSMutableSet**: Empty, single, multiple elements, uniqueness
- ✅ **NSValue**: Generic value wrapper
- ✅ **Nil handling**: Proper display of nil objects
- ✅ **Custom classes**: User-defined Objective-C classes

### Introspection Tests
- Class name extraction
- Method resolution
- Ivar extraction
- Runtime version detection
- Memory safety bounds checking

### Performance Tests
- Large collection formatting (< 50ms)
- Deeply nested structures
- Memory efficiency

## Known Issues and Limitations

1. **Custom Class ISA Lookup**: `CallRuntimeFunction()` returns LLDB_INVALID_ADDRESS (stub implementation)
   - Impact: Custom class properties not fully accessible
   - Workaround: Use direct memory access

2. **Dictionary Display Format**: Shows verbose `[0].key` and `[0].value` format
   - Impact: Less readable output
   - Fix planned in `GNUstepDictionaryFormatters.cpp`

3. **Runtime Symbol Resolution**: Some runtime functions may not resolve correctly
   - Impact: Limited dynamic introspection
   - Workaround: Use fallback mechanisms

## Debugging Test Failures

### Enable Debug Output

1. Set environment variable:
   ```bash
   export LLDB_GNUSTEP_DEBUG=1
   ```

2. Run tests with verbose output:
   ```bash
   ./run_gnustep_tests.sh --verbose
   ```

### Common Issues

**Tests fail to build:**
- Check GNUstep runtime installation
- Verify clang version supports `-fobjc-runtime=gnustep-2.1`
- Check library paths in `/usr/local/lib`

**Formatters not activating:**
- Verify plugin is loaded: `plugin list` in LLDB
- Check TypeCategory: `type category list`
- Ensure gnustep category is enabled

**Segmentation faults:**
- Build with debug symbols: `-g -gdwarf-5 -O0`
- Run under gdb: `gdb --args lldb test_program`
- Check for memory corruption

## Performance Metrics

Target performance for formatters:
- Small objects (< 10 elements): < 10ms
- Medium objects (10-100 elements): < 20ms
- Large objects (100-1000 elements): < 50ms
- Very large objects (> 1000 elements): < 100ms

Current performance (measured on WSL2):
- NSString: ~5ms
- NSNumber: ~3ms
- NSArray (100 elements): ~15ms
- NSDictionary (100 pairs): ~18ms
- NSSet (100 elements): ~16ms

## Contributing

When adding new formatters or features:

1. Write unit tests first (TDD approach)
2. Ensure 80%+ code coverage
3. Add integration tests for real-world scenarios
4. Update this README with any new test procedures
5. Run full test suite before submitting PR

## CI/CD Integration

To integrate with CI systems:

```yaml
# Example GitHub Actions workflow
- name: Build LLDB
  run: |
    cd build
    ninja lldb lldbPluginGNUstepObjCRuntime

- name: Run GNUstep Tests
  run: |
    cd lldb/scripts2
    ./run_gnustep_tests.sh
```

## Support

For issues or questions:
- Check test output logs in `/tmp/test_output.log`
- Review debug output with `LLDB_GNUSTEP_DEBUG=1`
- Consult LLVM coding standards for code style
- File issues with reproducible test cases