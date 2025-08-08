# GNUstep ObjC Runtime V2 - Architecture Research & Development Backlog

## 📋 Project Overview

**Goal**: Create a comprehensive, modular LLDB runtime plugin for debugging GNUstep Objective-C applications on Windows/WSL using the libobjc2 runtime.

**Current Status**: ✅ Build system working, modular architecture established, basic infrastructure complete

---

## 🔬 Architecture Research Analysis

### Apple ObjC Runtime vs GNUstep Implementation Comparison

#### **Apple Implementation Structure**
Located in: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/AppleObjCRuntime/`

**Key Files:**
- `AppleObjCRuntime.cpp/h` - Main runtime class
- `AppleObjCRuntimeV1.cpp/h` - Legacy runtime support  
- `AppleObjCRuntimeV2.cpp/h` - Modern runtime support
- `AppleObjCClassDescriptor.cpp/h` - Class introspection
- `AppleObjCDeclVendor.cpp/h` - AST synthesis
- `AppleObjCTrampolineHandler.cpp/h` - Method dispatch
- `AppleObjCTypeEncodingParser.cpp/h` - Type encoding

**Apple's Architecture Strengths:**
1. **Version Segmentation**: Separate V1/V2 implementations for different runtime versions
2. **Comprehensive Type System**: Robust type encoding parser
3. **Dynamic Discovery**: Runtime class discovery and caching
4. **Method Dispatch**: Sophisticated trampoline handling
5. **ISA Analysis**: Deep understanding of object identity

#### **Our GNUstep V2 Implementation**
Located in: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/`

**Current Structure:**
```
GNUstepObjCRuntime/
├── GNUstepObjCRuntime.cpp/h                    # Main runtime [✅ COMPLETE]
├── GNUstepObjCRuntimeIntrospector.cpp/h        # Object introspection [🔄 BASIC]
├── GNUstepObjCDeclVendor.cpp/h                 # AST synthesis [🔄 BASIC]
└── formatters/                                 # [✅ ARCHITECTURE COMPLETE]
    ├── GNUstepFormattersBase.cpp/h              # Common utilities
    ├── GNUstepStringFormatters.cpp/h           # NSString support [🔄 BASIC]
    └── GNUstepFormattersRegistry.cpp/h          # Central registration
```

**Our Architecture Advantages:**
1. **Modular Design**: Clean separation of concerns
2. **Scalable Formatters**: Easy to add new data types
3. **Modern C++ Patterns**: Uses current LLDB APIs
4. **libobjc2 Focused**: Specifically designed for GNUstep 2.0

---

## 🧬 libobjc2 Runtime Constraints Analysis

### Key Runtime Characteristics (from `/home/robk/code/llvm-project/lldb/libobjc2/`)

#### **Object Layout**
```c
// From libobjc2/class.h and runtime structures
struct objc_object {
    Class isa;              // 8 bytes on 64-bit, 4 on 32-bit
    // Object data follows...
};

struct objc_class {
    struct objc_class *isa;          // Metaclass pointer
    struct objc_class *super_class;  // Superclass pointer  
    const char *name;                // Class name
    long version;                    // Class version
    long info;                       // Class info flags
    long instance_size;              // Instance size
    struct objc_ivar_list *ivars;    // Instance variables
    struct objc_method_list **methodLists; // Method lists
    struct objc_cache *cache;        // Method cache
    struct objc_protocol_list *protocols; // Protocols
};
```

#### **String Implementation**
From `libobjc2/constant_string.h`:
```c
struct __NSConstantString_struct {
    Class isa;                // NSConstantString class
    const char *str;          // UTF-8 string data
    NSUInteger length;        // String length
};
```

#### **Method Dispatch**
- Uses GNU's libffi for dynamic method calls
- Method signatures encoded differently than Apple
- Different selector registration mechanism

#### **Memory Management**
- ARC support through runtime calls
- Different weak reference implementation
- Custom autorelease pool implementation

---

## 📚 V1 Legacy Analysis

### What to Learn From (Located in `GNUstepObjCRuntime_V1_archived/`)

**Functional Components to Study:**
1. **GNUstepNSString.cpp** - String extraction patterns
2. **GNUstepCollectionUtilities.cpp** - Collection introspection
3. **GNUstepRuntimeAPI.cpp** - Runtime interaction patterns
4. **GNUstepUtilities.cpp** - Memory reading utilities

**Known V1 Issues to Avoid:**
1. Monolithic architecture (hard to maintain)
2. Inconsistent error handling
3. Complex interdependencies
4. Limited test coverage
5. Performance issues with large collections

---

## 🎯 Development Backlog

### Phase 1: Foundation Stability (Priority 1)
- [ ] **1.1** Fix NSString formatter to correctly read libobjc2 constant string layout
- [ ] **1.2** Add comprehensive error handling to base utilities
- [ ] **1.3** Implement proper object validation using libobjc2 runtime checks
- [ ] **1.4** Create unit tests for GNUstepFormattersBase utilities
- [ ] **1.5** Test string formatter with various string types (constant, mutable, UTF-8)

### Phase 2: Core Data Types (Priority 1)
- [ ] **2.1** Implement NSNumber formatters
  - [ ] NSInteger, NSUInteger support
  - [ ] NSFloat, NSDouble support  
  - [ ] NSDecimalNumber support
