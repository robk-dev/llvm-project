# AI Coding Agent Instructions for LLDB GNUstep Runtime

## Project Overview
This LLDB project implements dynamic runtime introspection for GNUstep Objective-C, enabling Apple-style debugging capabilities. The architecture eliminates hardcoded fallbacks in favor of runtime discovery through IRForTarget integration.

## Core Architecture

### GNUstep Plugin Components (`source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/`)
- **`GNUstepObjCRuntime`** - Main runtime class, coordinates all components 
- **`GNUstepObjCRuntimeIntrospector`** - Core runtime discovery engine (FindClass, GetInstanceMethods)
- **`GNUstepRuntimeV2API`** - libobjc2 data structure parsing and class enumeration
- **`GNUstepObjCDeclVendor`** - AST synthesis for missing debug info
- **`GNUstepClassDescriptor`** - Class metadata representation
- **`formatters/`** - Type-specific data formatters (NSString, NSArray, etc.)

### Key Architectural Principle
**Dynamic Discovery over Hardcoded Fallbacks**: Use `m_introspector_up->FindClass()` and IRForTarget's `objc_getClass` dynamic calls instead of embedded utility functions. This mirrors Apple's LLDB implementation.

## Development Workflow

### Essential Build Commands
```bash
# Quick development cycle
./dev.sh build     # Build LLDB + plugin
./dev.sh test      # Run basic functionality tests

# Manual build (from lldb/ directory)
cd ../build && ninja lldb lldb-server

# Plugin-only rebuild (faster iteration)
ninja lldbPluginGNUstepObjCRuntime
```

### Debug Testing Pipeline
```bash
# Setup debug environment and test
source debug_setup.sh
cd examples && ./lldb_wrapper.sh simple_test -o "b main" -o "run"

# Test runtime introspection in LLDB session:
(lldb) po @[@"runtime", @"introspection"]      # Array literals
(lldb) po @{@"works": @"perfectly"}            # Dictionary literals  
(lldb) po [NSArray arrayWithObjects:@"a", nil] # Method calls
```

### Critical Integration Points
- **IRForTarget Hook**: `LookupRuntimeSymbol()` method provides symbol resolution for expression evaluation
- **Runtime Symbol Cache**: `ResolveAndCacheRuntimeSymbols()` loads objc_msgSend, objc_getClass addresses
- **Class Resolution**: `GetClassDescriptorFromClassName()` uses `FindClass()` not hardcoded utilities

## Project-Specific Patterns

### Expression Evaluation Architecture
**Pattern**: Runtime introspection + IRForTarget integration
```cpp
// CORRECT: Use runtime introspector
lldb::addr_t class_addr = m_introspector_up->FindClass(class_name);

// AVOID: Hardcoded utility functions (removed in Phase 5 cleanup)
// CallRuntimeFunction(), EnsureArrayDictionaryLiteralSupport()
```

### Component Communication
- `GNUstepObjCRuntime` orchestrates via `m_introspector_up`, `m_runtime_api_up`, `m_decl_vendor_up`
- `GNUstepObjCDeclVendor` calls back to runtime via `GetRuntimeIntrospector()`
- Formatters access class descriptors through main runtime instance

### Error Handling Convention
Use LLVM's `Expected<T>` pattern:
```cpp
auto result = SomeOperation();
if (!result) {
  llvm::consumeError(result.takeError());
  // fallback logic
}
```

## Testing & Validation

### Test Program Structure (`examples/`)
- `simple_test.m` - Basic runtime functionality
- `custom_class_test.m` - Custom classes with methods/properties  
- `foundation_test.m` - Foundation types (NSString, NSArray, NSDictionary)
- Comprehensive formatter testing suite

### Debug Validation Checklist
1. Class name resolution: `po [NSArray class]`
2. Array literals: `po @[@"a", @"b"]` 
3. Dictionary literals: `po @{@"key": @"value"}`
4. Method calls: `po [someArray objectAtIndex:0]`
5. Custom class introspection: `po customObject`

## Common Pitfalls

### Build Issues
- Always build both `lldb` and `lldb-server` - debugger requires both
- Plugin compilation errors usually indicate missing symbol resolution
- Use `ninja -v` for verbose build output when debugging

### Runtime Integration
- Never bypass `GNUstepObjCRuntimeIntrospector` for class resolution
- Don't create utility functions - use IRForTarget dynamic calls
- Cache symbol addresses in `ResolveAndCacheRuntimeSymbols()`

### Cross-Platform Considerations  
- MSYS2/Windows paths use `/c/code/` format in bash environments
- Build system expects Ninja generator, not Make
- Debug setup requires `source /c/code/llvm-project/debug_setup.sh` for proper environment

## File Modification Guidelines

When editing core runtime files:
1. **Preserve** `GNUstepObjCRuntimeIntrospector` functionality - it's the discovery engine
2. **Remove** hardcoded utility functions, embedded C code, fallback chains
3. **Use** `FindClass()`, `GetInstanceMethods()` for runtime queries
4. **Test** with `ninja lldbPluginGNUstepObjCRuntime && ./dev.sh test`

## Context Files
Reference `NEXT_AGENT_HANDOFF.md` for Phase 6 cleanup mission details and `examples/README.md` for testing procedures.
