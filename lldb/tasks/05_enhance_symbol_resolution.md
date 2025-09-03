# Task 05: Enhance Runtime Symbol Resolution (COFF Support)

## Problem Statement
The current runtime symbol resolution may not handle Windows COFF import symbols correctly. On Windows, DLL-imported functions often have `__imp_` prefixes or other decorations that need special handling for successful symbol resolution.

From the logs, we can see that some runtime function lookups might be failing, leading to expression evaluation issues.

## Technical Background

### Windows DLL Import Mechanisms
1. **Direct Imports**: Function name as-is (e.g., `objc_msgSend`)
2. **Import Thunks**: Prefixed with `__imp_` (e.g., `__imp_objc_msgSend`)
3. **Decorated Names**: Leading/trailing underscores (e.g., `_objc_msgSend`)
4. **Weak Linking**: Alternative symbol names for optional functions

### Current Symbol Resolution Issues
The existing `ResolveAndCacheRuntimeSymbols()` method may only try one symbol name variant, missing imports that use different naming conventions.

## Implementation Strategy

### Step 1: Enhanced Symbol Resolution Helper
```cpp
// In GNUstepObjCRuntime.cpp
lldb::addr_t GNUstepObjCRuntime::ResolveRuntimeSymbol(const char* base_name) {
    if (!m_process)
        return LLDB_INVALID_ADDRESS;
    
    Target &target = m_process->GetTarget();
    
    // Generate symbol name variants for different platforms/compilers
    std::vector<std::string> candidates;
    
    // Direct name
    candidates.push_back(base_name);
    
    // Windows import thunk variants
    candidates.push_back(std::string("__imp_") + base_name);
    candidates.push_back(std::string("__imp__") + base_name);
    
    // Underscore variants (some Windows/MinGW builds)
    candidates.push_back(std::string("_") + base_name);
    
    // Try each candidate
    for (const auto& candidate : candidates) {
        SymbolContextList sc_list;
        target.GetImages().FindSymbolsWithNameAndType(
            ConstString(candidate), eSymbolTypeCode, sc_list);
        
        if (sc_list.GetSize() > 0) {
            SymbolContext sc;
            if (sc_list.GetContextAtIndex(0, sc) && sc.symbol) {
                Address symbol_addr = sc.symbol->GetAddress();
                if (symbol_addr.IsValid()) {
                    lldb::addr_t load_addr = symbol_addr.GetLoadAddress(&target);
                    if (load_addr != LLDB_INVALID_ADDRESS) {
                        LLDB_LOG(GetLog(LLDBLog::Language), 
                                 "GNUstep: Resolved '{0}' as '{1}' at {2:x}", 
                                 base_name, candidate, load_addr);
                        return load_addr;
                    }
                }
            }
        }
    }
    
    LLDB_LOG(GetLog(LLDBLog::Language), 
             "GNUstep: Failed to resolve symbol '{0}'", base_name);
    return LLDB_INVALID_ADDRESS;
}
```

### Step 2: Comprehensive Symbol Resolution
```cpp
void GNUstepObjCRuntime::ResolveAndCacheRuntimeSymbols() {
    if (m_symbols_resolved)
        return;
    
    LLDB_LOG(GetLog(LLDBLog::Language), 
             "GNUstep: Beginning runtime symbol resolution");
    
    // Core messaging functions
    m_objc_msgSend_addr = ResolveRuntimeSymbol("objc_msgSend");
    m_objc_msgSend_stret_addr = ResolveRuntimeSymbol("objc_msgSend_stret");
    m_objc_msgSend_fpret_addr = ResolveRuntimeSymbol("objc_msgSend_fpret");
    
    // Class and selector functions
    m_objc_getClass_addr = ResolveRuntimeSymbol("objc_getClass");
    m_sel_getUid_addr = ResolveRuntimeSymbol("sel_getUid");
    m_object_getClass_addr = ResolveRuntimeSymbol("object_getClass");
    
    // Method introspection
    m_class_getMethodImplementation_addr = ResolveRuntimeSymbol("class_getMethodImplementation");
    
    // Optional ARC functions (non-fatal if missing)
    m_objc_retain_addr = ResolveRuntimeSymbol("objc_retain");
    m_objc_release_addr = ResolveRuntimeSymbol("objc_release");
    m_objc_autoreleaseReturnValue_addr = ResolveRuntimeSymbol("objc_autoreleaseReturnValue");
    m_objc_retainAutoreleasedReturnValue_addr = ResolveRuntimeSymbol("objc_retainAutoreleasedReturnValue");
    
    // Log resolution results
    LLDB_LOG(GetLog(LLDBLog::Language),
             "GNUstep: Symbol resolution complete - "
             "objc_msgSend:{0:x} objc_getClass:{1:x} sel_getUid:{2:x}",
             m_objc_msgSend_addr, m_objc_getClass_addr, m_sel_getUid_addr);
    
    m_symbols_resolved = true;
}
```

### Step 3: Validate Critical Symbols
```cpp
bool GNUstepObjCRuntime::HasRequiredRuntimeSymbols() {
    // These are absolutely required for expression evaluation
    return (m_objc_msgSend_addr != LLDB_INVALID_ADDRESS &&
            m_objc_getClass_addr != LLDB_INVALID_ADDRESS &&
            m_sel_getUid_addr != LLDB_INVALID_ADDRESS);
}
```

