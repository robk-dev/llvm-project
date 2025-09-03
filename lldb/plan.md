# relevant paths

```
source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/
  GNUstepObjCRuntime.h
  GNUstepObjCRuntime.cpp
  GNUstepObjCRuntimeIntrospector.h
  GNUstepObjCRuntimeIntrospector.cpp
  GNUstepRuntimeV2API.h
  GNUstepRuntimeV2API.cpp
  GNUstepObjCDeclVendor.h
  GNUstepObjCDeclVendor.cpp
  GNUstepClassDescriptor.h
  GNUstepClassDescriptor.cpp

  // Foundation formatters (tons of them, all present)
  Formatters/NSNumberFormatter.cpp
  Formatters/NSStringFormatter.cpp
  Formatters/NSArrayFormatter.cpp
  ... (NSMutable*, NSDictionary*, NSSet*, NSAttributedString*, NSDate*, NSData*, NSUUID*, NSIndexPath*, NSIndexSet*, NSValue*, NSNull*, ...)

  README.md
```

These are the only places we need to change for the runtime, decl-vendor, and introspection work.

---

# 2) what you already have vs. gaps that cause our issues

## Already implemented (good!)

* **Runtime detection & ISA→class name**: present in `GNUstepObjCRuntime.cpp`/`Introspector.cpp`.
* **Tagged pointer detection & decode**: you already have `GNUstepObjCRuntimeIntrospector::IsTaggedPointer(...)` and `DecodeTaggedString(...)` etc. (nice).
* **Class enumeration**: `GNUstepRuntimeV2API::GetObjCClassInfo(...)` uses *in-target* `class_copyMethodList`, `method_getName`, `method_getTypeEncoding`, etc., via LLDB expression evaluation — and `GNUstepClassDescriptor::EnsureClassInfoLoaded(...)` consumes that. You also merge inherited methods.
* **Decl vendor**: `GNUstepObjCDeclVendor` builds ObjC interfaces on demand and uses the class descriptor’s `Describe(...)` to add methods/ivars/protocols.
* **Module load handling**: `GNUstepObjCRuntime::ModulesDidLoad(...)` + `UpdateISAToDescriptorMapIfNeeded(...)` re-scan when new modules show up.

## Where things still break / what’s fragile

1. **Tagged pointers are still treated as memory** in places:

   * In `GNUstepObjCRuntime::GetDynamicTypeAndAddress(...)` you **set `ValueType::Scalar` first**, but later you **always override to `LoadAddress`** once you’ve found a class name. That will force LLDB to dereference bogus (non-canonical) addresses like `0x00000000D3AE6288`, leading to the crash you saw in `objc_msgSend`.
     → We must *never* convert a tagged pointer into a load address.

2. **Expression evaluation options aren’t fully hardened for Windows**:

   * You set `IgnoreBreakpoints=true`, `UnwindOnError=true`, `TryAllThreads=false`, timeouts, etc., but don’t explicitly **disable exception trapping** (first-chance exceptions interfere on Windows when we poke the runtime) and don’t mark expressions as **short-living/JIT only** aggressively. That’s why you see evaluation being interrupted with the single-step/EXCEPTION codes during `expr` calls.

3. **Selector/class ref storage on PE/COFF**:

   * The `.objcrt` section stores selector/class refs in PE/COFF-specific ways (often indirects/RVAs). Your plugin **doesn’t write** there (good), but a debugging session that writes there (even by accident) will cause exactly the non-canonical RCX you caught. We must enforce *read-only* invariants in our helper paths and be very careful with any “remote malloc/free” calls or persistent expressions that could stomp globals.

4. **DeclVendor completeness/dedup & metaclass**:

   * You already populate methods through `ClassDescriptor::Describe(...)`. Two robustness items remain:

     * ensure **metaclass (class methods)** are fully merged onto the interface used by Clang when evaluating `+[Cls sel]`.
     * guard **deduplication** (avoid re-adding identical method decls) when both static tables and runtime enumeration add the same selector.

5. **Cache & re-scan semantics**:

   * You do the right thing on module loads, but the “complete” flags are easy to get wrong with reentrancy. A couple of guards ensure we never serve a stale, half-populated descriptor.

6. **User watchpoints on tagged objects**:

   * If a user attempts a watchpoint that involves a tagged object, LLDB can still try to compute an invalid base address. We should detect this and return a clear error rather than letting it fall through.

---

# 3) step-by-step plan (edits + snippets)

