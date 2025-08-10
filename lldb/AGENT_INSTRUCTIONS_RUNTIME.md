# GNUstep LLDB Plugin - Runtime Introspection Instructions

## Overview
This document provides detailed instructions for fixing and enhancing runtime introspection capabilities in the GNUstep LLDB plugin.

## Critical Issue: ISA Display and Custom Class Resolution

### Problem Statement
Custom classes show `<unknown type>` instead of their actual class names, and properties/ivars are not accessible. This is the most critical runtime issue preventing production use.

### Root Cause Analysis
**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.cpp`

The issue stems from:
1. `CallRuntimeFunction()` returns `LLDB_INVALID_ADDRESS` (stub implementation)
2. Missing `GetDynamicTypeAndAddress()` implementation
3. Incomplete ISA resolution for custom classes

### Solution Architecture

#### Step 1: Implement CallRuntimeFunction
```cpp
// GNUstepObjCRuntime.cpp
lldb::addr_t GNUstepObjCRuntime::CallRuntimeFunction(
    const char *name, 
    const std::vector<lldb::addr_t> &args) {
    
    // 1. Get the runtime function address
    ModuleSP module_sp = GetRuntimeModule();
    if (!module_sp) return LLDB_INVALID_ADDRESS;
    
    const Symbol *symbol = module_sp->FindFirstSymbolWithNameAndType(
        ConstString(name), eSymbolTypeCode);
    if (!symbol) return LLDB_INVALID_ADDRESS;
    
    // 2. Prepare function call
    Thread *thread = m_process->GetThreadList().GetSelectedThread().get();
    if (!thread) return LLDB_INVALID_ADDRESS;
    
    // 3. Use expression evaluator
    EvaluateExpressionOptions options;
    options.SetUnwindOnError(true);
    options.SetIgnoreBreakpoints(true);
    options.SetTryAllThreads(false);
    
    // 4. Build and execute call
    StreamString expr;
    expr.Printf("((void*)%s)(", name);
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) expr.Printf(", ");
        expr.Printf("0x%" PRIx64, args[i]);
    }
    expr.Printf(")");
    
    ValueObjectSP result_sp;
    ExecutionContext exe_ctx = thread->GetStackFrameAtIndex(0)->GetExecutionContext();
    
    auto eval_result = UserExpression::Evaluate(
        exe_ctx, options, expr.GetString(), "", result_sp);
    
    if (eval_result != eExpressionCompleted)
        return LLDB_INVALID_ADDRESS;
    
    return result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
}
```

#### Step 2: Implement GetDynamicTypeAndAddress
```cpp
// GNUstepObjCRuntime.cpp
TypeAndOrName GNUstepObjCRuntime::GetDynamicTypeAndAddress(
    ValueObject &in_value,
    lldb::DynamicValueType use_dynamic,
    TypeAndOrName &class_type_or_name,
    Address &address,
    Value::ValueType &value_type) {
    
    // 1. Get object address
    lldb::addr_t obj_addr = in_value.GetPointerValue();
    if (obj_addr == LLDB_INVALID_ADDRESS)
        return TypeAndOrName();
    
    // 2. Read ISA pointer
    Process *process = in_value.GetProcessSP().get();
    if (!process)
        return TypeAndOrName();
    
    Status error;
    lldb::addr_t isa_addr = process->ReadPointerFromMemory(obj_addr, error);
    if (error.Fail())
        return TypeAndOrName();
    
    // 3. Check for tagged pointer
    if (IsTaggedPointer(isa_addr)) {
        return GetDynamicTypeForTaggedPointer(isa_addr);
    }
    
    // 4. Resolve class from ISA
    ObjCISA isa(isa_addr);
    ConstString class_name = GetClassNameFromISA(isa);
    
    if (!class_name)
        return TypeAndOrName();
    
    // 5. Build TypeAndOrName
    TypeAndOrName ret(class_name);
    
    // 6. Try to find actual type
    TypeList type_list;
    if (GetModule()->FindTypes(class_name, true, 1, type_list)) {
        ret.SetCompilerType(type_list.GetTypeAtIndex(0)->GetForwardCompilerType());
    }
    
    // 7. Set output parameters
    value_type = Value::eValueTypeScalar;
    address.SetRawAddress(obj_addr);
    
    return ret;
}
```

#### Step 3: Fix ISA Resolution for Custom Classes
```cpp
// GNUstepObjCRuntimeIntrospector.cpp
ConstString GNUstepObjCRuntimeIntrospector::GetClassNameFromISA(ObjCISA isa) {
    // 1. Handle nil ISA
    if (!isa)
        return ConstString();
    
    // 2. Check cache first
    auto it = m_isa_to_name_cache.find(isa);
    if (it != m_isa_to_name_cache.end())
        return it->second;
    
    // 3. Read class structure
    Process *process = GetProcess();
    if (!process)
        return ConstString();
    
    Status error;
    lldb::addr_t class_addr = isa.GetAddress();
    
    // 4. Read class name from class structure
    // GNUstep class layout:
    // struct objc_class {
    //     Class isa;          // offset 0
    //     Class super_class;  // offset 8
    //     const char *name;   // offset 16
    //     ...
    // };
    
    lldb::addr_t name_ptr = process->ReadPointerFromMemory(
        class_addr + 16, error);
    
    if (error.Fail() || name_ptr == LLDB_INVALID_ADDRESS)
        return ConstString();
    
    // 5. Read the class name string
    char name_buffer[256];
    size_t bytes_read = process->ReadCStringFromMemory(
        name_ptr, name_buffer, sizeof(name_buffer), error);
    
    if (error.Fail() || bytes_read == 0)
        return ConstString();
    
    // 6. Cache and return
    ConstString class_name(name_buffer);
    m_isa_to_name_cache[isa] = class_name;
    
    return class_name;
}
```

## Enhancement: Property and Method Introspection

### Implement Property Enumeration
```cpp
// GNUstepObjCRuntimeIntrospector.cpp
std::vector<PropertyInfo> 
GNUstepObjCRuntimeIntrospector::GetClassProperties(ObjCISA isa) {
    std::vector<PropertyInfo> properties;
    
    // 1. Get runtime module
    ModuleSP runtime_module = GetRuntimeModule();
    if (!runtime_module)
        return properties;
    
    // 2. Call class_copyPropertyList
    std::vector<lldb::addr_t> args = {isa.GetAddress(), 0};
    lldb::addr_t prop_list = CallRuntimeFunction(
        "class_copyPropertyList", args);
    
    if (prop_list == LLDB_INVALID_ADDRESS)
        return properties;
    
    // 3. Read property count
    Process *process = GetProcess();
    Status error;
    uint32_t count = process->ReadUnsignedIntegerFromMemory(
        args[1], 4, 0, error);
    
    // 4. Iterate properties
    for (uint32_t i = 0; i < count; i++) {
        lldb::addr_t prop_addr = process->ReadPointerFromMemory(
            prop_list + (i * 8), error);
        
        if (error.Success()) {
            PropertyInfo info = ReadPropertyInfo(prop_addr);
            properties.push_back(info);
        }
    }
    
    // 5. Free property list
    CallRuntimeFunction("free", {prop_list});
    
    return properties;
}
```

### Implement Method Enumeration
```cpp
std::vector<MethodInfo>
GNUstepObjCRuntimeIntrospector::GetClassMethods(ObjCISA isa) {
    std::vector<MethodInfo> methods;
    
    // Similar pattern to property enumeration
    // Call class_copyMethodList
    // Parse method structures
    // Extract selector names and type encodings
    
    return methods;
}
```

## Performance Optimizations

### 1. Implement ISA Cache
```cpp
class ISACache {
private:
    std::unordered_map<lldb::addr_t, ClassInfo> m_cache;
    std::mutex m_mutex;
    
public:
    bool GetClassInfo(lldb::addr_t isa, ClassInfo &info) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.find(isa);
        if (it != m_cache.end()) {
            info = it->second;
            return true;
        }
        return false;
    }
    
    void CacheClassInfo(lldb::addr_t isa, const ClassInfo &info) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache[isa] = info;
    }
    
    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.clear();
    }
};
```

### 2. Batch Memory Reads
```cpp
// Read multiple values in one operation
Status ReadMultiplePointers(
    Process *process,
    lldb::addr_t base_addr,
    size_t count,
    std::vector<lldb::addr_t> &pointers) {
    
    size_t bytes_to_read = count * process->GetAddressByteSize();
    std::vector<uint8_t> buffer(bytes_to_read);
    
    Status error;
    size_t bytes_read = process->ReadMemory(
        base_addr, buffer.data(), bytes_to_read, error);
    
    if (error.Success() && bytes_read == bytes_to_read) {
        DataExtractor data(buffer.data(), bytes_read, 
                          process->GetByteOrder(),
                          process->GetAddressByteSize());
        
        lldb::offset_t offset = 0;
        for (size_t i = 0; i < count; i++) {
            pointers.push_back(data.GetAddress(&offset));
        }
    }
    
    return error;
}
```

### 3. Lazy Loading
```cpp
class LazyClassInfo {
private:
    lldb::addr_t m_isa;
    mutable bool m_loaded;
    mutable ClassInfo m_info;
    mutable std::mutex m_mutex;
    
public:
    const ClassInfo& GetInfo() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_loaded) {
            LoadClassInfo(m_isa, m_info);
            m_loaded = true;
        }
        return m_info;
    }
};
```

## Memory Layout Documentation

### GNUstep/libobjc2 Class Structure
```cpp
// From libobjc2/runtime.h
struct objc_class {
    Class isa;                      // offset 0:  Metaclass pointer
    Class super_class;              // offset 8:  Superclass pointer  
    const char *name;               // offset 16: Class name
    long version;                   // offset 24: Version info
    unsigned long info;             // offset 32: Flags
    long instance_size;             // offset 40: Instance size
    struct objc_ivar_list *ivars;  // offset 48: Instance variables
    struct objc_method_list *methods; // offset 56: Methods
    struct objc_cache *cache;      // offset 64: Method cache
    struct objc_protocol_list *protocols; // offset 72: Protocols
    const char *ivar_layout;       // offset 80: Strong ivar layout
    struct objc_class_ext *ext;    // offset 88: Extended info
};

