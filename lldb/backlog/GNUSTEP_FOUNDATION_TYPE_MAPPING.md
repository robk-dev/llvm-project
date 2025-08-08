# GNUstep Foundation Type Hierarchy and Selector Mapping

**Critical Fix for P0 Debugging Crashes**: `NSInvalidArgumentException: -[GSTinyString ]: unrecognized selector sent to instance`

## Problem Analysis

The debugging crashes occur because our `GNUstepObjCDeclVendor` lacks proper selector declarations for GNUstep Foundation types. When LLDB attempts introspection, it sends empty/unrecognized selectors to GNUstep objects, causing immediate crashes.

## Complete GNUstep Foundation Type Hierarchy

### String Type Family

**Tagged Pointer Types (Critical for Crash Fix):**
- `GSTinyString` - Tagged pointer strings (≤8 characters)

**Base String Classes:**
- `GSCString` - C-string based implementation
- `GSUnicodeString` - Unicode character based implementation

**Buffer Management Variants:**
- `GSCBufferString` - C-string with managed buffer
- `GSUnicodeBufferString` - Unicode with managed buffer

**Inline Storage Variants:**
- `GSCInlineString` - C-string with inline storage
- `GSUInlineString` - Unicode with inline storage

**Substring Variants:**
- `GSCSubString` - C-string substring view
- `GSUnicodeSubString` - Unicode substring view

**Standard Classes:**
- `NSString` - Abstract base class
- `NSMutableString` - Mutable string abstract base
- `NSConstantString` - Compile-time constant strings

### Array Type Family

**Base Array Classes:**
- `GSArray` - Basic array implementation
- `GSInlineArray` - Array with inline object storage
- `GSMutableArray` - Mutable array base

**GNUstep Extensions:**
- `NSGArray` - GNUstep-specific array implementation
- `NSGMutableArray` - GNUstep-specific mutable array

**Enumerators:**
- `GSArrayEnumerator` - Forward enumeration
- `GSArrayEnumeratorReverse` - Reverse enumeration

### Dictionary Type Family

**Base Dictionary Classes:**
- `GSDictionary` - Basic dictionary implementation
- `GSMutableDictionary` - Mutable dictionary base
- `GSCachedDictionary` - Dictionary with caching optimization

**GNUstep Extensions:**
- `NSGDictionary` - GNUstep-specific dictionary
- `NSGMutableDictionary` - GNUstep-specific mutable dictionary

**Enumerators:**
- `GSDictionaryKeyEnumerator` - Key enumeration
- `GSDictionaryObjectEnumerator` - Value enumeration

### Set Type Family

**Base Set Classes:**
- `GSSet` - Basic set implementation
- `GSMutableSet` - Mutable set base

**GNUstep Extensions:**
- `NSGSet` - GNUstep-specific set
- `NSGMutableSet` - GNUstep-specific mutable set

**Enumerators:**
- `GSSetEnumerator` - Set enumeration

### Number Type Family

**Signed Integer Types:**
- `NSSignedIntegerNumber` - Base signed integer
- `NSIntNumber` - Integer numbers
- `NSBoolNumber` - Boolean values (inherits from NSIntNumber)
- `NSLongLongNumber` - 64-bit signed integers

**Unsigned Integer Types:**
- `NSUnsignedLongLongNumber` - 64-bit unsigned integers

**Tagged Pointer Integer Types (Critical):**
- `NSSmallInt` - Tagged pointer integers (small values)

**Floating Point Types:**
- `NSFloatingPointNumber` - Base floating point
- `NSFloatNumber` - 32-bit float
- `NSDoubleNumber` - 64-bit double

**Tagged Pointer Float Types (Critical):**
- `NSSmallExtendedDouble` - Tagged pointer extended precision
- `NSSmallRepeatingDouble` - Tagged pointer repeating decimals

## Critical Selector Mappings

### GSTinyString Core Selectors

