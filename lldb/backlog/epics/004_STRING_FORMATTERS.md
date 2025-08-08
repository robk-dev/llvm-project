# Epic 004: String Formatters

## Overview
Implement comprehensive string formatting capabilities for GNUstep NSString and related classes, enabling developers to see actual string content instead of raw memory addresses during debugging sessions.

## Business Value
- **Immediate visible impact**: Developers see string content, not pointer addresses
- **Debugging efficiency**: 80% faster identification of string-related issues
- **Developer satisfaction**: Most requested debugging feature for GNUstep
- **Foundation for collections**: String formatting enables array/dictionary value display

## Current Status: 25% Complete

### ✅ Completed
- Formatter infrastructure and registration system in place
- `GNUstepStringFormatters.cpp` with complete architecture
- String content extraction logic implemented
- Type registration for NSString variants

### 🔄 In Progress  
- Formatter activation (currently disabled with TODO comment)
- LLDB API compatibility for DataVisualization system
- Integration testing with real NSString objects

### 📋 Planned
- UTF-8, UTF-16, and UTF-32 encoding support
- Mutable string handling (NSMutableString)
- Constant string optimization (@"literal" strings)
- Performance optimization for large strings
- Truncation and display formatting options

## Technical Scope

### String Types Supported
1. **NSString** - Immutable string objects
2. **NSMutableString** - Mutable string objects  
3. **NSConstantString** - Compile-time string literals (@"hello")
4. **__NSCFString** - Core Foundation bridged strings
5. **Custom String Subclasses** - User-defined NSString subclasses

### Encoding Support
- **UTF-8**: Primary encoding for display
- **UTF-16**: Native GNUstep internal encoding
- **UTF-32**: Wide character support
- **ASCII**: Optimized path for simple strings
- **Error Handling**: Invalid encoding detection and fallback

