# Epic 003: Data Formatters & Visualization

## Overview
Implement rich, user-friendly visualization of GNUstep Objective-C objects in LLDB, providing developers with readable and actionable object state information during debugging sessions.

## Business Value
- **Immediate visible impact**: Developers see formatted objects instead of raw memory addresses
- **Debugging efficiency**: 50% faster object state analysis with rich visualization
- **Developer satisfaction**: Apple-quality debugging experience on open platforms
- **Learning curve reduction**: New GNUstep developers can understand object contents easily

## Current Status: 25% Complete

### ✅ Completed
- Complete formatter infrastructure and registration system
- Modular architecture supporting multiple object types
- String formatter implementation (needs activation)
- Integration with LLDB DataVisualization APIs
- CMake build system integration

### 🔄 In Progress
- String formatter activation and testing
- Formatter registration debugging in Initialize() method
- Integration with runtime detection system

### 📋 Planned
- NSArray and NSMutableArray formatters
- NSDictionary and NSMutableDictionary formatters
- NSNumber and primitive wrapper formatters
- NSSet and NSMutableSet formatters
- Custom object summary providers

## Technical Scope

### Core Formatter Types
1. **String Formatters** - NSString, NSMutableString, NSConstantString
2. **Collection Formatters** - NSArray, NSDictionary, NSSet variants
3. **Number Formatters** - NSNumber, NSDecimalNumber, primitive wrappers
4. **Foundation Formatters** - NSDate, NSURL, NSData, NSUUID
5. **Custom Object Formatters** - Generic object summary providers

### Key Files
- **Formatter Registry**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepFormattersRegistry.cpp`
- **String Formatters**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.cpp`
- **Main Integration**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp` (lines 36-49)
- **Formatter Stub**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepFormatters.cpp`

## Dependencies
- **Epic 001**: Core runtime foundation for object introspection
- **Epic 002**: Object introspection capabilities for reading object data
- **LLDB APIs**: DataVisualization and TypeCategory systems
- **Test Cases**: Real GNUstep objects for validation

## Acceptance Criteria

### Must Have

1. **String Formatting**
   - Display NSString content as `@"Hello World"` instead of memory addresses
   - Handle UTF-8, UTF-16, and ASCII encodings correctly
   - Support strings up to 10KB length with truncation for larger strings
   - Show encoding information for non-ASCII strings

2. **Collection Formatting**
   - NSArray displays as `@[ item1, item2, item3 ]` with configurable depth
   - NSDictionary displays as `@{ key1: value1, key2: value2 }`
   - Handle nested collections with proper indentation
   - Show collection count and capacity information

3. **Number Formatting**
   - NSNumber displays underlying value and type (`@42 (NSInteger)`)
   - Handle all numeric types (integers, floats, booleans)
   - Show decimal precision for floating-point numbers

4. **Error Handling**
   - Graceful handling of corrupted objects
   - Clear error messages for unsupported object types
   - Fallback to memory address for unformattable objects

### Should Have

1. **Performance**
   - Formatter execution in <50ms for typical objects
   - Lazy evaluation for large collections
   - Configurable depth limits to prevent infinite recursion

2. **Customization**
   - User-configurable formatting preferences
   - Depth limits for nested objects
   - String length limits with ellipsis

3. **Rich Information**
   - Object retain count (if available)
   - Class inheritance hierarchy in tooltips
   - Memory usage information

### Could Have

1. **Advanced Visualization**
   - Graphical tree view for complex nested structures
   - Color coding for different object types
   - Export functionality for object state

## Risk Assessment

### High Risk
- **Memory Layout Differences**: GNUstep vs Apple NSString internal structure
- **Encoding Complexity**: Handling various string encodings safely

### Medium Risk
- **Performance Impact**: Large collections causing debugging slowdown
- **Infinite Recursion**: Circular references in object graphs

### Mitigation Strategies
- Extensive testing with real GNUstep applications
- Conservative memory access with bounds checking
- Configurable limits and timeouts
- Progressive enhancement (basic functionality first)

## Implementation Tasks

### [01_String_Formatter_Activation](../tasks/003_DATA_FORMATTERS/01_String_Formatter_Activation.md)
**Priority**: P0 (Critical)  
**Effort**: 3 days  
**Description**: Activate existing string formatter infrastructure and fix registration

### [02_String_Content_Extraction](../tasks/003_DATA_FORMATTERS/02_String_Content_Extraction.md)
**Priority**: P0 (Critical)  
**Effort**: 5 days  
**Description**: Implement GNUstep NSString content extraction and encoding handling

### [03_Collection_Formatters](../tasks/003_DATA_FORMATTERS/03_Collection_Formatters.md)
**Priority**: P1 (High)  
**Effort**: 8 days  
**Description**: Implement NSArray, NSDictionary, and NSSet formatters

### [04_Number_Formatters](../tasks/003_DATA_FORMATTERS/04_Number_Formatters.md)
**Priority**: P1 (High)  
**Effort**: 4 days  
**Description**: Implement NSNumber and primitive wrapper formatters

### [05_Foundation_Formatters](../tasks/003_DATA_FORMATTERS/05_Foundation_Formatters.md)
**Priority**: P2 (Medium)  
**Effort**: 6 days  
**Description**: Implement NSDate, NSURL, NSData, and other Foundation class formatters

### [06_Performance_Optimization](../tasks/003_DATA_FORMATTERS/06_Performance_Optimization.md)
**Priority**: P2 (Medium)  
**Effort**: 4 days  
**Description**: Optimize formatter performance and add caching strategies

## Test Strategy

### Unit Testing
- Individual formatter functions with mock objects
- Edge cases: empty strings, nil objects, corrupted data
- Performance testing with large collections

### Integration Testing  
- Real GNUstep applications with various object types
- Cross-platform testing (Linux, Windows, macOS)
- Multiple GNUstep versions compatibility

### User Acceptance Testing
- Developer workflow validation
- Debugging session performance impact
- Usability feedback from GNUstep community

## Definition of Done

- [ ] String formatters show readable content instead of addresses
- [ ] Collection formatters display structured, readable output
- [ ] Number formatters show values and types clearly
- [ ] All formatters handle error cases gracefully
- [ ] Performance meets <50ms target for typical objects
- [ ] Integration tests pass with real GNUstep applications
- [ ] User documentation updated with formatter examples
- [ ] Code review completed and approved

## Success Metrics

### User Experience
- **Developer Time Savings**: 50% reduction in object inspection time
- **Error Reduction**: 30% fewer debugging mistakes due to better visibility
- **Adoption Rate**: 90% of debugging sessions use formatted output

### Technical Performance
- **Latency**: <50ms average formatting time
- **Memory**: <500KB additional memory per debugging session
- **Reliability**: 99.9% successful formatting rate

### Quality Measures
- **Test Coverage**: 95%+ for all formatter components
- **Bug Rate**: <1 formatting error per 10,000 objects
- **Compatibility**: Works with all major GNUstep Foundation classes

## Future Enhancements

### Phase 2 Features
- Custom object summary providers for application-specific classes
- Interactive object exploration (drill-down navigation)
- Export/import functionality for object state

### Phase 3 Features
- Visual object graph representation
- Diff visualization for object state changes
- Integration with debugging UIs beyond command-line

---

*Epic Owner*: Development Team  
*Created*: December 2024  
*Target Completion*: Q1 2025  
*Dependencies*: Epic 001 (Core Runtime Foundation)
