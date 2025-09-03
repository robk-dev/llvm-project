You’re right—I dug through the *actual* code you attached and cross-checked against your LLDB logs. Here’s what’s going on and how to fix it without hacks, using runtime introspection wherever we can.

---

# What the logs + code are telling us (succinct)

1. **CFString literal rewrite still isn’t wired end-to-end.**
   `@"hello"` fails with *“Rewriting an Objective-C constant string requires CFStringCreateWithBytes”*. That means the expression IR is emitting a call to **`CFStringCreateWithBytes`** but LLDB can’t resolve a symbol for it at JIT/link time. Your DeclVendor has the right scaffolding: it declares runtime prototypes (objc\_getClass/sel\_getUid/objc\_msgSend/…) and has a fallback **UtilityFunction** that implements `CFStringCreateWithBytes` via `objc_getClass + sel_getUid + objc_msgSend` (bridging to `+[NSString stringWithUTF8String:]`)—see `EnsureRuntimeDecls` and `EnsureCFStringCreateWithBytes` in **GNUstepObjCDeclVendor.cpp**.
   **But** the symbol isn’t always installed before the expression is compiled, so the linker can’t find it.

2. **The runtime is sometimes rejected for Language=C (2).**
   Your latest logs show `GNUstepObjCRuntime::CreateInstance called for language 2 … Not ObjC/ObjC++ language (2), returning nullptr`. That blocks DeclVendor from participating when LLDB picks C for a sub-expression (which it *does* occasionally during ObjC++ expression setup). Your current runtime has a path that accepts C/ObjC/ObjC++ (good), but another version (or an earlier branch) returns `nullptr` for C—make sure the build is using the “accept C” branch consistently. See `CreateInstance` variants in **GNUstepObjCRuntime.cpp**.

3. **`objc_msgSend` undeclared in some sessions.**
   You saw “function ‘objc\_msgSend’ with unknown type must be given a function type”. That means **DeclVendor didn’t inject prototypes** into the expression’s scratch AST for that compilation. You already have `EnsureRuntimeDecls(...)` that declares `objc_msgSend`, `objc_getClass`, `sel_getUid`, `object_getClass`, `class_getMethodImplementation`. We need to **guarantee** this runs *before* expression parsing, not only on demand during FindDecls, and for C/ObjC/ObjC++.&#x20;

4. **Crashes on `[NSNumber numberWithInt:]`.**
   You do have a proper **object/selector checker** utility function compiled into the target that probes `object_getClass` + `class_getMethodImplementation` (good). But when the runtime isn’t attached for C, or `objc_msgSend` isn’t typed/declared, or the checker is called with a **metaclass** and we don’t use `class_respondsToSelector`, we can trip an access violation in libobjc while preflighting (that 0x7ffa… address). Let’s tighten the checker and the prototypes. (More below.)

5. **Your DeclVendor already does the “minimal Foundation augmentation.”**
   I can see `EnsureMinimalFoundationInterfaces`, per-class method population, and the subscript shim mapping (so LLDB can understand `objectAtIndexedSubscript:` etc.). That’s the right path—keep it dynamic via runtime queries.&#x20;

---

# Tight, actionable plan (no hardcoding; fully dynamic)

> **Goal:** make every ObjC/ObjC++ expression (and the occasional C-typed sub-expression) succeed by ensuring the runtime + decl vendor are always present, prototypes are injected up-front, the CFString fallback is installed *before* IR/JIT, and the object checker is ABI-safe.

### A. Normalize runtime activation (accept C) and one-time hook install

* **Ensure there is exactly one `CreateInstance`** in **GNUstepObjCRuntime.cpp** and that it **returns an instance for `eLanguageTypeC`, `eLanguageTypeObjC`, and `eLanguageTypeObjC_plus_plus`**. Keep the “continuing…” log you added for clarity.
* In `CreateInstance`, **always call** your `InstallExpressionEvaluationHooks(target)` once (guarded by a flag on the runtime). That hook must:

  * Obtain the target’s `TypeSystemClang` and call **`DeclVendor::EnsureRuntimeDecls(ts)`** to pre-declare *all* runtime C APIs (`objc_msgSend`, `objc_getClass`, `sel_getUid`, `object_getClass`, `class_getMethodImplementation`, `class_respondsToSelector`) and **`CFStringCreateWithBytes`**.&#x20;
  * Then **call `DeclVendor::EnsureCFStringCreateWithBytes(target)`** to **install** the UtilityFunction *right away* and cache its address. If install fails, log an explicit error with the result string.&#x20;

> Why: sometimes LLDB sets up internal helpers using Language=C, and without a runtime instance you miss the early declaration/installation window.

### B. Strengthen DeclVendor integration

* In **GNUstepObjCDeclVendor.cpp**, keep `EnsureRuntimeDecls(ts)` as the single place that creates the C prototypes for:

  * `id objc_msgSend(id, SEL, ...)`, `Class objc_getClass(const char*)`, `SEL sel_getUid(const char*)`,
    `Class object_getClass(id)`, `IMP class_getMethodImplementation(Class, SEL)`, and **`BOOL class_respondsToSelector(Class, SEL)`** (add this one).&#x20;
