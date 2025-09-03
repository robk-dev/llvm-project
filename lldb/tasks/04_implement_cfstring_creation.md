# Task 04: Implement CFStringCreateWithBytes for @"" Literals

## Problem Statement
LLDB's expression parser expects a `CFStringCreateWithBytes` function to be available for creating Objective-C string literals (@"hello"). Without this, @"" literal expressions fail with errors like:
> "requires CFStringCreateWithBytes"

## Technical Background

### How @"" Literals Work in LLDB
1. Clang IR generation maps @"literal" to `CFStringCreateWithBytes()` calls
2. LLDB's IRForTarget looks up this symbol during expression JIT compilation
3. If not found, string literal rewriting fails

### GNUstep vs CoreFoundation
- **Apple**: Uses CoreFoundation's `CFStringCreateWithBytes`
- **GNUstep**: No CoreFoundation, but can simulate via `[NSString stringWithUTF8String:]`

## Implementation Strategy

### Option A: AST Body Builder (Recommended)
Create a proper function with AST body that calls GNUstep runtime:

```objc
CFStringRef CFStringCreateWithBytes(CFAllocatorRef allocator,
                                   const UInt8 *bytes,
                                   CFIndex numBytes, 
                                   CFStringEncoding encoding,
                                   Boolean isExternalRepresentation) {
    // Ignore allocator, encoding, isExternalRepresentation for simplicity
    return (CFStringRef)((id(*)(Class,SEL,const char*))objc_msgSend)(
        objc_getClass("NSString"),
        sel_getUid("stringWithUTF8String:"),
        (const char*)bytes
    );
}
```

### Option B: JIT Utility Function  
Compile a C function at runtime and install it at the expected symbol address.

### Option C: Symbol Redirector
Redirect `CFStringCreateWithBytes` symbol to existing GNUstep function.

## Implementation Details

### Step 1: AST Type Helpers
```cpp
// In GNUstepObjCDeclVendor.cpp
static QualType GetCFStringRefTy(ASTContext &ctx) {
    // CFStringRef is typedef'd to const struct __CFString *
    // For simplicity, use 'id' since NSString is toll-free bridged
    return GetIdTy(ctx);
}

static QualType GetCFAllocatorRefTy(ASTContext &ctx) {
    return ctx.VoidPtrTy; // CFAllocatorRef -> void*
}

static QualType GetCFIndexTy(ASTContext &ctx) {
    return ctx.LongTy; // CFIndex -> long
}

static QualType GetCFStringEncodingTy(ASTContext &ctx) {
    return ctx.UnsignedIntTy; // CFStringEncoding -> unsigned int
}

static QualType GetBooleanTy(ASTContext &ctx) {
    return ctx.BoolTy; // Boolean -> bool
}

static QualType GetUInt8PtrTy(ASTContext &ctx) {
    return ctx.UnsignedCharTy.withConst().getPointerTo(); // const UInt8*
}
```

### Step 2: Function Declaration
```cpp
void GNUstepObjCDeclVendor::EnsureCFStringCreateWithBytes(ASTContext &ctx) {
    TranslationUnitDecl *TU = ctx.getTranslationUnitDecl();
    
    // Check if already declared
    if (LookupFuncByName(ctx, "CFStringCreateWithBytes"))
        return;
    
    // Create function declaration
    FunctionDecl *CFDecl = AddCFunctionDecl(
        ctx, TU, "CFStringCreateWithBytes", GetCFStringRefTy(ctx),
        {
            GetCFAllocatorRefTy(ctx),  // allocator (ignored)
            GetUInt8PtrTy(ctx),        // bytes  
            GetCFIndexTy(ctx),         // numBytes
            GetCFStringEncodingTy(ctx), // encoding (ignored)
            GetBooleanTy(ctx)          // isExternalRepresentation (ignored)
        });
    
    // Build AST body
    DefineCFStringCreateWithBytesBody(ctx, CFDecl);
}
```