**String Content Access:**
```objc
- (NSUInteger)length
- (unichar)characterAtIndex:(NSUInteger)index
- (void)getCharacters:(unichar*)buffer
- (void)getCharacters:(unichar*)buffer range:(NSRange)range
- (const char*)UTF8String
- (BOOL)getCString:(char*)buffer maxLength:(NSUInteger)maxLength encoding:(NSStringEncoding)encoding
```

**String Comparison:**
```objc
- (BOOL)isEqualToString:(NSString*)aString
- (NSUInteger)hash
- (NSComparisonResult)compare:(NSString*)aString options:(NSUInteger)mask range:(NSRange)range
```

**Value Conversion:**
```objc
- (BOOL)boolValue
- (int)intValue
- (NSInteger)integerValue
- (long long)longLongValue
- (double)doubleValue
- (float)floatValue
```

**Memory Management:**
```objc
- (id)copy
- (id)copyWithZone:(NSZone*)zone
- (id)mutableCopyWithZone:(NSZone*)zone
- (id)retain
- (oneway void)release
- (id)autorelease
- (NSUInteger)retainCount
```

**Debugging Support:**
```objc
- (NSString*)description
- (NSString*)debugDescription
- (NSUInteger)sizeOfContentExcluding:(NSHashTable*)exclude
- (NSUInteger)sizeOfInstance
- (NSUInteger)sizeInBytesExcluding:(NSHashTable*)exclude
```

### Foundation Collection Core Selectors

**NSArray/GSArray:**
```objc
- (NSUInteger)count
- (id)objectAtIndex:(NSUInteger)index
- (NSEnumerator*)objectEnumerator
- (NSEnumerator*)reverseObjectEnumerator
- (NSString*)description
- (NSString*)descriptionWithLocale:(id)locale
- (BOOL)containsObject:(id)anObject
- (NSUInteger)indexOfObject:(id)anObject
```

**NSDictionary/GSDictionary:**
```objc
- (NSUInteger)count
- (id)objectForKey:(id)aKey
- (NSEnumerator*)keyEnumerator
- (NSEnumerator*)objectEnumerator
- (NSArray*)allKeys
- (NSArray*)allValues
- (NSString*)description
- (NSString*)descriptionWithLocale:(id)locale
```

**NSSet/GSSet:**
```objc
- (NSUInteger)count
- (id)member:(id)object
- (NSEnumerator*)objectEnumerator
- (BOOL)containsObject:(id)anObject
- (NSString*)description
- (NSString*)descriptionWithLocale:(id)locale
```

### NSNumber Core Selectors

**Value Access:**
```objc
- (BOOL)boolValue
- (char)charValue
- (short)shortValue
- (int)intValue
- (long)longValue
- (long long)longLongValue
- (unsigned char)unsignedCharValue
- (unsigned short)unsignedShortValue
- (unsigned int)unsignedIntValue
- (unsigned long)unsignedLongValue
- (unsigned long long)unsignedLongLongValue
- (float)floatValue
- (double)doubleValue
- (NSInteger)integerValue
- (NSUInteger)unsignedIntegerValue
```

**Type Information:**
```objc
- (const char*)objCType
- (NSString*)stringValue
- (NSString*)description
```

## Tagged Pointer Detection

### GSTinyString Detection
```cpp
// Tagged string detection (used in GNUstep)
bool isGSTinyString(uintptr_t ptr) {
    return (ptr & TINY_STRING_TAG_MASK) == TINY_STRING_TAG;
}

// Constants from GNUstep source
#define TINY_STRING_TAG_MASK     0x7
#define TINY_STRING_TAG          0x1
#define TINY_STRING_LENGTH_SHIFT 3
#define TINY_STRING_LENGTH_MASK  0x7
```

### NSSmallInt Detection
```cpp
// Tagged integer detection
bool isNSSmallInt(uintptr_t ptr) {
    return (ptr & 0x1) == 0x1;  // Odd pointers are tagged ints
}
```

