# Task 06: Complete DeclVendor Runtime Function Declarations

## Problem Statement
The GNUstepObjCDeclVendor needs complete, accurate runtime function declarations to enable proper expression evaluation. Missing or incorrect function signatures can cause compilation errors or runtime crashes.

Currently the DeclVendor may be missing key declarations or have incorrect type signatures for Windows/cross-platform compatibility.

## Analysis of Required Functions

### Core Runtime Functions (Critical)
```cpp
// Basic messaging
id objc_msgSend(id self, SEL op, ...);

// Class and selector operations  
Class objc_getClass(const char *name);
SEL sel_getUid(const char *str);
Class object_getClass(id obj);

// Method introspection
IMP class_getMethodImplementation(Class cls, SEL name);
BOOL class_respondsToSelector(Class cls, SEL sel);
```

### String Creation (Required for literals)
```cpp
// Our custom implementation
CFStringRef CFStringCreateWithBytes(CFAllocatorRef alloc, const UInt8 *bytes, 
                                   CFIndex numBytes, CFStringEncoding encoding, 
                                   Boolean isExternalRepresentation);
```

### Optional ARC Functions (Nice-to-have)
```cpp
id objc_retain(id obj);
void objc_release(id obj);  
id objc_autoreleaseReturnValue(id obj);
id objc_retainAutoreleasedReturnValue(id obj);
```

### Foundation Method Signatures (Expression support)
From existing code analysis, these are needed for basic NSString, NSNumber, NSArray, NSDictionary operations.

## Implementation Plan

### Step 1: Consolidate Type Helpers
```cpp
// In GNUstepObjCDeclVendor.cpp anonymous namespace
namespace {
    // Core Objective-C types
    static QualType GetIdTy(ASTContext &ctx) {
        return ctx.getObjCIdType();
    }
    
    static QualType GetClassTy(ASTContext &ctx) {
        return ctx.getObjCClassType();
    }
    
    static QualType GetSelTy(ASTContext &ctx) {
        return ctx.getObjCSelType();
    }
    
    static QualType GetConstCharPtrTy(ASTContext &ctx) {
        return ctx.getPointerType(ctx.CharTy.withConst());
    }
    
    static QualType GetIMPTy(ASTContext &ctx) {
        // IMP is a function pointer: id (*)(id, SEL, ...)
        FunctionProtoType::ExtProtoInfo epi;
        epi.Variadic = true;
        QualType fnTy = ctx.getFunctionType(GetIdTy(ctx), 
                                           {GetIdTy(ctx), GetSelTy(ctx)}, epi);
        return ctx.getPointerType(fnTy);
    }
    
    static QualType GetBOOLTy(ASTContext &ctx) {
        // BOOL is typically signed char on GNUstep
        return ctx.SignedCharTy;
    }
    
    // Platform-specific calling convention
    static CallingConv::ID GetTargetCallingConv(ASTContext &ctx) {
        auto &TI = ctx.getTargetInfo();
        auto Triple = TI.getTriple();
        
        if (Triple.isOSWindows() && Triple.getArch() == llvm::Triple::x86_64) {
            return CallingConv::CC_X86_64_Win64;
        }
        return CallingConv::CC_C;
    }
}
```

### Step 2: Enhanced Function Declaration Helper
```cpp
static FunctionDecl *AddCFunctionDecl(ASTContext &ctx, DeclContext *dc,
                                      const char *name, QualType result_qt,
                                      ArrayRef<QualType> param_qts,
                                      bool is_variadic = false) {
    // Create function type with correct calling convention
    FunctionProtoType::ExtProtoInfo epi;
    epi.Variadic = is_variadic;
    epi.ExtInfo = epi.ExtInfo.withCallingConv(GetTargetCallingConv(ctx));
    
    QualType fn_qt = ctx.getFunctionType(result_qt, param_qts, epi);
    
    // Create function declaration
    DeclarationName decl_name = &ctx.Idents.get(name);
    FunctionDecl *fn_decl = FunctionDecl::Create(
        ctx, dc, SourceLocation(), SourceLocation(), decl_name, fn_qt,
        ctx.getTrivialTypeSourceInfo(fn_qt), SC_Extern);
    
    // Add parameters
    SmallVector<ParmVarDecl *, 4> params;
    for (size_t i = 0; i < param_qts.size(); ++i) {
        auto *param = ParmVarDecl::Create(
            ctx, fn_decl, SourceLocation(), SourceLocation(), nullptr,
            param_qts[i], ctx.getTrivialTypeSourceInfo(param_qts[i]), 
            SC_None, nullptr);
        params.push_back(param);
    }
    fn_decl->setParams(params);
    
    // Add to declaration context
    dc->addDecl(fn_decl);
    
    return fn_decl;
}
```

