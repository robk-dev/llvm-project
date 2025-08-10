# Missing Foundation Formatters Analysis

## Test Program Results

Based on the test program execution, here are the current behaviors:

### Current LLDB Behavior (Without Formatters)

**NSBundle:**
- Main bundle: `0x555555833458` (raw pointer)
- Foundation bundle: `(nil)` (failed to load path)  
- Invalid bundle: `(nil)` (correctly returns nil for invalid paths)

**NSProcessInfo:**
- Process info: `0x555555617598` (raw pointer)
- Successfully shows process name: "test_missing_foundation"
- Successfully shows PID: varying process ID

**NSUserDefaults:**
- Standard defaults: `0x5555558f0eb8` (raw pointer)
- Custom defaults: `0x55555584ddf8` (raw pointer)  
- Both instances created successfully

**NSLocale:**  
- Current locale: `0x555555963f78` (raw pointer)
- System locale: `0x555555963f78` (same as current)
- US locale: `0x5555559640c8` (raw pointer)
- French locale: `0x5555559728f8` (raw pointer)
- Invalid locale: `0x5555559da4b8` (doesn't return nil for invalid identifier)

**NSScanner:**
- String scanner: `0x5555559da0b8` (raw pointer)
- Number scanner: `0x5555559da168` (raw pointer)
- Empty scanner: `0x5555559da208` (raw pointer)
- All scanners functional (scanning operations work correctly)

## GNUstep Memory Layout Analysis

### NSBundle Structure
```objc
@interface NSBundle : NSObject {
  NSString       *_path;              // Bundle path - HIGH VALUE
  NSMutableArray *_bundleClasses;     // Classes in bundle
  Class          _principalClass;     // Main class
  NSDictionary   *_infoDict;         // Info.plist contents - HIGH VALUE  
  NSMutableDictionary *_localizations; // Localizations
  unsigned       _bundleType;         // Bundle type
  BOOL           _codeLoaded;         // Code loaded flag
  unsigned       _version;            // Version
  NSString       *_frameworkVersion; // Framework version - HIGH VALUE
}
```

### NSProcessInfo Structure
- **No exposed instance variables** (using private implementation)
- Must use method calls for process name, PID, arguments, environment
- Cannot access internal state directly

### NSUserDefaults Structure  
```objc
@interface NSUserDefaults : NSObject {
@private
  NSMutableArray      *_searchList;     // Search domains - HIGH VALUE
  NSMutableDictionary *_persDomains;    // Persistent domains - HIGH VALUE
  NSMutableDictionary *_tempDomains;    // Volatile domains  
  NSMutableArray      *_changedDomains; // Changed domains
  NSDictionary        *_dictionaryRep;  // Cached representation
  NSString            *_defaultsDatabase; // Database path
  NSDate              *_lastSync;       // Last sync time
  NSRecursiveLock     *_lock;          // Thread lock
  NSDistributedLock   *_fileLock;      // File lock
}
```

### NSLocale Structure
```objc  
@interface NSLocale : NSObject {
@private
  NSString            *_localeId;      // Locale identifier - HIGH VALUE
  NSMutableDictionary *_components;    // Locale components - HIGH VALUE
}
```

### NSScanner Structure
```objc
@interface NSScanner : NSObject {
@private
  NSString       *_string;                 // String being scanned - HIGH VALUE
  NSCharacterSet *_charactersToBeSkipped; // Skip characters
  BOOL (*_skipImp)(NSCharacterSet*, SEL, unichar); // Skip implementation
  NSDictionary   *_locale;               // Locale for scanning
  NSUInteger     _scanLocation;          // Current position - HIGH VALUE
  unichar        _decimal;               // Decimal separator
  BOOL           _caseSensitive;         // Case sensitivity
  BOOL           _isUnicode;            // Unicode flag
}
```

## Implementation Requirements

### 1. NSBundle Formatter (High Priority)
**Debugging Value:** Very High - Essential for framework/bundle debugging
**Implementation Approach:** Memory reading from instance variables
**Display Format:** `NSBundle(path="/usr/local/lib/...", version="1.2.3", loaded=YES)`
**Key Fields:**
- `_path`: Bundle path (most important)
- `_frameworkVersion`: Version string
- `_codeLoaded`: Loading status
- `_bundleType`: Bundle type

### 2. NSProcessInfo Formatter (High Priority)  
**Debugging Value:** High - Process information and environment debugging
**Implementation Approach:** Method calls via expression evaluator (no exposed ivars)
**Display Format:** `NSProcessInfo(name="myapp", pid=1234, args=["arg1", "arg2"])`
**Key Methods to Call:**
- `processName`: Process name
- `processIdentifier`: Process ID  
- `arguments`: Command line arguments
- `operatingSystemVersionString`: OS version (if available)

### 3. NSScanner Formatter (Medium Priority)
**Debugging Value:** Medium - Text parsing debugging  
**Implementation Approach:** Memory reading from instance variables
**Display Format:** `NSScanner(string="Hello World", position=5, remaining="World")`
**Key Fields:**
- `_string`: String being scanned
- `_scanLocation`: Current scan position  
- Calculated remaining string: `[_string substringFromIndex:_scanLocation]`

### 4. NSLocale Formatter (Medium Priority)
**Debugging Value:** Medium - Internationalization debugging
**Implementation Approach:** Memory reading from instance variables
**Display Format:** `NSLocale(identifier="en_US", currency="USD", language="English")`  
**Key Fields:**
- `_localeId`: Locale identifier (primary)
- `_components`: Dictionary of locale components
- Key components: currency code, language, country

### 5. NSUserDefaults Formatter (Low Priority)
**Debugging Value:** Low-Medium - Configuration debugging, but complex
**Implementation Approach:** Memory reading + domain analysis
**Display Format:** `NSUserDefaults(domains=3, keys=42, changed=YES)`
**Key Fields:**  
- `_searchList`: Count of search domains
- `_persDomains`: Count of persistent domain keys
- `_changedDomains`: Changed domains count
- Database path from `_defaultsDatabase`

## Priority Ranking

1. **NSBundle** - Critical for framework debugging, simple implementation
2. **NSProcessInfo** - High value, requires method calls (medium complexity)  
3. **NSScanner** - Useful for parsing debugging, simple implementation
4. **NSLocale** - Moderate value, simple implementation
5. **NSUserDefaults** - Complex implementation, moderate debugging value

## Implementation Complexity Analysis

### Easy (1-2 hours)
- NSBundle: Direct ivar access, static display format
- NSLocale: Direct ivar access, minimal processing  
- NSScanner: Direct ivar access, simple string manipulation

### Medium (3-4 hours)
- NSProcessInfo: Requires CallRuntimeFunction implementation for method calls

### Complex (5-8 hours)  
- NSUserDefaults: Multiple dictionaries, domain analysis, count calculations

## Next Steps

1. Implement NSBundle formatter first (highest priority, easiest implementation)
2. Test NSProcessInfo method calling requirements  
3. Implement NSScanner and NSLocale formatters
4. Consider NSUserDefaults for later implementation phase