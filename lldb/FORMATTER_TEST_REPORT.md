# GNUstep Formatter Comprehensive Test Report
Date: 2025-08-10
LLDB Version: 20.1.8 (commit 8542c0cf7a0c)

## Executive Summary

Comprehensive testing of the GNUstep formatters reveals a mixed state of functionality. While the `po` command works correctly for all tested types, the `frame variable` command shows several issues with formatters displaying placeholder values instead of actual content.

## Test Environment

- **Test Program**: `/home/robk/code/llvm-project/lldb/examples/custom_class_test`
- **Breakpoint Location**: Line 594 (end of main, all variables initialized)
- **LLDB Binary**: `/home/robk/code/llvm-project/build/bin/lldb` (built Aug 10 21:55)
- **Plugin Library**: `liblldbPluginGNUstepObjCRuntime.a` (built Aug 10 21:45)

## Formatter Test Results

### 1. NSNumber Formatter ✅ WORKING
- **frame variable**: `42` - Correctly displays numeric value
- **po command**: `42` - Works correctly
- **Status**: Production-ready

### 2. NSArray Formatter ❌ BROKEN
- **frame variable**: Shows memory read errors:
  ```
  @[ <read memory from 0xc3c386cca000002c failed (0 of 8 bytes read)>, ...]
  ```
- **po command**: `(apple, banana, cherry, date)` - Works correctly
- **Issue**: Memory reading failure in synthetic children provider
- **Priority**: CRITICAL

### 3. NSDictionary Formatter ⚠️ PARTIALLY WORKING
- **frame variable**: Shows placeholder values:
  ```
  @{"age": 30, <key>: <value>, "skills": "(4156632232 elements)", "namerr": <value>, "name": "John Doe"}
  ```
- **po command**: Works correctly:
  ```
  {age = 30; name = "John Doe"; namerr = "John Doe2"; occupation = Developer; skills = ("Objective-C", Swift, Python); }
  ```
- **Issues**:
  - Shows `<key>` and `<value>` placeholders for some entries
  - Incorrect element count for nested collections ("4156632232 elements")
- **Priority**: HIGH

### 4. NSSet Formatter ❌ BROKEN
- **frame variable**: Shows generic placeholders:
  ```
  {<object>, <object>, <object>, <object>}
  ```
- **po command**: `(German, French, Spanish, English)` - Works correctly
- **Issue**: Synthetic children provider not resolving actual objects
- **Priority**: HIGH

### 5. NSLocale Formatter ⚠️ PARTIALLY WORKING
- **frame variable**: Shows incomplete information:
  ```
  NSLocale(id=<unknown>)
  ```
- **po command**: `en_US_POSIX` - Works correctly
- **Issue**: Summary provider not extracting locale identifier
- **Priority**: MEDIUM

### 6. BankAccount (Custom Class) ✅ WORKING
- **frame variable**: Correctly displays all properties:
  ```
  BankAccount(_accountNumber="ACC-001", _ownerName="John Doe", _balance=1100.00, _transactions=(4156632554 elements), _authorizedUsers={})
  ```
- **po command**: Works correctly:
  ```
  BankAccount(ACC-001, owner=John Doe, balance=1100.00, transactions=4)
  ```
- **Minor Issue**: Incorrect element count for _transactions array
- **Status**: Mostly working, needs minor fix for collection counting

## Key Findings

### Critical Issues
1. **NSArray**: Complete memory read failure preventing any element display
2. **NSSet**: Elements not being resolved, only showing placeholders
3. **NSDictionary**: Some entries show as `<key>: <value>` instead of actual content

### Pattern Analysis
- All `po` commands work correctly, indicating the runtime bridge is functional
- Issues are primarily in the synthetic children providers and summary providers
- Memory reading failures suggest potential issues with:
  - Tagged pointer handling in collections
  - Address calculation in synthetic providers
  - Type resolution for collection elements

### Build System Observations

**Plugin-Only Rebuild**: The plugin library (`liblldbPluginGNUstepObjCRuntime.a`) is statically linked into the LLDB binary. When you rebuild only the plugin:
1. The `.a` file is updated (Aug 10 21:45)
2. LLDB binary must be relinked (Aug 10 21:55)
3. **Result**: Plugin-only rebuild still requires LLDB relink, taking ~10 seconds

**Suggested Test Change**: To verify plugin changes are reflected:
1. Add a debug printf in a formatter (e.g., in `GNUstepArrayFormatters.cpp`)
2. Rebuild with `ninja lldbPluginGNUstepObjCRuntime lldb`
3. Test to see if the debug output appears

## Recommendations

### Immediate Actions Required
1. **Fix NSArray memory reading**: Debug `GetChildAtIndex` in `GNUstepArrayFormatters.cpp`
2. **Fix NSSet element resolution**: Update synthetic provider to properly resolve objects
3. **Fix NSDictionary placeholders**: Investigate why some keys/values show as placeholders

### Testing Improvements
1. Add unit tests for memory reading functions
2. Create integration tests for each formatter type
3. Add regression tests for the specific failure cases found

### Code Areas to Investigate
1. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepArrayFormatters.cpp`
   - Focus on `GetChildAtIndex` method
   - Check tagged pointer handling

2. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepSetFormatters.cpp`
   - Review synthetic children provider
   - Ensure proper object resolution

3. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDictionaryFormatters.cpp`
   - Fix child naming in `GetChildAtIndex`
   - Resolve placeholder display issues

## Success Metrics

To consider the formatters production-ready:
- [ ] All `frame variable` commands show actual data (no placeholders)
- [ ] Memory read errors are eliminated
- [ ] Collection element counts are accurate
- [ ] Performance remains under 50ms per formatter
- [ ] All existing unit tests pass
- [ ] New regression tests added and passing

## Conclusion

While the GNUstep runtime bridge and `po` command functionality are working well, the formatter synthetic providers need significant fixes. The issues are primarily in memory reading and child resolution, not in the core runtime integration. With focused fixes to the three critical formatters (NSArray, NSSet, NSDictionary), the plugin can reach production quality.