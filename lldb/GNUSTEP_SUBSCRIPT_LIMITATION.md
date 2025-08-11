# GNUstep Objective-C Subscript Syntax Limitation in LLDB

## Problem Summary

Dictionary and array subscript syntax (`dict[@"key"]` and `array[0]`) does not work correctly with GNUstep runtime in LLDB, resulting in errors:

- **Dictionary subscripts**: "array subscript is not an integer" error
- **Array subscripts**: Returns garbage data (treats NSArray pointer as C array)

## Root Cause Analysis

The issue stems from multiple factors:

1. **Missing Runtime Methods**: GNUstep Foundation does not implement the modern Objective-C subscript methods:
   - `-[NSArray objectAtIndexedSubscript:]`
   - `-[NSDictionary objectForKeyedSubscript:]`

2. **Expression Parser Behavior**: When LLDB's Clang expression parser encounters subscript syntax:
   - Without `HasNewLiteralsAndIndexing()` returning true: Treats as C-style array access
   - With `HasNewLiteralsAndIndexing()` returning true: Converts to method calls that don't exist

## Investigation Performed

### Changes Made
1. **Added method declarations** in `GNUstepObjCDeclVendor.cpp`:
   - Declared `objectAtIndexedSubscript:` for NSArray
   - Declared `objectForKeyedSubscript:` for NSDictionary

2. **Implemented `CalculateHasNewLiteralsAndIndexing()`** in `GNUstepObjCRuntime`:
   - Checks for Foundation library presence
   - Returns true to enable subscript syntax transformation

3. **Overrode `GetRuntimeVersion()`**:
   - Returns `eGNUstep_libobjc2` to identify the runtime correctly

### Result
- Clang now properly transforms subscript syntax to method calls
- Runtime throws NSInvalidArgumentException because methods don't exist
- Error: "Can not determine type information for -[GSInlineArray objectAtIndexedSubscript:]"

## Current Workaround

Use traditional method calls instead of subscript syntax:

```objc
// Instead of:
id value = dict[@"key"];     // ❌ Doesn't work
id item = array[0];           // ❌ Doesn't work

// Use:
id value = [dict objectForKey:@"key"];    // ✅ Works
id item = [array objectAtIndex:0];        // ✅ Works
```

## Potential Solutions

### Option 1: Patch GNUstep Foundation (Recommended)
Add the subscript methods to GNUstep Foundation classes:

```objc
// In NSArray+Subscripting.m
@implementation NSArray (Subscripting)
- (id)objectAtIndexedSubscript:(NSUInteger)idx {
    return [self objectAtIndex:idx];
}
@end

// In NSDictionary+Subscripting.m
@implementation NSDictionary (Subscripting)
- (id)objectForKeyedSubscript:(id)key {
    return [self objectForKey:key];
}
@end
```

### Option 2: Runtime Method Injection
Dynamically add methods at runtime when LLDB attaches:
- Use `class_addMethod()` to add forwarding implementations
- Complex and may have side effects

### Option 3: Expression Preprocessing
Preprocess expressions in LLDB to rewrite subscript syntax:
- Transform `dict[@"key"]` → `[dict objectForKey:@"key"]`
- Transform `array[0]` → `[array objectAtIndex:0]`
- Requires significant changes to expression parser

## Files Modified

1. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h`
   - Added `GetRuntimeVersion()` override
   - Added `CalculateHasNewLiteralsAndIndexing()` override

2. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`
   - Implemented `CalculateHasNewLiteralsAndIndexing()`

3. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCDeclVendor.cpp`
   - Already had subscript method declarations in place

## Recommendation

The cleanest solution is to contribute patches to GNUstep Foundation to add native support for subscript methods. This would benefit all GNUstep users, not just LLDB users.

Until then, users should use traditional method syntax when debugging GNUstep applications with LLDB.

## Testing

Test case available at: `/home/robk/code/llvm-project/lldb/examples/test_dict_subscript.m`

```bash
# Compile test
make test_dict_subscript

# Test in LLDB
lldb test_dict_subscript
(lldb) b main
(lldb) run
(lldb) expr [testDict objectForKey:@"fruit"]  # Works
(lldb) expr testDict[@"fruit"]                 # Currently fails
```