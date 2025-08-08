# GNUstep LLDB Formatter Issues Analysis

## Executive Summary

Analysis of 7 critical formatter issues in the GNUstep LLDB plugin reveals a fundamental problem: **formatters are not being applied to synthetic children created by collection types**. The root cause is that synthetic children are created with generic 'id' type, but formatters are registered only for specific class names using exact matching.

## Issue Analysis

### 1. NSNumber Showing Hex Instead of Value (0x00000000000001f1)

**Problem**: In personInfo dictionary, the age NSNumber displays as raw hex address instead of "30"

**Evidence**:
- Screenshot shows: `[0].value = 0x00000000000001f1`
- This is a tagged pointer with tag 1 (NSSmallInt)
- Decoded value: 0x1f1 >> 3 = 0x3e = 62 (not 30 as expected)

**Root Cause**:
- GNUstepNSDictionaryFormatters.cpp:964-1001 creates synthetic children
- For regular objects, it uses `CreateValueObjectFromAddress` with 'id' type
- NSNumber formatter is registered for "NSNumber", "NSIntNumber" etc. but NOT for 'id'
- GNUstepFormattersRegistry.cpp:100-115 shows exact type matching only

### 2. Skills Array Not Expandable

**Problem**: The skills array in personInfo shows memory address without expansion capability

**Evidence**:
- Screenshot shows: `[3].value = 0x0000055555587a68` with no expansion arrow
- This is a regular object pointer to an NSArray

**Root Cause**:
- Same as Issue #1 - synthetic child created with 'id' type doesn't match registered NSArray formatters
- Array formatters registered in GNUstepFormattersRegistry.cpp:151-164 for specific types only

### 3. Infinite ISA Recursion

**Problem**: All objects show endless isa → isa → isa expansion

**Evidence**:
- Screenshot shows multiple nested isa entries with expansion arrows
- Each isa points to another expandable isa

**Root Cause**:
- No synthetic children provider filters out the isa pointer
- LLDB's default ObjC handling creates synthetic children for all ivars including isa
- GNUstepGenericFormatter.cpp doesn't provide a synthetic children filter
- Missing implementation to hide isa from synthetic children

### 4. Account _transactions Not Visible

**Problem**: Cannot see or preview the transactions array in BankAccount object

**Evidence**:
- Screenshot shows: `_transactions = 4 objects @{@[4 pairs], @[4 pairs], @[4 pairs], @[4 pairs]}`
- The preview text is malformed (showing dictionaries instead of transaction objects)

**Root Cause**:
- Custom class (BankAccount) doesn't have a specific formatter
- Generic formatter in GNUstepGenericFormatter.cpp:273-305 doesn't properly handle object ivars
- Line 281 calls FormatObjectIvar which may not apply proper formatters

### 5. Preferences Set Preview Missing

**Problem**: NSMutableSet preferences no longer shows element preview

**Evidence**:
- Screenshot shows: `preferences = 3 objects {<object>, <object>, <object>}`
- Elements display as generic "<object>" instead of actual values

**Root Cause**:
- Set formatter's GetElementSummary (similar to dictionary's version) returns "<object>" fallback
- GNUstepDictionaryFormatters.cpp:520 shows the fallback return
- Formatters not applied to set elements due to 'id' type issue

### 6. AccountSummary Only Shows Keys

**Problem**: NSDictionary accountSummary displays keys but values show as hex addresses

**Evidence**:
- Screenshot shows: `[0].key = "summary"` but `[0].value = 0x00005555557add5`
- Values are raw addresses instead of formatted strings

**Root Cause**:
- Same as Issue #1 - value objects created as 'id' type don't get formatters applied
- GNUstepDictionaryFormatters.cpp:956-962 creates child with generic 'id' type

### 7. Collection/Synthetic Children Value Display

**Problem**: Collection elements show memory addresses instead of formatted values

**Evidence**:
- Throughout screenshot, collection values show as hex addresses
- This affects arrays, dictionaries, and sets uniformly

**Root Cause**:
- Systematic issue with formatter registration scope
- All synthetic children created with 'id' type (by design for dynamic resolution)
- But formatters only registered for specific class names with exact matching

## Core Implementation Issues

### A. Formatter Registration Scope

**File**: GNUstepFormattersRegistry.cpp

**Issue**: Formatters use `eFormatterMatchExact` which doesn't match 'id' typed objects
```cpp
// Line 100-101 - Only matches "NSNumber" exactly, not 'id'
category.AddTypeSummary("NSNumber", eFormatterMatchExact, number_summary);
```

**Required Fix**: Need to either:
1. Register formatters for 'id' type with runtime checking
2. Use regex matching to catch all variants
3. Implement dynamic type resolution before formatter selection

### B. Synthetic Children Type Resolution

**File**: GNUstepDictionaryFormatters.cpp

**Issue**: GetChildAtIndex creates children with generic 'id' type
```cpp
// Line 959-962 - Always returns 'id' type
CompilerType element_type = GetConcreteTypeForObject(object_ptr);
if (!element_type.IsValid()) {
    element_type = id_type;  // Falls back to 'id'
}
```

