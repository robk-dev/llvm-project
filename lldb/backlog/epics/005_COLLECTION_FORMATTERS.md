# Epic 005: Collection Formatters

## Overview
Implement comprehensive visualization for GNUstep collection classes (NSArray, NSMutableArray, NSDictionary, NSMutableDictionary, NSSet, NSMutableSet), providing rich summary information and child element access during debugging sessions.

## Business Value
- **Complex data visualization**: Navigate nested data structures efficiently
- **Debugging productivity**: Quickly understand collection contents and relationships
- **Memory debugging**: Identify collection-related memory issues and retain cycles
- **Apple parity**: Match Xcode's collection debugging experience

## Current Status: 10% Complete

### ✅ Completed
- Collection formatter infrastructure scaffolding
- Type registration framework ready
- Basic synthetic provider architecture in place

### 🔄 In Progress  
- NSArray summary provider implementation
- Collection size calculation methods
- Memory layout analysis for GNUstep collections

### 📋 Planned
- Complete NSArray/NSMutableArray visualization
- NSDictionary key-value pair display
- NSSet member enumeration
- Nested collection traversal
- Performance optimization for large collections
- Custom collection subclass support

## Technical Scope

### Collection Types Supported
1. **NSArray/NSMutableArray** - Ordered collections
   - Summary: "4 elements" or "empty array"
   - Children: [0], [1], [2], [3] with object descriptions
   - Performance: Efficient access by index

2. **NSDictionary/NSMutableDictionary** - Key-value mappings
   - Summary: "3 key-value pairs" or "empty dictionary"  
   - Children: [key1], [key2], [key3] with value descriptions
   - Key formatting: Show key objects properly

3. **NSSet/NSMutableSet** - Unordered unique collections
   - Summary: "5 unique objects" or "empty set"
   - Children: Object enumeration in deterministic order
   - Duplicate detection: Validate set uniqueness constraints

4. **NSCountedSet** - Sets with occurrence counting
   - Summary: "3 unique objects (7 total)" 
   - Children: Object with count information
   - Count display: Show occurrence counts per object

### Advanced Features
- **Nested Collections**: Arrays of dictionaries, dictionaries of arrays
- **Large Collection Handling**: Lazy loading for 1000+ element collections
- **Memory Efficiency**: Minimal memory overhead for collection inspection
- **Custom Subclasses**: Support for user-defined collection subclasses

### Key Files
- **Array Formatters**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepArrayFormatters.cpp`
- **Dictionary Formatters**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDictionaryFormatters.cpp`  
- **Set Formatters**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepSetFormatters.cpp`
- **Collection Utilities**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepCollectionUtilities.cpp`

## Dependencies
- **Epic 001**: Core runtime foundation (ISA resolution)
- **Epic 002**: Object introspection capabilities  
- **Epic 004**: String formatters (for displaying string elements)
- **GNUstep Base**: Understanding of collection internal structures
- **LLDB APIs**: SyntheticChildrenProvider and TypeSummary systems

## Acceptance Criteria

### Must Have

#### NSArray/NSMutableArray
1. **Summary Display**
   - Show element count: "4 elements", "1 element", "empty array"
   - Handle nil arrays: display as "nil" 
   - Performance: <10ms summary calculation for arrays up to 1000 elements

2. **Child Access**
   - Array elements accessible as [0], [1], [2], etc.
   - Element objects formatted according to their type (strings, numbers, etc.)
   - Handle nested arrays correctly (array of arrays)
   - Bounds checking: safe access beyond array length

#### NSDictionary/NSMutableDictionary  
1. **Summary Display**
   - Show pair count: "3 key-value pairs", "empty dictionary"
   - Handle nil dictionaries appropriately
   - Performance: <15ms summary for dictionaries with 500+ entries

2. **Key-Value Display**
   - Dictionary entries shown as expandable key nodes
   - Keys formatted according to their type  
   - Values formatted according to their type
   - Handle nested dictionaries (dictionary of dictionaries)

