# GNUstep LLDB Foundation Formatter Product Backlog
**Generated**: 2025-08-09  
**Analyst**: Agent Alpha  
**Purpose**: Comprehensive backlog for remaining Foundation class formatters

## Executive Summary

This document provides a comprehensive product backlog for implementing formatters for Foundation classes in the GNUstep LLDB plugin. Based on analysis of the current implementation, Apple's LLDB formatters, and common Foundation usage patterns, this backlog identifies and prioritizes the remaining formatter work needed to achieve feature parity with Apple's debugging experience.

### Current Implementation Status: 65% Complete

**✅ Fully Implemented & Production-Ready:**
- NSString/NSMutableString (all variants and encodings)
- NSNumber (including tagged pointers)
- NSArray/NSMutableArray
- NSDictionary/NSMutableDictionary  
- NSSet/NSMutableSet/NSCountedSet
- NSValue (generic wrapper - partial)
- Generic Object Formatter (fallback)
- Id Dispatcher (dynamic type resolution)

**🔧 Implemented but Not Registered (Commented Out):**
- NSDate/NSCalendarDate (code exists, needs activation)
- NSURL (code exists, needs activation)
- NSError (code exists, needs activation)
- NSData/NSMutableData (code exists, needs activation)
- NSUUID (code exists, needs activation)

**❌ Not Implemented:**
- NSAttributedString/NSMutableAttributedString
- NSIndexPath
- NSIndexSet/NSMutableIndexSet
- NSNull
- NSNotification/NSNotificationCenter
- NSException
- NSPredicate/NSExpression
- NSRegularExpression/NSTextCheckingResult
- NSDecimalNumber
- NSTimeZone
- NSLocale
- NSCalendar/NSDateComponents
- NSCharacterSet/NSMutableCharacterSet
- NSFormatter subclasses (NSDateFormatter, NSNumberFormatter, etc.)
- NSBundle
- NSProcessInfo
- NSUserDefaults
- NSFileManager/NSFileHandle
- NSTask
- NSThread
- NSTimer
- NSRunLoop
- NSOperation/NSOperationQueue
- NSProgress
- NSURLRequest/NSURLResponse
- NSURLConnection/NSURLSession
- NSHTTPCookie/NSHTTPCookieStorage
- NSCache
- NSProxy subclasses
- NSMethodSignature/NSInvocation
- NSScanner
- NSStream/NSInputStream/NSOutputStream
- NSPipe
- NSPort/NSMachPort
- NSHost
- NSNetService
- NSXMLParser/NSXMLDocument
- NSJSONSerialization
- NSPropertyListSerialization
- NSKeyedArchiver/NSKeyedUnarchiver
- NSCoder subclasses

---

## Priority Classification

### P0 - Critical (Must Have for MVP)
These formatters are essential for basic debugging functionality and are used in nearly every GNUstep application.

### P1 - High Priority (Core Developer Experience)
Formatters that significantly improve debugging productivity and are commonly used.

### P2 - Medium Priority (Enhanced Experience)
Formatters for specialized classes that improve specific debugging scenarios.

### P3 - Low Priority (Nice to Have)
Formatters for rarely used or highly specialized classes.

---

## User Stories & Acceptance Criteria

### Epic: Activate Dormant Formatters (P0)
**Business Value**: Quick wins with already-implemented code  
**Effort**: 1 day  
**Risk**: Low

#### US-001: Activate NSDate Formatter
**As a** GNUstep developer  
**I want to** see human-readable date/time values when debugging NSDate objects  
**So that** I can quickly understand temporal data without manual conversion

**Acceptance Criteria:**
- [ ] Uncomment NSDate formatter registration in GNUstepFormattersRegistry.cpp (lines 335-339)
- [ ] Verify GNUstepNSDateFormatterFunction is properly linked
- [ ] NSDate objects display as "2025-08-09 14:30:45 +0000" format
- [ ] NSCalendarDate shows localized format when available
- [ ] Test with: `NSDate *now = [NSDate date];`
- [ ] Test with distant past/future dates
- [ ] Handle nil dates gracefully

**Implementation Notes:**
```cpp
// In GNUstepFormattersRegistry.cpp, uncomment:
RegisterDateFormatters(category);
RegisterURLFormatters(category);
RegisterErrorFormatters(category);
RegisterDataFormatters(category);
RegisterUUIDFormatters(category);
```