> The steps are ordered so that you can land them safely, one at a time. All file and function names below match your zip.

## Step 1 — Make tagged pointers first-class “scalars” end-to-end

**Files**:

* `GNUstepObjCRuntime.cpp`
* `GNUstepObjCRuntimeIntrospector.{h,cpp}` (already has IsTaggedPointer/decode helpers)

**Changes**:

1. In `GNUstepObjCRuntime::GetDynamicTypeAndAddress(...)`:

   * If `is_tagged` is true, **never** set `value_type = LoadAddress`. Keep `Scalar`, and **do not** set a load address on the `Value`.

   Minimal patch (illustrative diff):

   ```diff
   @@ bool GNUstepObjCRuntime::GetDynamicTypeAndAddress(...)

     bool is_tagged = m_introspector_up->IsTaggedPointer(obj_addr);

   ```

* // (later on, after you resolve the class name)
* value.SetValueType(Value::ValueType::LoadAddress);
* value.GetScalar() = obj\_addr;

- if (!is\_tagged) {
- ```
   value.SetValueType(Value::ValueType::LoadAddress);
  ```
- ```
   value.GetScalar() = obj_addr;
  ```
- } else {
- ```
   // Tagged pointers are immediates; keep as Scalar and DO NOT set a load address.
  ```
- ```
   // Optionally attach a synthetic CompilerType (id / NSNumber / NSString),
  ```
- ```
   // but do not force memory reads.
  ```
- }

````

2. When `is_tagged`, optionally **attach a better dynamic type**:
- Use your existing decode helpers to decide a friendly type (e.g., small NSNumber vs. tiny NSString) and set a `CompilerType` accordingly. If uncertain, fall back to `id`.  
- Return `has_dynamic_type_info = true` so summary providers kick in.

3. In all other places where an `addr_t` could be de-referenced, gate with `IsTaggedPointer(...)` and avoid `ReadMemory(...)`.

**Why this fixes the crash**: LLDB stops trying to dereference `0x00000000D3AE6288` as if it were an object in memory, so we won’t end up in `objc_msgSend` with junk RCX when formatting/evaluating.

---

## Step 2 — Harden expression evaluation on Windows

**Files**:  
- `GNUstepObjCRuntime.cpp` (you build `ExpressionOptions options;`)  
- `GNUstepRuntimeV2API.cpp` (`GetObjCClassInfo` and similar `EvaluateExpression(...)` call sites)