#### NSSet/NSMutableSet
1. **Summary Display**
   - Show member count: "5 unique objects", "empty set"
   - Emphasize uniqueness in display
   - Performance: <10ms summary for sets with 500+ members

2. **Member Access**
   - Set members enumerated in consistent order
   - Member objects formatted according to their type
   - Handle nested sets appropriately

### Should Have
1. **Performance Optimization**
   - Lazy loading for collections with 1000+ elements
   - Caching of collection metadata (size, type information)
   - Efficient memory usage during collection traversal
   - Timeout mechanisms for extremely large collections

2. **Advanced Display Options**
   - Configurable element display limits (show first N elements)
   - Summary-only mode for very large collections
   - Optional element index/key display in summaries
   - Type-specific formatting hints

3. **Error Handling**
   - Graceful handling of corrupted collection objects
   - Detection of retain cycles in nested collections
   - Safe traversal of partially deallocated collections
   - Clear error messages for unsupported collection types

### Could Have
1. **Advanced Features**
   - Collection diff visualization (compare two collections)
   - Search/filter functionality within collections
   - Custom collection subclass auto-detection
   - Performance metrics (access time, memory usage)

2. **Developer Experience**
   - Integration with IDE collection visualizers
   - Export functionality for large collections
   - Custom format strings for collection display
   - Debugging hints for collection performance issues

## Implementation Strategy

### Phase 1: NSArray Foundation (Week 1)
1. **Array Summary Provider**
   - Implement count calculation for NSArray objects
   - Handle empty and nil array cases
   - Add basic error handling and bounds checking

2. **Array Child Provider**
   - Implement indexed access ([0], [1], [2]...)
   - Integrate with existing object formatters
   - Handle nested array scenarios

### Phase 2: NSDictionary Support (Week 2)  
1. **Dictionary Summary Provider**
   - Implement key-value pair counting
   - Handle dictionary enumeration performance
   - Add memory-safe key/value access

2. **Dictionary Child Provider**
   - Implement key-based access patterns
   - Format keys and values according to their types
   - Handle nested dictionary structures

### Phase 3: NSSet and Performance (Week 3)
1. **Set Summary and Child Providers** 
   - Implement set member enumeration
   - Handle set uniqueness validation
   - Add NSCountedSet special handling

2. **Performance Optimization**
   - Implement lazy loading for large collections
   - Add caching strategies for repeated access
   - Optimize memory allocation patterns

### Phase 4: Polish and Advanced Features (Week 4)
1. **Error Handling Enhancement**
   - Robust corruption detection
   - Retain cycle detection in nested collections
   - Improved error messaging and recovery

2. **Integration and Testing**
   - Comprehensive testing with real applications
   - Performance benchmarking across collection sizes
   - Cross-platform compatibility validation

## Risk Assessment

### High Risk
- **Memory Layout Complexity**: GNUstep collections have complex internal structures
- **Performance Impact**: Large collection enumeration overhead
- **Nested Collection Handling**: Retain cycles and infinite recursion

### Medium Risk  
- **Type Detection**: Distinguishing between collection types accurately
- **Corruption Handling**: Safe traversal of damaged collection objects
- **Memory Management**: Avoiding leaks during collection introspection

### Low Risk
- **Basic Array Support**: Well-understood array access patterns
- **Summary Display**: Straightforward count calculations
- **LLDB Integration**: Synthetic provider API is stable

### Mitigation Strategies
- Start with simple NSArray, add complexity incrementally
- Implement robust bounds checking and timeout mechanisms
- Extensive testing with various collection sizes and nesting levels
- Memory usage monitoring and optimization
- Fallback to raw object display if collection formatting fails

## Tasks Breakdown

### [01_NSArray_Summary_Provider](../tasks/005_COLLECTION_FORMATTERS/01_NSArray_Summary_Provider.md)
**Priority**: P0 (Critical)  
**Effort**: 3 days  
**Description**: Implement NSArray element counting and summary display

### [02_NSArray_Child_Provider](../tasks/005_COLLECTION_FORMATTERS/02_NSArray_Child_Provider.md)
**Priority**: P0 (Critical)  
**Effort**: 4 days  
**Description**: Enable indexed access to NSArray elements ([0], [1], etc.)