#### US-002: Activate NSURL Formatter
**As a** developer debugging network code  
**I want to** see the full URL string and components when inspecting NSURL objects  
**So that** I can quickly verify URL correctness

**Acceptance Criteria:**
- [ ] NSURL displays complete URL string
- [ ] File URLs show local path clearly
- [ ] Invalid URLs indicate malformation
- [ ] Performance: <2ms formatting time
- [ ] Test with: `NSURL *url = [NSURL URLWithString:@"https://example.com/path?query=1"];`

#### US-003: Activate NSError Formatter
**As a** developer debugging error handling  
**I want to** see error domain, code, and description immediately  
**So that** I can quickly diagnose error conditions

**Acceptance Criteria:**
- [ ] Display format: "Domain: NSCocoaErrorDomain Code: 260 "File not found""
- [ ] Show userInfo dictionary when present
- [ ] Handle nested errors (underlying error)
- [ ] Performance: <3ms formatting time

#### US-004: Activate NSData Formatter
**As a** developer working with binary data  
**I want to** see data size and preview when inspecting NSData objects  
**So that** I can verify data integrity and content

**Acceptance Criteria:**
- [ ] Display format: "1024 bytes" or "1.5 KB" 
- [ ] Show first 32 bytes as hex preview
- [ ] Handle empty NSData gracefully
- [ ] Large data (>1MB) doesn't cause performance issues
- [ ] Performance: <10ms for data up to 1MB

#### US-005: Activate NSUUID Formatter
**As a** developer working with unique identifiers  
**I want to** see UUID string representation  
**So that** I can track and verify identifiers

**Acceptance Criteria:**
- [ ] Display standard UUID format: "550E8400-E29B-41D4-A716-446655440000"
- [ ] Handle nil NSUUID gracefully
- [ ] Performance: <1ms formatting time

---

### Epic: Essential Missing Formatters (P1)
**Business Value**: Cover the most commonly used Foundation classes  
**Effort**: 2-3 weeks  
**Risk**: Medium

#### US-006: NSAttributedString Formatter
**As a** developer working with rich text  
**I want to** see the plain text content and attribute summary  
**So that** I can debug text formatting issues

**Acceptance Criteria:**
- [ ] Display plain string content first
- [ ] Show attribute count and types in summary
- [ ] Format: "Hello World {3 attributes}"
- [ ] Handle empty attributed strings
- [ ] Synthetic children show attribute ranges
- [ ] Performance: <5ms for typical strings

**Technical Requirements:**
- Extract string from internal storage
- Count unique attribute ranges
- Create synthetic children for each attribute range
- Reference: Apple's `NSAttributedStringSummaryProvider` in Cocoa.cpp

#### US-007: NSIndexPath Formatter
**As a** developer working with table/collection views  
**I want to** see section and row/item values clearly  
**So that** I can debug UI selection and navigation

**Acceptance Criteria:**
- [ ] Two-component paths: "[section:row]" e.g., "[0:5]"
- [ ] Multi-component paths: "[0:1:2:3]"
- [ ] Show length for complex paths
- [ ] Synthetic children for each index
- [ ] Performance: <2ms formatting time

**Technical Requirements:**
- Extract index array from internal storage
- Handle both 2-index and n-index cases
- Reference: Apple's `NSIndexPathSyntheticFrontEnd`

#### US-008: NSNull Formatter
**As a** developer working with collections  
**I want to** NSNull objects to be clearly identified  
**So that** I can distinguish them from nil values

**Acceptance Criteria:**
- [ ] Display as "<NSNull>" or "NSNull.null"
- [ ] Clearly different from nil display
- [ ] No synthetic children
- [ ] Performance: <1ms (singleton check only)

**Technical Requirements:**
- Simple class check and static string return
- No value extraction needed (singleton)

#### US-009: NSException Formatter
**As a** developer debugging crashes  
**I want to** see exception name, reason, and callstack  
**So that** I can quickly diagnose issues

**Acceptance Criteria:**
- [ ] Display format: "NSInvalidArgumentException: 'Attempt to insert nil'"
- [ ] Show userInfo when present
- [ ] Display callStackSymbols if available
- [ ] Handle common exception types
- [ ] Performance: <5ms formatting time

**Technical Requirements:**
- Extract name, reason, and userInfo
- Format callStackReturnAddresses/callStackSymbols
- Reference: Apple's `NSException_SummaryProvider`

