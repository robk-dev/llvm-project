# Epic 006: Foundation Formatters

## Overview
Implement comprehensive formatters for essential GNUstep Foundation classes (NSNumber, NSDate, NSData, NSURL, NSError, NSValue) to provide meaningful object representations during debugging sessions, completing the core Foundation debugging experience.

## Business Value
- **Complete Foundation coverage**: Debug all essential Foundation types effectively
- **Data comprehension**: Understand complex data types at a glance
- **Bug identification**: Quickly spot incorrect values, dates, and URLs
- **Apple ecosystem parity**: Match Xcode's Foundation object debugging capabilities

## Current Status: 5% Complete

### ✅ Completed
- Foundation formatter infrastructure framework
- Basic type registration patterns established
- Integration points with existing formatter system

### 🔄 In Progress  
- NSNumber value extraction and formatting
- NSDate timestamp parsing and display
- Foundation type detection and classification

### 📋 Planned
- Complete NSNumber formatting (integers, floats, booleans)
- NSDate human-readable timestamp display
- NSData hex dump and summary visualization
- NSURL component breakdown and validation
- NSError detailed error information display
- NSValue geometric and custom type unwrapping
- Performance optimization for complex Foundation types

## Technical Scope

### Foundation Types Supported

1. **NSNumber** - Numeric value wrapper
   - Integer display: "42" (int), "42" (NSInteger)
   - Float display: "3.14159" (float), "2.71828" (double)
   - Boolean display: "YES" or "NO" (BOOL)
   - Type annotation: Show underlying numeric type

2. **NSDate** - Date and time representation
   - Human readable: "2025-08-07 14:30:45 +0000"
   - Relative display: "2 hours ago", "in 3 days"
   - Timestamp: Unix timestamp for precision debugging
   - Timezone handling: Proper timezone conversion and display

3. **NSData** - Binary data container
   - Size summary: "1024 bytes", "2.5 KB", "1.2 MB"
   - Hex preview: First 32 bytes in hex format
   - ASCII preview: Printable characters where applicable
   - Data integrity: Corruption detection and validation

4. **NSURL** - URL resource locator
   - Full URL: Complete URL string display
   - Component breakdown: scheme, host, path, query, fragment
   - Validation status: Valid/invalid URL indication
   - File URL handling: Local file path extraction

5. **NSError** - Error information container
   - Error message: Localized description display
   - Error details: Domain, code, and user info
   - Nested errors: Underlying error chain display
   - Debugging aids: Stack trace hints when available

6. **NSValue** - Generic value wrapper
   - Geometric types: CGRect, CGPoint, CGSize, NSRange
   - Custom types: User-defined structures and objects
   - Type identification: Show wrapped value type
   - Value unwrapping: Extract and display wrapped content

### Advanced Features
- **Localization Support**: Respect user locale for date/number formatting
- **Custom Formatters**: Plugin system for domain-specific NSValue types
- **Memory Efficiency**: Lazy loading for large NSData objects
- **Cross-References**: Show relationships between Foundation objects

