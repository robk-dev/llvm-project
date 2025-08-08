# Task 02: ISA Resolution Implementation

## Epic: [001_CORE_RUNTIME_FOUNDATION](../../epics/001_CORE_RUNTIME_FOUNDATION.md)
**Priority**: P0 (Critical)  
**Effort Estimate**: 8 days  
**Assignee**: TBD  
**Status**: Ready for Development

## Overview
Implement complete ISA-to-class name resolution for GNUstep Objective-C objects, replacing the current stub implementation with production-ready code that can extract class information from object pointers.

## Background
The current GNUstepObjCRuntimeIntrospector has a stub implementation for `GetClassName()` that needs to be replaced with actual GNUstep runtime introspection. This is the foundation for all object visualization and debugging capabilities.

## Technical Requirements

### Current State Analysis

#### File: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.cpp`

**Current Stub Implementation:**
```cpp
std::string GNUstepObjCRuntimeIntrospector::GetClassName(lldb::addr_t object_addr) {
  // TODO: Implement actual class name resolution for GNUstep
  // For now, return a placeholder
  return "GNUstepObject";
}
```

**Required Implementation:**
1. Read object's ISA pointer from memory
2. Resolve ISA to class structure in GNUstep runtime
3. Extract class name from class structure
4. Handle various GNUstep runtime versions
5. Implement error handling for invalid objects

### GNUstep Runtime Structure Analysis

#### Object Layout (libobjc2)
```c
// Typical GNUstep object layout
struct objc_object {
    Class isa;  // First 8 bytes (64-bit) or 4 bytes (32-bit)
    // ... instance variables follow
};

// Class structure (simplified)
struct objc_class {
    struct objc_class *isa;           // Metaclass
    struct objc_class *super_class;   // Superclass
    const char *name;                 // Class name string
    // ... other fields
};
```

#### Memory Reading Strategy
1. **Object Validation**: Verify object pointer is valid and aligned
2. **ISA Extraction**: Read first pointer-sized word from object
3. **Class Validation**: Verify ISA points to valid class structure
4. **Name Extraction**: Read class name string from class structure
5. **String Validation**: Ensure class name string is valid and readable

### Implementation Details

#### Required Methods to Implement

```cpp
class GNUstepObjCRuntimeIntrospector {
private:
    // Core memory reading
    bool ReadPointer(lldb::addr_t addr, lldb::addr_t &value);
    bool ReadCString(lldb::addr_t addr, std::string &str, size_t max_len = 256);
    
    // Object validation
    bool IsValidObjectPointer(lldb::addr_t object_addr);
    bool IsValidClassPointer(lldb::addr_t class_addr);
    
    // Runtime structure navigation
    lldb::addr_t GetISAFromObject(lldb::addr_t object_addr);
    lldb::addr_t GetClassNamePointer(lldb::addr_t class_addr);
    lldb::addr_t GetSuperClass(lldb::addr_t class_addr);
    
    // Runtime version detection
    enum class GNUstepVersion { V1, V2, Unknown };
    GNUstepVersion DetectRuntimeVersion();
    
public:
    // Main interface (already exists, needs implementation)
    std::string GetClassName(lldb::addr_t object_addr) override;
    
    // New methods to support complete introspection
    std::string GetSuperClassName(lldb::addr_t object_addr);
    bool IsRootClass(lldb::addr_t class_addr);
    std::vector<std::string> GetClassHierarchy(lldb::addr_t object_addr);
};
```

#### Memory Access Implementation

```cpp
bool GNUstepObjCRuntimeIntrospector::ReadPointer(lldb::addr_t addr, lldb::addr_t &value) {
    lldb::Status error;
    size_t bytes_read = 0;
    
    // Read pointer-sized value
    size_t pointer_size = m_process->GetAddressByteSize();
    value = m_process->ReadPointerFromMemory(addr, error);
    
    if (error.Fail()) {
        printf("[DEBUG] Failed to read pointer from 0x%" PRIx64 ": %s\n", 
               addr, error.AsCString());
        return false;
    }
    
    return true;
}
```

## Acceptance Criteria

### Must Have

1. **Basic Class Name Resolution**
   - [ ] Extract correct class name for NSString objects: "NSString"
   - [ ] Extract correct class name for NSArray objects: "NSArray" 
   - [ ] Extract correct class name for custom classes: "MyCustomClass"
   - [ ] Handle NSObject and other root classes correctly

2. **Memory Safety**
   - [ ] No crashes when given invalid object pointers
   - [ ] Proper error handling for corrupted objects
   - [ ] Bounds checking for all memory reads
   - [ ] Timeout protection against infinite loops

3. **Error Handling**
   - [ ] Return empty string or error indicator for invalid objects
   - [ ] Log diagnostic information for debugging failures
   - [ ] Graceful degradation when runtime structures are corrupted

4. **Performance**
   - [ ] Class name resolution completes in <10ms for typical objects
   - [ ] Minimal memory allocation during resolution process
   - [ ] No memory leaks during repeated operations

### Should Have