### NSSmallDouble Detection
```cpp
// Tagged double detection (platform specific)
bool isNSSmallDouble(uintptr_t ptr) {
    return (ptr & 0x7) == 0x4;  // Specific tag for tagged doubles
}
```

## Integration Strategy

### 1. Enhance GNUstepRuntimeV2API

**Add Type Detection Methods:**
```cpp
// In GNUstepRuntimeV2API class
bool IsTaggedPointer(uintptr_t ptr);
TaggedPointerType GetTaggedPointerType(uintptr_t ptr);
std::string GetTaggedPointerClassName(uintptr_t ptr);
```

**Extend Existing Methods:**
```cpp
// Enhance existing method to handle tagged pointers
Class GetClass(uintptr_t object) {
    if (IsTaggedPointer(object)) {
        return GetTaggedPointerClass(object);
    }
    return GetRegularClass(object);
}
```

### 2. Fix GNUstepObjCDeclVendor

**Add Foundation Selector Declarations:**
```cpp
void GNUstepObjCDeclVendor::RegisterFoundationSelectors() {
    // Register core NSObject selectors for all classes
    RegisterSelector("description", "@0:0");
    RegisterSelector("debugDescription", "@0:0");
    RegisterSelector("hash", "L0:0");
    RegisterSelector("isEqual:", "B0:0@0:0");
    
    // Register NSString selectors
    RegisterSelector("length", "L0:0");
    RegisterSelector("characterAtIndex:", "S0:0L0:0");
    RegisterSelector("UTF8String", "*0:0");
    RegisterSelector("isEqualToString:", "B0:0@0:0");
    RegisterSelector("boolValue", "B0:0");
    RegisterSelector("intValue", "i0:0");
    RegisterSelector("integerValue", "l0:0");
    
    // Register NSArray selectors
    RegisterSelector("count", "L0:0");
    RegisterSelector("objectAtIndex:", "@0:0L0:0");
    RegisterSelector("objectEnumerator", "@0:0");
    
    // Register NSDictionary selectors
    RegisterSelector("objectForKey:", "@0:0@0:0");
    RegisterSelector("keyEnumerator", "@0:0");
    RegisterSelector("allKeys", "@0:0");
    
    // Register NSSet selectors
    RegisterSelector("member:", "@0:0@0:0");
    RegisterSelector("containsObject:", "B0:0@0:0");
    
    // Register NSNumber selectors
    RegisterSelector("doubleValue", "d0:0");
    RegisterSelector("floatValue", "f0:0");
    RegisterSelector("longLongValue", "q0:0");
    RegisterSelector("objCType", "*0:0");
    RegisterSelector("stringValue", "@0:0");
}
```

### 3. Class Registration Strategy

