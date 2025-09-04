# GNUstep LLDB Plugin - Dynamic Interface Discovery Implementation Plan

## Critical Finding: We Already Have Runtime Introspection!

After investigation, we discovered that **we already have comprehensive runtime introspection capabilities** in:
- `GNUstepRuntimeV2API::GetAllClasses()` - Can enumerate ALL runtime classes
- `GNUstepRuntimeV2API::GetClassMethods()` - Can get all methods for a class
- `GNUstepRuntimeV2API::GetClassProperties()` - Can get all properties
- `GNUstepObjCRuntimeIntrospector` - Has runtime function addresses

**The Problem**: Our `GNUstepObjCDeclVendor` is using hardcoded AST building instead of leveraging these capabilities!

## Current Architecture Analysis

### What We Have (But Aren't Using)
```cpp
// In GNUstepRuntimeV2API.h - ALREADY IMPLEMENTED!
llvm::Expected<std::vector<Class>> GetAllClasses();
llvm::Expected<std::vector<Method>> GetClassMethods(Class cls);
llvm::Expected<std::vector<Property>> GetClassProperties(Class cls);
llvm::Expected<std::vector<Protocol>> GetClassProtocols(Class cls);
```

### What We're Doing Wrong
```cpp
// In GNUstepObjCDeclVendor::EnsureMinimalFoundationInterfaces()
// HARDCODED instead of dynamic!
void EnsureMinimalFoundationInterfaces(TypeSystemClang &ts) {
  // Manually creating NSNumber, NSString, NSArray, NSDictionary
  // Instead of discovering them from runtime!
}
```

## The Real Solution: Connect Existing Pieces

### Phase 1: Wire Up Runtime Discovery (1-2 days)
**We don't need to implement runtime introspection - we need to USE it!**

```cpp
// Modified GNUstepObjCDeclVendor::FindDecls
uint32_t GNUstepObjCDeclVendor::FindDecls(ConstString name, ...) {
  // Step 1: Check if we've cached this class
  auto cached = m_isa_to_interface.find(name);
  if (cached != m_isa_to_interface.end()) {
    return cached->second;
  }
  
  // Step 2: Use EXISTING runtime API to find class
  GNUstepRuntimeV2API api(m_runtime.GetProcess());
  auto class_info = api.GetClassInfo(name.GetCString());
  if (!class_info) {
    return 0;
  }
  
  // Step 3: Build interface from runtime data
  auto *interface = BuildInterfaceFromRuntime(class_info);
  m_isa_to_interface[class_info.isa] = interface;
  
  return 1;
}
```

### Phase 2: Dynamic Interface Building (2-3 days)
```cpp
ObjCInterfaceDecl* BuildInterfaceFromRuntime(ClassInfo &info) {
  // Create interface declaration
  auto *interface = CreateInterfaceDecl(info.name);
  
  // Add ALL methods from runtime (not hardcoded!)
  auto methods = m_api->GetClassMethods(info.cls);
  for (const auto &method : methods) {
    auto *method_decl = CreateMethodFromEncoding(
      method.selector,
      method.type_encoding,
      method.is_instance
    );
    interface->addDecl(method_decl);
  }
  
  // Add ALL properties from runtime
  auto properties = m_api->GetClassProperties(info.cls);
  for (const auto &prop : properties) {
    AddPropertyToInterface(interface, prop);
  }
  
  return interface;
}
```

### Phase 3: Fix Expression Evaluation Crashes (1 day)
The crashes (`@123`, `[NSNumber numberWithInt:7]`) happen because:
1. We're not populating interfaces when expressions need them
2. The AST is incomplete when LLDB tries to compile expressions

