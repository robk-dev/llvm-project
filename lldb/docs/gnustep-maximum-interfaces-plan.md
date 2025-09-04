# GNUstep LLDB Plugin - Maximum Interface Support Implementation Plan

## Executive Summary
Transform the GNUstep LLDB plugin from a minimal hardcoded interface approach to a comprehensive dynamic runtime introspection system that discovers and exposes ALL GNUstep/Objective-C classes, methods, properties, and protocols at debug time.

## Current State Analysis

### What We Have (Minimal Approach)
- **Hardcoded interfaces** for 4 Foundation classes (NSString, NSNumber, NSArray, NSDictionary)
- **Static method definitions** manually added via AST manipulation
- **Basic expression evaluation** working for simple cases
- **CFStringCreateWithBytes shim** for string literals
- **Runtime function declarations** (objc_msgSend, objc_getClass, sel_getUid)

### Critical Blockers
1. **ISA Lookup Failure** - Custom classes fail with LLDB_INVALID_ADDRESS
2. **Limited Coverage** - Only 4 classes out of 300+ in Foundation
3. **No Property Support** - Properties not accessible in expressions
4. **No Protocol Support** - Protocol methods not available
5. **Static Nature** - Cannot adapt to runtime changes

## Implementation Phases

### Phase 1: Fix ISA Resolution (CRITICAL BLOCKER)
**Goal**: Enable custom class introspection by fixing ISA lookup

**Technical Requirements**:
```cpp
// In GNUstepObjCRuntimeIntrospector::GetISA()
// Current issue: Returns LLDB_INVALID_ADDRESS for custom classes
// Root cause: Incorrect memory layout assumptions for GNUstep class structure

struct gnustep_class_t {
    Class isa;           // Metaclass pointer
    Class super_class;   // Superclass pointer  
    const char *name;    // Class name
    long version;        // Class version
    unsigned long info;  // Runtime info flags
    long instance_size;  // Instance size
    struct objc_ivar_list *ivars;
    struct objc_method_list *methods;
    struct objc_cache *cache;
    struct objc_protocol_list *protocols;
};
```

**Implementation Steps**:
1. Analyze libobjc2 class structure layout (objc/runtime.h)
2. Update GetISA() to correctly read GNUstep class pointers
3. Handle compact vs full class representations
4. Validate with custom class test cases
5. Add defensive checks for corrupted memory

**Success Criteria**: 
- `po customObject` displays correct summary
- Custom class properties accessible

### Phase 2: Runtime Class Discovery
**Goal**: Dynamically enumerate all runtime classes

**Technical Approach**:
```cpp
class GNUstepRuntimeClassEnumerator {
    // Use runtime API or memory traversal
    std::vector<ObjCClassInfo> EnumerateAllClasses() {
        // Option 1: Runtime API (preferred)
        unsigned int count;
        Class *classes = objc_copyClassList(&count);
        
        // Option 2: Memory traversal of class list
        // Read __objc_class_list section
        
        // Build class info with metadata
        for (unsigned i = 0; i < count; i++) {
            ObjCClassInfo info;
            info.name = class_getName(classes[i]);
            info.isa = reinterpret_cast<ObjCISA>(classes[i]);
            info.superclass = class_getSuperclass(classes[i]);
            result.push_back(info);
        }
    }
};
```

**Implementation Steps**:
1. Create `GNUstepRuntimeClassEnumerator` class
2. Implement runtime API binding (dlsym or direct linking)
3. Fall back to memory traversal if API unavailable
4. Cache discovered classes with refresh mechanism
5. Hook into FindDecls to trigger enumeration

### Phase 3: Complete Method Discovery
**Goal**: Extract all methods with full signatures from runtime

**Technical Requirements**:
```cpp
struct MethodInfo {
    std::string selector;
    std::string type_encoding;  // e.g., "v@:i" = void, id, SEL, int
    bool is_instance;
    IMP implementation;
};

std::vector<MethodInfo> GetAllMethodsForClass(Class cls) {
    unsigned int count;
    Method *methods = class_copyMethodList(cls, &count);
    
    for (unsigned i = 0; i < count; i++) {
        MethodInfo info;
        info.selector = sel_getName(method_getName(methods[i]));
        info.type_encoding = method_getTypeEncoding(methods[i]);
        info.is_instance = true;
        info.implementation = method_getImplementation(methods[i]);
        result.push_back(info);
    }
    
    // Also get class methods from metaclass
    Class meta = object_getClass(cls);
    // ... repeat for metaclass
}
```

**Type Encoding Parser Enhancement**:
```cpp
// Enhance ParseTypeEncoding to handle all GNUstep types
// Current: Basic types (@, :, i, f, d, etc.)
// Needed: Structs {CGRect=dddd}, Unions, Bitfields, Blocks @?
CompilerType ParseGNUstepTypeEncoding(const char *encoding) {
    // Handle complex encodings:
    // - Struct: {name=type1type2...}
    // - Array: [count type]
    // - Pointer: ^type
    // - Bitfield: bnum
    // - Block: @?
    // - Atomic: Atype
}
```