**Register All Foundation Classes:**
```cpp
void GNUstepObjCRuntime::RegisterFoundationClasses() {
    // String classes
    RegisterClass("GSTinyString", "NSString");
    RegisterClass("GSCString", "NSString");
    RegisterClass("GSUnicodeString", "NSString");
    RegisterClass("GSCBufferString", "GSCString");
    RegisterClass("GSUnicodeBufferString", "GSUnicodeString");
    RegisterClass("GSCInlineString", "GSCString");
    RegisterClass("GSUInlineString", "GSUnicodeString");
    RegisterClass("GSCSubString", "GSCString");
    RegisterClass("GSUnicodeSubString", "GSUnicodeString");
    
    // Array classes  
    RegisterClass("GSArray", "NSArray");
    RegisterClass("GSInlineArray", "GSArray");
    RegisterClass("NSGArray", "NSArray");
    RegisterClass("GSMutableArray", "NSMutableArray");
    RegisterClass("NSGMutableArray", "NSMutableArray");
    
    // Dictionary classes
    RegisterClass("GSDictionary", "NSDictionary");
    RegisterClass("GSMutableDictionary", "NSMutableDictionary");
    RegisterClass("GSCachedDictionary", "GSDictionary");
    RegisterClass("NSGDictionary", "NSDictionary");
    RegisterClass("NSGMutableDictionary", "NSMutableDictionary");
    
    // Set classes
    RegisterClass("GSSet", "NSSet");
    RegisterClass("GSMutableSet", "NSMutableSet");
    RegisterClass("NSGSet", "NSSet");
    RegisterClass("NSGMutableSet", "NSMutableSet");
    
    // Number classes
    RegisterClass("NSSignedIntegerNumber", "NSNumber");
    RegisterClass("NSIntNumber", "NSSignedIntegerNumber");
    RegisterClass("NSBoolNumber", "NSIntNumber");
    RegisterClass("NSLongLongNumber", "NSSignedIntegerNumber");
    RegisterClass("NSUnsignedLongLongNumber", "NSNumber");
    RegisterClass("NSSmallInt", "NSSignedIntegerNumber"); // Tagged
    RegisterClass("NSFloatingPointNumber", "NSNumber");
    RegisterClass("NSFloatNumber", "NSFloatingPointNumber");
    RegisterClass("NSDoubleNumber", "NSFloatingPointNumber");
    RegisterClass("NSSmallExtendedDouble", "NSFloatingPointNumber"); // Tagged
    RegisterClass("NSSmallRepeatingDouble", "NSFloatingPointNumber"); // Tagged
}
```

## Implementation Priority

### Phase 1: Critical Crash Fix (P0)
1. **Fix GSTinyString selector registration** - This directly addresses the crash
2. **Add tagged pointer detection** - Prevents incorrect object introspection
3. **Register core NSObject selectors** - description, hash, isEqual:

### Phase 2: Foundation Core (P1) 
1. **Complete string class selector registration** - All GSString variants
2. **Add NSNumber tagged pointer support** - NSSmallInt, NSSmallDouble variants
3. **Basic collection selectors** - count, objectAtIndex:, objectForKey:

### Phase 3: Advanced Features (P2)
1. **Complete collection class hierarchy** - All variants and enumerators
2. **Advanced selector support** - Locale-specific methods, complex comparisons
3. **Memory debugging selectors** - sizeOfContentExcluding:, etc.

## Testing Strategy

### Unit Tests for Each Type
```cpp
// Test that each Foundation class responds to core selectors
TEST(GNUstepFoundationMapping, GSTinyStringSelectors) {
    auto runtime = GetGNUstepRuntime();
    auto declVendor = runtime->GetDeclVendor();
    
    // Test that GSTinyString has required selectors
    EXPECT_TRUE(declVendor->HasSelector("GSTinyString", "length"));
    EXPECT_TRUE(declVendor->HasSelector("GSTinyString", "UTF8String"));
    EXPECT_TRUE(declVendor->HasSelector("GSTinyString", "description"));
}
```

### Integration Test with Real Objects
```cpp
TEST(GNUstepFoundationMapping, RealObjectIntrospection) {
    // Create actual GNUstep objects and test introspection
    lldb::ValueObjectSP stringObj = GetGSTinyStringObject();
    
    // Should not crash when getting description
    auto desc = stringObj->GetSummaryAsCString();
    EXPECT_NE(desc, nullptr);
    
    // Should not crash when calling methods
    auto length = stringObj->CallMethod("length");
    EXPECT_TRUE(length.IsValid());
}
```

## Files to Modify

1. **GNUstepObjCDeclVendor.cpp/h** - Add Foundation selector registration
2. **GNUstepRuntimeV2API.cpp/h** - Add tagged pointer detection  
3. **GNUstepObjCRuntime.cpp** - Initialize Foundation class registration
4. **GNUstepFormattersRegistry.cpp** - Ensure formatters handle all types

This mapping provides the foundation to fix the P0-Critical debugging crashes by ensuring our DeclVendor properly declares all selectors that GNUstep Foundation classes actually implement.