### Step 4: Symbol Address Mapping for IRForTarget
```cpp
std::map<std::string, lldb::addr_t> GNUstepObjCRuntime::GetObjCRuntimeAddresses() {
    std::map<std::string, lldb::addr_t> addresses;
    
    // Ensure symbols are resolved
    ResolveAndCacheRuntimeSymbols();
    
    // Core runtime functions
    if (m_objc_msgSend_addr != LLDB_INVALID_ADDRESS)
        addresses["objc_msgSend"] = m_objc_msgSend_addr;
    
    if (m_objc_getClass_addr != LLDB_INVALID_ADDRESS)
        addresses["objc_getClass"] = m_objc_getClass_addr;
    
    if (m_sel_getUid_addr != LLDB_INVALID_ADDRESS)
        addresses["sel_getUid"] = m_sel_getUid_addr;
    
    if (m_object_getClass_addr != LLDB_INVALID_ADDRESS)
        addresses["object_getClass"] = m_object_getClass_addr;
    
    // Specialized messaging (optional)
    if (m_objc_msgSend_stret_addr != LLDB_INVALID_ADDRESS)
        addresses["objc_msgSend_stret"] = m_objc_msgSend_stret_addr;
    
    if (m_objc_msgSend_fpret_addr != LLDB_INVALID_ADDRESS)
        addresses["objc_msgSend_fpret"] = m_objc_msgSend_fpret_addr;
    
    // Custom utility functions
    if (m_cfstring_create_addr != LLDB_INVALID_ADDRESS)
        addresses["CFStringCreateWithBytes"] = m_cfstring_create_addr;
    
    LLDB_LOG(GetLog(LLDBLog::Language),
             "GNUstep: Providing {0} runtime addresses to IRForTarget", 
             addresses.size());
    
    return addresses;
}
```

### Step 5: Override LookupRuntimeSymbol for IRForTarget
```cpp
lldb::addr_t GNUstepObjCRuntime::LookupRuntimeSymbol(ConstString name) {
    std::string symbol_name = name.GetStringRef().str();
    
    // Check our cached symbols first
    auto addresses = GetObjCRuntimeAddresses();
    auto it = addresses.find(symbol_name);
    if (it != addresses.end()) {
        LLDB_LOG(GetLog(LLDBLog::Language),
                 "GNUstep: LookupRuntimeSymbol found cached '{0}' at {1:x}",
                 symbol_name, it->second);
        return it->second;
    }
    
    // Try dynamic resolution
    lldb::addr_t addr = ResolveRuntimeSymbol(symbol_name.c_str());
    if (addr != LLDB_INVALID_ADDRESS) {
        LLDB_LOG(GetLog(LLDBLog::Language),
                 "GNUstep: LookupRuntimeSymbol dynamically resolved '{0}' at {1:x}",
                 symbol_name, addr);
    }
    
    return addr;
}
```

## Platform-Specific Considerations

### Windows Symbol Variations
```cpp
// Windows-specific symbol name generation
std::vector<std::string> GenerateWindowsSymbolNames(const char* base_name) {
    std::vector<std::string> names;
    
    // Direct export
    names.push_back(base_name);
    
    // Import library thunks
    names.push_back(std::string("__imp_") + base_name);
    names.push_back(std::string("__imp__") + base_name);
    
    // MinGW/MSYS2 variants
    names.push_back(std::string("_") + base_name);
    names.push_back(std::string("__") + base_name);
    
    // DLL export decorations
    names.push_back(base_name + std::string("@4"));   // stdcall with 4 bytes
    names.push_back(base_name + std::string("@8"));   // stdcall with 8 bytes
    
    return names;
}
```

### Linux/POSIX Symbol Variations
```cpp
std::vector<std::string> GeneratePosixSymbolNames(const char* base_name) {
    std::vector<std::string> names;
    
    // Direct symbol
    names.push_back(base_name);
    
    // Some builds have underscores
    names.push_back(std::string("_") + base_name);
    
    return names;
}
```

## Files to Modify
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h`

## Testing Strategy

### Unit Tests
1. Test symbol resolution with various naming conventions
2. Verify caching behavior  
3. Test failure handling for missing symbols

### Integration Tests
1. Test on different Windows builds (MinGW, MSVC, MSYS2)
2. Test on Linux with different GNUstep installations
3. Verify IRForTarget symbol lookup integration

### Symbol Resolution Validation
```cpp
// Test function to validate symbol resolution
void TestSymbolResolution() {
    const char* test_symbols[] = {
        "objc_msgSend", "objc_getClass", "sel_getUid", 
        "object_getClass", "class_getMethodImplementation"
    };
    
    for (auto* symbol : test_symbols) {
        lldb::addr_t addr = ResolveRuntimeSymbol(symbol);
        if (addr != LLDB_INVALID_ADDRESS) {
            printf("✓ %s: 0x%llx\n", symbol, addr);
        } else {
            printf("✗ %s: NOT FOUND\n", symbol);
        }
    }
}
```

## Success Criteria
- [ ] All critical runtime symbols resolve correctly on Windows
- [ ] Symbol resolution works with different libobjc2 builds
- [ ] Caching improves performance (no repeated lookups)
- [ ] IRForTarget integration provides required addresses
- [ ] Cross-platform compatibility maintained

## Performance Considerations
- Cache symbol addresses to avoid repeated lookups
- Use efficient symbol search algorithms
- Minimize overhead during critical path operations

## Error Handling
- Graceful degradation when optional symbols are missing
- Clear error messages for missing critical symbols
- Fallback strategies for partial symbol availability

## Implementation Status
- [ ] Enhanced symbol resolution helper implemented
- [ ] Comprehensive symbol caching added
- [ ] Platform-specific name generation
- [ ] IRForTarget integration updated
- [ ] Testing completed across platforms
- [ ] Ready for review

## Dependencies
- Required for Task 04 (CFString implementation)
- Supports Task 03 (calling conventions)
- Enables Task 06 (DeclVendor completion)