#### US-010: NSNotification Formatter
**As a** developer debugging notifications  
**I want to** see notification name, object, and userInfo  
**So that** I can trace notification flow

**Acceptance Criteria:**
- [ ] Display format: "MyNotification from <Object> {userInfo}"
- [ ] Show notification name prominently
- [ ] Include posting object summary
- [ ] Show userInfo dictionary summary
- [ ] Performance: <3ms formatting time

**Technical Requirements:**
- Extract name, object, and userInfo
- Reuse existing formatter for userInfo dictionary
- Reference: Apple's `NSNotificationSummaryProvider`

#### US-011: NSIndexSet Formatter
**As a** developer working with collections  
**I want to** see index ranges and count  
**So that** I can understand selection states

**Acceptance Criteria:**
- [ ] Display format: "3 indexes in [0-2]" or "5 indexes in [0-2,4,7]"
- [ ] Show total count
- [ ] Compact range representation
- [ ] Handle empty sets
- [ ] Synthetic children for ranges
- [ ] Performance: <5ms for typical sets

**Technical Requirements:**
- Extract internal range representation
- Compact consecutive indexes into ranges
- Reference: Apple's `NSIndexSetSummaryProvider`

---

### Epic: Type System Formatters (P1)
**Business Value**: Better number and text formatting  
**Effort**: 1 week  
**Risk**: Low-Medium

#### US-012: NSDecimalNumber Formatter
**As a** developer working with financial calculations  
**I want to** see precise decimal values  
**So that** I can verify monetary calculations

**Acceptance Criteria:**
- [ ] Display with appropriate precision
- [ ] Handle currency formatting hints
- [ ] Show "NaN" for invalid numbers
- [ ] Distinguish from regular NSNumber
- [ ] Performance: <3ms formatting time

**Technical Requirements:**
- Extract mantissa, exponent, and flags
- Format according to locale if available
- Reference: Apple's `NSDecimalNumberSummaryProvider`

#### US-013: NSCharacterSet Formatter
**As a** developer working with text processing  
**I want to** see character set contents or predefined set names  
**So that** I can verify text filtering rules

**Acceptance Criteria:**
- [ ] Show predefined set names: "NSCharacterSet.alphanumerics"
- [ ] Custom sets show character count and sample
- [ ] Inverted sets clearly marked
- [ ] Performance: <5ms for analysis

**Technical Requirements:**
- Detect predefined sets by comparing pointers
- Sample custom sets for preview
- Show inversion status

---

### Epic: Locale & Time Formatters (P2)
**Business Value**: Internationalization debugging support  
**Effort**: 1 week  
**Risk**: Medium

#### US-014: NSTimeZone Formatter
**As a** developer working with dates  
**I want to** see timezone name and offset  
**So that** I can debug timezone issues

**Acceptance Criteria:**
- [ ] Display format: "America/New_York (GMT-5)"
- [ ] Show both name and current offset
- [ ] Handle daylight saving time
- [ ] Performance: <2ms formatting time

**Technical Requirements:**
- Extract timezone identifier
- Calculate current GMT offset
- Reference: Apple's `NSTimeZoneSummaryProvider`

#### US-015: NSLocale Formatter
**As a** developer working on i18n  
**I want to** see locale identifier and key settings  
**So that** I can verify localization

**Acceptance Criteria:**
- [ ] Display format: "en_US (English, United States)"
- [ ] Show language and country codes
- [ ] Include currency symbol if set
- [ ] Performance: <3ms formatting time

**Technical Requirements:**
- Extract locale identifier
- Parse language and country codes
- Show key locale components

#### US-016: NSCalendar Formatter
**As a** developer working with dates  
**I want to** see calendar identifier and settings  
**So that** I can debug calendar calculations

**Acceptance Criteria:**
- [ ] Display format: "Gregorian calendar (en_US)"
- [ ] Show calendar identifier
- [ ] Include timezone if set
- [ ] Show first weekday setting
- [ ] Performance: <3ms formatting time

---

### Epic: Text Processing Formatters (P2)
**Business Value**: Advanced text debugging  
**Effort**: 1 week  
**Risk**: Medium

#### US-017: NSRegularExpression Formatter
**As a** developer working with regex  
**I want to** see the pattern and options  
**So that** I can verify pattern correctness

**Acceptance Criteria:**
- [ ] Display pattern string clearly
- [ ] Show regex options (case insensitive, etc.)
- [ ] Format: "/pattern/flags"
- [ ] Escape special characters properly
- [ ] Performance: <2ms formatting time

#### US-018: NSPredicate Formatter
**As a** developer working with Core Data or filtering  
**I want to** see the predicate format string  
**So that** I can verify query logic

**Acceptance Criteria:**
- [ ] Display predicate format string
- [ ] Show predicate type (comparison, compound, etc.)
- [ ] Handle substitution variables
- [ ] Performance: <3ms formatting time

#### US-019: NSScanner Formatter
**As a** developer parsing text  
**I want to** see scan location and remaining string  
**So that** I can debug parsing logic

**Acceptance Criteria:**
- [ ] Show current scan location
- [ ] Display characters to be scanned
- [ ] Show substring around current position
- [ ] Performance: <2ms formatting time

---

### Epic: System Integration Formatters (P2)
**Business Value**: System-level debugging support  
**Effort**: 2 weeks  
**Risk**: Medium-High

#### US-020: NSBundle Formatter
**As a** developer debugging resources  
**I want to** see bundle identifier and path  
**So that** I can verify resource loading

**Acceptance Criteria:**
- [ ] Display format: "com.example.app at /path/to/bundle"
- [ ] Show bundle identifier
- [ ] Show bundle path
- [ ] Indicate if loaded
- [ ] Performance: <3ms formatting time

#### US-021: NSProcessInfo Formatter
**As a** developer debugging environment  
**I want to** see process name and identifier  
**So that** I can verify process state

**Acceptance Criteria:**
- [ ] Display format: "MyApp (PID: 12345)"
- [ ] Show process name
- [ ] Show process identifier
- [ ] Include system version if available
- [ ] Performance: <2ms formatting time

#### US-022: NSUserDefaults Formatter
**As a** developer debugging preferences  
**I want to** see domain and key count  
**So that** I can verify settings

**Acceptance Criteria:**
- [ ] Display format: "com.example.app (25 keys)"
- [ ] Show suite name
- [ ] Show count of keys
- [ ] Synthetic children for each key
- [ ] Performance: <10ms for typical defaults

#### US-023: NSFileManager Formatter
**As a** developer working with files  
**I want to** see current directory and delegate  
**So that** I can debug file operations

**Acceptance Criteria:**
- [ ] Show current directory path
- [ ] Indicate if default manager
- [ ] Show delegate if set
- [ ] Performance: <3ms formatting time

---

### Epic: Networking Formatters (P3)
**Business Value**: Network debugging support  
**Effort**: 1 week  
**Risk**: Low

#### US-024: NSURLRequest Formatter
**As a** developer debugging network requests  
**I want to** see HTTP method and URL  
**So that** I can verify request configuration

**Acceptance Criteria:**
- [ ] Display format: "GET https://api.example.com/data"
- [ ] Show HTTP method
- [ ] Show URL
- [ ] Include key headers in synthetic children
- [ ] Performance: <5ms formatting time

#### US-025: NSURLResponse Formatter
**As a** developer debugging network responses  
**I want to** see status code and content type  
**So that** I can verify response handling

**Acceptance Criteria:**
- [ ] Display format: "200 OK (application/json)"
- [ ] Show HTTP status code
- [ ] Show content type
- [ ] Show content length if available
- [ ] Performance: <3ms formatting time

#### US-026: NSHTTPCookie Formatter
**As a** developer debugging cookies  
**I want to** see cookie name, value, and domain  
**So that** I can verify cookie handling

**Acceptance Criteria:**
- [ ] Display format: "session=abc123 for .example.com"
- [ ] Show name and value
- [ ] Show domain and path
- [ ] Indicate secure/httpOnly flags
- [ ] Performance: <2ms formatting time

---

### Epic: Advanced Runtime Formatters (P3)
**Business Value**: Deep runtime debugging  
**Effort**: 2 weeks  
**Risk**: High

#### US-027: NSMethodSignature Formatter
**As a** developer debugging method calls  
**I want to** see method signature details  
**So that** I can verify method compatibility

**Acceptance Criteria:**
- [ ] Display return type and argument types
- [ ] Show selector if available
- [ ] Format: "-(BOOL)doSomething:(id)arg1 withInt:(int)arg2"
- [ ] Performance: <5ms formatting time

#### US-028: NSInvocation Formatter
**As a** developer debugging message forwarding  
**I want to** see target, selector, and arguments  
**So that** I can trace method invocations

