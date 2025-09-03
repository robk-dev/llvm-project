# Memory-Based Class Method Discovery - Ultra Deep Migration Plan

## 🎯 **Core Problem Analysis**

### Current Architecture Flaws
1. **Expression Evaluation During Interface Declaration** → Infinite Recursion
2. **Hardcoded Method Tables** → 500+ lines of manual maintenance
3. **Fighting Runtime Instead of Using It** → Architectural anti-pattern
4. **No Metaclass Support** → Array/dictionary literals fail

### Apple's Proven Solution
Apple's LLDB avoids recursion by:
1. **Direct Memory Reading** of runtime structures
2. **Metaclass ISA traversal** without expression evaluation
3. **Pure memory introspection** during interface declaration
4. **Expression evaluation only for runtime calls** outside declaration context

## 🏗️ **Phase 1: Foundation Architecture**

### 1.1 GNUstep Runtime Data Structure Analysis

**Critical Research Needed:**
```cpp
// GNUstep libobjc2 class structure (from runtime.h)
struct objc_class {
    Class isa;                    // offset 0: Metaclass pointer  
    Class superclass;             // offset 8: Superclass pointer
    const char *name;             // offset 16: Class name
    long version;                 // offset 24: Class version
    unsigned long info;           // offset 32: Class info flags
    unsigned long instance_size;  // offset 40: Instance size
    struct objc_ivar_list *ivars; // offset 48: Instance variables
    struct objc_method_list *methodLists; // offset 56: Method lists
    struct objc_cache *cache;     // offset 64: Method cache
    struct objc_protocol_list *protocols; // offset 72: Protocols
};

// Method list structure
struct objc_method_list {
    struct objc_method_list *obsolete;
    int method_count;
    struct objc_method methods[1];  // Variable length
};

// Individual method structure  
struct objc_method {
    SEL method_name;        // Method selector
    const char *method_types; // Type encoding string
    IMP method_imp;         // Implementation pointer
};
```

**Action Items:**
- [ ] Verify GNUstep runtime structure layout on Windows/Linux
- [ ] Document exact memory offsets for each field
- [ ] Test structure alignment differences between platforms
- [ ] Create robust structure reading utilities

### 1.2 Memory Reading Infrastructure Enhancement

**Extend GNUstepRuntimeV2API with Direct Memory Access:**

```cpp
class GNUstepRuntimeV2API {
private:
    // New memory-based introspection structures
    struct GNUstepClass {
        lldb::addr_t isa;           // Metaclass pointer
        lldb::addr_t superclass;    // Superclass pointer  
        lldb::addr_t name_ptr;      // Class name pointer
        std::string name;           // Resolved class name
        lldb::addr_t method_lists;  // Method list pointer
        // ... other fields
    };
    
    struct GNUstepMethodList {
        int method_count;
        std::vector<lldb::addr_t> method_ptrs;
    };
    
    struct GNUstepMethod {
        lldb::addr_t selector_ptr;
        lldb::addr_t types_ptr;
        lldb::addr_t implementation;
        std::string selector_name;
        std::string type_encoding;
    };

public:
    // Core memory reading methods
    llvm::Expected<GNUstepClass> ReadClassStructure(lldb::addr_t class_addr);
    llvm::Expected<GNUstepMethodList> ReadMethodList(lldb::addr_t method_list_addr);
    llvm::Expected<GNUstepMethod> ReadMethod(lldb::addr_t method_addr);
    
    // High-level metaclass introspection
    llvm::Expected<std::vector<MethodInfo>> GetClassMethodsFromMetaclass(const std::string &class_name);
};
```

## 🏗️ **Phase 2: Memory-Based Method Discovery Implementation**

### 2.1 Class Pointer Resolution

**Step 1: Safe Class Lookup Without Expression Evaluation**