### Phase 4: Property & Ivar Support
**Goal**: Complete property introspection with attributes

**Implementation**:
```cpp
struct PropertyInfo {
    std::string name;
    std::string type_encoding;
    std::string getter_sel;
    std::string setter_sel;
    PropertyAttributes attributes; // strong, weak, copy, etc.
};

std::vector<PropertyInfo> GetAllProperties(Class cls) {
    unsigned int count;
    objc_property_t *props = class_copyPropertyList(cls, &count);
    
    for (unsigned i = 0; i < count; i++) {
        PropertyInfo info;
        info.name = property_getName(props[i]);
        
        // Parse attributes string "T@\"NSString\",&,N,V_name"
        const char *attrs = property_getAttributes(props[i]);
        ParsePropertyAttributes(attrs, info);
        
        // Synthesize getter/setter methods in AST
        CreatePropertyAccessors(info);
    }
}
```

### Phase 5: Protocol Support
**Goal**: Expose protocol conformance and required methods

**Implementation**:
```cpp
struct ProtocolInfo {
    std::string name;
    std::vector<MethodInfo> required_methods;
    std::vector<MethodInfo> optional_methods;
    std::vector<PropertyInfo> properties;
};

void AddProtocolConformance(clang::ObjCInterfaceDecl *interface,
                           Class cls) {
    unsigned int count;
    Protocol **protocols = class_copyProtocolList(cls, &count);
    
    for (unsigned i = 0; i < count; i++) {
        // Create protocol decl in AST
        auto *proto = CreateProtocolDecl(protocol_getName(protocols[i]));
        
        // Add methods from protocol
        AddProtocolMethods(proto, protocols[i]);
        
        // Mark interface as conforming
        interface->addProtocol(proto);
    }
}
```

### Phase 6: Categories & Extensions
**Goal**: Support dynamically loaded categories

**Technical Challenge**: Categories can be loaded at runtime via dlopen

**Implementation**:
```cpp
class CategoryMonitor {
    // Monitor for new categories
    void MonitorCategoryLoading() {
        // Hook into objc_addCategory or monitor dyld notifications
        // Update AST when new categories detected
    }
    
    // Merge category methods into main interface
    void MergeCategoryMethods(ObjCInterfaceDecl *interface,
                             ObjCCategoryDecl *category) {
        // Add methods from category to interface
        // Handle property extensions
        // Update method forwarding table
    }
};
```

### Phase 7: Performance Optimization
**Goal**: Sub-100ms response time for expression evaluation

**Caching Strategy**:
```cpp
class InterfaceCache {
    struct CacheEntry {
        ObjCISA isa;
        clang::ObjCInterfaceDecl *decl;
        uint64_t last_accessed;
        uint32_t method_count;
        uint32_t version;  // Detect runtime changes
    };
    
    std::unordered_map<ObjCISA, CacheEntry> cache;
    
    // LRU eviction for memory management
    void EvictLRU(size_t max_entries = 1000);
    
    // Lazy population - only build full interface when accessed
    ObjCInterfaceDecl* GetOrCreate(ObjCISA isa);
    
    // Incremental updates instead of full rebuild
    void UpdateInterface(ObjCISA isa, MethodInfo new_method);
};
```

**Optimization Techniques**:
1. **Lazy Loading**: Only populate interfaces when accessed
2. **Batch Operations**: Group AST modifications
3. **Memory Pooling**: Reuse AST nodes
4. **Background Loading**: Preload common classes
5. **Delta Updates**: Track runtime changes incrementally

## Technical Dependencies

### Required Runtime Functions
```cpp
// Core enumeration (must have)
Class* objc_copyClassList(unsigned int *count);
Method* class_copyMethodList(Class cls, unsigned int *count);
objc_property_t* class_copyPropertyList(Class cls, unsigned int *count);
Protocol** class_copyProtocolList(Class cls, unsigned int *count);
Ivar* class_copyIvarList(Class cls, unsigned int *count);

// Method introspection
SEL method_getName(Method m);
const char* method_getTypeEncoding(Method m);
IMP method_getImplementation(Method m);

// Property introspection  
const char* property_getName(objc_property_t prop);
const char* property_getAttributes(objc_property_t prop);

// Protocol introspection
const char* protocol_getName(Protocol *p);
struct objc_method_description* protocol_copyMethodDescriptionList(
    Protocol *p, BOOL isRequiredMethod, BOOL isInstanceMethod, 
    unsigned int *count);
```

### Memory Layout Structures
```cpp
// Must match libobjc2 exactly
struct objc_object {
    Class isa;
};

struct objc_class {
    Class isa;
    Class super_class;
    const char *name;
    long version;
    unsigned long info;
    long instance_size;
    struct objc_ivar_list *ivars;
    struct objc_method_list *methods;
    struct objc_cache *cache;
    struct objc_protocol_list *protocols;
};

struct objc_method {
    SEL method_name;
    const char *method_types;
    IMP method_imp;
};
```

## Testing Strategy

### Unit Tests
```cpp
// Test type encoding parser
TEST(TypeEncoding, ComplexStruct) {
    auto type = ParseGNUstepTypeEncoding("{CGRect={CGPoint=dd}{CGSize=dd}}");
    EXPECT_TRUE(type.IsStructType());
    EXPECT_EQ(type.GetNumFields(), 2);
}

// Test class enumeration
TEST(RuntimeEnum, FindAllClasses) {
    auto classes = EnumerateAllClasses();
    EXPECT_GT(classes.size(), 100); // Foundation has 300+ classes
    EXPECT_TRUE(HasClass(classes, "NSString"));
}

// Test method discovery
TEST(MethodDiscovery, NSArrayMethods) {
    auto methods = GetAllMethodsForClass("NSArray");
    EXPECT_TRUE(HasMethod(methods, "objectAtIndex:"));
    EXPECT_TRUE(HasMethod(methods, "count"));
}
```

### Integration Tests
```python
# Test expression evaluation with discovered interfaces
def test_custom_class_properties():
    lldb_session = start_lldb("custom_class_test")
    lldb_session.breakpoint("main")
    lldb_session.run()
    
    # Should work with dynamic discovery
    result = lldb_session.expression("account.balance")
    assert result == "1000.00"
    
    result = lldb_session.expression("[account description]")
    assert "BankAccount" in result
```

### Performance Benchmarks
```cpp
TEST(Performance, ExpressionEvaluation) {
    auto start = high_resolution_clock::now();
    EvaluateExpression("[NSString stringWithFormat:@\"%d\", 42]");
    auto end = high_resolution_clock::now();
    
    auto duration = duration_cast<milliseconds>(end - start);
    EXPECT_LT(duration.count(), 100); // Sub-100ms requirement
}

TEST(Performance, ClassEnumeration) {
    auto start = high_resolution_clock::now();
    EnumerateAllClasses();
    auto end = high_resolution_clock::now();
    
    auto duration = duration_cast<milliseconds>(end - start);
    EXPECT_LT(duration.count(), 500); // Initial enumeration < 500ms
}
```

## Migration Path

### Step 1: Parallel Implementation
- Keep existing hardcoded interfaces as fallback
- Implement dynamic discovery alongside
- Use feature flag to toggle approaches

### Step 2: Gradual Rollout
```cpp
bool UsesDynamicDiscovery() {
    // Start with opt-in
    if (getenv("LLDB_GNUSTEP_DYNAMIC_DISCOVERY"))
        return true;
        
    // Gradually enable for specific classes
    static std::set<std::string> dynamic_classes = {
        "NSDate", "NSData", "NSURL", "NSError"
    };
    return dynamic_classes.count(class_name) > 0;
}
```

### Step 3: Full Migration
- Remove hardcoded interfaces after validation
- Make dynamic discovery the default
- Keep minimal bootstrap interfaces for core runtime

## Success Metrics

### Functional Metrics
- ✅ All Foundation classes accessible (300+)
- ✅ All methods discoverable and callable
- ✅ Properties work with dot syntax
- ✅ Custom classes fully introspectable
- ✅ Protocols and categories supported
- ✅ Dynamic loading handled

### Performance Metrics  
- ✅ First expression < 100ms
- ✅ Subsequent expressions < 50ms  
- ✅ Class enumeration < 500ms initial
- ✅ Cache hit rate > 90%
- ✅ Memory usage < 50MB overhead

### Quality Metrics
- ✅ Zero crashes from malformed data
- ✅ Graceful degradation on errors
- ✅ 100% backward compatibility
- ✅ Thread-safe implementation
- ✅ Comprehensive test coverage

## Risk Mitigation

### Risk: Runtime API Unavailable
**Mitigation**: Implement fallback memory traversal using sections

### Risk: Performance Degradation
**Mitigation**: Aggressive caching, lazy loading, background prefetch

### Risk: Memory Layout Changes
**Mitigation**: Version detection, multiple layout support

### Risk: Thread Safety Issues  
**Mitigation**: Read-only access, atomic operations, proper locking

### Risk: Breaking Existing Code
**Mitigation**: Feature flags, gradual rollout, extensive testing

## Timeline Estimate

- **Phase 1 (ISA Fix)**: 2-3 days - Critical blocker
- **Phase 2 (Class Discovery)**: 3-4 days  
- **Phase 3 (Method Discovery)**: 4-5 days
- **Phase 4 (Properties/Ivars)**: 3-4 days
- **Phase 5 (Protocols)**: 2-3 days
- **Phase 6 (Categories)**: 2-3 days  
- **Phase 7 (Optimization)**: 3-4 days
- **Testing & Integration**: 5-7 days

**Total**: 4-5 weeks for full implementation

## Conclusion

This plan transforms the GNUstep LLDB plugin from a minimal proof-of-concept into a production-ready system with complete runtime introspection. By shifting from static hardcoded interfaces to dynamic discovery, we achieve true "maximum" support - every class, method, property, and protocol in the runtime becomes fully accessible in LLDB expressions.

The phased approach allows incremental progress with Phase 1 (ISA fix) being the critical enabler for everything else. Each phase delivers value independently while building toward the complete solution.