### Key Files
- **Number Formatters**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepNumberFormatters.cpp`
- **Date Formatters**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDateFormatters.cpp`
- **Data Formatters**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDataFormatters.cpp`
- **URL Formatters**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepURLFormatters.cpp`
- **Foundation Registry**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepFoundationFormatters.cpp`

## Dependencies
- **Epic 001**: Core runtime foundation (ISA resolution)
- **Epic 002**: Object introspection capabilities
- **Epic 004**: String formatters (for NSError descriptions, NSURL strings)
- **GNUstep Base**: Understanding of Foundation class internal structures
- **LLDB APIs**: TypeSummary and ValueObject systems

## Acceptance Criteria

### Must Have

#### NSNumber
1. **Numeric Value Display**
   - Integer values: Show actual number, not pointer
   - Floating point: Proper precision and scientific notation
   - Boolean values: Display as "YES"/"NO", not 1/0
   - Type information: Indicate underlying C type when relevant

2. **Edge Case Handling**
   - NaN and infinity values: Clear indication of special values
   - Zero handling: Distinguish between different zero types
   - Range validation: Detect and report out-of-range values
   - Performance: <2ms formatting time for any NSNumber

#### NSDate
1. **Date/Time Display**
   - ISO format: "2025-08-07T14:30:45Z" for precision
   - Readable format: "August 7, 2025 at 2:30:45 PM GMT"
   - Relative time: "2 hours ago" for recent dates
   - Timezone awareness: Proper timezone conversion and display

2. **Special Cases**
   - Distant past/future: Handle NSDate extreme values
   - Invalid dates: Detect and report malformed date objects
   - Precision: Millisecond accuracy when relevant
   - Performance: <5ms formatting time for any NSDate

#### NSData
1. **Data Summary**
   - Size display: Human-readable size (bytes, KB, MB, GB)
   - Content preview: First 16-32 bytes in hex format
   - ASCII preview: Show printable characters when applicable
   - Type detection: Identify common data formats (image, text, etc.)

2. **Performance and Safety**
   - Large data handling: Efficient preview without loading entire data
   - Memory safety: Bounds checking for data access
   - Corruption detection: Identify and report damaged NSData objects
   - Performance: <10ms formatting for data up to 1MB

### Should Have

#### NSURL
1. **URL Component Display**
   - Full URL string display
   - Component breakdown: scheme, host, port, path, query, fragment
   - Validation: Indicate whether URL is well-formed
   - File URLs: Show local file path clearly

#### NSError
1. **Comprehensive Error Information**
   - Error message: Localized description
   - Error context: Domain and error code
   - User info: Key-value pairs from userInfo dictionary
   - Error chain: Show underlying errors when present

#### NSValue
1. **Geometric Type Support**
   - CGRect: "{x, y, width, height}" format
   - CGPoint: "{x, y}" format
   - CGSize: "{width, height}" format
   - NSRange: "{location, length}" format

### Could Have
1. **Advanced Features**
   - Custom NSValue type detection and formatting
   - Localized number and date formatting
   - Interactive data exploration for large NSData objects
   - Cross-reference detection between Foundation objects

## Implementation Strategy

### Phase 1: NSNumber Foundation (Week 1)
1. **Basic Number Formatting**
   - Implement integer value extraction and display
   - Add floating-point formatting with proper precision
   - Handle boolean NSNumber special cases
   - Add basic type annotation

2. **Number Edge Cases**
   - NaN and infinity detection and display
   - Zero value special handling
   - Range validation and error reporting

### Phase 2: NSDate and NSData (Week 2)
1. **Date Formatting**
   - ISO timestamp display implementation
   - Human-readable date formatting
   - Timezone handling and conversion
   - Relative time calculation for recent dates

2. **Data Summary**
   - Size calculation and human-readable display
   - Hex dump generation for data preview
   - ASCII preview for text-like data
   - Large data handling optimization

### Phase 3: NSURL and NSError (Week 3)
1. **URL Formatting**
   - Complete URL string display
   - Component parsing and breakdown
   - URL validation and status indication
   - File URL special handling

2. **Error Information**
   - Error message extraction and display
   - Domain and code information
   - User info dictionary formatting
   - Error chain traversal and display

### Phase 4: NSValue and Polish (Week 4)
1. **Value Unwrapping**
   - Geometric type detection and formatting
   - Custom value type handling
   - Type identification and annotation
   - Value extraction safety and validation

2. **Integration and Optimization**
   - Performance optimization across all formatters
   - Memory usage optimization
   - Integration testing with complex Foundation usage
   - Cross-platform compatibility validation

## Risk Assessment

### High Risk
- **Binary Compatibility**: Foundation object layouts may vary across GNUstep versions
- **Floating Point Precision**: Accurate representation of floating-point values
- **Date/Time Complexity**: Timezone handling and calendar calculations

### Medium Risk
- **Large Data Performance**: NSData formatting for multi-megabyte objects
- **URL Parsing Complexity**: Handling malformed or edge-case URLs
- **Memory Management**: Avoiding leaks during Foundation object introspection

### Low Risk
- **Basic Number Formatting**: Integer and simple float display
- **String Integration**: Leveraging existing string formatter infrastructure
- **Type Registration**: LLDB formatter registration is well-understood

### Mitigation Strategies
- Start with simple integer NSNumber formatting, add complexity incrementally
- Implement robust bounds checking and timeout mechanisms
- Extensive testing across GNUstep versions and platforms
- Performance monitoring and early optimization
- Fallback to raw object display if Foundation formatting fails

## Tasks Breakdown

### [01_NSNumber_Basic_Formatting](../tasks/006_FOUNDATION_FORMATTERS/01_NSNumber_Basic_Formatting.md)
**Priority**: P0 (Critical)  
**Effort**: 3 days  
**Description**: Implement basic NSNumber value extraction and display

### [02_NSNumber_Advanced_Types](../tasks/006_FOUNDATION_FORMATTERS/02_NSNumber_Advanced_Types.md)
**Priority**: P1 (High)  
**Effort**: 2 days  
**Description**: Handle floating-point, boolean, and edge case NSNumber values

### [03_NSDate_Formatting](../tasks/006_FOUNDATION_FORMATTERS/03_NSDate_Formatting.md)
**Priority**: P1 (High)  
**Effort**: 4 days  
**Description**: Implement NSDate timestamp parsing and human-readable display

### [04_NSData_Summary_Display](../tasks/006_FOUNDATION_FORMATTERS/04_NSData_Summary_Display.md)
**Priority**: P1 (High)  
**Effort**: 3 days  
**Description**: NSData size summary and content preview functionality

### [05_NSURL_Component_Display](../tasks/006_FOUNDATION_FORMATTERS/05_NSURL_Component_Display.md)
**Priority**: P2 (Medium)  
**Effort**: 3 days  
**Description**: NSURL parsing and component breakdown display

### [06_NSError_Information_Display](../tasks/006_FOUNDATION_FORMATTERS/06_NSError_Information_Display.md)
**Priority**: P2 (Medium)  
**Effort**: 3 days  
**Description**: Comprehensive NSError information extraction and display

### [07_NSValue_Geometric_Types](../tasks/006_FOUNDATION_FORMATTERS/07_NSValue_Geometric_Types.md)
**Priority**: P2 (Medium)  
**Effort**: 2 days  
**Description**: NSValue unwrapping for CGRect, CGPoint, NSRange, etc.

### [08_Performance_Optimization](../tasks/006_FOUNDATION_FORMATTERS/08_Performance_Optimization.md)
**Priority**: P2 (Medium)  
**Effort**: 2 days  
**Description**: Optimize performance and memory usage across all Foundation formatters

## Testing Strategy

### Unit Tests
- NSNumber value extraction accuracy across all numeric types
- NSDate formatting correctness for various date ranges and timezones
- NSData summary generation and preview functionality
- NSURL component parsing for valid and invalid URLs
- NSError information extraction completeness
- NSValue unwrapping for all supported geometric types

### Integration Tests
- Foundation formatters in real GNUstep applications
- Performance testing with large Foundation objects
- Memory leak detection during Foundation object formatting
- Cross-platform compatibility (Linux, BSD, Windows)

### Edge Case Testing
- NSNumber with NaN, infinity, and extreme values
- NSDate with distant past/future and invalid dates
- NSData with zero-length, huge, and corrupted data
- NSURL with malformed, relative, and edge-case URLs
- NSError with complex error chains and missing information
- NSValue with custom types and invalid data

## Definition of Done

- [ ] NSNumber shows actual numeric values, not pointer addresses
- [ ] Integer, float, and boolean NSNumbers display correctly
- [ ] NSDate shows human-readable timestamps with timezone info
- [ ] NSData displays size summary and content preview
- [ ] NSURL shows complete URL string and component breakdown
- [ ] NSError displays error message, domain, code, and user info
- [ ] NSValue unwraps and displays geometric types (CGRect, etc.)
- [ ] Performance targets met (<10ms for any Foundation object)
- [ ] Memory safety validated with corrupted Foundation objects
- [ ] Integration tests pass with real GNUstep applications
- [ ] No regressions in existing formatter functionality
- [ ] Code review completed and approved
- [ ] Documentation updated with Foundation formatter examples

## Success Metrics

### Functional
- **Coverage**: 100% of core Foundation types supported
- **Accuracy**: 99%+ correct value extraction and display
- **Safety**: 0 crashes during Foundation object formatting

### Performance
- **Speed**: <2ms NSNumber, <5ms NSDate, <10ms NSData formatting
- **Memory**: <1MB additional memory for Foundation object debugging
- **Scalability**: Efficient handling of large NSData objects (100MB+)

### Developer Experience
- **Value Recognition**: 90% improvement in identifying Foundation object values
- **Debugging Speed**: 60% reduction in time to understand Foundation object state
- **Error Diagnosis**: 75% improvement in NSError-related debugging efficiency

---

*Epic Owner*: Development Team  
*Created*: August 2025  
*Target Completion*: Q2 2025  
*Dependencies*: Epic 001 (Core Runtime), Epic 002 (Object Introspection), Epic 004 (String Formatters)