```cpp
llvm::Expected<lldb::addr_t> GNUstepRuntimeV2API::FindClassPointer(const std::string &class_name) {
    // Option A: Use symbol table lookup for known classes
    // Look for class symbols like "OBJC_CLASS_$_NSArray" 
    
    // Option B: Cache class pointers from previous successful lookups
    auto cached = m_class_pointer_cache.find(class_name);
    if (cached != m_class_pointer_cache.end()) {
        return cached->second;
    }
    
    // Option C: Use CallRuntimeFunction ONLY if we're not in interface declaration context
    if (!m_in_interface_declaration) {
        return CallObjCGetClass(class_name);
    }
    
    return CreateError("Class lookup deferred to avoid recursion");
}
```

### 2.2 Direct Memory Class Structure Reading

**Step 2: Read Class Structure from Memory**

```cpp
llvm::Expected<GNUstepRuntimeV2API::GNUstepClass> 
GNUstepRuntimeV2API::ReadClassStructure(lldb::addr_t class_addr) {
    const size_t ptr_size = m_process->GetAddressByteSize();
    const size_t class_struct_size = ptr_size * 10; // Estimated size
    
    auto memory_or_error = ReadMemory(class_addr, class_struct_size);
    if (!memory_or_error) {
        return memory_or_error.takeError();
    }
    
    DataExtractor data(memory_or_error->data(), memory_or_error->size(),
                       m_process->GetByteOrder(), ptr_size);
    
    lldb::offset_t offset = 0;
    GNUstepClass cls;
    
    // Read class structure fields
    cls.isa = data.GetPointer(&offset);           // offset 0: metaclass
    cls.superclass = data.GetPointer(&offset);    // offset 8: superclass  
    cls.name_ptr = data.GetPointer(&offset);      // offset 16: name pointer
    offset += 8; // skip version
    offset += 8; // skip info flags
    offset += 8; // skip instance_size
    offset += 8; // skip ivars
    cls.method_lists = data.GetPointer(&offset);  // offset 56: method lists
    
    // Read class name string
    if (cls.name_ptr != 0) {
        auto name_or_error = ReadCStringFromTarget(cls.name_ptr);
        if (name_or_error) {
            cls.name = *name_or_error;
        }
    }
    
    return cls;
}
```

### 2.3 Metaclass Method Discovery

**Step 3: Apple's Pattern - Metaclass Instance Methods = Class Methods**

```cpp
llvm::Expected<std::vector<GNUstepRuntimeV2API::MethodInfo>>
GNUstepRuntimeV2API::GetClassMethodsFromMetaclass(const std::string &class_name) {
    // Step 1: Get class pointer (without expression evaluation)
    auto class_addr_or_error = FindClassPointer(class_name);
    if (!class_addr_or_error) {
        return class_addr_or_error.takeError();
    }
    
    // Step 2: Read class structure from memory
    auto class_or_error = ReadClassStructure(*class_addr_or_error);
    if (!class_or_error) {
        return class_or_error.takeError();
    }
    
    // Step 3: Get metaclass ISA (this is the key insight!)
    lldb::addr_t metaclass_addr = class_or_error->isa;
    if (metaclass_addr == 0) {
        return CreateError("Invalid metaclass pointer");
    }
    
    // Step 4: Read metaclass structure  
    auto metaclass_or_error = ReadClassStructure(metaclass_addr);
    if (!metaclass_or_error) {
        return metaclass_or_error.takeError();
    }
    
    // Step 5: Read method list from metaclass
    // CRITICAL: Metaclass instance methods = Class methods!
    return ReadMethodsFromMethodList(metaclass_or_error->method_lists, class_name);
}
```

### 2.4 Method List Memory Parsing

**Step 4: Parse Method Lists Directly from Memory**

