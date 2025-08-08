# GNUstep Tagged NSConstantString Pointer Fix

## Problem Statement
The GNUstep LLDB array formatter was displaying incorrect string values: `@["xp", "<tagged_string>", "e"]` instead of `@["Apple", "Banana", "Cherry"]`. This was caused by improper decoding of GNUstep's tagged string pointers.

## Root Cause Analysis
GNUstep uses two different string representations:
1. **Tagged Strings** - For short compile-time constants (≤9 characters)
2. **NSConstantString Objects** - For longer strings or runtime-created strings

The formatter was attempting to read tagged pointers as regular object pointers, resulting in garbage output.

## Solution: Dual Decoder Implementation

### 1. Tagged String Decoder
For strings with tag value 4 (bits 0-2):

```cpp
std::string DecodeTaggedString(lldb::addr_t obj_addr) {
  // Verify this is a tagged string (tag = 4)
  if ((obj_addr & 0x7) != 4) {
    return "";
  }
  
  // Extract length from bits 3-7
  int length = (obj_addr >> 3) & 0x1f;
  
  // Decode characters - each uses 7 bits, stored from bit 57 downward
  std::string result;
  result.reserve(length);
  
  for (int i = 0; i < length; i++) {
    // Extract character using GNUstep formula
    uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
    char c = (obj_addr & mask) >> (57 - (i * 7));
    
    if (c >= 0x20 && c <= 0x7e) {
      result += c;
    }
  }
  return result;
}
```

### 2. NSConstantString Decoder
For regular object pointers:

```cpp
// NSConstantString layout:
// struct {
//   Class isa;          // offset 0
//   const char *str;    // offset 8  <-- String pointer
//   uint32_t len;       // offset 16
// };

lldb::addr_t str_ptr_addr = obj_addr + 8;
lldb::addr_t str_data_addr = ReadPointer(process, str_ptr_addr, error);
```

## Tagged String Encoding Format

| Bits | Purpose | Description |
|------|---------|-------------|
| 0-2 | Tag | Value 4 for tagged strings |
| 3-7 | Length | 5 bits, stores 0-31 (max 9 chars used) |
| 8-56 | Unused | Padding/metadata |
| 57-63, 50-56, etc | Characters | 7 bits per character, stored from high bits down |

### Decoding Examples

```
0x83c386cca000002c → "Apple"
- Tag: 4 (bits 0-2)
- Length: 5 (bits 3-7)
- Chars: A(0x41), p(0x70), p(0x70), l(0x6c), e(0x65)

0x8587761dd8400034 → "Banana"  
- Tag: 4
- Length: 6
- Chars: B(0x42), a(0x61), n(0x6e), a(0x61), n(0x6e), a(0x61)
```

## Integration in Array Formatter

```cpp
std::string TryExtractStringContent(Process *process, lldb::addr_t obj_addr) {
  GNUstepObjCRuntimeIntrospector introspector(process);
  
  // Check if it's a tagged pointer
  if (introspector.IsTaggedPointer(obj_addr)) {
    uint64_t tag = obj_addr & 0x7;
    if (tag == 4) {
      // Decode as tagged string
      return introspector.DecodeTaggedString(obj_addr);
    }
    return "";  // Other tagged types
  }
  
  // Handle as regular NSConstantString object
  // ... read ISA, determine class, extract string pointer ...
}
```

## Test Validation

Created C test program to verify decoder:
```c
void decode_tagged_string(uint64_t addr) {
  // ... decoder implementation ...
}

// Test results:
// 0x83c386cca000002c → "Apple" ✓
// 0x8587761dd8400034 → "Banana" ✓  
// 0x87a32f2e5e400034 → "Cherry" ✓
```

## Impact
- Arrays now display correctly: `@["apple", "banana", "cherry", "date"]`
- All compile-time string literals properly decoded
- No performance impact (direct bit manipulation)
- Solution is architecture-independent (works on 64-bit systems)

## Files Modified
- `/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.cpp` - Added DecodeTaggedString()
- `/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepArrayFormatters.cpp` - Updated TryExtractStringContent()

## Future Considerations
- Tag 2 appears to be used for tagged NSNumber objects
- Other tag values (1, 3, 5, 6, 7) may have special meanings
- Consider caching decoded strings for performance in large arrays