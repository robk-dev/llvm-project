# Task 03: Fix Windows Calling Convention for objc_msgSend

## Problem Statement
Expression evaluation crashes with access violations on Windows x64 because `objc_msgSend` calls use incorrect calling conventions. GNUstep/libobjc2 on Windows uses the standard Win64 calling convention, but LLDB's generated IR may not reflect this correctly.

## Technical Background

### Windows x64 Calling Convention
- **Microsoft x64**: RCX, RDX, R8, R9 for first 4 integer/pointer args
- **Varargs**: Caller allocates shadow space, varargs spilled to stack
- **Return values**: RAX for integers/pointers, XMM0 for floats

### Current Issue
When LLDB creates function pointer casts for `objc_msgSend` in expressions, it may not specify the correct calling convention, leading to:
- Register misalignment 
- Stack corruption
- Access violations at call sites

## Analysis of Current Code

### DeclVendor Function Creation
Current code in `GNUstepObjCDeclVendor.cpp` likely creates function declarations without explicit calling convention:

```cpp
// This may not specify calling convention
QualType fTy = ctx.getFunctionType(retTy, paramTys, epi);
```

### Required Fix
```cpp
FunctionProtoType::ExtProtoInfo epi;
auto &TI = ctx.getTargetInfo();
auto Triple = TI.getTriple();

// Set calling convention based on target
if (Triple.isOSWindows() && Triple.getArch() == llvm::Triple::x86_64) {
    epi.ExtInfo = epi.ExtInfo.withCallingConv(CallingConv::CC_X86_64_Win64);
} else {
    epi.ExtInfo = epi.ExtInfo.withCallingConv(CallingConv::CC_C);
}

// For varargs functions like objc_msgSend
epi.Variadic = true;

QualType fTy = ctx.getFunctionType(retTy, paramTys, epi);
```

## Implementation Plan

### Step 1: Audit Function Declaration Sites
Find all places where `objc_msgSend` and related functions are declared:

1. **GNUstepObjCDeclVendor::EnsureRuntimeDecls()**
2. **CFStringCreateWithBytes implementation**  
3. **Subscript utility functions**
4. **Dynamic method resolution**

### Step 2: Create CC Helper Function
```cpp
// In GNUstepObjCDeclVendor.cpp
static CallingConv::ID GetTargetCallingConv(ASTContext &ctx) {
    auto &TI = ctx.getTargetInfo();
    auto Triple = TI.getTriple();
    
    if (Triple.isOSWindows()) {
        if (Triple.getArch() == llvm::Triple::x86_64) {
            return CallingConv::CC_X86_64_Win64;
        } else if (Triple.getArch() == llvm::Triple::x86) {
            return CallingConv::CC_X86_StdCall; // or CC_X86_FastCall
        }
    }
    
    // Default for POSIX systems
    return CallingConv::CC_C;
}
```

### Step 3: Update Function Declaration Helper
```cpp
static FunctionDecl *AddCFunctionDecl(ASTContext &ctx, 
                                      DeclContext *dc,
                                      const char *name,
                                      QualType result_qt,
                                      ArrayRef<QualType> param_qts,
                                      bool is_variadic = false) {
    FunctionProtoType::ExtProtoInfo epi;
    epi.Variadic = is_variadic;
    epi.ExtInfo = epi.ExtInfo.withCallingConv(GetTargetCallingConv(ctx));
    
    QualType fn_qt = ctx.getFunctionType(result_qt, param_qts, epi);
    
    // ... rest of function creation
}
```

### Step 4: Fix Specific Function Declarations

**objc_msgSend Family:**
```cpp
// Variadic runtime functions
AddCFunctionDecl(ctx, TU, "objc_msgSend", GetIdTy(ctx),
                 { GetIdTy(ctx), GetSelTy(ctx) }, /*variadic*/true);

// Specific return type variants (may not need varargs)
AddCFunctionDecl(ctx, TU, "objc_msgSend_stret", ctx.VoidTy,
                 { ctx.VoidPtrTy, GetIdTy(ctx), GetSelTy(ctx) });
                 
AddCFunctionDecl(ctx, TU, "objc_msgSend_fpret", ctx.LongDoubleTy,
                 { GetIdTy(ctx), GetSelTy(ctx) });
```

**Core Runtime Functions:**
```cpp
AddCFunctionDecl(ctx, TU, "objc_getClass", GetClassTy(ctx),
                 { GetConstCharPtrTy(ctx) });
                 
AddCFunctionDecl(ctx, TU, "sel_getUid", GetSelTy(ctx),
                 { GetConstCharPtrTy(ctx) });
```

### Step 5: Verify IR Generation
After declarations, check that generated IR contains correct calling convention attributes:
```llvm
declare i8* @objc_msgSend(i8*, i8*, ...) #0

attributes #0 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
```

## Windows-Specific Considerations

### Symbol Name Variations
libobjc2 on Windows may export symbols with different decorations:
- `objc_msgSend` (direct export)
- `__imp_objc_msgSend` (import thunk)
- `_objc_msgSend` (leading underscore)

### DLL Import Handling
Ensure function declarations are marked appropriately for DLL imports:
```cpp
// May need __declspec(dllimport) equivalent
FunctionDecl *fn_decl = ...;
fn_decl->addAttr(DLLImportAttr::Create(ctx, SourceLocation()));
```

## Files to Modify
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCDeclVendor.cpp`
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCDeclVendor.h`

## Testing Strategy

### Unit Tests
1. Verify function declarations have correct calling convention attributes
2. Test on different target triples (Windows x64, Linux x64, etc.)
3. Validate IR generation for `objc_msgSend` calls

### Integration Tests
```cpp
// Test basic messaging
expr -l objc++ -- (id)[NSNumber numberWithInt:42]

// Test varargs messaging  
expr -l objc++ -- (id)[NSArray arrayWithObjects:@"a", @"b", nil]

// Test return value handling
expr -l objc++ -- (int)[(id)[NSNumber numberWithInt:42] intValue]
```

### Windows-Specific Tests
1. Test with different libobjc2 builds (MinGW vs MSVC)
2. Verify no access violations during expression evaluation
3. Test symbol resolution with various import decorations

## Success Criteria
- [ ] Function declarations specify correct calling conventions
- [ ] IR generation includes proper attributes
- [ ] No access violations on Windows x64
- [ ] Expression evaluation works consistently
- [ ] Cross-platform compatibility maintained

## Performance Considerations
- Calling convention detection should be cached
- Minimal overhead during function declaration creation
- No impact on non-Windows platforms

## Rollback Plan
If calling convention changes break other platforms:
1. Revert to platform-specific implementations
2. Add runtime detection of required calling convention
3. Fall back to dynamic symbol resolution

## Implementation Status
- [ ] Analysis completed
- [ ] Helper functions created
- [ ] Function declarations updated
- [ ] IR validation completed
- [ ] Cross-platform testing completed
- [ ] Ready for review

## Dependencies
- Related to Task 05 (symbol resolution)
- Required for Task 04 (CFString implementation)