```cpp
llvm::Expected<std::vector<GNUstepRuntimeV2API::MethodInfo>>
GNUstepRuntimeV2API::ReadMethodsFromMethodList(lldb::addr_t method_list_addr, 
                                                const std::string &class_name) {
    if (method_list_addr == 0) {
        return std::vector<MethodInfo>(); // Empty list
    }
    
    // Read method list header
    auto method_list_or_error = ReadMethodList(method_list_addr);
    if (!method_list_or_error) {
        return method_list_or_error.takeError();
    }
    
    std::vector<MethodInfo> methods;
    methods.reserve(method_list_or_error->method_count);
    
    // Read each method structure
    for (lldb::addr_t method_addr : method_list_or_error->method_ptrs) {
        auto method_or_error = ReadMethod(method_addr);
        if (!method_or_error) {
            continue; // Skip invalid methods
        }
        
        MethodInfo info;
        info.selector_name = method_or_error->selector_name;
        info.type_encoding = method_or_error->type_encoding;
        info.defining_class_name = class_name;
        info.implementation = method_or_error->implementation;
        
        methods.push_back(info);
    }
    
    return methods;
}
```

## 🏗️ **Phase 3: Integration and Recursion Prevention**

### 3.1 Declaration Context Tracking

**Critical: Prevent Recursion with Context Flags**

```cpp
class GNUstepRuntimeV2API {
private:
    // Recursion prevention
    thread_local static bool m_in_interface_declaration;
    std::unordered_set<std::string> m_classes_being_declared;
    
public:
    // RAII guard for interface declaration
    class InterfaceDeclarationGuard {
        bool m_prev_state;
    public:
        InterfaceDeclarationGuard() : m_prev_state(m_in_interface_declaration) {
            m_in_interface_declaration = true;
        }
        ~InterfaceDeclarationGuard() {
            m_in_interface_declaration = m_prev_state;
        }
    };
};
```

### 3.2 Modified FinishDecl Implementation

**Step 5: Replace Expression Evaluation with Memory Reading**

```cpp
// In GNUstepObjCDeclVendor::FinishDecl()
bool GNUstepObjCDeclVendor::FinishDecl(clang::ObjCInterfaceDecl *interface_decl) {
    const std::string class_name = interface_decl->getNameAsString();
    
    // Set declaration context to prevent recursion
    GNUstepRuntimeV2API::InterfaceDeclarationGuard guard;
    
    if (m_runtime_api) {
        // Instance methods (existing code - working)
        auto instance_methods_or_err = m_runtime_api->GetAllMethodsIncludingInherited(class_ptr);
        // ... handle instance methods
        
        // CLASS METHODS - NEW MEMORY-BASED APPROACH
        auto class_methods_or_err = m_runtime_api->GetClassMethodsFromMetaclass(class_name);
        if (class_methods_or_err) {
            for (const auto &method : *class_methods_or_err) {
                // Create method declaration without expression evaluation
                CreateMethodDecl(interface_decl, 
                               method.selector_name.c_str(),
                               method.type_encoding.c_str(),
                               false); // is_instance = false (class method)
            }
        } else {
            // Only fall back to hardcoded if memory reading fails
            AddHardcodedClassMethodsFallback(interface_decl, class_name);
        }
    }
    
    return true;
}
```

## 🏗️ **Phase 4: Advanced Memory Management**

### 4.1 Caching Strategy

**Optimize Performance with Smart Caching**

```cpp
class GNUstepRuntimeV2API {
private:
    // Multi-level caching
    std::unordered_map<std::string, lldb::addr_t> m_class_pointer_cache;
    std::unordered_map<lldb::addr_t, GNUstepClass> m_class_structure_cache;
    std::unordered_map<lldb::addr_t, std::vector<MethodInfo>> m_method_list_cache;
    
    // Cache invalidation on runtime changes
    uint64_t m_cache_generation = 0;
    
public:
    void InvalidateCache() { 
        m_cache_generation++;
        m_class_pointer_cache.clear();
        m_class_structure_cache.clear();
        m_method_list_cache.clear();
    }
};
```

### 4.2 Error Handling and Fallbacks

**Robust Error Recovery**