### Key Files
- **Primary Implementation**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.cpp`
- **Header Definitions**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.h`
- **Registry Integration**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepFormattersRegistry.cpp`
- **Test Cases**: `/home/robk/code/llvm-project/lldb/examples/string_test.m`

## Dependencies
- **Epic 001**: Core runtime foundation (ISA resolution)
- **Epic 002**: Object introspection capabilities
- **GNUstep Base**: Understanding of NSString memory layout
- **LLDB APIs**: DataVisualization and TypeSummary systems

## Acceptance Criteria

### Must Have
1. **Basic String Display**
   - Show actual string content for `po myString` commands
   - Handle empty strings ("") correctly
   - Display nil strings as "nil" or "(null)"
   - Support strings up to 1MB in size

2. **Encoding Correctness**
   - Properly decode UTF-8 encoded strings
   - Handle multi-byte characters correctly
   - Display non-printable characters as escape sequences
   - Preserve string boundaries (no buffer overruns)

3. **Type Coverage**
   - Format NSString objects correctly
   - Handle NSMutableString identically to NSString
   - Recognize NSConstantString (@"literal") objects
   - Support subclassed NSString types

4. **Error Handling**
   - Gracefully handle corrupted string objects
   - Detect and report invalid string pointers
   - Handle partially deallocated strings safely
   - Timeout for extremely large strings

### Should Have
1. **Display Options**
   - Truncate very long strings (>1000 chars) with "..."
   - Show string length in summary format: "Hello World" (11 chars)
   - Option to display raw bytes for debugging encoding issues
   - Escape sequence display for special characters

2. **Performance**
   - String content extraction in <5ms for typical strings
   - Lazy loading for very large strings
   - Caching of string content for repeated access
   - Memory-efficient display of partial content

3. **Advanced Features**
   - Support for NSAttributedString content extraction
   - Display string encoding information when relevant
   - Show string immutability status (mutable vs immutable)

### Could Have
1. **Developer Experience**
   - Configurable display length limits
   - Custom format specifiers for string display
   - Integration with IDE string visualization
   - Export functionality for large strings

## Implementation Strategy

### Phase 1: Core String Support (Week 1)
1. **Activate Formatter Registration**
   - Remove TODO comment in `GNUstepObjCRuntime::Initialize()`
   - Fix DataVisualization API compatibility issues
   - Test basic formatter loading

2. **Basic String Extraction**
   - Implement `ExtractStringContent()` for simple UTF-8 strings
   - Handle basic NSString memory layout
   - Add safety bounds checking

### Phase 2: Encoding & Types (Week 2)
1. **Multi-Encoding Support**
   - Add UTF-16 and UTF-32 decoding capabilities
   - Implement encoding detection logic
   - Handle byte order marks (BOM)

2. **String Type Variants**
   - Support NSMutableString formatting
   - Handle NSConstantString special cases
   - Add subclass detection logic

### Phase 3: Performance & Polish (Week 3)
1. **Performance Optimization**
   - Implement string content caching
   - Add lazy loading for large strings
   - Optimize memory allocation patterns

2. **Display Formatting**
   - Add truncation for very long strings
   - Implement escape sequence handling
   - Create summary format with length information

### Phase 4: Advanced Features (Week 4)
1. **Error Handling Enhancement**
   - Robust invalid pointer detection
   - Corruption detection and recovery
   - Improved error messaging

2. **Developer Experience**
   - Integration testing with real applications
   - Performance benchmarking
   - Documentation and examples

## Risk Assessment

### High Risk
- **Memory Layout Compatibility**: GNUstep NSString internals may vary
- **Encoding Complexity**: Multi-byte character handling edge cases

### Medium Risk  
- **Performance Impact**: Large string processing overhead
- **LLDB API Changes**: DataVisualization system stability

### Low Risk
- **Basic UTF-8 Support**: Well-understood and documented
- **Type Registration**: LLDB formatter system is mature

### Mitigation Strategies
- Start with simple UTF-8 strings, add encoding complexity incrementally
- Extensive testing with various string sizes and encodings
- Performance monitoring and early optimization
- Fallback to raw pointer display if formatting fails

## Tasks Breakdown

### [01_Activate_Formatter_Registration](../tasks/004_STRING_FORMATTERS/01_Activate_Formatter_Registration.md)
**Priority**: P0 (Critical)  
**Effort**: 2 days  
**Description**: Enable string formatter registration in runtime initialization

### [02_Basic_String_Content_Extraction](../tasks/004_STRING_FORMATTERS/02_Basic_String_Content_Extraction.md)
**Priority**: P0 (Critical)  
**Effort**: 4 days  
**Description**: Implement core string content extraction for UTF-8 NSString objects

### [03_Multi_Encoding_Support](../tasks/004_STRING_FORMATTERS/03_Multi_Encoding_Support.md)
**Priority**: P1 (High)  
**Effort**: 5 days  
**Description**: Add UTF-16, UTF-32, and encoding detection capabilities

### [04_String_Type_Variants](../tasks/004_STRING_FORMATTERS/04_String_Type_Variants.md)
**Priority**: P1 (High)  
**Effort**: 3 days  
**Description**: Support NSMutableString, NSConstantString, and subclasses

### [05_Performance_Optimization](../tasks/004_STRING_FORMATTERS/05_Performance_Optimization.md)
**Priority**: P2 (Medium)  
**Effort**: 4 days  
**Description**: Implement caching, lazy loading, and performance optimizations

### [06_Display_Formatting_Polish](../tasks/004_STRING_FORMATTERS/06_Display_Formatting_Polish.md)
**Priority**: P2 (Medium)  
**Effort**: 3 days  
**Description**: Add truncation, escape sequences, and summary formatting

## Testing Strategy

### Unit Tests
- String content extraction for various encodings
- Memory safety with invalid pointers
- Performance testing with large strings
- Edge cases (empty, nil, corrupted strings)

### Integration Tests
- Real GNUstep application debugging scenarios
- Cross-platform compatibility testing
- Performance regression testing
- Memory leak detection

### Validation Criteria
- 95%+ successful string content display
- <5ms average string formatting time
- No memory leaks or crashes
- Correct display for all supported encodings

## Definition of Done

- [ ] String formatters successfully registered and active
- [ ] NSString content displays correctly in `po` commands
- [ ] NSMutableString handled identically to NSString
- [ ] NSConstantString (@"literal") objects formatted properly
- [ ] UTF-8, UTF-16, and UTF-32 encoding support working
- [ ] Memory safety validated with corrupted objects
- [ ] Performance targets met (<5ms formatting time)
- [ ] Integration tests pass with real GNUstep applications
- [ ] No regressions in existing LLDB functionality
- [ ] Code review completed and approved
- [ ] Documentation updated with examples

## Success Metrics

### Functional
- **Coverage**: 100% of standard NSString types supported
- **Accuracy**: 99%+ correct string content display
- **Safety**: 0 crashes during string formatting operations

### Performance  
- **Latency**: <5ms average string formatting time
- **Memory**: <100KB additional memory per formatted string
- **Scalability**: Support for strings up to 1MB efficiently

### Developer Experience
- **Usability**: 95% developer satisfaction in usability testing
- **Productivity**: 50% reduction in time to identify string-related bugs
- **Adoption**: Used in 80% of GNUstep debugging sessions

---

*Epic Owner*: Development Team  
*Created*: August 2025  
*Target Completion*: Q1 2025  
*Dependencies*: Epic 001 (Core Runtime), Epic 002 (Object Introspection)