1. **Runtime Version Support**
   - [ ] Detect GNUstep runtime version (libobjc vs libobjc2)
   - [ ] Handle version-specific structure layouts
   - [ ] Provide appropriate fallbacks for unsupported versions

2. **Class Hierarchy Support**
   - [ ] Extract superclass information
   - [ ] Traverse class hierarchy up to root classes
   - [ ] Detect circular references and handle appropriately

3. **Advanced Validation**
   - [ ] Verify ISA pointer validity before dereferencing
   - [ ] Check class structure magic numbers or signatures
   - [ ] Validate class name string encoding and content

### Could Have

1. **Caching and Optimization**
   - [ ] Cache class name lookups for repeated queries
   - [ ] Batch memory reads for efficiency
   - [ ] Lazy loading of class hierarchy information

2. **Debugging Support**
   - [ ] Detailed logging of resolution process
   - [ ] Ability to dump raw class structures
   - [ ] Debug commands for manual introspection

## Implementation Plan

### Phase 1: Basic Memory Reading (2 days)
1. Implement `ReadPointer()` and `ReadCString()` utilities
2. Add object and class pointer validation
3. Create basic error handling framework
4. Unit tests for memory reading functions

### Phase 2: ISA Resolution (3 days)
1. Implement `GetISAFromObject()` method
2. Add class structure navigation
3. Implement basic `GetClassName()` for simple objects
4. Test with NSString and NSObject instances

### Phase 3: Robustness and Error Handling (2 days)
1. Add comprehensive input validation
2. Implement timeout and bounds checking
3. Handle corrupted or invalid objects gracefully
4. Add diagnostic logging and error reporting

### Phase 4: Testing and Optimization (1 day)
1. Integration testing with real GNUstep applications
2. Performance optimization and profiling
3. Memory leak detection and fixes
4. Cross-platform compatibility testing

## Test Cases

### Test Case 1: Basic Class Name Resolution
```cpp
// Test objects from: /home/robk/code/llvm-project/lldb/examples/simple_test.m
NSString *greeting = @"Hello World";
NSString *greeting2 = [NSString stringWithUTF8String:"Hello, GNUstep World!"];

// Expected results:
GetClassName(greeting_addr) → "NSConstantString" or "NSString"
GetClassName(greeting2_addr) → "NSString"
```

### Test Case 2: Custom Class Resolution
```cpp
// Test objects from: /home/robk/code/llvm-project/lldb/examples/custom_class_test.m
// (Assuming custom class implementation exists)
MyCustomClass *obj = [[MyCustomClass alloc] init];

// Expected result:
GetClassName(obj_addr) → "MyCustomClass"
```

### Test Case 3: Error Handling
```cpp
// Invalid pointers
GetClassName(0x0) → "" (empty string)
GetClassName(0xdeadbeef) → "" (invalid address)
GetClassName(0x123) → "" (misaligned pointer)
```

### Test Case 4: Root Classes
```cpp
// NSObject and Protocol objects
NSObject *obj = [[NSObject alloc] init];
Protocol *proto = @protocol(NSCopying);

// Expected results:
GetClassName(obj_addr) → "NSObject"
GetClassName(proto_addr) → "Protocol"
```

## Definition of Done

- [ ] `GetClassName()` returns correct class names for 95% of standard GNUstep objects
- [ ] Memory access is safe with proper bounds checking and error handling
- [ ] Performance meets <10ms target for class name resolution
- [ ] Integration tests pass with real GNUstep applications
- [ ] No memory leaks during 1000 consecutive class name resolutions
- [ ] Code review completed and approved
- [ ] Unit tests achieve 90%+ code coverage

## Success Metrics

### Functional Metrics
- **Accuracy**: 95%+ correct class name resolution
- **Coverage**: Works with NSString, NSArray, NSObject, custom classes
- **Error Handling**: 0 crashes with invalid inputs

### Performance Metrics  
- **Latency**: <10ms average resolution time
- **Memory**: <1KB allocation per resolution operation
- **Throughput**: >100 resolutions per second

### Quality Metrics
- **Reliability**: 99.9% successful operation rate
- **Memory Safety**: 0 crashes or memory corruption
- **Test Coverage**: 90%+ code coverage with unit tests

## Dependencies

- **Process Memory Access**: LLDB Process API for reading target memory
- **Test Applications**: Working GNUstep applications for validation
- **Build System**: Ability to compile and test changes
- **GNUstep Runtime**: Understanding of libobjc2 and GNUstep Base structures

## Risks and Mitigation

### Risk: Runtime Structure Changes
**Likelihood**: Medium  
**Impact**: High  
**Mitigation**: Test with multiple GNUstep versions, implement version detection

### Risk: Platform Differences
**Likelihood**: Medium  
**Impact**: Medium  
**Mitigation**: Test on different architectures (32-bit, 64-bit, ARM, x86)

### Risk: Performance Impact
**Likelihood**: Low  
**Impact**: Medium  
**Mitigation**: Profile memory access patterns, implement caching

---

*Created*: December 2024  
*Last Updated*: December 2024  
*Epic*: 001_CORE_RUNTIME_FOUNDATION  
*Dependencies*: Basic runtime detection, GNUstep test applications