### Step 3: Core Runtime Function Declarations
```cpp
void GNUstepObjCDeclVendor::EnsureCoreRuntimeDecls(ASTContext &ctx) {
    TranslationUnitDecl *TU = ctx.getTranslationUnitDecl();
    
    // objc_msgSend family
    AddCFunctionDecl(ctx, TU, "objc_msgSend", GetIdTy(ctx),
                     {GetIdTy(ctx), GetSelTy(ctx)}, /*variadic*/true);
    
    AddCFunctionDecl(ctx, TU, "objc_msgSend_stret", ctx.VoidTy,
                     {ctx.VoidPtrTy, GetIdTy(ctx), GetSelTy(ctx)}, /*variadic*/true);
    
    AddCFunctionDecl(ctx, TU, "objc_msgSend_fpret", ctx.LongDoubleTy,
                     {GetIdTy(ctx), GetSelTy(ctx)}, /*variadic*/true);
    
    // Class operations
    AddCFunctionDecl(ctx, TU, "objc_getClass", GetClassTy(ctx),
                     {GetConstCharPtrTy(ctx)});
    
    AddCFunctionDecl(ctx, TU, "object_getClass", GetClassTy(ctx),
                     {GetIdTy(ctx)});
    
    // Selector operations
    AddCFunctionDecl(ctx, TU, "sel_getUid", GetSelTy(ctx),
                     {GetConstCharPtrTy(ctx)});
    
    AddCFunctionDecl(ctx, TU, "sel_getName", GetConstCharPtrTy(ctx),
                     {GetSelTy(ctx)});
    
    // Method introspection
    AddCFunctionDecl(ctx, TU, "class_getMethodImplementation", GetIMPTy(ctx),
                     {GetClassTy(ctx), GetSelTy(ctx)});
    
    AddCFunctionDecl(ctx, TU, "class_respondsToSelector", GetBOOLTy(ctx),
                     {GetClassTy(ctx), GetSelTy(ctx)});
    
    AddCFunctionDecl(ctx, TU, "class_getInstanceMethod", ctx.VoidPtrTy,
                     {GetClassTy(ctx), GetSelTy(ctx)});
}
```

### Step 4: Optional ARC Function Declarations
```cpp
void GNUstepObjCDeclVendor::EnsureARCRuntimeDecls(ASTContext &ctx) {
    TranslationUnitDecl *TU = ctx.getTranslationUnitDecl();
    
    // Basic memory management
    AddCFunctionDecl(ctx, TU, "objc_retain", GetIdTy(ctx),
                     {GetIdTy(ctx)});
    
    AddCFunctionDecl(ctx, TU, "objc_release", ctx.VoidTy,
                     {GetIdTy(ctx)});
    
    // Autorelease return value optimization
    AddCFunctionDecl(ctx, TU, "objc_autoreleaseReturnValue", GetIdTy(ctx),
                     {GetIdTy(ctx)});
    
    AddCFunctionDecl(ctx, TU, "objc_retainAutoreleasedReturnValue", GetIdTy(ctx),
                     {GetIdTy(ctx)});
    
    // Autorelease pool management
    AddCFunctionDecl(ctx, TU, "objc_autoreleasePoolPush", ctx.VoidPtrTy, {});
    
    AddCFunctionDecl(ctx, TU, "objc_autoreleasePoolPop", ctx.VoidTy,
                     {ctx.VoidPtrTy});
}
```

### Step 5: CF Types for CFString Implementation
```cpp
void GNUstepObjCDeclVendor::EnsureCFRuntimeDecls(ASTContext &ctx) {
    TranslationUnitDecl *TU = ctx.getTranslationUnitDecl();
    
    // CFString types (we typedef to simple types for compatibility)
    // typedef const struct __CFString * CFStringRef;
    // typedef const void * CFAllocatorRef; 
    // typedef long CFIndex;
    // typedef unsigned int CFStringEncoding;
    // typedef unsigned char Boolean;
    
    // For simplicity, just use basic types in function signatures
    AddCFunctionDecl(ctx, TU, "CFStringCreateWithBytes", GetIdTy(ctx), // CFStringRef -> id
                     {
                         ctx.VoidPtrTy,                    // CFAllocatorRef
                         ctx.UnsignedCharTy.getPointerTo(), // const UInt8 *
                         ctx.LongTy,                       // CFIndex  
                         ctx.UnsignedIntTy,                // CFStringEncoding
                         ctx.UnsignedCharTy                // Boolean
                     });
}
```

