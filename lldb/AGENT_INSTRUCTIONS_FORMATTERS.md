# GNUstep LLDB Plugin - Formatter Implementation Instructions

## Overview
This document provides detailed instructions for implementing missing formatters and fixing existing formatter issues in the GNUstep LLDB plugin.

## Critical Bug Fixes (Priority 0 - MUST FIX FIRST)

### Bug 1: Array First Element Display Issue
**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepArrayFormatters.cpp`
**Line**: 467
**Issue**: First element shows `<NSConstantString>` instead of actual string value
**Root Cause**: String length read failure for index 0

**Fix Steps**:
1. Locate the `GetChildAtIndex()` method
2. Check memory offset calculation for NSConstantString at index 0
3. Verify the string extraction logic handles NSConstantString's specific layout
4. Test with: `@[@"First", @"Second", @"Third"]`

**Expected Output**:
```
(NSArray *) fruits = @"3 objects" {
  [0] = @"Apple"
  [1] = @"Banana"
  [2] = @"Cherry"
}
```

### Bug 2: Custom Class String Ivar Display
**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepGenericFormatter.cpp`
**Line**: 429
**Issue**: Shows `<invalid object>` for valid string ivars
**Root Cause**: Double indirection - treating direct pointer as pointer-to-pointer

**Fix Steps**:
1. Remove the incorrect `ReadPointer` call at line 429
2. Use the ivar value directly as the object address
3. Validate with custom class containing NSString properties

**Test Case**:
```objc
@interface BankAccount : NSObject
@property NSString *owner;
@end
// Should display: owner = @"John Doe"
```

### Bug 3: Dictionary Key Corruption
**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDictionaryFormatters.cpp`
**Line**: 1207
**Issue**: Shows "namerr" instead of "name"
**Root Cause**: Buffer overflow in string extraction

**Fix Steps**:
1. Add proper null termination after string extraction
2. Implement bounds checking for buffer operations
3. Ensure buffer size matches actual string length + 1

**Test Case**:
```objc
NSDictionary *dict = @{@"name": @"John", @"age": @42};
// Should display: { name = "John"; age = 42 }
```

### Bug 4: Dictionary Display Format
**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDictionaryFormatters.cpp`
**Lines**: 1047-1050
**Issue**: Verbose `[0].key` and `[0].value` instead of clean `key = value`

**Fix Steps**:
1. Modify `GetChildAtIndex()` method
2. Change child naming from `[%zu].key` to just the key name
3. Combine key-value pairs into single entries
4. Follow Apple's format: `key = value`

### Bug 5: Re-enable NSIndexPath Formatter
**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepFormattersRegistry.cpp`
**Line**: 342 (commented out)
**Issue**: Causes infinite recursion/crashes

**Debug Steps**:
1. Uncomment the registration line
2. Add recursion depth tracking in formatter
3. Implement cycle detection
4. Test with: `[NSIndexPath indexPathWithIndex:0]`

### Bug 6: Re-enable NSNotification Formatter
**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepFormattersRegistry.cpp`
**Line**: 343 (commented out)
**Issue**: Causes crashes

**Debug Steps**:
1. Add memory validation before accessing notification fields
2. Check for null pointers in name/object/userInfo
3. Implement graceful fallback for invalid memory
4. Test with: `[NSNotification notificationWithName:@"TestNote" object:nil]`

## New Formatter Implementation Instructions

### TDD Approach for All New Formatters

1. **Create Test Program First**
```objc
// test_[formatter_name].m
#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Test Case 1: Nil object
        NSFormatClass *nil_obj = nil;
        
        // Test Case 2: Empty/default object
        NSFormatClass *empty_obj = [[NSFormatClass alloc] init];
        
        // Test Case 3: Simple valid object
        NSFormatClass *simple_obj = [NSFormatClass simpleExample];
        
        // Test Case 4: Complex object
        NSFormatClass *complex_obj = [NSFormatClass complexExample];
        
        // Test Case 5: Edge cases
        NSFormatClass *edge_obj = [NSFormatClass edgeCase];
        
        NSLog(@"Breakpoint here for testing");
        return 0;
    }
}
```

2. **Define Expected Output**
```
// Document expected LLDB output for each test case
nil_obj = nil
empty_obj = <NSFormatClass: empty>
simple_obj = <NSFormatClass: simple representation>
complex_obj = <NSFormatClass: complex representation>
edge_obj = <NSFormatClass: edge case handled>
```

3. **Implement Formatter**
4. **Validate Against Expected Output**
5. **Performance Test** (must be < 50ms)

### Priority 1: NSIndexSet Formatter

**Requirements**:
- Display format: "3 indexes in [0-2]" for contiguous ranges
- Display format: "3 indexes" for non-contiguous
- Handle empty sets: "0 indexes"
- Handle large sets efficiently