### Step 3: AST Body Builder
```cpp
static void DefineCFStringCreateWithBytesBody(ASTContext &ctx, FunctionDecl *CFDecl) {
    if (!CFDecl || CFDecl->hasBody())
        return;
    
    // Lookup runtime function declarations
    FunctionDecl *objc_getClassFD = LookupFuncByName(ctx, "objc_getClass");
    FunctionDecl *sel_getUidFD = LookupFuncByName(ctx, "sel_getUid");
    FunctionDecl *objc_msgSendFD = LookupFuncByName(ctx, "objc_msgSend");
    
    if (!objc_getClassFD || !sel_getUidFD || !objc_msgSendFD) {
        // Runtime functions not available, create stub that returns null
        CreateNullReturnBody(ctx, CFDecl);
        return;
    }
    
    // Build: objc_getClass("NSString")
    Expr *classNameLiteral = MakeCStringLiteral(ctx, "NSString");
    CallExpr *getClassCall = CallExpr::Create(
        ctx, MakeFuncRef(ctx, objc_getClassFD),
        {classNameLiteral}, GetClassTy(ctx), 
        VK_PRValue, SourceLocation(), FPOptionsOverride());
    
    // Build: sel_getUid("stringWithUTF8String:")
    Expr *selectorLiteral = MakeCStringLiteral(ctx, "stringWithUTF8String:");
    CallExpr *getSelectorCall = CallExpr::Create(
        ctx, MakeFuncRef(ctx, sel_getUidFD),
        {selectorLiteral}, GetSelTy(ctx),
        VK_PRValue, SourceLocation(), FPOptionsOverride());
    
    // Get 'bytes' parameter (2nd parameter, index 1)
    ParmVarDecl *bytesParam = GetParam(CFDecl, 1);
    Expr *bytesRef = DeclRefExpr::Create(
        ctx, NestedNameSpecifierLoc(), SourceLocation(), bytesParam,
        false, SourceLocation(), bytesParam->getType(), VK_LValue);
    
    // Cast bytes to (const char*)
    Expr *bytesAsConstChar = MakeCast(ctx, bytesRef, GetConstCharPtrTy(ctx));
    
    // Build cast for objc_msgSend: (id(*)(Class,SEL,const char*))
    QualType retTy = GetIdTy(ctx);
    QualType p0 = GetClassTy(ctx);
    QualType p1 = GetSelTy(ctx);
    QualType p2 = GetConstCharPtrTy(ctx);
    
    FunctionProtoType::ExtProtoInfo epi;
    epi.ExtInfo = epi.ExtInfo.withCallingConv(GetTargetCallingConv(ctx));
    QualType fnTy = ctx.getFunctionType(retTy, {p0, p1, p2}, epi);
    QualType fnPtrTy = ctx.getPointerType(fnTy);
    
    Expr *msgSendRef = MakeFuncRef(ctx, objc_msgSendFD);
    Expr *msgSendCast = MakeCast(ctx, msgSendRef, fnPtrTy);
    
    // Build final call: msgSendCast(getClassCall, getSelectorCall, bytesAsConstChar)
    CallExpr *finalCall = CallExpr::Create(
        ctx, msgSendCast,
        {getClassCall, getSelectorCall, bytesAsConstChar},
        retTy, VK_PRValue, SourceLocation(), FPOptionsOverride());
    
    // return finalCall;
    ReturnStmt *returnStmt = ReturnStmt::Create(ctx, SourceLocation(), finalCall, nullptr);
    
    // Create compound statement body
    llvm::SmallVector<Stmt*, 1> stmts;
    stmts.push_back(returnStmt);
    CompoundStmt *body = CompoundStmt::Create(ctx, stmts, SourceLocation(), SourceLocation());
    
    CFDecl->setBody(body);
    CFDecl->setDeclaredInline(); // Optional optimization hint
}
```

### Step 4: Integration with DeclVendor
```cpp
void GNUstepObjCDeclVendor::EnsureRuntimeDecls(TypeSystemClang &ts) {
    if (m_runtime_decls_injected)
        return;
    
    ASTContext &ctx = ts.getASTContext();
    
    // 1) Declare core runtime functions first
    EnsureCoreRuntimeDecls(ctx);
    
    // 2) Create CFStringCreateWithBytes with AST body
    EnsureCFStringCreateWithBytes(ctx);
    
    m_runtime_decls_injected = true;
}
```

### Step 5: Symbol Registration
Ensure IRForTarget knows about our function:
```cpp
void GNUstepObjCRuntime::RegisterSymbolsWithIRForTarget() {
    // Register CFString creation function
    ExecutionContext exe_ctx;
    exe_ctx.SetTargetSP(m_process->GetTarget().shared_from_this());
    exe_ctx.SetProcessSP(m_process->shared_from_this());
    
    // Compile and install the function if needed
    auto result = CreateCFStringUtilityFunction(exe_ctx);
    if (result) {
        m_cfstring_utility_fn = std::move(*result);
        m_cfstring_create_addr = m_cfstring_utility_fn->GetStartAddress();
        
        LLDB_LOG(GetLog(LLDBLog::Language), 
                 "GNUstep: Installed CFStringCreateWithBytes at {0:x}", 
                 m_cfstring_create_addr);
    }
}
```

## Files to Modify
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCDeclVendor.cpp`
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCDeclVendor.h`
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`

## Testing Strategy

### Unit Tests
1. Test AST body generation for CFStringCreateWithBytes
2. Verify function signature matches expected CFString API
3. Test null handling when runtime functions unavailable

### Integration Tests
```cpp
// Basic string literal
expr -l objc++ -O -- @"hello"

// String literal in expressions
expr -l objc++ -O -- [@"test" length]

// Complex expressions with string literals
expr -l objc++ -O -- [[NSArray arrayWithObjects:@"a", @"b", nil] objectAtIndex:0]
```

### Edge Cases
1. Empty string literals: @""
2. Unicode strings: @"héllo"
3. String literals with escapes: @"line1\nline2"
4. Large string literals

## Success Criteria
- [ ] @"" literals compile without errors
- [ ] String literals return valid NSString objects
- [ ] No memory leaks or crashes
- [ ] Performance comparable to native implementation
- [ ] Cross-platform compatibility

## Alternative Approaches

### Fallback Strategy
If AST body approach fails, implement utility function approach:
```cpp
extern "C" void* CFStringCreateWithBytes(void* alloc, const char* bytes, 
                                       long len, unsigned enc, int ext) {
    return (void*)objc_msgSend(objc_getClass("NSString"), 
                               sel_getUid("stringWithUTF8String:"), 
                               bytes);
}
```

## Performance Considerations
- AST body is generated once per expression context
- Runtime calls are minimal (one objc_getClass + one objc_msgSend)
- No additional memory allocations beyond NSString creation

## Implementation Status
- [ ] Research completed
- [ ] AST helpers implemented
- [ ] Function declaration and body generation
- [ ] Integration with DeclVendor
- [ ] Symbol registration
- [ ] Testing completed
- [ ] Ready for review

## Dependencies
- Requires Task 03 (calling conventions)
- Requires Task 06 (runtime declarations)