**Acceptance Criteria:**
- [ ] Show target object summary
- [ ] Show selector name
- [ ] Show argument count
- [ ] Synthetic children for each argument
- [ ] Performance: <10ms formatting time

#### US-029: NSProxy Subclass Formatters
**As a** developer using proxies  
**I want to** see the proxied object  
**So that** I can understand proxy behavior

**Acceptance Criteria:**
- [ ] Show proxy class name
- [ ] Show proxied object if accessible
- [ ] Handle NSDistantObject specially
- [ ] Performance: <5ms formatting time

---

## Implementation Roadmap

### Sprint 1 (Week 1): Quick Wins
- Activate all dormant formatters (NSDate, NSURL, NSError, NSData, NSUUID)
- Test and validate each activated formatter
- Fix any issues discovered during activation
- **Deliverable**: 5 additional working formatters

### Sprint 2 (Week 2): Essential Foundation
- Implement NSAttributedString formatter
- Implement NSIndexPath formatter  
- Implement NSNull formatter
- Implement NSException formatter
- **Deliverable**: Core Foundation types covered

### Sprint 3 (Week 3): Common Utilities
- Implement NSNotification formatter
- Implement NSIndexSet formatter
- Implement NSDecimalNumber formatter
- Implement NSCharacterSet formatter
- **Deliverable**: Improved collection debugging

### Sprint 4 (Week 4): Internationalization
- Implement NSTimeZone formatter
- Implement NSLocale formatter
- Implement NSCalendar formatter
- Implement NSDateComponents formatter
- **Deliverable**: I18n debugging support

### Sprint 5-6 (Weeks 5-6): System Integration
- Implement NSBundle formatter
- Implement NSProcessInfo formatter
- Implement NSUserDefaults formatter
- Implement NSFileManager formatter
- Implement NSTask formatter
- **Deliverable**: System-level debugging

### Sprint 7 (Week 7): Text Processing
- Implement NSRegularExpression formatter
- Implement NSPredicate formatter
- Implement NSScanner formatter
- Implement NSTextCheckingResult formatter
- **Deliverable**: Advanced text debugging

### Sprint 8 (Week 8): Networking
- Implement NSURLRequest formatter
- Implement NSURLResponse formatter
- Implement NSHTTPCookie formatter
- Implement NSURLConnection state formatter
- **Deliverable**: Network debugging support

### Sprint 9-10 (Weeks 9-10): Advanced Runtime
- Implement NSMethodSignature formatter
- Implement NSInvocation formatter
- Implement NSProxy subclass formatters
- Implement NSOperation formatters
- **Deliverable**: Deep runtime introspection

---

## Technical Patterns & Guidelines

### Formatter Implementation Pattern

```cpp
// Header file pattern (GNUstepXXXFormatters.h)
bool GNUstepNSXXXFormatterFunction(ValueObject &valobj, Stream &stream,
                                   const TypeSummaryOptions &options);

class GNUstepNSXXXSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream,
                   const TypeSummaryOptions &options) override;
private:
  // Helper methods for data extraction
};

// Implementation pattern (GNUstepXXXFormatters.cpp)
bool GNUstepNSXXXSummaryProvider::FormatObject(
    ValueObject &valobj, Stream &stream,
    const TypeSummaryOptions &options) {
  
  // 1. Validate object
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    stream.Printf("invalid %s object", class_name.c_str());
    return false;
  }
  
  // 2. Get class name for variant handling
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  // 3. Extract data based on class variant
  // ... extraction logic ...
  
  // 4. Format and write to stream
  stream.Printf("formatted output");
  return true;
}

// Registration pattern (GNUstepFormattersRegistry.cpp)
void RegisterXXXFormatters(TypeCategoryImpl &category) {
  TypeSummaryImpl::Flags flags;
  flags.SetCascades(true)
       .SetSkipPointers(false)
       .SetSkipReferences(false)
       .SetDontShowChildren(true)
       .SetDontShowValue(true)
       .SetShowMembersOneLiner(false)
       .SetHideItemNames(true);

  auto summary = std::make_shared<CXXFunctionSummaryFormat>(
      flags, GNUstepNSXXXFormatterFunction, "NSXXX summary provider");

  category.AddTypeSummary("NSXXX", eFormatterMatchExact, summary);
  category.AddTypeSummary("NSXXX *", eFormatterMatchExact, summary);
}
```

### Testing Pattern