struct objc_object {
    Class isa;                      // offset 0: Class pointer
};

struct objc_ivar {
    const char *name;               // offset 0: Ivar name
    const char *type;               // offset 8: Type encoding
    int offset;                     // offset 16: Byte offset
    int size;                       // offset 20: Size in bytes
    int alignment;                  // offset 24: Alignment
};

struct objc_method {
    SEL selector;                   // offset 0: Method selector
    const char *types;              // offset 8: Type encoding
    IMP imp;                        // offset 16: Implementation
};
```

## Testing Requirements

### Unit Tests for Runtime Introspection
```cpp
// test_runtime_introspection.cpp
TEST(GNUstepRuntime, ResolveCustomClassISA) {
    // Create custom class instance
    // Verify ISA resolution returns correct class name
    // Verify properties are accessible
    // Verify methods are enumerable
}

TEST(GNUstepRuntime, CallRuntimeFunction) {
    // Test calling objc_getClass
    // Test calling class_getName
    // Test error handling for invalid functions
}

TEST(GNUstepRuntime, GetDynamicType) {
    // Test with NSString
    // Test with custom class
    // Test with nil object
    // Test with tagged pointer
}
```

### Integration Tests
```bash
# test_custom_class_introspection.lldb
b main
r
expr BankAccount *account = [[BankAccount alloc] init]
expr account.accountNumber = 12345
expr account.owner = @"John Doe"
po account
# Should show: BankAccount(12345, owner=John Doe, ...)

