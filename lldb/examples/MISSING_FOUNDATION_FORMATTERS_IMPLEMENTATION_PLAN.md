# Missing Foundation Formatters Implementation Plan

## Executive Summary

Analysis of 5 high-priority missing Foundation formatters: NSBundle, NSProcessInfo, NSUserDefaults, NSLocale, and NSScanner. These formatters would significantly improve debugging experience for configuration, system info, and text parsing scenarios.

## Current State Analysis

**Problem:** All Foundation objects currently display as raw pointers (`0x555555833458`) with no meaningful debugging information.

**Impact:** Developers cannot inspect:
- Bundle paths and versions (NSBundle)
- Process information (NSProcessInfo)  
- Configuration settings (NSUserDefaults)
- Locale settings (NSLocale)
- Text scanning state (NSScanner)

## Implementation Priority & Effort Matrix

| Formatter | Priority | Debugging Value | Complexity | Est. Hours | Status |
|-----------|----------|-----------------|------------|------------|--------|
| NSBundle | 1 (Critical) | Very High | Low | 2h | Ready |
| NSProcessInfo | 2 (High) | High | Medium | 4h | Blocked* |
| NSScanner | 3 (Medium) | Medium | Low | 2h | Ready |
| NSLocale | 4 (Medium) | Medium | Low | 2h | Ready |
| NSUserDefaults | 5 (Low) | Medium | High | 6h | Future |

*Blocked by CallRuntimeFunction implementation requirement

## Detailed Implementation Specifications

### 1. NSBundle Formatter (Priority 1)

**Target Display Format:**
```
NSBundle(path="/usr/local/lib/GNUstep/Libraries/gnustep-base", version="1.29", loaded=YES)
NSBundle(path="/invalid/path", version=nil, loaded=NO)  
NSBundle(nil)
```

**Implementation Approach:**
- **File:** `GNUstepBundleFormatters.cpp/h`
- **Method:** Direct memory reading from exposed instance variables
- **Key Fields:**
  - `_path` (NSString*) - Bundle path
  - `_frameworkVersion` (NSString*) - Framework version
  - `_codeLoaded` (BOOL) - Whether code is loaded
  - `_bundleType` (unsigned) - Bundle type

**Memory Layout (GNUstep):**
```cpp
struct NSBundle_GNUstep {
    // NSObject header
    void* isa;
    // NSBundle instance variables  
    NSString* _path;                    // Offset: 8
    NSMutableArray* _bundleClasses;     // Offset: 16
    Class _principalClass;              // Offset: 24
    NSDictionary* _infoDict;           // Offset: 32
    NSMutableDictionary* _localizations; // Offset: 40
    unsigned _bundleType;               // Offset: 48
    BOOL _codeLoaded;                  // Offset: 52
    unsigned _version;                  // Offset: 56
    NSString* _frameworkVersion;       // Offset: 64
};
```

**Error Handling:**
- Null objects: `NSBundle(nil)`
- Invalid paths: Check if `_path` is nil
- Memory read failures: `NSBundle(<invalid>)`

### 2. NSProcessInfo Formatter (Priority 2)

**Target Display Format:**
```
NSProcessInfo(name="test_app", pid=1234, args=["arg1", "arg2"])
NSProcessInfo(name="daemon", pid=567, uptime=3600s)
NSProcessInfo(nil)
```

**Implementation Approach:**
- **File:** `GNUstepProcessInfoFormatters.cpp/h`  
- **Method:** CallRuntimeFunction for method invocation (NO exposed ivars)
- **Required Methods:**
  - `processName` → NSString*
  - `processIdentifier` → int
  - `arguments` → NSArray*
  - `systemUptime` → NSUInteger (if available)

**Dependency:** Requires working `CallRuntimeFunction` implementation
**Status:** **BLOCKED** until CallRuntimeFunction is implemented

### 3. NSScanner Formatter (Priority 3)

**Target Display Format:**
```
NSScanner(string="Hello 123 World", position=6, remaining="123 World")
NSScanner(string="", position=0, remaining="")
NSScanner(nil)
```

**Implementation Approach:**
- **File:** `GNUstepScannerFormatters.cpp/h`
- **Method:** Direct memory reading from exposed instance variables
- **Key Fields:**
  - `_string` (NSString*) - String being scanned
  - `_scanLocation` (NSUInteger) - Current scan position
  - Calculated: remaining string = `[_string substringFromIndex:_scanLocation]`

**Memory Layout (GNUstep):**
```cpp
struct NSScanner_GNUstep {
    void* isa;
    NSString* _string;                  // Offset: 8
    NSCharacterSet* _charactersToBeSkipped; // Offset: 16
    void* _skipImp;                     // Offset: 24
    NSDictionary* _locale;              // Offset: 32  
    NSUInteger _scanLocation;           // Offset: 40
    unichar _decimal;                   // Offset: 48
    BOOL _caseSensitive;               // Offset: 50
    BOOL _isUnicode;                   // Offset: 51
};
```

