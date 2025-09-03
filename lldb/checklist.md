# ✅ GNUstep LLDB Expression Evaluation Fix Checklist

> Goal: make expression evaluation reliable on Windows for ObjC/ObjC++ with GNUstep, using runtime introspection (no hardcoding).

---

## A. Runtime Activation
- [ ] Ensure **only one** `GNUstepObjCRuntime::CreateInstance`.
- [ ] Accept `eLanguageTypeC`, `eLanguageTypeObjC`, `eLanguageTypeObjC_plus_plus`.
- [ ] Add a **one-time guard** to call `InstallExpressionEvaluationHooks(target)`.
- [ ] Inside hooks:
  - [ ] Get `TypeSystemClang`.
  - [ ] Call `DeclVendor::EnsureRuntimeDecls(ts)` to inject prototypes.
  - [ ] Call `DeclVendor::EnsureCFStringCreateWithBytes(target)` to install utility function.
  - [ ] Log resolved addresses of `objc_msgSend`, `objc_getClass`, `sel_getUid`, and `CFStringCreateWithBytes`.

---

## B. DeclVendor Prototypes
- [ ] `EnsureRuntimeDecls(ts)` declares:
  - [ ] `id objc_msgSend(id, SEL, ...)`
  - [ ] `Class objc_getClass(const char*)`
  - [ ] `SEL sel_getUid(const char*)`
  - [ ] `Class object_getClass(id)`
  - [ ] `void* class_getMethodImplementation(Class, SEL)`
  - [ ] `int class_respondsToSelector(Class, SEL)`
- [ ] Make `FindDecls` always call `EnsureRuntimeDecls(ts)` once per AST.

---

## C. CFString Literal Fallback
- [ ] Add `AddCFunctionDecl` for:
