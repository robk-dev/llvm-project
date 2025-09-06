# GNUstep LLDB Integration Tests

This directory contains comprehensive integration tests for the GNUstep LLDB plugin. The tests validate that the debugger commands work correctly with GNUstep objects and the plugin provides proper formatting and inspection capabilities.

## Test Structure

### Core Test Files

1. **`TestGNUstepCore.py`** - Main comprehensive test that validates all functionality
   - Basic object formatting (NSString, NSNumber, NSDate)
   - Collection formatting (NSArray, NSDictionary, NSSet)
   - Custom class inspection
   - Expression evaluation with literals
   - Performance tests with large collections
   - Error handling and edge cases

2. **`TestGNUstepFoundationTypes.py`** - Foundation-specific types testing
   - NSDate, NSURL, NSUUID, NSError formatters
   - NSDecimalNumber, NSIndexSet, NSCharacterSet
   - NSNull and NSException handling
   - Property access and method calls on Foundation objects

3. **`TestGNUstepExpressions.py`** - Expression evaluation testing
   - Literal syntax (@"string", @42, @[], @{})
   - Method chaining and complex expressions
   - Variable modification through expressions
   - Class method calls
   - Type casting and error handling

### Test Program

- **`main.m`** - Comprehensive test program that creates all necessary test objects
  - Includes all Foundation types needed for testing
  - Creates BankAccount custom class instances
  - Sets up nested collections and large collections for performance testing
  - Provides breakpoint location for all tests

### Build System

- **`Makefile`** - Build configuration with proper GNUstep compiler flags
- **`Makefile.rules`** - Standard build rules for LLDB tests

## Test Categories

### Basic Object Formatting Tests
- NSString (empty, ASCII, Unicode, tagged strings)
- NSNumber (integers, floats, booleans, tagged pointers)
- NSDate (current time, epoch, future dates)

### Collection Formatting Tests
- NSArray/NSMutableArray (empty, single element, multiple elements, nested)
- NSDictionary/NSMutableDictionary (empty, key-value pairs, complex values)
- NSSet/NSMutableSet (empty, unique elements, duplicates)
- NSIndexSet/NSMutableIndexSet (empty, single, ranges, scattered)

### Foundation Types Tests
- NSNull (singleton)
- NSException (with name, reason, userInfo)
- NSError (domain, code, userInfo)
- NSURL (web, file, complex URLs)
- NSUUID (random, specific)
- NSDecimalNumber (various precisions, NaN, arithmetic results)
- NSCharacterSet (predefined, custom ranges)

### Custom Classes Tests
- BankAccount class from the test program
- Property access (accountNumber, owner, balance, transactions)
- Method calls (deposit, withdraw)
- Description formatting

### Expression Evaluation Tests
- Literal creation (@"test", @42, @[], @{})
- Arithmetic expressions (@(1+1), @(6*7))
- Method chaining ([obj method1] method2])
- Property access (obj.property)
- Complex nested access (dict[@"key"][@"subkey"])

## Expected Output Patterns

### String Formatters
- Empty string: `@""`
- Regular strings: Show actual content
- Unicode strings: Properly display Unicode characters
- Nil strings: `nil`

### Number Formatters
- Integers: Show numeric value (e.g., `42`)
- Floats: Show decimal value (e.g., `3.14`)
- Booleans: Show `YES` or `NO`
- Nil numbers: `nil`

### Array Formatters
- Empty arrays: `@[]`
- Non-empty arrays: `@[element1, element2, ...]`
- Synthetic children: `array[0]` returns first element
- Large arrays: Complete within 5 second timeout

### Dictionary Formatters
- Empty dictionaries: `@{}`
- Non-empty dictionaries: `@{key1 = value1; key2 = value2; ...}`
- Synthetic children: `dict[@"key"]` returns value
- Large dictionaries: Complete within 5 second timeout

### Set Formatters
- Empty sets: `{}`
- Non-empty sets: `{element1, element2, ...}` (order may vary)
- Nil sets: `nil`

### Foundation Type Formatters
- NSDate: ISO-style date format or readable date string
- NSURL: Full URL string
- NSUUID: UUID format (XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX)
- NSError: Domain and error code
- NSDecimalNumber: Decimal value representation
- NSIndexSet: `<NSIndexSet: 0x... [index1 index2 ...]>`

### Custom Class Formatters
- BankAccount: Uses `-description` method result
- Shows account number, owner, balance, transaction count

## Running the Tests

### Individual Test Files
```bash
# Run core functionality tests
python3 -m lldbsuite.test.dotest TestGNUstepCore.py

# Run Foundation types tests
python3 -m lldbsuite.test.dotest TestGNUstepFoundationTypes.py

# Run expression evaluation tests
python3 -m lldbsuite.test.dotest TestGNUstepExpressions.py
```

### All Tests
```bash
# Run all GNUstep tests
python3 -m lldbsuite.test.dotest .
```

### With Verbose Output
```bash
# Run with detailed output for debugging
python3 -m lldbsuite.test.dotest -v TestGNUstepCore.py
```

## Test Environment Requirements

### System Requirements
- Linux platform (WSL supported)
- GNUstep Base Library installed
- libobjc2 runtime installed
- LLDB built with GNUstep plugin support

### GNUstep Installation
```bash
# Install GNUstep dependencies
sudo apt-get install gnustep-devel gnustep-base-runtime libobjc-gnustep-dev

# Or build from source for latest version
```

### LLDB Requirements
- LLDB built with the GNUstepObjCRuntime plugin
- Plugin properly loaded and active during debugging sessions
- GNUstep type category enabled for formatters

## Expected Test Results

### Success Criteria
- ✅ **All tests pass**: Plugin is production-ready
- ✅ **Formatters display correctly**: Objects show expected content
- ✅ **Synthetic children work**: Array/dictionary access succeeds
- ✅ **Expression evaluation works**: po, expr, frame variable commands succeed
- ✅ **Performance acceptable**: Large collections complete within timeout
- ✅ **Error handling robust**: Invalid operations fail gracefully

### Common Issues
- ⚠️ **Plugin not loading**: Check GNUstep libraries are linked
- ⚠️ **Formatters not working**: Verify TypeCategory activation
- ⚠️ **Expressions failing**: Check runtime detection and symbol availability
- ⚠️ **Performance issues**: Verify formatter optimization

## Integration with LLVM Test Suite

These tests are designed to integrate with the standard LLVM test framework:
- Use standard `lldbsuite.test` framework
- Follow LLDB test conventions
- Include platform-specific decorators
- Provide meaningful test names and documentation
- Handle expected failures gracefully

## Debugging Test Failures

### Enable Debug Output
Add debug output to formatters and inspect LLDB logs:
```bash
# Set LLDB logging
(lldb) log enable lldb types
(lldb) log enable lldb formatters
```

### Manual Testing
Use the test program manually for interactive debugging:
```bash
# Build test program
make

# Debug with LLDB
lldb ./a.out
(lldb) b main.m:200  # Set breakpoint after object creation
(lldb) run
(lldb) po stringObject  # Test individual formatters
```

### Check Plugin Status
Verify plugin is loaded and active:
```bash
(lldb) plugin list
(lldb) type category list
(lldb) type summary list
```
