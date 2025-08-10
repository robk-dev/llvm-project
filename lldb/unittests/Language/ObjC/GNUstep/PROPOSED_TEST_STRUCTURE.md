# Proposed Hierarchical Test File Structure

## Current Structure (Monolithic)
```
GNUstepFormattersTest.cpp (86,786 bytes - too large!)
GNUstepIntrospectorTest.cpp
GNUstepTaggedPointerTest.cpp
```

## Proposed Structure (Modular)

### Directory Layout
```
/home/robk/code/llvm-project/lldb/unittests/Language/ObjC/GNUstep/
├── CMakeLists.txt
├── README.md
│
├── Core/
│   ├── GNUstepRuntimeTest.cpp         # Runtime detection and initialization
│   ├── GNUstepIntrospectorTest.cpp    # Memory introspection
│   └── GNUstepTaggedPointerTest.cpp   # Tagged pointer handling
│
├── Formatters/
│   ├── Collections/
│   │   ├── NSArrayFormatterTest.cpp
│   │   ├── NSDictionaryFormatterTest.cpp
│   │   ├── NSSetFormatterTest.cpp
│   │   └── NSIndexSetFormatterTest.cpp
│   │
│   ├── Primitives/
│   │   ├── NSStringFormatterTest.cpp
│   │   ├── NSNumberFormatterTest.cpp
│   │   ├── NSValueFormatterTest.cpp
│   │   └── NSNullFormatterTest.cpp
│   │
│   ├── Foundation/
│   │   ├── NSDateFormatterTest.cpp
│   │   ├── NSURLFormatterTest.cpp
│   │   ├── NSUUIDFormatterTest.cpp
│   │   ├── NSDataFormatterTest.cpp
│   │   └── NSErrorFormatterTest.cpp
│   │
│   ├── Text/
│   │   ├── NSCharacterSetFormatterTest.cpp
│   │   ├── NSAttributedStringFormatterTest.cpp
│   │   └── NSDecimalNumberFormatterTest.cpp
│   │
│   └── Common/
│       └── FormatterTestHelpers.h     # Shared test utilities
│
├── Integration/
│   ├── CustomClassTest.cpp            # Custom class debugging
│   ├── PerformanceTest.cpp            # Performance benchmarks
│   └── EndToEndTest.cpp               # Full debugging scenarios
│
└── disabled_tests/                    # Tests awaiting API updates
    └── README.md
```

## Benefits

1. **Maintainability**: Each formatter gets its own focused test file
2. **Scalability**: Easy to add new formatter tests without growing monolithic files
3. **Organization**: Logical grouping by functionality
4. **Parallel Development**: Multiple developers can work on different test files
5. **Compilation Speed**: Smaller files compile faster
6. **Test Isolation**: Easier to run specific test subsets

## Implementation Plan

### Phase 1: Extract Existing Tests (Immediate)
1. Create directory structure
2. Split GNUstepFormattersTest.cpp into individual files
3. Update CMakeLists.txt to include all new files
4. Verify all tests still pass

### Phase 2: Enhance Test Coverage (This Week)
1. Add missing test cases to individual files
2. Create FormatterTestHelpers.h with common utilities
3. Add performance benchmarks
4. Document test patterns

### Phase 3: Integration Tests (Next Week)
1. Create end-to-end debugging scenarios
2. Add cross-formatter interaction tests
3. Implement regression test suite

## CMakeLists.txt Structure

```cmake
# Core tests
add_lldb_unittest(GNUstepCoreTests
  Core/GNUstepRuntimeTest.cpp
  Core/GNUstepIntrospectorTest.cpp
  Core/GNUstepTaggedPointerTest.cpp
)

# Formatter tests
add_lldb_unittest(GNUstepFormatterTests
  Formatters/Collections/NSArrayFormatterTest.cpp
  Formatters/Collections/NSDictionaryFormatterTest.cpp
  # ... etc
)

# Integration tests
add_lldb_unittest(GNUstepIntegrationTests
  Integration/CustomClassTest.cpp
  Integration/PerformanceTest.cpp
  Integration/EndToEndTest.cpp
)
```

## Test Naming Convention

Each test file should follow this pattern:
- `TEST(GNUstep<Type>Formatter, <TestCase>)`
- Example: `TEST(GNUstepNSArrayFormatter, EmptyArray)`
- Example: `TEST(GNUstepNSArrayFormatter, LargeArray)`

## Shared Test Utilities

`FormatterTestHelpers.h` should provide:
- Mock process creation
- Memory setup utilities
- Common validation functions
- Performance measurement helpers
- Test data generators

This structure will make the test suite more maintainable and scalable as we continue to add more formatters and functionality.