**Changes**:
1. Add:
```c++
options.SetTrapExceptions(false);
options.SetGenerateDebugInfo(false);
options.SetOneThreadTimeoutUS(0); // keep zero; we already do TryAllThreads(false)
options.SetPoundLineFilePath(""); // reduce debug info pollution
options.SetAllowJIT(true);
options.SetUseCustomSummary(true); // avoid deep formatter recursion during expr
````

You already have: `SetUnwindOnError(true)`, `SetIgnoreBreakpoints(true)`, `SetTryAllThreads(false)`, timeouts. Keep those.

2. Wrap all runtime helper expressions with **guard rails**:

   * Use short, side-effect-free snippets only (you’re already doing that).
   * **Never** persist them; use non-persistent `EvaluateExpression` so they’re not re-executed at weird times.
   * If an evaluation throws/interrupts (Windows first-chance), **catch** and **retry once** with a slightly longer timeout, otherwise return “unknown” gracefully.

   Example in `GNUstepRuntimeV2API::GetObjCClassInfo(...)` (pseudo):

   ```c++
   auto try_eval = [&](llvm::StringRef expr, TypedValue &out) -> bool {
     EvaluateExpressionOptions opts = MakeSafeOpts(); // includes TrapExceptions(false)
     auto result = m_process.CalculateTarget().EvaluateExpression(expr, frame, opts);
     if (result.Success()) { out = ParseResult(result); return true; }
     // One retry:
     opts.SetTimeout(lldb::TimeDuration::FromMilliseconds(5000));
     result = m_process.CalculateTarget().EvaluateExpression(expr, frame, opts);
     if (!result.Success()) return false;
     out = ParseResult(result); return true;
   };
   ```

**Why this helps**: it prevents our helper expressions from tripping up on Windows’ exception machinery and avoids reentrancy issues that lead to surprising stop reasons during `expr`.

---

## Step 3 — DeclVendor finish work: class + metaclass, dedup, and completeness

**Files**:

* `GNUstepObjCDeclVendor.cpp`
* `GNUstepClassDescriptor.{h,cpp}`

**Changes**:

1. In `GNUstepObjCDeclVendor::FinishDecl(...)`:

   * You already call into `desc->Describe(...)`. Verify you **apply both instance and class methods** to the correct decls. For class methods, you may either:

     * synthesize a **metaclass** interface and hang +methods there (Apple parity), **or**
     * inject +methods onto the same interface but mark them as `isInstance=false` (your code seems to prefer that — fine).
   * Add a micro-dedup guard:

     ```c++
     if (!InterfaceAlreadyHasMethod(interface_decl, sel_name, is_instance)) {
       CreateMethodDeclFromTypeEncoding(...);
     }
     ```

2. If you also pre-seed with Foundation statics anywhere, guard against dupes by selector string key.

3. Cache completeness:

   * once `FinishDecl` runs, **mark the decl completed** and store it in the ISA/name maps you already maintain so repeated requests don’t repopulate.

**Why this helps**: stops “unknown selector” compilation errors during expression and prevents churn in the AST from duplicate methods.

---

## Step 4 — Cache invalidation/sharp edges

**Files**:

* `GNUstepObjCRuntime.cpp` (`ModulesDidLoad(...)`, `UpdateISAToDescriptorMapIfNeeded(...)`)

**Changes**:

1. `ModulesDidLoad(...)`: you already iterate new modules. Add two guards:

   * Only rescan modules that **can** contain ObjC (`.objcrt` or `OBJC_CLASS_$_` symbols)
   * Use a **reentrancy guard** (`std::atomic<bool> scanning`) so we never re-enter during nested stop events.

2. In `UpdateISAToDescriptorMapIfNeeded(...)`, flip the “complete” notion to **“up-to-date”** and set it **per module** (small set) rather than global, to avoid O(N modules) rescan each stop.

---

## Step 5 — Don’t allow bogus watchpoints on tagged objects

**Files**:

* `GNUstepObjCRuntime.cpp` (hook via `GetDynamicTypeAndAddress(...)` result)
* (No changes to watchpoint command parsing required)

**Changes**:

* If the base object `addr` is a tagged pointer, propagate a “not in memory” status (return false for address resolution), so a later `watchpoint set expression` on `obj->ivar` yields a clear LLDB error:

  ```
  error: cannot set a watchpoint on a tagged Objective-C object (no backing memory)
  ```

---

## Step 6 — Add an internal breakpoint hook for libobjc2 class loads (optional, nice-to-have)

**Files**:

* `GNUstepObjCRuntime.cpp`

**Changes**:

* On attach/launch, try to set an internal breakpoint on a stable symbol like `__objc_load` (libobjc2). If it’s present:

  * breakpoint callback = invalidate class maps for that module and rescan minimal set.

(Purely an optimization; your module-load hook already gets the job done.)

---

## Step 7 — Defensive logging & diagnostics

**Files**:

* `GNUstepObjCRuntime.cpp`
* `GNUstepRuntimeV2API.cpp`
* `GNUstepObjCRuntimeIntrospector.cpp`

**Changes**:

* Add `LLDB_LOG` breadcrumbs at:

  * start/end of `GetDynamicTypeAndAddress` (log `is_tagged`, chosen `ValueType`)
  * before/after each runtime `EvaluateExpression` (expr snippet + success/failure)
  * when `ModulesDidLoad` triggers a rescan and how many classes were added
* Ensure logs are under a runtime category (e.g. `"gnustep-runtime"`), so we can turn them on/off with `log enable lldb gnustep-runtime`.

---

## Step 8 — tests you can run immediately

* **Tagged pointer**: evaluate/print small `NSNumber` and tiny `NSString` values; ensure no memory reads occur (no crashes, correct summaries).
* **Expression**: `po [NSString stringWithFormat:@"x%d", 42]` and arbitrary method calls on `NSString`, `NSArray`, etc.
* **Dynamic class**: load a bundle that defines a new class; after stop, `expr (Class)NSClassFromString(@"NewClass")` should succeed; then `po [NewClass new]`.
* **Watchpoints**: trying a watchpoint on a tagged object should produce a clear error.

---

# 4) concrete code snippets to drop in

### A) `GNUstepObjCRuntime::GetDynamicTypeAndAddress(...)` — don’t force LoadAddress on tagged

```c++
bool GNUstepObjCRuntime::GetDynamicTypeAndAddress(
    ValueObject &in_valobj, CompilerType &dynamic_type, Address &address,
    Value::ValueType &value_type, bool &is_class_ptr) {
  // ... existing setup

  const addr_t obj_addr = obj_ptr;
  const bool is_tagged  = m_introspector_up->IsTaggedPointer(obj_addr);

  // Establish a safe default: tagged -> Scalar, heap -> LoadAddress.
  value_type = is_tagged ? Value::eValueTypeScalar : Value::eValueTypeLoadAddress;

  // Resolve class name/CompilerType (your existing logic)
  // ...

  if (!is_tagged) {
    // normal heap object: provide a load address so summaries/formatters can read memory
    value.GetScalar() = obj_addr;
    address.SetLoadAddress(obj_addr, &process->GetTarget());
  } else {
    // tagged: keep as immediate; DO NOT set an address.
    // Optional: attach a better dynamic CompilerType (NSNumber/NSString) based on tag decode.
    // dynamic_type = DetectTaggedDynamicType(obj_addr);
  }

  has_dynamic_type_info = true;
  return true;
}
```

### B) “safe” expression options utility

Add a tiny helper you can reuse in both `GNUstepObjCRuntime.cpp` and `GNUstepRuntimeV2API.cpp`:

```c++
static EvaluateExpressionOptions MakeSafeExprOpts() {
  EvaluateExpressionOptions opts;
  opts.SetUnwindOnError(true);
  opts.SetIgnoreBreakpoints(true);
  opts.SetTryAllThreads(false);
  opts.SetTimeout(lldb::TimeDuration::FromMilliseconds(2500));
  opts.SetTrapExceptions(false);       // important on Windows
  opts.SetAllowJIT(true);
  opts.SetGenerateDebugInfo(false);
  return opts;
}
```

Use it everywhere you `EvaluateExpression(...)`.

### C) DeclVendor dedup guard

Inside `GNUstepObjCDeclVendor::FinishDecl(...)` when adding a method:

```c++
auto has_method = [&](clang::ObjCInterfaceDecl *iface,
                      llvm::StringRef sel, bool is_instance) {
  for (auto *m : iface->methods()) {
    if (m->isInstanceMethod() == is_instance &&
        m->getSelector().getAsString() == sel) return true;
  }
  return false;
};

