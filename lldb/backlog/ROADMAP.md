# GNUstep LLDB Runtime - Product Roadmap

## Project Vision

Create a comprehensive, Apple-quality debugging experience for GNUstep Objective-C applications using LLDB. This project bridges the gap between the rich debugging capabilities available for Apple's Objective-C runtime and the open-source GNUstep ecosystem, providing developers with powerful introspection, visualization, and debugging tools.

## Business Value

- **Developer Productivity**: Dramatically improve debugging experience for GNUstep developers
- **Ecosystem Growth**: Lower barriers to entry for Objective-C development on Linux/open platforms  
- **Feature Parity**: Bring Apple-like debugging capabilities to open-source Objective-C
- **Community Impact**: Enable better tooling for educational and commercial GNUstep projects

## Current State Assessment

### ✅ Completed (Q4 2024 - Q1 2025)
- **Basic Plugin Architecture**: Fully functional LLDB plugin that compiles and loads ✅
- **Runtime Detection**: Automatic detection of GNUstep libraries (`libgnustep-base.so`, `libobjc2`) ✅
- **Core Infrastructure**: Complete virtual method implementations, no undefined symbols ✅
- **Formatter Framework**: Modular formatter architecture with registration system ✅
- **Live Debugging Validation**: Confirmed runtime loads and detects GNUstep processes ✅
- **Build System Integration**: ccache enabled, optimized build pipeline ✅

### 🔄 In Progress (Q1 2025)
- **String Formatters**: Infrastructure exists, needs activation and testing
- **Object Introspection**: Basic framework in place, needs GNUstep-specific implementation  
- **Runtime Integration**: Core detection works, ISA resolution needs completion
- **Foundation Formatters**: NSNumber, NSDate basic implementation started

### 📋 Planned (Q1-Q4 2025)
- **Rich Collection Visualization**: NSArray, NSDictionary, NSSet formatters
- **Expression Evaluation**: Full `po` command support and method invocation
- **Advanced Debugging**: Exception breakpoints, method breakpoints
- **Performance Optimization**: Caching and lazy loading strategies
- **Cross-Platform Validation**: Comprehensive testing across platforms
- **Developer Experience**: Documentation, examples, and tooling

## Strategic Priorities

### Phase 1: Core Foundation (Q1 2025)
**Goal**: Establish robust runtime detection and basic object introspection

- Complete ISA resolution and class hierarchy traversal
- Implement fundamental object introspection capabilities  
- Validate runtime stability across GNUstep versions

### Phase 2: Essential Formatters (Q1-Q2 2025)  
**Goal**: Provide immediate visible value to developers

- Activate and perfect NSString formatters
- Implement NSArray/NSMutableArray visualization
- Add NSDictionary and NSSet formatters
- Create NSNumber and primitive object formatters

### Phase 3: Advanced Debugging (Q2-Q3 2025)
**Goal**: Feature parity with Apple's debugging experience

- Full expression evaluation (`po`, `expr` commands)
- Method invocation and property access
- Objective-C exception breakpoints
- Dynamic type resolution and casting

### Phase 4: Performance & Polish (Q3-Q4 2025)
**Goal**: Production-ready performance and developer experience

- Performance optimization and caching strategies
- Comprehensive testing and validation
- Documentation and developer guides
- Advanced features (blocks, categories, protocols)

## Risk Assessment

### Technical Risks
- **GNUstep Runtime Complexity**: Different memory layouts and ABI variations
- **LLDB API Evolution**: Keeping up with LLDB API changes across versions
- **Performance**: Runtime introspection overhead in large applications

### Mitigation Strategies  
- Extensive testing across GNUstep versions and platforms
- Modular architecture allowing graceful degradation
- Performance monitoring and optimization from early phases

## Success Metrics

### Developer Experience
- **Time to Debug**: 50% reduction in time to identify object state issues
- **Adoption Rate**: Usage by 80% of active GNUstep development teams  
- **Bug Report Quality**: Improved bug reports with rich object introspection

### Technical Excellence
- **Performance**: <100ms overhead for object introspection
- **Reliability**: 99.9% uptime in debugging sessions
- **Compatibility**: Support for 95% of GNUstep configurations

## Key File References

### Core Implementation
- `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`
- `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h`

### Formatter System
- `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/`
- `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepFormatters.cpp`

### Runtime Introspection
- `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.cpp`

### Test Cases
- `/home/robk/code/llvm-project/lldb/examples/simple_test.m`
- `/home/robk/code/llvm-project/lldb/examples/custom_class_test.m`

## Epic Overview

| Epic | Priority | Status | Business Value | Technical Complexity | Target |
|------|----------|--------|----------------|---------------------|---------|
| [001_CORE_RUNTIME_FOUNDATION](epics/001_CORE_RUNTIME_FOUNDATION.md) | High | 30% Complete | Foundation for all features | High | Q1 2025 |
| [002_OBJECT_INTROSPECTION](epics/002_OBJECT_INTROSPECTION.md) | High | 20% Complete | Enables all visualization | High | Q1 2025 |
| [003_DATA_FORMATTERS](epics/003_DATA_FORMATTERS.md) | High | 25% Complete | Immediate visible value | Medium | Q1 2025 |
| [004_STRING_FORMATTERS](epics/004_STRING_FORMATTERS.md) | High | 25% Complete | Most requested feature | Medium | Q1 2025 |
| [005_COLLECTION_FORMATTERS](epics/005_COLLECTION_FORMATTERS.md) | High | 10% Complete | Complex data navigation | Medium | Q2 2025 |
| [006_FOUNDATION_FORMATTERS](epics/006_FOUNDATION_FORMATTERS.md) | High | 5% Complete | Complete Foundation coverage | Medium | Q2 2025 |
| [007_EXPRESSION_EVALUATION](epics/007_EXPRESSION_EVALUATION.md) | High | 0% Complete | Core debugging workflow | High | Q2 2025 |
| [008_DYNAMIC_TYPE_RESOLUTION](epics/008_DYNAMIC_TYPE_RESOLUTION.md) | Medium | 0% Complete | Debugging accuracy | Medium | Q3 2025 |
| [009_BREAKPOINT_EXCEPTION_HANDLING](epics/009_BREAKPOINT_EXCEPTION_HANDLING.md) | Medium | 0% Complete | Advanced debugging | Medium | Q3 2025 |
| [010_PERFORMANCE_OPTIMIZATION](epics/010_PERFORMANCE_OPTIMIZATION.md) | Medium | 0% Complete | Production readiness | Medium | Q3 2025 |
| [011_TESTING_VALIDATION](epics/011_TESTING_VALIDATION.md) | Medium | 0% Complete | Quality assurance | Low | Q3 2025 |
| [012_CROSS_PLATFORM_COMPATIBILITY](epics/012_CROSS_PLATFORM_COMPATIBILITY.md) | Medium | 0% Complete | Ecosystem adoption | Medium | Q4 2025 |
| [013_DEVELOPER_EXPERIENCE](epics/013_DEVELOPER_EXPERIENCE.md) | Low | 0% Complete | Adoption & usability | Low | Q4 2025 |
| [014_ADVANCED_FEATURES](epics/014_ADVANCED_FEATURES.md) | Low | 0% Complete | Feature completeness | High | Q4 2025 |

---

*Last Updated: December 2024*  
*Next Review: Q1 2025 Planning*