- [ ] **2.2** Implement NSArray formatters
  - [ ] Summary provider (count display)
  - [ ] Synthetic children provider (element access)
  - [ ] Handle both NSArray and NSMutableArray
- [ ] **2.3** Create comprehensive data type tests using `custom_class_test`

### Phase 3: Collection Types (Priority 2)
- [ ] **3.1** Implement NSDictionary formatters
  - [ ] Key-value pair display
  - [ ] Synthetic children for keys/values
  - [ ] Handle both NSDictionary and NSMutableDictionary
- [ ] **3.2** Implement NSSet formatters
  - [ ] Element enumeration
  - [ ] Handle both NSSet and NSMutableSet
- [ ] **3.3** Performance optimization for large collections

### Phase 4: Foundation Types (Priority 2)
- [ ] **4.1** NSDate formatter with human-readable timestamp display
- [ ] **4.2** NSURL formatter with URL string extraction
- [ ] **4.3** NSData formatter with hex dump and size information
- [ ] **4.4** NSUUID formatter with standard UUID string representation

### Phase 5: Runtime Introspection (Priority 2)
- [ ] **5.1** Enhanced GNUstepObjCRuntimeIntrospector
  - [ ] Class discovery and caching
  - [ ] Method list introspection
  - [ ] Instance variable discovery
- [ ] **5.2** Dynamic class detection and type resolution
- [ ] **5.3** Custom class debugging support

### Phase 6: Advanced Features (Priority 3)
- [ ] **6.1** GNUstepObjCDeclVendor improvements
  - [ ] Proper AST synthesis for custom classes
  - [ ] Protocol support
  - [ ] Category method resolution
- [ ] **6.2** Method dispatch trampoline handling
- [ ] **6.3** ARC debugging support
- [ ] **6.4** Exception handling and debugging

### Phase 7: Testing & Validation (Priority 1)
- [ ] **7.1** Create comprehensive test suite in `/test/` or `/unittests/`
  - [ ] Unit tests for each formatter
  - [ ] Integration tests with real GNUstep programs
  - [ ] Performance benchmarks
- [ ] **7.2** Cross-platform validation (WSL, native Linux, Windows)
- [ ] **7.3** Memory leak detection and performance optimization
- [ ] **7.4** Edge case handling (nil objects, corrupted memory, etc.)

### Phase 8: Documentation & Deployment (Priority 3)
- [ ] **8.1** Comprehensive API documentation
- [ ] **8.2** User guide for debugging GNUstep applications
- [ ] **8.3** Integration with VS Code LLDB extension
- [ ] **8.4** Performance tuning and optimization
- [ ] **8.5** Prepare for upstream contribution to LLVM project

---

## 🧪 Testing Strategy

### Test Categories

#### **Unit Tests** (`/unittests/`)
- Test each formatter in isolation
- Mock ValueObject and Process interfaces
- Validate memory reading utilities
- Test error handling paths

#### **Integration Tests** (`/test/`)
- Use real GNUstep programs like `custom_class_test`
- Test with various object configurations
- Cross-platform compatibility tests
- Performance regression tests

#### **Test Programs**
1. **Existing**: `examples/custom_class_test.m` - Good foundation
2. **Create**: String-focused test program
3. **Create**: Collection-focused test program  
4. **Create**: Foundation types test program
5. **Create**: Custom class hierarchy test program

---

## 🔧 Technical Debt & Known Issues

### Current Technical Debt
1. **Incomplete Object Validation**: Need better libobjc2 runtime integration
2. **Limited Error Handling**: Need consistent error propagation
3. **Performance**: Not optimized for large object graphs
4. **Memory Safety**: Need bounds checking on memory reads

### Known Limitations
1. **String Encoding**: Currently assumes UTF-8, need encoding detection
2. **32/64-bit Support**: Architecture-specific pointer handling needed
3. **Version Compatibility**: Only targets libobjc2 v2.1+
4. **Platform Support**: Primarily designed for Linux/WSL

---

## 🎖️ Success Metrics

### Phase 1 Success Criteria
- [ ] NSString objects display content correctly in debugger
- [ ] No crashes when inspecting various string types
- [ ] Unit tests pass for string formatters
- [ ] Integration with VS Code debugger working

### Phase 2 Success Criteria  
- [ ] NSNumber, NSArray display correctly
- [ ] Array elements are accessible in debugger
- [ ] Performance acceptable for arrays up to 1000 elements

### Phase 3+ Success Criteria
- [ ] All major Foundation types supported
- [ ] Custom class debugging functional
- [ ] Performance benchmarks meet targets
- [ ] Test suite coverage > 90%

---

## 🚀 Implementation Notes

### Development Workflow
1. **Feature Development**: Work on one formatter family at a time
2. **Testing**: Test with `custom_class_test` after each major change
3. **Integration**: Use LLDB MCP tools for interactive testing
4. **Documentation**: Update this backlog as work progresses

### Key Principles
1. **Modular Architecture**: Keep formatters independent
2. **Robust Error Handling**: Fail gracefully, never crash debugger
3. **Performance**: O(1) operations where possible
4. **Maintainability**: Clear code with comprehensive tests

---

*Last Updated: August 7, 2025*
*Status: Foundation complete, ready for Phase 1 implementation*
