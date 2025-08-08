# Task 02: Basic String Content Extraction

## Overview
Implement core NSString content extraction functionality to display actual string values instead of pointer addresses when debugging GNUstep applications.

## Priority: P0 (Critical)
**Effort**: 4 days  
**Epic**: [004_STRING_FORMATTERS](../../epics/004_STRING_FORMATTERS.md)  
**Assignee**: TBD  
**Status**: Blocked by Task 01 (Formatter Registration)

## Business Value
- **Immediate Developer Impact**: Developers see "Hello World" instead of "0x7fff12345678"  
- **Debugging Efficiency**: 90% reduction in time to identify string values
- **Feature Validation**: Proves formatter system works end-to-end
- **Foundation for Collections**: Enables array/dictionary string element display

## Technical Scope

### Current State Analysis
String content extraction logic exists but is not functional:

**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.cpp`
**Method**: `ExtractStringContent()` - Currently returns empty string
**Method**: `GNUstepNSStringFormatterFunction()` - Calls ExtractStringContent()

### GNUstep NSString Memory Layout
Based on GNUstep Base library analysis, NSString objects have this general structure:
```
NSString object:
  +0x00: isa pointer (Class pointer) 
  +0x08: string data pointer OR inline data
  +0x10: length (NSUInteger)
  +0x18: additional metadata (encoding, etc.)
```

**Key Variations**:
- **NSConstantString**: Compile-time strings (@"hello") with different layout
- **NSMutableString**: May have capacity and mutation tracking
- **Subclassed NSString**: Custom implementations with unique layouts

### Required Implementation

#### Step 1: ISA Resolution Integration
Connect with existing introspector to get class information:
```cpp
bool ExtractStringContent(ValueObject &valobj, StreamString &summary) {
    // Get ISA and verify this is actually an NSString
    auto isa_addr = valobj.GetPointerValue();
    if (!m_introspector || isa_addr == LLDB_INVALID_ADDRESS) {
        return false;
    }
    
    std::string class_name = m_introspector->GetClassName(isa_addr);
    if (!IsStringClass(class_name)) {
        return false;
    }
    
    // Proceed with string extraction...
}
```

#### Step 2: Memory Layout Reading
Extract string data from GNUstep NSString structure:
```cpp
// Read string data pointer (offset +0x08)
ValueObjectSP data_ptr_valobj = valobj.GetChildAtIndex(1, true);
if (!data_ptr_valobj) return false;

addr_t string_data_addr = data_ptr_valobj->GetPointerValue();
if (string_data_addr == LLDB_INVALID_ADDRESS) return false;

// Read string length (offset +0x10) 
ValueObjectSP length_valobj = valobj.GetChildAtIndex(2, true);
if (!length_valobj) return false;

uint64_t string_length = length_valobj->GetValueAsUnsigned(0);
if (string_length == 0 || string_length > MAX_STRING_LENGTH) {
    return false; // Empty string or suspiciously long
}
```

#### Step 3: String Data Extraction
Read actual string bytes from memory:
```cpp
// Read string data from target memory
Error error;
std::vector<uint8_t> buffer(string_length + 1);
size_t bytes_read = valobj.GetProcess()->ReadMemory(
    string_data_addr, buffer.data(), string_length, error);

if (error.Fail() || bytes_read != string_length) {
    return false;
}