**Implementation Guide**:
```cpp
class NSIndexSetSummaryProvider : public GNUstepSummaryProvider {
    // 1. Read count from offset 16
    // 2. Check if contiguous (single range)
    // 3. If contiguous, read start and end
    // 4. Format as "X indexes in [start-end]"
    // 5. If not contiguous, format as "X indexes"
};
```

**Test Cases**:
```objc
NSIndexSet *empty = [NSIndexSet indexSet];
NSIndexSet *single = [NSIndexSet indexSetWithIndex:5];
NSIndexSet *range = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, 10)];
NSMutableIndexSet *multiple = [NSMutableIndexSet indexSet];
[multiple addIndex:1];
[multiple addIndex:5];
[multiple addIndex:10];
```

### Priority 2: NSDecimalNumber Formatter

**Requirements**:
- Display decimal value with precision
- Handle locale-specific formatting
- Show "NaN" for not-a-number
- Handle overflow/underflow

**Implementation Guide**:
```cpp
class NSDecimalNumberSummaryProvider : public GNUstepSummaryProvider {
    // 1. Read _mantissa, _exponent, _isNegative, _isCompact
    // 2. Calculate decimal value
    // 3. Format with appropriate precision
    // 4. Handle special values (NaN, infinity)
};
```

### Priority 3: NSCharacterSet Formatter

**Requirements**:
- Detect and display predefined sets: "[NSCharacterSet alphanumericCharacterSet]"
- Show character count for custom sets: "45 characters"
- Handle inverted sets: "[inverted: 45 characters]"

**Implementation Guide**:
```cpp
class NSCharacterSetSummaryProvider : public GNUstepSummaryProvider {
    // 1. Check if predefined set (compare pointers)
    // 2. If custom, count characters in bitmap
    // 3. Check inverted flag
    // 4. Format appropriately
};
```

### Priority 4: System Class Formatters

#### NSTimeZone
- Format: "America/New_York (GMT-5)"
- Extract: name, secondsFromGMT, isDaylightSavingTime

#### NSLocale
- Format: "en_US (English, United States)"
- Extract: localeIdentifier, languageCode, countryCode

#### NSCalendar
- Format: "Gregorian calendar (en_US)"
- Extract: calendarIdentifier, locale, timeZone

#### NSBundle
- Format: "com.example.app at /path/to/bundle"
- Extract: bundleIdentifier, bundlePath, isLoaded

#### NSUserDefaults
- Format: "com.example.app (25 keys)"
- Extract: suiteName, count of keys
- Provide synthetic children for keys

#### NSProcessInfo
- Format: "MyApp (PID: 12345)"
- Extract: processName, processIdentifier, systemVersion

## Performance Requirements

All formatters MUST:
1. Complete in < 50ms (measure with `std::chrono`)
2. Handle nil objects gracefully (return "nil")
3. Handle corrupted memory (return fallback)
4. Avoid infinite recursion
5. Cache expensive computations

## Memory Layout Documentation

Document discovered memory layouts:
```cpp
// NSIndexSet layout (GNUstep)
struct NSIndexSet {
    Class isa;           // offset 0
    NSUInteger count;    // offset 16
    void *ranges;        // offset 24
    // ...
};
```

## Testing Checklist

For each formatter:
- [ ] Nil object handling
- [ ] Empty/default object
- [ ] Simple valid object
- [ ] Complex object with all fields
- [ ] Edge cases (max values, special characters)
- [ ] Performance < 50ms
- [ ] Memory corruption handling
- [ ] Thread safety
- [ ] Recursive structure handling

## Integration Steps

1. Create formatter class files (.cpp/.h)
2. Implement summary/synthetic providers
3. Register in `GNUstepFormattersRegistry.cpp`
4. Add to CMakeLists.txt
5. Create test program
6. Validate with LLDB
7. Add unit tests
8. Document memory layout

## Code Template

```cpp
// GNUstep[ClassName]Formatters.h
#pragma once
#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

class GNUstep[ClassName]SummaryProvider : public GNUstepSummaryProvider {
public:
    static bool WouldMatch(ValueObject &valobj);
    llvm::Expected<std::string> GetSummary() override;
};

} // namespace formatters
} // namespace lldb_private

// GNUstep[ClassName]Formatters.cpp
#include "GNUstep[ClassName]Formatters.h"

bool GNUstep[ClassName]SummaryProvider::WouldMatch(ValueObject &valobj) {
    // Check class name
    return MatchesClassName(valobj, {"[ClassName]", "GS[ClassName]"});
}

llvm::Expected<std::string> 
GNUstep[ClassName]SummaryProvider::GetSummary() {
    // Implementation
    return "<formatted output>";
}
```

## Success Criteria

A formatter is considered complete when:
1. All test cases pass
2. Performance < 50ms
3. No crashes or hangs
4. Output matches expected format
5. Memory layout documented
6. Unit tests written
7. Integration test added
8. Code reviewed and approved