* Make **`FindDecls`** call `EnsureRuntimeDecls(ts)` **unconditionally on first use** (you already do this; keep it). This ensures unknown identifiers (like `objc_msgSend`) are resolvable during semantic analysis.&#x20;
* **CFString fallback**:

  * Keep your utility function body builder (`DefineCFStringCreateWithBytesBody`) that does:

    ```
    Class nsstring = objc_getClass("NSString");
    SEL sel = sel_getUid("stringWithUTF8String:");
    return ((id (*)(Class, SEL, const char*))objc_msgSend)(nsstring, sel, bytes);
    ```

    (where `bytes` is the incoming argument) and compile it via `ClangUtilityFunction::Install(Target)` under the **exact** symbol name `CFStringCreateWithBytes`.&#x20;
  * **Also declare** a C prototype in the scratch AST with the *same* name and a **simple** ABI-safe signature that matches your body:

    ```
    id CFStringCreateWithBytes(void* alloc, const char* bytes, long numBytes, unsigned int enc, int isExt);
    ```

    You already create this via `AddCFunctionDecl(...)`—keep it consistent with the body so the call lowers with the correct calling convention on Win64.&#x20;

> Why: the IR stage links by symbol name. The declaration alone isn’t enough; the symbol **must** be in the target (your utility function provides it). Doing both guarantees resolution and correct CC.

### C. Make the **object/selector checker** robust (and no writes)

* Replace the preflight helper body with a conservative, ABI-safe check:

  * If `obj == nil` → OK.
  * Let `Class cls = object_getClass(obj) ? object_getClass(obj) : (Class)obj;`
    (handles **metaclass** vs instance automatically).
  * If `class_respondsToSelector(cls, sel)` → OK; else fail.
* Only *read* from the runtime; **never** write into process memory or touch message send thunks.
* Add `class_respondsToSelector` to `EnsureRuntimeDecls`.

> Why: this avoids false negatives for class objects (metaclasses) and prevents the access-violation you’re seeing when the preflight walks the wrong structure.

### D. Keep dynamic Foundation augmentation

* Your `EnsureMinimalFoundationInterfaces(...)` and per-class population via runtime method lists are good; keep them. This gives the parser enough method decls (e.g., `numberWithInt:`) without hardcoding.&#x20;
* The subscript shim mapping is fine as an enhancement path; it won’t affect correctness if absent.

### E. Instrumentation (so we can *see* what’s happening)

Add **LLDB category-level logs** from your plugin (not the generic `lldb` log category):

* After hook install, log the **resolved addresses** of: `objc_msgSend`, `objc_getClass`, `sel_getUid`, and the **installed address** of `CFStringCreateWithBytes`.
* On each expression, log whether DeclVendor ran `EnsureRuntimeDecls` for this AST and whether `CFStringCreateWithBytes` was **already present** in the target module list (or installed just now).
* When the object checker is created, log that its function compiled and its address in the target.

*(You already have some targeted logging; expand it just around these states.)*

### F. Quick verification sequence (after rebuilding)

Run these in your same shell after hitting your first breakpoint:

```
(lldb) expr -l objc++ -O -- (void*)objc_getClass
(lldb) expr -l objc++ -O -- (void*)sel_getUid
(lldb) expr -l objc++ -O -- (void*)objc_msgSend
# All three must print non-null pointers

# CFString: should now JIT without the rewrite error
(lldb) expr -l objc++ -O -- @"hello"

# NSNumber class method
(lldb) expr -l objc++ -O -- (id)[NSNumber numberWithInt:7]
(lldb) expr -l objc++ -O -- (int)[(id)[NSNumber numberWithInt:7] intValue]

# Array/dict with “classic” selectors (no subscript sugar needed)
(lldb) expr -l objc++ -O -- id a=[NSArray arrayWithObjects:@"a",@"b",nil]; (id)[a objectAtIndex:0]
(lldb) expr -l objc++ -O -- id d=[NSDictionary dictionaryWithObjectsAndKeys:@"v",@"k",nil]; (id)[d objectForKey:@"k"]
```

If any of the three function pointer queries prints “must be given a function type”, DeclVendor didn’t inject prototypes early enough—check A/B.

---

## “Context capsule” you can keep handy

* Project: LLDB plugin for GNUstep/ObjC on Windows (Eggplant), dynamic runtime introspection, no hardcoding.
* Symptoms: CFString literal rewrite error; `objc_msgSend` undeclared in some sessions; `[NSNumber numberWithInt:]` crashes; LLDB sometimes asks for Language=C.
* Key fixes: accept Language=C; inject & install runtime decls early; install CFString fallback symbol **before** expression compile; robust object checker using `class_respondsToSelector`; keep dynamic Foundation augmentation.
* Critical tests: addresses for objc\_getClass/sel\_getUid/objc\_msgSend show up; `@"hello"` and `+[NSNumber numberWithInt:]` succeed.