// Null-terminate and convert to summary
buffer[string_length] = '\0';
summary.Printf("\"%s\"", (char*)buffer.data());
return true;
```

#### Step 4: Safety and Error Handling
- **Bounds Checking**: Validate all memory addresses and sizes
- **Encoding Detection**: Handle UTF-8/UTF-16 properly  
- **Corruption Detection**: Identify malformed string objects
- **Performance Limits**: Cap string length and processing time

## Acceptance Criteria

### Must Have
1. **Basic String Display**
   - `po myString` shows actual string content in quotes: "Hello World"
   - Empty strings display as empty quotes: ""
   - nil NSString objects display as "nil" or similar
   - Handle strings up to 1000 characters efficiently

2. **Memory Safety**
   - No crashes when examining invalid NSString pointers
   - Proper bounds checking for all memory reads
   - Timeout/limit for extremely long strings
   - Graceful handling of corrupted string objects

3. **Type Safety**
   - Only attempt extraction on actual NSString objects
   - Use ISA resolution to verify object type
   - Handle NSString subclasses (NSMutableString, etc.)
   - Reject non-string objects safely

4. **Performance**
   - String extraction completes in <10ms for typical strings
   - Memory usage bounded (no excessive allocations)
   - Efficient handling of repeated string access
   - No significant impact on debugging session performance

### Should Have
1. **UTF-8 Support**
   - Properly display UTF-8 encoded strings
   - Handle multi-byte characters correctly
   - Show escape sequences for non-printable characters
   - Detect and handle encoding issues gracefully

2. **String Variants**
   - Support NSMutableString identically to NSString
   - Handle NSConstantString (@"literal" strings)  
   - Recognize common NSString subclasses
   - Provide fallback for unknown string types

3. **Display Quality**
   - Truncate very long strings with "..." indicator
   - Show string length in summary when helpful
   - Escape special characters properly (newlines, tabs, etc.)
   - Handle binary data in strings gracefully

### Testing Requirements
1. **Functional Testing**
   - Test with various string lengths (0, 1, 100, 1000 characters)
   - Test with UTF-8 multi-byte characters
   - Test with NSMutableString objects
   - Test with NSConstantString literals

2. **Error Condition Testing**
   - Test with null/invalid NSString pointers
   - Test with corrupted string objects
   - Test with extremely long strings (>1MB)
   - Test with non-NSString objects passed to formatter

3. **Performance Testing**
   - Measure string extraction time across various lengths
   - Verify no memory leaks during string processing
   - Test with thousands of repeated string accesses
   - Validate acceptable performance impact

## Implementation Tasks

### Day 1: Memory Layout Research and Design
**Morning (4 hours)**:
- Analyze GNUstep NSString source code for memory layout
- Create test NSString objects and examine memory in debugger
- Document exact offset calculations for string data and length
- Design memory access strategy with safety bounds

**Afternoon (4 hours)**:
- Implement ISA-based type checking integration
- Create memory reading utilities with error handling
- Design string extraction algorithm
- Plan UTF-8 handling approach

### Day 2: Core Extraction Implementation
**Morning (4 hours)**:
- Implement basic string data reading from memory
- Add string length validation and bounds checking
- Create string content formatting for summary display
- Add basic error handling and safety checks

**Afternoon (4 hours)**:
- Test with simple NSString objects in live debugging session
- Debug memory access issues and offset calculations
- Validate string content appears correctly in debugger
- Fix any basic functionality issues

### Day 3: UTF-8 and String Variants
**Morning (4 hours)**:
- Implement UTF-8 character handling and validation
- Add support for multi-byte character sequences
- Handle escape sequences for special characters
- Test with international characters and symbols

**Afternoon (4 hours)**:
- Add NSMutableString support (verify same memory layout)
- Implement NSConstantString handling if layout differs
- Test string variants and subclasses
- Validate consistent behavior across string types

### Day 4: Performance and Polish
**Morning (4 hours)**:
- Implement string length limits and truncation
- Add performance optimization (caching, lazy loading)
- Optimize memory allocation patterns
- Add comprehensive error handling

**Afternoon (4 hours)**:
- Conduct performance testing and optimization
- Test edge cases and error conditions
- Validate memory safety with corrupted objects
- Final integration testing and bug fixes

## Risk Assessment

### High Risk
- **Memory Layout Assumptions**: GNUstep NSString layout may differ from analysis
- **Encoding Complexity**: UTF-8 multi-byte handling edge cases
- **Performance Impact**: String extraction overhead in debugging sessions

### Medium Risk
- **String Variants**: NSMutableString/NSConstantString may have different layouts  
- **Memory Safety**: Reading from invalid addresses could crash LLDB
- **Large String Handling**: Memory usage for very long strings

### Low Risk
- **Basic ASCII Strings**: Simple UTF-8 string extraction is well-understood
- **Integration**: String formatter registration system is already implemented
- **LLDB APIs**: Memory reading and ValueObject APIs are stable

### Mitigation Strategies
- Start with simple ASCII strings, add UTF-8 complexity incrementally
- Extensive bounds checking and validation before memory access
- Conservative memory allocation and deallocation patterns
- Comprehensive testing with various string types and sizes
- Fallback to raw pointer display if string extraction fails

## Dependencies
- **Task 01**: Formatter registration must be completed first
- **Epic 001**: ISA resolution and runtime introspection
- **GNUstep Test Apps**: Need simple string test cases for validation
- **Memory Analysis Tools**: For understanding NSString layout in live processes

## Definition of Done
- [ ] ExtractStringContent() function implemented and functional
- [ ] NSString objects display actual content, not pointer addresses
- [ ] Empty strings display as empty quotes ("")
- [ ] Nil strings display as "nil" or appropriate null indicator
- [ ] UTF-8 multi-byte characters display correctly
- [ ] NSMutableString handled identically to NSString  
- [ ] Memory safety validated with invalid pointers
- [ ] Performance targets met (<10ms extraction time)
- [ ] String length limits implemented (prevent excessive memory usage)
- [ ] Comprehensive testing completed across string types
- [ ] No regressions in existing runtime functionality
- [ ] Code reviewed and approved
- [ ] Integration tested with real GNUstep applications

## Success Metrics
- **Functionality**: 95% successful string content extraction
- **Performance**: <10ms average extraction time
- **Safety**: 0 crashes during string formatting operations  
- **Coverage**: Works with NSString, NSMutableString, NSConstantString
- **Developer Experience**: Immediate visible improvement in debugging sessions

---

*Task Owner*: Development Team  
*Created*: August 2025  
*Target Completion*: August 13, 2025 (4 days after Task 01 completion)  
*Dependencies*: Task 01 (Activate Formatter Registration)