**Special Logic:**
- Calculate remaining string length: `_string.length - _scanLocation`
- Handle scan position beyond string length gracefully

### 4. NSLocale Formatter (Priority 4)  

**Target Display Format:**
```
NSLocale(identifier="en_US", currency="USD", language="English")
NSLocale(identifier="fr_FR", currency="EUR", language="French")
NSLocale(nil)
```

**Implementation Approach:**
- **File:** `GNUstepLocaleFormatters.cpp/h`
- **Method:** Direct memory reading from exposed instance variables
- **Key Fields:**
  - `_localeId` (NSString*) - Locale identifier (primary)
  - `_components` (NSMutableDictionary*) - Locale component dictionary
  - Extract: currency, language from components if available

**Memory Layout (GNUstep):**
```cpp
struct NSLocale_GNUstep {
    void* isa;
    NSString* _localeId;               // Offset: 8
    NSMutableDictionary* _components;  // Offset: 16
};
```

**Component Analysis:**
- Primary: Display locale identifier
- Secondary: Extract currency code and language if components available
- Fallback: Show only identifier if components unavailable

### 5. NSUserDefaults Formatter (Priority 5 - Future)

**Target Display Format:**
```
NSUserDefaults(domains=3, keys=42, database="/Users/.../Preferences/.plist")  
NSUserDefaults(domains=1, keys=0, database="<memory>")
NSUserDefaults(nil)
```

**Implementation Approach:**
- **File:** `GNUstepUserDefaultsFormatters.cpp/h`
- **Method:** Complex memory reading + dictionary analysis
- **Key Fields:**
  - `_searchList` → domain count
  - `_persDomains` → persistent domain count
  - `_defaultsDatabase` → database file path
  - `_changedDomains` → changed status

**Complexity Factors:**
- Multiple nested dictionaries
- Domain counting logic
- Changed status calculation
- Database path extraction

## File Structure Plan

```
lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/
├── GNUstepBundleFormatters.cpp       (NEW)
├── GNUstepBundleFormatters.h         (NEW)  
├── GNUstepScannerFormatters.cpp      (NEW)
├── GNUstepScannerFormatters.h        (NEW)
├── GNUstepLocaleFormatters.cpp       (NEW) 
├── GNUstepLocaleFormatters.h         (NEW)
├── GNUstepProcessInfoFormatters.cpp  (NEW - blocked)
├── GNUstepProcessInfoFormatters.h    (NEW - blocked)
├── GNUstepUserDefaultsFormatters.cpp (FUTURE)
├── GNUstepUserDefaultsFormatters.h   (FUTURE)
└── GNUstepFormattersRegistry.cpp     (UPDATE)
```

## Implementation Phases

### Phase 1: Direct Memory Access Formatters (Week 1)
1. **NSBundle** - 2 hours
2. **NSScanner** - 2 hours  
3. **NSLocale** - 2 hours
4. **Registry Updates** - 1 hour
5. **Testing** - 1 hour

**Total: 8 hours**

### Phase 2: Method-Based Formatters (Blocked)
1. **NSProcessInfo** - 4 hours (after CallRuntimeFunction implemented)

### Phase 3: Complex Formatters (Future)
1. **NSUserDefaults** - 6 hours (low priority)

## Testing Strategy

### Test Cases per Formatter
1. **Valid objects** with typical data
2. **Nil objects**
3. **Invalid/corrupted objects** 
4. **Edge cases** (empty strings, invalid locales, etc.)
5. **Memory corruption scenarios**

### Validation Criteria
- **Performance:** <50ms response time per formatter
- **Safety:** No crashes on invalid input
- **Accuracy:** Correctly extract and display key information
- **UX:** Clear, concise display format

## Integration Requirements

### Registry Updates
Add to `GNUstepFormattersRegistry::RegisterFormatters()`:
```cpp
// Phase 1 formatters
RegisterNSBundleFormatters(category);
RegisterNSScannerFormatters(category); 
RegisterNSLocaleFormatters(category);

// Phase 2 formatters (when unblocked)
RegisterNSProcessInfoFormatters(category);
```

### CMakeLists.txt Updates
Add new source files to the build system.

## Success Metrics

1. **Developer Experience:** Foundation objects show meaningful info instead of raw pointers
2. **Debugging Efficiency:** Faster inspection of bundle paths, scan state, locale settings
3. **Code Coverage:** 90%+ test coverage for new formatters
4. **Performance:** Sub-50ms formatter execution time
5. **Robustness:** Zero crashes on production codebases

## Conclusion

**Immediate Impact:** Phase 1 will deliver 3 new formatters (NSBundle, NSScanner, NSLocale) in approximately 8 hours of development effort. These cover the most common Foundation debugging scenarios.

**Long-term Value:** NSProcessInfo formatter provides high value but requires CallRuntimeFunction implementation first.

**Recommendation:** Implement Phase 1 formatters immediately as they provide significant debugging value with minimal complexity and no dependencies on blocked functionality.