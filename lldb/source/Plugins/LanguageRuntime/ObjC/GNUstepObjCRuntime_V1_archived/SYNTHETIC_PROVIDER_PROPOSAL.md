# Proposal: Add Synthetic Children Support to GNUstepObjCRuntime Plugin

## Current State
The GNUstepObjCRuntime plugin has excellent ISA resolution but lacks synthetic children providers for object introspection in the debugger.

## Proposed Enhancement

### 1. Add Synthetic Provider Classes to GNUstepObjCRuntime.h

```cpp
// In GNUstepObjCRuntime.h, add:

class GNUstepSyntheticProvider : public SyntheticChildren {
public:
    GNUstepSyntheticProvider(ValueObject &valobj);
    
    size_t CalculateNumChildren() override;
    lldb::ValueObjectSP GetChildAtIndex(size_t idx) override;
    size_t GetIndexOfChildWithName(ConstString name) override;
    bool Update() override;
    bool MightHaveChildren() override { return true; }
    
private:
    struct IvarInfo {
        std::string name;
        std::string type_encoding;
        ptrdiff_t offset;
    };
    
    std::vector<IvarInfo> m_ivars;
    bool m_needs_update = true;
};
```

### 2. Implement Ivar Discovery Using Runtime APIs

```cpp
// In GNUstepObjCRuntime.cpp:

bool GNUstepSyntheticProvider::Update() {
    m_ivars.clear();
    
    // Get object address and ISA
    addr_t obj_addr = m_valobj.GetValueAsUnsigned(0);
    if (!obj_addr) return false;
    
    // Use ISAResolver to get class
    GNUstepObjCRuntime *runtime = GetGNUstepRuntime(m_valobj);
    std::string class_name = runtime->GetClassNameFromObject(obj_addr);
    
    // Get ivars using runtime APIs (class_copyIvarList)
    // This avoids expression evaluation and infinite recursion
    
    return true;
}
```

### 3. Register the Provider

```cpp
void GNUstepObjCRuntime::Initialize() {
    // ... existing code ...
    
    // Register synthetic provider
    SyntheticChildrenProviders::Add(
        ConstString("^NS.*"),  // NSObject and subclasses
        lldb::eSyntheticProviderGNUstep,
        "GNUstep object synthetic children",
        CreateSyntheticProvider<GNUstepSyntheticProvider>);
        
    SyntheticChildrenProviders::Add(
        ConstString("^GS.*"),  // GNUstep classes
        lldb::eSyntheticProviderGNUstep,
        "GNUstep object synthetic children",
        CreateSyntheticProvider<GNUstepSyntheticProvider>);
}
```

## Benefits

1. **Native Performance** - No Python overhead
2. **ISA Resolution Built-in** - Uses existing ISAResolver
3. **No Expression Evaluation** - Direct memory access via Process API
4. **No Infinite Recursion** - ISA is not shown as a child
5. **Proper Type Resolution** - Can use LLDB's type system directly
6. **Thread Safe** - Native code with proper locking

## Implementation Steps

1. Add synthetic provider class declarations to GNUstepObjCRuntime.h
2. Implement the provider in GNUstepObjCRuntime.cpp
3. Add runtime API calls for ivar discovery (class_copyIvarList, etc.)
4. Register the providers in Initialize()
5. Test with VS Code to ensure no hanging and proper expansion

## Alternative: Hybrid Approach

Keep Python for flexibility but have it call into the native plugin for ISA resolution:

```python
# In Python bridge
class HybridProvider:
    def get_child_at_index(self, index):
        # Use LLDB's native type system
        runtime = self.valobj.GetProcess().GetObjCLanguageRuntime()
        
        # This would use the GNUstepObjCRuntime's ISAResolver
        class_desc = runtime.GetClassDescriptor(self.valobj)
        class_name = class_desc.GetClassName()
        
        # Then use runtime introspection for ivars
        # No infinite ISA recursion because we don't show ISA as a child
```

## Recommendation

Implement synthetic children support directly in the GNUstepObjCRuntime plugin. This provides:
- Better performance
- Tighter integration with ISAResolver
- No Python/C++ boundary issues
- Native thread safety
- Direct access to LLDB internals

The Python bridge can remain for prototyping and special cases, but the core functionality should be in the native plugin.