### [03_NSDictionary_Summary_Provider](../tasks/005_COLLECTION_FORMATTERS/03_NSDictionary_Summary_Provider.md)
**Priority**: P1 (High)  
**Effort**: 4 days  
**Description**: Implement NSDictionary key-value pair counting and summary

### [04_NSDictionary_Child_Provider](../tasks/005_COLLECTION_FORMATTERS/04_NSDictionary_Child_Provider.md)
**Priority**: P1 (High)  
**Effort**: 5 days  
**Description**: Enable key-based access to dictionary values

### [05_NSSet_Formatters](../tasks/005_COLLECTION_FORMATTERS/05_NSSet_Formatters.md)
**Priority**: P1 (High)  
**Effort**: 3 days  
**Description**: Implement NSSet and NSMutableSet summary and child providers

### [06_Performance_Optimization](../tasks/005_COLLECTION_FORMATTERS/06_Performance_Optimization.md)
**Priority**: P2 (Medium)  
**Effort**: 4 days  
**Description**: Lazy loading, caching, and large collection optimization

### [07_Nested_Collection_Handling](../tasks/005_COLLECTION_FORMATTERS/07_Nested_Collection_Handling.md)
**Priority**: P2 (Medium)  
**Effort**: 3 days  
**Description**: Handle arrays of dictionaries, dictionaries of arrays, etc.

### [08_Error_Handling_Enhancement](../tasks/005_COLLECTION_FORMATTERS/08_Error_Handling_Enhancement.md)
**Priority**: P2 (Medium)  
**Effort**: 2 days  
**Description**: Corruption detection, retain cycle handling, error recovery

## Testing Strategy

### Unit Tests
- Collection summary calculation accuracy
- Child provider indexed/keyed access
- Memory safety with corrupted collections
- Performance testing with large collections (1K, 10K, 100K elements)
- Nested collection traversal correctness

### Integration Tests
- Real GNUstep application debugging scenarios
- Cross-platform compatibility (Linux, BSD, Windows)
- Memory leak detection during collection introspection
- Performance regression testing

### Edge Case Testing
- Empty collections (arrays, dictionaries, sets)
- Single-element collections
- Collections containing nil objects
- Deeply nested collections (arrays of arrays of dictionaries)
- Circular references and retain cycles
- Partially deallocated collections

## Definition of Done

- [ ] NSArray summary shows correct element count
- [ ] NSArray elements accessible via [0], [1], [2] notation
- [ ] NSMutableArray handled identically to NSArray
- [ ] NSDictionary summary shows correct key-value pair count  
- [ ] NSDictionary entries accessible via key expansion
- [ ] NSMutableDictionary handled identically to NSDictionary
- [ ] NSSet summary shows correct member count
- [ ] NSSet members enumerable in consistent order
- [ ] NSMutableSet handled identically to NSSet
- [ ] Performance targets met (<15ms for 1000-element collections)
- [ ] Memory safety validated with corrupted collections
- [ ] Nested collection scenarios work correctly
- [ ] Integration tests pass with real applications
- [ ] No regressions in existing functionality
- [ ] Code review completed and approved
- [ ] Documentation updated with examples

## Success Metrics

### Functional
- **Coverage**: 100% of standard collection types supported
- **Accuracy**: 99%+ correct summary and child access
- **Safety**: 0 crashes during collection formatting

### Performance  
- **Summary Speed**: <10ms for arrays/sets, <15ms for dictionaries (1K elements)
- **Memory Efficiency**: <10MB additional memory for large collection debugging
- **Scalability**: Support collections up to 100K elements efficiently

### Developer Experience
- **Navigation**: 90% reduction in time to find specific collection elements
- **Comprehension**: 75% improvement in understanding complex data structures
- **Reliability**: 99.9% successful collection formatting operations

---

*Epic Owner*: Development Team  
*Created*: August 2025  
*Target Completion*: Q2 2025  
*Dependencies*: Epic 001 (Core Runtime), Epic 002 (Object Introspection), Epic 004 (String Formatters)