```cpp
llvm::Expected<std::vector<MethodInfo>>
GNUstepRuntimeV2API::GetClassMethodsFromMetaclass(const std::string &class_name) {
    try {
        // Primary: Memory-based approach
        auto result = GetClassMethodsFromMetaclassMemory(class_name);
        if (result) {
            return result;
        }
        
        Log *log = GetLog(LLDBLog::Language);
        LLDB_LOG(log, "[{0}] Memory-based class method discovery failed for {1}: {2}",
                 LLDB_LOG_TAG, class_name, llvm::toString(result.takeError()));
        
        // Secondary: Runtime function calls (only outside declaration context)
        if (!m_in_interface_declaration) {
            return GetClassMethodsViaRuntimeCalls(class_name);
        }
        
        // Tertiary: Return empty (triggers hardcoded fallback)
        return std::vector<MethodInfo>();
        
    } catch (const std::exception &e) {
        return CreateError("Exception in class method discovery: %s", e.what());
    }
}
```

## 🏗️ **Phase 5: Testing and Validation**

### 5.1 Unit Test Strategy

**Comprehensive Testing Framework**

```cpp
// Test cases to implement
class GNUstepMemoryIntrospectionTest : public ::testing::Test {
public:
    void TestClassStructureReading();
    void TestMetaclassTraversal();  
    void TestMethodListParsing();
    void TestRecursionPrevention();
    void TestArrayLiteralSupport();
    void TestDictionaryLiteralSupport();
    void TestCacheInvalidation();
    void TestErrorRecovery();
};
```

### 5.2 Integration Testing

**Real-World Validation**

```objc
// Must work after implementation:
@"Hello World"                    // ✅ Already working
[greeting length]                 // ✅ Already working  
@[@"test1", @"test2"]            // 🎯 Target: arrayWithObjects:count:
@{@"key": @"value"}              // 🎯 Target: dictionaryWithObjects:forKeys:count:
[NSArray arrayWithObjects:...]    // 🎯 Target: Direct class method calls
[NSNumber numberWithInt:42]       // 🎯 Target: Number literals
```

## 🎯 **Phase 6: Migration Timeline**

### Week 1: Foundation (Phase 1-2)
- [ ] Research GNUstep runtime structures
- [ ] Implement basic memory reading utilities
- [ ] Create class structure parsing
- [ ] Test memory access on Windows/Linux

### Week 2: Core Implementation (Phase 3)
- [ ] Implement metaclass traversal
- [ ] Add method list parsing
- [ ] Integrate with FinishDecl
- [ ] Test recursion prevention

### Week 3: Polish and Testing (Phase 4-5)
- [ ] Add caching and optimization
- [ ] Comprehensive error handling
- [ ] Unit and integration tests
- [ ] Performance benchmarking

### Week 4: Validation and Cleanup (Phase 6)
- [ ] Real-world testing with complex expressions
- [ ] Remove hardcoded fallback methods
- [ ] Documentation and code review
- [ ] Prepare for LLVM PR submission

## 🚀 **Success Metrics**

### Primary Goals
1. **✅ Array literals work**: `@[@"test1", @"test2"]`
2. **✅ Dictionary literals work**: `@{@"key": @"value"}`  
3. **✅ No recursion crashes**: Stable expression evaluation
4. **✅ No hardcoded methods**: Pure runtime introspection

### Secondary Goals
1. **🎯 Performance**: < 50ms for class method discovery
2. **🎯 Reliability**: 99.9% success rate for standard Foundation classes
3. **🎯 Maintainability**: < 200 lines total vs 500+ hardcoded lines
4. **🎯 Extensibility**: Works with custom ObjC classes automatically

## 🎉 **Final Outcome**

This architecture will transform our GNUstep LLDB plugin from a **maintenance nightmare of hardcoded methods** to a **robust, self-discovering runtime introspection system** that matches Apple's proven approach while avoiding all recursion issues.

**The key insight**: Apple never uses expression evaluation during interface declaration - they read memory directly and let the runtime data structures tell them what methods exist. We'll do exactly the same for GNUstep.