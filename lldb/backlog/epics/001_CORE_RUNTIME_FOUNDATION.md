# Epic 001: Core Runtime Foundation

## Overview
Establish a robust, production-ready foundation for GNUstep Objective-C runtime detection, ISA resolution, and class hierarchy traversal within LLDB.

## Business Value
- **Foundation for all features**: Every debugging capability depends on reliable runtime detection
- **Developer confidence**: Stable foundation ensures reliable debugging experience
- **Technical debt prevention**: Proper architecture now prevents costly refactoring later

## Current Status: 30% Complete

### ✅ Completed
- Basic plugin registration and initialization
- GNUstep library detection (`libgnustep-base.so`, `libobjc2`)
- LLDB virtual method implementations (no undefined symbols)
- Compilation and linking success
- Live debugging validation with real GNUstep processes

### 🔄 In Progress  
- ISA resolution implementation (currently stubbed)
- Class name extraction from runtime structures
- Memory layout understanding for GNUstep objects

### 📋 Planned
- Complete ISA-to-class resolution
- Robust class hierarchy traversal
- Runtime version detection and compatibility
- Error handling and graceful degradation

## Technical Scope

### Core Components
1. **Runtime Detection** - Identify GNUstep vs Apple runtime
2. **ISA Resolution** - Extract class information from object pointers  
3. **Class Hierarchy** - Navigate inheritance chains and categories
4. **Memory Layout** - Understand GNUstep object structure
5. **Version Compatibility** - Handle different GNUstep runtime versions

### Key Files
- **Primary Implementation**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`
- **Header Definitions**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h`
- **Introspector**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.cpp`

## Dependencies
- **LLDB Core APIs**: ObjCLanguageRuntime base class
- **GNUstep Runtime**: Understanding of libobjc2 and GNUstep Base
- **Test Applications**: Simple GNUstep programs for validation

## Acceptance Criteria

### Must Have
1. **Reliable Runtime Detection**
   - Detect GNUstep/libobjc2 runtime in 100% of valid GNUstep processes
   - No false positives with Apple runtime or other Objective-C implementations
   - Handle processes with multiple Objective-C runtimes

2. **ISA Resolution**
   - Extract correct class name from any valid GNUstep object pointer
   - Handle root classes (NSObject, Protocol, etc.)
   - Graceful failure for invalid or corrupted pointers

3. **Class Hierarchy Navigation**
   - Traverse superclass chains up to NSObject
   - Handle classes with no superclass (root classes)
   - Detect and handle circular references (error case)

4. **Memory Safety**
   - No crashes when examining malformed objects
   - Proper bounds checking for all memory reads
   - Timeout mechanisms for infinite loops

### Should Have
1. **Performance**
   - Class name resolution in <10ms for typical objects
   - Cached results for repeated queries
   - Lazy loading of runtime information

2. **Diagnostics**
   - Debug logging for troubleshooting runtime issues
   - Clear error messages for unsupported configurations
   - Runtime version detection and warnings

### Could Have
1. **Advanced Features**
   - Category method detection
   - Protocol conformance checking
   - Custom root class support

## Risk Assessment

### High Risk
- **GNUstep ABI Variations**: Different versions may have incompatible layouts
- **Runtime Initialization**: Detection timing relative to library loading

### Medium Risk  
- **Memory Corruption**: Debugging corrupted processes safely
- **Performance Impact**: Runtime introspection overhead

### Mitigation Strategies
- Extensive testing across GNUstep versions (1.28+)
- Conservative memory access with bounds checking
- Fallback mechanisms for unsupported configurations
- Performance monitoring and optimization

## Implementation Tasks

### [01_Runtime_Detection_Enhancement](../tasks/001_CORE_RUNTIME_FOUNDATION/01_Runtime_Detection_Enhancement.md)
**Priority**: P0 (Critical)  
**Effort**: 5 days  
**Description**: Improve runtime detection robustness and add version identification

### [02_ISA_Resolution_Implementation](../tasks/001_CORE_RUNTIME_FOUNDATION/02_ISA_Resolution_Implementation.md)
**Priority**: P0 (Critical)  
**Effort**: 8 days  
**Description**: Complete ISA-to-class name resolution for GNUstep objects

### [03_Class_Hierarchy_Traversal](../tasks/001_CORE_RUNTIME_FOUNDATION/03_Class_Hierarchy_Traversal.md)
**Priority**: P1 (High)  
**Effort**: 5 days  
**Description**: Implement superclass chain navigation and root class handling

### [04_Memory_Safety_Hardening](../tasks/001_CORE_RUNTIME_FOUNDATION/04_Memory_Safety_Hardening.md)
**Priority**: P1 (High)  
**Effort**: 3 days  
**Description**: Add bounds checking and error handling for memory operations

### [05_Performance_Optimization](../tasks/001_CORE_RUNTIME_FOUNDATION/05_Performance_Optimization.md)
**Priority**: P2 (Medium)  
**Effort**: 4 days  
**Description**: Implement caching and lazy loading strategies

## Definition of Done

- [ ] All virtual methods have complete, non-stub implementations
- [ ] ISA resolution works for 95% of standard GNUstep objects (NSString, NSArray, custom classes)
- [ ] Class hierarchy traversal handles all standard inheritance patterns
- [ ] Memory access is safe with proper bounds checking
- [ ] Performance meets <10ms target for class name resolution
- [ ] Integration tests pass with real GNUstep applications
- [ ] No regressions in existing LLDB functionality
- [ ] Code review completed and approved
- [ ] Documentation updated for new capabilities

## Success Metrics

### Functional
- **Detection Rate**: 100% accurate runtime detection
- **Resolution Rate**: 95%+ successful class name resolution
- **Stability**: 0 crashes during 1000 object introspections

### Performance  
- **Latency**: <10ms average class name resolution
- **Memory**: <1MB additional memory usage per debugging session
- **CPU**: <5% CPU overhead during active debugging

### Quality
- **Test Coverage**: 90%+ code coverage for core components
- **Bug Rate**: <1 critical bug per 1000 debugging operations
- **Compatibility**: Works with GNUstep 1.28, 1.29, and latest releases

---

*Epic Owner*: Development Team  
*Created*: December 2024  
*Target Completion*: Q1 2025