if (!has_method(interface_decl, sel_name, /*is_instance*/true)) {
  CreateMethodDeclFromTypeEncoding(...);
}
```

(Repeat for class methods with `is_instance=false`.)

---

# 5) one Windows-specific caution (re: the triage you did)

You manually wrote to `.objcrt` (`memory write -s8 0x7ff9d3b23618 ...`), which turned the **class ref** storage into a **non-canonical** pointer (RCX became `0x00000000D3AE6288`) and that alone explains the `objc_msgSend` access violation you hit when the runtime tried to call `+[NSThread exit]`. Our plugin changes above will *avoid dereferencing tagged pointers*, but please keep `.objcrt` strictly read-only in future investigation runs — PE/COFF stores many of those as RVAs or thunked indirections; writing 64-bit absolute values there will corrupt call sites.

---

# 6) quick launch.json tune-ups (so your DAP session behaves like your CLI session)

Add to your LLDB init commands (you can keep what you have and add these):

```jsonc
"initCommands": [
  "settings set target.language objc++",
  "settings set target.process.stop-on-sharedlibrary-events false",
  "settings set target.prefer-dynamic-value no-dynamic-values",
  "settings set target.auto-import-clang-modules false",
  "settings set target.clang-module-search-paths \"\"",
  "settings set target.process.follow-fork-mode child",      // auto-attach children
  "settings set target.process.thread.step-avoid-libraries libobjc-*,gnustep-*",
  "settings set plugin.objc.runtime gnustep",                // ensure our plugin
  "log enable lldb gnustep-runtime formatters"
]
```

(And keep your PATH/env the way you showed earlier.)

---

## summary / landing order

1. **Step 1** (tagged pointers never → LoadAddress) — fixes the crash.
2. **Step 2** (expression options) — stabilizes runtime queries on Windows.
3. **Step 3** (decl-vendor completeness + dedup) — eliminates “unknown selector” eval failures.
4. **Step 4** (cache semantics) — correctness over time with dynamic loads.
5. **Step 5** (watchpoint messaging) — prevents user foot-guns.
6. **Step 6/7** (optional breakpoint + logs) — nicer DX and easier triage.