### Step 6: Foundation Method Signatures
Keep the existing Foundation method signature tables but ensure they're correctly integrated:

```cpp
void GNUstepObjCDeclVendor::AddFoundationClassMethods(
    clang::ObjCInterfaceDecl *interface_decl, const std::string &class_name) {
    
    // Use existing method signature tables but ensure calling conventions are correct
    
    // Find the method signature table for this class
    const FoundationMethodSignature *methods = nullptr;
    for (const auto &class_methods : foundation_class_methods) {
        if (class_methods.class_name && class_name == class_methods.class_name) {
            methods = class_methods.methods;
            break;
        }
    }
    
    if (!methods) {
        return; // No methods defined for this class
    }
    
    // Add each method with correct calling convention
    for (int i = 0; methods[i].name != nullptr; i++) {
        const auto &method = methods[i];
        
        // Skip if method already exists
        if (InterfaceAlreadyHasMethod(interface_decl, method.name, method.is_instance)) {
            continue;
        }
        
        CreateMethodDecl(interface_decl, method.name, method.types, method.is_instance);
    }
}
```

### Step 7: Comprehensive Declaration Entry Point
```cpp
void GNUstepObjCDeclVendor::EnsureRuntimeDecls(TypeSystemClang &ts) {
    if (m_runtime_decls_injected)
        return;
    
    ASTContext &ctx = ts.getASTContext();
    
    LLDB_LOG(GetLog(LLDBLog::Language), 
             "GNUstep: Injecting runtime function declarations");
    
    // 1. Core Objective-C runtime functions
    EnsureCoreRuntimeDecls(ctx);
    
    // 2. CoreFoundation compatibility functions  
    EnsureCFRuntimeDecls(ctx);
    
    // 3. Optional ARC functions (non-fatal if runtime doesn't support)
    EnsureARCRuntimeDecls(ctx);
    
    // 4. Build CFStringCreateWithBytes with AST body
    EnsureCFStringCreateWithBytes(ctx);
    
    m_runtime_decls_injected = true;
    
    LLDB_LOG(GetLog(LLDBLog::Language), 
             "GNUstep: Runtime function declarations complete");
}
```

## Files to Modify
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCDeclVendor.cpp`
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCDeclVendor.h`

## Testing Strategy

### Declaration Validation Tests
```cpp
// Test that all required functions are declared
void TestRuntimeDeclarations(TypeSystemClang &ts) {
    ASTContext &ctx = ts.getASTContext();
    TranslationUnitDecl *TU = ctx.getTranslationUnitDecl();
    
    const char *required_functions[] = {
        "objc_msgSend", "objc_getClass", "sel_getUid",
        "object_getClass", "class_getMethodImplementation",
        "CFStringCreateWithBytes", nullptr
    };
    
    for (int i = 0; required_functions[i]; i++) {
        FunctionDecl *decl = LookupFuncByName(ctx, required_functions[i]);
        assert(decl && "Required function declaration missing");
        
        // Verify calling convention
        QualType fn_type = decl->getType();
        // ... validate function signature
    }
}
```

### Expression Compilation Tests
```cpp
// Test that declarations enable expression compilation
expr -l objc++ -- (void*)objc_msgSend
expr -l objc++ -- (Class)objc_getClass("NSString") 
expr -l objc++ -- (SEL)sel_getUid("description")
expr -l objc++ -- CFStringCreateWithBytes(NULL, (const void*)"test", 4, 0, 0)
```

## Success Criteria
- [ ] All core runtime functions have correct declarations
- [ ] Function signatures match target platform ABI
- [ ] CFString creation function is properly declared and defined
- [ ] Foundation class methods compile correctly
- [ ] No compilation errors in expression evaluation
- [ ] Cross-platform calling convention handling

## Performance Considerations
- Declarations are cached to avoid repeated AST construction
- Lazy declaration creation (only when needed)
- Minimal overhead during expression parsing

## Error Handling
- Graceful handling of missing optional functions
- Clear error messages for declaration failures
- Fallback strategies for incomplete declarations

## Implementation Status
- [ ] Type helper functions implemented
- [ ] Core runtime function declarations
- [ ] CF compatibility declarations
- [ ] ARC function declarations (optional)
- [ ] Foundation method integration
- [ ] Testing and validation completed
- [ ] Ready for review

## Dependencies  
- Builds on Task 03 (calling conventions)
- Required for Task 04 (CFString implementation)
- Supports Task 02 (literals/subscripting)