**Required Fix**: Need to:
1. Resolve concrete type BEFORE creating ValueObject
2. OR ensure formatters can match 'id' typed objects
3. OR force dynamic type resolution on synthetic children

### C. ISA Recursion

**File**: Missing synthetic children filtering

**Issue**: No filter to hide isa pointer from synthetic children
```cpp
// Need to implement in base synthetic provider:
bool ShouldShowIvar(const std::string& ivar_name) {
    return ivar_name != "isa";
}
```

## Recommended Fixes

### Priority 1: Fix Formatter Matching for Synthetic Children

**Solution**: Add 'id' type formatter that performs runtime type checking

```cpp
// In GNUstepFormattersRegistry.cpp
bool GNUstepIdFormatterFunction(ValueObject &valobj, Stream &stream, 
                                const TypeSummaryOptions &options) {
    // Get runtime type
    ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process);
    auto class_descriptor = runtime->GetClassDescriptor(valobj);
    std::string class_name = class_descriptor->GetClassName().AsCString("");
    
    // Dispatch to appropriate formatter
    if (class_name.find("NSNumber") != std::string::npos) {
        return GNUstepNSNumberFormatterFunction(valobj, stream, options);
    } else if (class_name.find("NSString") != std::string::npos) {
        return GNUstepNSStringFormatterFunction(valobj, stream, options);
    }
    // ... etc
}

// Register for 'id' type
category.AddTypeSummary("id", eFormatterMatchExact, id_summary);
```

### Priority 2: Fix ISA Recursion

**Solution**: Filter isa from synthetic children

```cpp
// In GNUstepObjCRuntime.cpp or base synthetic provider
class GNUstepObjectSyntheticProvider : public SyntheticChildrenFrontEnd {
    bool ShouldShowChild(const std::string& name) override {
        // Hide isa pointer and other runtime internals
        return name != "isa" && !name.starts_with("_");
    }
};
```

### Priority 3: Fix Tagged Pointer Value Display

**Solution**: Ensure tagged pointer decoding in synthetic children

```cpp
// In GNUstepDictionaryFormatters.cpp:969-991
if (is_tagged_pointer) {
    // Decode tagged pointer value properly
    if ((object_ptr & 0x7) == 1) { // NSSmallInt
        int64_t value = ((int64_t)object_ptr) >> 3;
        // Create summary string directly
        stream.Printf("%lld", value);
    }
}
```

## Test Cases

### Test 1: NSNumber in Dictionary
```objc
NSDictionary *dict = @{@"age": @30};
// Expected: [0].key = "age", [0].value = 30
// Current: [0].key = "age", [0].value = 0x00000000000001f1
```

### Test 2: NSArray in Dictionary
```objc
NSDictionary *dict = @{@"skills": @[@"C++", @"ObjC"]};
// Expected: [0].value = @[2 objects] with expandable children
// Current: [0].value = 0x0000055555587a68 (no expansion)
```

### Test 3: ISA Display
```objc
NSObject *obj = [[NSObject alloc] init];
// Expected: No isa shown in variables view
// Current: isa → isa → isa (infinite recursion)
```

## Implementation Priority

1. **Immediate** (Blocks all formatting):
   - Fix formatter registration to handle 'id' type objects
   - Add runtime type resolution for synthetic children

2. **High** (Major UX issue):
   - Filter isa from synthetic children display
   - Fix tagged pointer value decoding

3. **Medium** (Functionality):
   - Improve custom class formatting
   - Fix collection preview generation

## Files Requiring Modification

1. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepFormattersRegistry.cpp`
   - Add 'id' type formatter registration
   - Implement runtime type dispatch

2. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDictionaryFormatters.cpp`
   - Fix GetChildAtIndex type resolution
   - Improve tagged pointer handling

3. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepArrayFormatters.cpp`
   - Similar fixes as dictionary formatter

4. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepSetFormatters.cpp`
   - Similar fixes as dictionary formatter

5. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`
   - Add synthetic children filter for isa

## Validation Criteria

After implementing fixes, all the following should work:

1. ✅ NSNumber values display as numbers, not hex
2. ✅ Arrays in dictionaries are expandable
3. ✅ No isa recursion visible
4. ✅ Custom class ivars properly formatted
5. ✅ Set elements show actual values
6. ✅ Dictionary values properly formatted
7. ✅ All collection elements display values, not addresses

## Risk Assessment

- **High Risk**: Formatter registration changes could affect all GNUstep object formatting
- **Medium Risk**: ISA filtering might hide legitimate debugging information
- **Low Risk**: Tagged pointer fixes are localized to specific code paths

## Conclusion

The primary issue is a design mismatch between LLDB's dynamic type resolution system and the GNUstep formatter registration strategy. Synthetic children are intentionally created with generic 'id' type to allow dynamic resolution, but formatters are registered only for specific class names with exact matching. The fix requires either:

1. Registering formatters that work with 'id' type and perform runtime type checking
2. Forcing concrete type resolution before creating synthetic children
3. Using regex or pattern matching for formatter registration

The recommended approach is option 1, as it aligns with LLDB's architecture and Apple's formatter patterns.