**Solution**: Ensure interfaces are populated BEFORE expression compilation:
```cpp
// In GNUstepObjCDeclVendor constructor or initialization
void GNUstepObjCDeclVendor::PreloadFoundationClasses() {
  // Use runtime API to get all Foundation classes
  auto foundation_classes = m_api->GetAllFoundationClasses();
  
  // Build interfaces for common classes immediately
  for (const auto &cls : foundation_classes) {
    if (IsCommonClass(cls.name)) { // NSNumber, NSString, etc.
      BuildInterfaceFromRuntime(cls);
    }
  }
}
```

## Testing Integration

### Use Existing Test Infrastructure
```bash
# We already have test programs!
cd /home/robk/code/llvm-project/lldb/examples

# Test with our LLDB MCP tool
mcp__llvm_lldb_debug__lldb_start
mcp__llvm_lldb_debug__lldb_load custom_class_test
mcp__llvm_lldb_debug__lldb_run

# Test expressions that currently crash
expr -l objc++ -- @123
expr -l objc++ -- [NSNumber numberWithInt:7]
expr -l objc++ -- id arr = [NSArray arrayWithObjects:@"a",@"b",nil]
expr -l objc++ -- arr[0]
```

### Validation Checklist
- [ ] `po num` works (already works)
- [ ] `@123` doesn't crash
- [ ] `[NSNumber numberWithInt:7]` works
- [ ] `[NSArray arrayWithObjects:...]` works
- [ ] Array subscript `arr[0]` works
- [ ] Custom classes show properties

## Implementation Priority

### Immediate Fix (Today)
1. **Remove hardcoded interfaces** in `EnsureMinimalFoundationInterfaces`
2. **Use GNUstepRuntimeV2API** in `FindDecls` to discover classes dynamically
3. **Test with existing examples** to verify crashes are fixed

### Short Term (This Week)
1. Cache discovered interfaces for performance
2. Add property synthesis from runtime data
3. Handle categories and protocols

### Long Term (Next Sprint)
1. Background preloading of common classes
2. Incremental updates when new classes loaded
3. Performance optimization

## Key Insights

1. **We're reinventing the wheel** - The runtime introspection is already implemented!
2. **The crashes are due to incomplete AST** - We need to populate interfaces before expression evaluation
3. **Dynamic is better than static** - Using runtime discovery will automatically support ALL classes

## Minimal Working Implementation (< 100 lines)

```cpp
// In GNUstepObjCDeclVendor::FindDecls
uint32_t GNUstepObjCDeclVendor::FindDecls(ConstString name, ...) {
  // Initialize API if needed
  if (!m_runtime_api) {
    m_runtime_api = std::make_unique<GNUstepRuntimeV2API>(
      m_runtime.GetProcess()
    );
  }
  
  // Try to find class in runtime
  auto class_opt = m_runtime_api->GetClassInfo(name.GetCString());
  if (!class_opt) {
    return 0;
  }
  
  // Create interface declaration
  auto &ctx = m_ast_ctx->getASTContext();
  auto *interface = ObjCInterfaceDecl::Create(
    ctx, ctx.getTranslationUnitDecl(),
    SourceLocation(), 
    &ctx.Idents.get(name.GetCString()),
    nullptr, nullptr
  );
  
  // Get and add methods from runtime
  if (auto methods = m_runtime_api->GetClassMethods(class_opt->cls)) {
    for (const auto &method : *methods) {
      // Parse type encoding and create method
      auto *method_decl = CreateMethodFromRuntime(
        interface, method
      );
      if (method_decl) {
        interface->addDecl(method_decl);
      }
    }
  }
  
  // Cache and return
  m_isa_to_interface[class_opt->isa] = interface;
  decls.push_back(CompilerDecl(m_ast_ctx, interface));
  return 1;
}
```

## Conclusion

**We don't need to build new runtime introspection - we need to USE what we already have!**

The path forward is clear:
1. Stop hardcoding interfaces
2. Use `GNUstepRuntimeV2API` to discover classes/methods dynamically
3. Build AST from runtime data
4. Test with existing examples

This will fix the crashes and provide true dynamic discovery for ALL classes, not just the 4 we hardcoded.