# Foundation Formatter Architecture Guide

## Overview

This guide documents the complete Foundation formatter architecture for GNUstep LLDB debugging, covering the systematic approach to creating production-ready formatters.

## Core Architecture Principles

### 1. Consistent Validation Pattern

All Foundation formatters follow this validation sequence:

```cpp
bool SomeFormatterProvider::FormatObject(ValueObject &valobj, Stream &stream, 
                                        const TypeSummaryOptions &options) {
    // Step 1: Validate the object is a proper GNUstep object
    if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
        stream.Printf("invalid object");
        return false;
    }

    // Step 2: Get process and validate
    ProcessSP process_sp = valobj.GetProcessSP();
    if (!process_sp)
        return false;

    // Step 3: Get object address and validate
    addr_t obj_ptr = valobj.GetPointerValue();
    if (obj_ptr == 0 || obj_ptr == LLDB_INVALID_ADDRESS) {
        stream.Printf("nil");
        return true;
    }

    // Step 4: Extract and format content
    std::string content = ExtractContent(valobj);
    stream.Printf("%s", content.c_str());
    return true;
}
```

### 2. Layered Extraction Strategy

Content extraction follows a three-tier approach:

1. **Child Ivar Access** (Most Reliable)
   ```cpp
   // Try to find ivars via LLDB's introspection
   auto num_children = valobj.GetNumChildren();
   for (size_t i = 0; i < *num_children; i++) {
       ValueObjectSP child = valobj.GetChildAtIndex(i);
       const char *name = child->GetName().GetCString();
       if (strcmp(name, "_targetIvar") == 0) {
           // Use child->GetSummaryAsCString() or child->GetValueAsUnsigned()
       }
   }
   ```

2. **Memory Layout Reading** (Reliable Fallback)
   ```cpp
   // Use known GNUstep memory layouts
   uint32_t addr_size = process_sp->GetAddressByteSize();
   addr_t ivar_addr = obj_ptr + known_offset;
   addr_t ivar_value = GNUstepRuntimeHelper::ReadPointer(process, ivar_addr, error);
   ```

3. **Tagged Pointer Handling** (Special Cases)
   ```cpp
   GNUstepObjCRuntimeIntrospector introspector(process);
   if (introspector.IsTaggedPointer(obj_ptr)) {
       return introspector.DecodeTaggedString(obj_ptr);
   }
   ```

### 3. Error Handling Patterns

```cpp
// Always use Status objects for memory operations
Status error;
addr_t result = GNUstepRuntimeHelper::ReadPointer(process, address, error);
if (error.Fail() || result == 0 || result == LLDB_INVALID_ADDRESS) {
    return ""; // Graceful degradation
}

// Bounds checking for safety
if (length > 0 && length < 100) { // Reasonable limits
    // Process the data
} else {
    return ""; // Avoid reading unbounded data
}
```

## GNUstep Memory Layouts

### NSString/NSConstantString Layout
```
Offset 0:  Class isa
Offset 8:  uint32_t length
Offset 12: uint32_t padding  
Offset 16: uint64_t length2
Offset 24: const char* cstring_ptr
```

### NSAttributedString Layout
```
Offset 0:  Class isa
Offset 8:  NSString* _string
Offset 16: id _attributes
```

### NSIndexPath Layout
```
Offset 0:  Class isa
Offset 8:  NSUInteger* _indexes
Offset 16: NSUInteger _length
```

### NSArray Layout
```
Offset 0:  Class isa
Offset 8:  NSUInteger _count
Offset 16: id* _objects
```

### NSDictionary Layout
```
Offset 0:  Class isa
Offset 8:  NSUInteger _count
Offset 16: NSMapTable* _table (or similar internal structure)
```

## Utility Functions

### GNUstepRuntimeHelper Functions

```cpp
// Object validation
bool IsValidGNUstepObject(ValueObject &valobj);

// Memory reading
addr_t ReadPointer(Process *process, addr_t address, Status &error);
std::string ReadUTF8String(Process *process, addr_t address, size_t max_length);

// Class name extraction
std::string GetGNUstepClassName(ValueObject &valobj);

// Process utilities
Process* GetProcessFromValueObject(ValueObject &valobj);
```

### GNUstepObjCRuntimeIntrospector Functions

```cpp
// Tagged pointer handling
bool IsTaggedPointer(addr_t obj_addr);
std::string DecodeTaggedString(addr_t obj_addr);

// Class introspection
std::string GetClassName(addr_t isa_addr);
addr_t GetISAFromObject(ValueObject &valobj);
```

## Formatter Registration

All formatters are registered in `GNUstepFormattersRegistry.cpp`:

```cpp
void GNUstepFormattersRegistry::RegisterFormatters(TypeCategoryImpl &category) {
    // Register in priority order
    RegisterStringFormatters(category);      // Priority 1: Strings
    RegisterNumberFormatters(category);      // Priority 2: Numbers
    RegisterCollectionFormatters(category);  // Priority 3: Collections
    RegisterFoundationFormatters(category);  // Priority 4: Foundation
    RegisterGenericFormatter(category);      // Priority 5: Fallback
}
```

Each formatter uses this pattern:

```cpp
void GNUstepFormattersRegistry::RegisterSpecificFormatter(TypeCategoryImpl &category) {
    TypeSummaryImpl::Flags flags;
    flags.SetCascades(true)
         .SetSkipPointers(false)
         .SetSkipReferences(false)
         .SetDontShowChildren(true)
         .SetDontShowValue(true);

    auto summary = std::make_shared<CXXFunctionSummaryFormat>(
        flags, SpecificFormatterFunction, "Description");

    // Register all relevant type names
    category.AddTypeSummary("TypeName", eFormatterMatchExact, summary);
    category.AddTypeSummary("TypeName *", eFormatterMatchExact, summary);
    category.AddTypeSummary("GSTypeName", eFormatterMatchExact, summary);
    category.AddTypeSummary("GSTypeName *", eFormatterMatchExact, summary);
    
    // Prevent recursion with NoOp synthetic children
    auto noop_synth = std::make_shared<CXXSyntheticChildren>(
        synth_flags, GNUstepNoOpSyntheticProvider, "NoOp synthetic");
    category.AddTypeSynthetic("TypeName", eFormatterMatchExact, noop_synth);
    category.AddTypeSynthetic("TypeName *", eFormatterMatchExact, noop_synth);
}
```

## Performance Requirements

All formatters must meet these performance criteria:

- **Response Time**: <50ms for typical objects
- **Memory Safety**: Bounded reads (max 256 chars, max 100 elements, etc.)
- **Error Resilience**: Graceful degradation on memory access failures
- **Resource Usage**: No memory leaks, minimal allocations

## Testing Framework

### Test Program Structure

```objective-c
int main() {
    @autoreleasepool {
        // Create test objects
        NSString *test = @"content";
        
        // Print addresses for LLDB testing
        printf("Object: %p\n", test);
        printf("Expected: \"content\"\n");
        
        // Wait for debugger
        getchar();
        return 0;
    }
}
```

### LLDB Testing Commands

```bash
# Build with debug symbols
clang -fobjc-runtime=gnustep-2.1 -g -gdwarf-5 -O0 [other flags] -o test test.m [libs]

# Test in LLDB
(lldb) target create test
(lldb) run
# When program waits:
(lldb) po 0x<address>  # Test formatter output
```

## Debugging Formatter Issues

### Common Issues and Solutions

1. **"Invalid Object" Errors**
   - Check `GNUstepRuntimeHelper::IsValidGNUstepObject()` implementation
   - Verify object is properly constructed with GNUstep runtime

2. **Empty/Null Output**
   - Add debug printf statements to track execution path
   - Verify memory layout assumptions with `x/32xg <address>`
   - Check error status from memory reads

3. **Incorrect Type Matching**
   - Verify type names in formatter registration
   - Check if object is being handled by generic formatter instead

4. **Memory Access Failures**
   - Use `Status error` objects and check `error.Fail()`
   - Verify addresses are valid before reading
   - Add bounds checking for safety

### Debug Output Pattern

```cpp
#ifdef DEBUG_FORMATTER
    printf("[DEBUG] Formatter: %s processing object at 0x%llx\n", 
           __FUNCTION__, (uint64_t)obj_ptr);
    printf("[DEBUG] Found %zu children\n", num_children);
    printf("[DEBUG] Memory read result: 0x%llx\n", (uint64_t)result);
#endif
```

## Future Extensions

The architecture supports easy addition of new formatters:

1. **Copy Existing Pattern**: Use NSString or NSArray as template
2. **Determine Memory Layout**: Use LLDB memory examination or GNUstep source
3. **Implement Extraction**: Follow the 3-tier approach
4. **Add Registration**: Update `GNUstepFormattersRegistry.cpp`
5. **Create Tests**: Add to test program and verify output

## Production Checklist

Before marking a formatter as production-ready:

- ✅ Follows validation pattern
- ✅ Implements 3-tier extraction
- ✅ Has proper error handling
- ✅ Meets performance requirements
- ✅ Has comprehensive tests
- ✅ Handles edge cases (null, empty, malformed)
- ✅ Registered correctly with all type variants
- ✅ Documented with expected output examples

This architecture has proven successful for NSString, NSNumber, NSArray, NSDictionary, NSSet, NSAttributedString, and NSIndexPath formatters, providing clean, reliable debugging output for GNUstep Foundation objects.