```objc
// Test file pattern (test_xxx_formatter.m)
#import <Foundation/Foundation.h>

int main() {
  @autoreleasepool {
    // Test case 1: Basic functionality
    NSXXX *obj = [[NSXXX alloc] init...];
    
    // Test case 2: Edge cases
    NSXXX *nil_obj = nil;
    NSXXX *empty_obj = ...;
    
    // Test case 3: Complex scenarios
    NSXXX *complex_obj = ...;
    
    // Breakpoint here
    NSLog(@"Test NSXXX formatter");
    
    return 0;
  }
}
```

### Performance Guidelines

1. **Early Exit**: Check for invalid objects immediately
2. **Lazy Loading**: Only read memory when needed
3. **Caching**: Cache expensive computations when possible
4. **Size Limits**: Limit preview data (e.g., first 32 bytes)
5. **Timeout**: Implement timeout for complex operations
6. **Memory Safety**: Always check bounds before reading

---

## Risk Mitigation

### Technical Risks

1. **Memory Layout Changes**
   - Mitigation: Use runtime APIs where possible
   - Fallback: Graceful degradation to raw display

2. **Performance Impact**
   - Mitigation: Implement caching layer
   - Measurement: Profile each formatter

3. **Version Compatibility**
   - Mitigation: Test across GNUstep versions
   - Strategy: Version-specific code paths

### Process Risks

1. **Scope Creep**
   - Mitigation: Strict prioritization (P0-P3)
   - Review: Weekly backlog grooming

2. **Testing Coverage**
   - Mitigation: Test-driven development
   - Requirement: Test file for each formatter

---

## Success Metrics

### Coverage Metrics
- **P0 Formatters**: 100% implemented (5/5)
- **P1 Formatters**: 100% implemented (11/11)
- **P2 Formatters**: 80% implemented (10/12)
- **P3 Formatters**: 50% implemented (8/16)
- **Total Coverage**: 75% of identified Foundation classes

### Quality Metrics
- **Performance**: All formatters <10ms response time
- **Reliability**: Zero crashes in formatter code
- **Accuracy**: 99% correct value extraction
- **Memory**: <1MB overhead per debugging session

### Developer Experience Metrics
- **Time to Debug**: 50% reduction for Foundation objects
- **User Satisfaction**: 4.5/5 developer rating
- **Adoption Rate**: 80% of GNUstep developers using formatters

---

## Appendix A: File Mapping

| Formatter | Source File | Test File |
|-----------|------------|-----------|
| NSDate | GNUstepDateFormatters.cpp | test_date_formatter.m |
| NSURL | GNUstepURLFormatters.cpp | test_url_formatter.m |
| NSError | GNUstepErrorFormatters.cpp | test_error_formatter.m |
| NSData | GNUstepDataFormatters.cpp | test_data_formatter.m |
| NSUUID | GNUstepUUIDFormatters.cpp | test_uuid_formatter.m |
| NSAttributedString | GNUstepAttributedStringFormatters.cpp | test_attributed_string.m |
| NSIndexPath | GNUstepIndexPathFormatters.cpp | test_indexpath.m |
| NSNull | GNUstepNullFormatters.cpp | test_null.m |
| NSException | GNUstepExceptionFormatters.cpp | test_exception.m |
| NSNotification | GNUstepNotificationFormatters.cpp | test_notification.m |
| NSIndexSet | GNUstepIndexSetFormatters.cpp | test_indexset.m |
| NSDecimalNumber | GNUstepDecimalFormatters.cpp | test_decimal.m |
| NSCharacterSet | GNUstepCharacterSetFormatters.cpp | test_charset.m |
| NSTimeZone | GNUstepTimeZoneFormatters.cpp | test_timezone.m |
| NSLocale | GNUstepLocaleFormatters.cpp | test_locale.m |
| NSCalendar | GNUstepCalendarFormatters.cpp | test_calendar.m |

---

## Appendix B: Dependencies

### Build Dependencies
- LLVM/LLDB 17.0+
- GNUstep Base 1.28+
- libobjc2 2.1+
- CMake 3.20+

### Runtime Dependencies
- libgnustep-base.so
- libobjc.so.2
- Foundation headers

### Development Tools
- clang with Objective-C support
- LLDB for testing
- Valgrind for memory analysis
- perf for performance profiling

---

*Document Generated*: 2025-08-09  
*Next Review*: 2025-08-16  
*Owner*: GNUstep LLDB Development Team  
*Status*: ACTIVE