expr -d run -- account
# Should show dynamic type and all properties

image lookup -t BankAccount
# Should show class layout
```

## Performance Targets

All runtime operations must meet:
- ISA resolution: < 1ms
- Property enumeration: < 5ms for typical class
- Method enumeration: < 10ms for typical class
- CallRuntimeFunction: < 20ms per call
- Cache hit ratio: > 90% for repeated lookups

## Success Criteria

Runtime introspection is complete when:
1. Custom classes display correct class names
2. Properties are visible and accessible
3. Methods can be enumerated
4. ISA resolution works for all class types
5. Performance targets are met
6. No crashes or hangs
7. Cache invalidation works correctly
8. Thread-safe operation verified

## Debugging Tips

1. Enable debug output:
```cpp
#define DEBUG_RUNTIME 1
#if DEBUG_RUNTIME
    printf("ISA resolution: 0x%llx -> %s\n", isa, class_name);
#endif
```

2. Validate memory reads:
```cpp
if (!process->IsAlive()) {
    return error("Process not alive");
}
if (!process->IsValid(address)) {
    return error("Invalid address");
}
```

3. Handle runtime version differences:
```cpp
if (GetRuntimeVersion() >= OBJC_VERSION_2_1) {
    // Use modern runtime functions
} else {
    // Fall back to legacy approach
}
```

## Next Steps

1. Fix CallRuntimeFunction implementation (Priority 0)
2. Implement GetDynamicTypeAndAddress (Priority 0)
3. Fix ISA resolution for custom classes (Priority 0)
4. Add property enumeration (Priority 1)
5. Add method enumeration (Priority 1)
6. Implement caching layer (Priority 2)
7. Add comprehensive tests (Priority 2)
8. Document all memory layouts (Priority 3)