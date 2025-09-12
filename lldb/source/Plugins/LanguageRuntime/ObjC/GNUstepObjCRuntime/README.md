# GNUstep Objective‑C Runtime Plugin (LLDB)

This plugin brings first‑class Objective‑C debugging on Linux and Windows using GNUstep/libobjc2. It detects GNUstep runtimes, provides dynamic type resolution, high‑quality object summaries and synthetic children, and powers expression evaluation by synthesizing Objective‑C interfaces on the fly.

## What it does
- Runtime detection and activation for GNUstep/libobjc2 (`libobjc.so`, `libobjc2`, `libgnustep-base`)
- Dynamic type discovery for `id`/`NSObject*` et al. (incl. tagged pointers)
- Object descriptions for `po`/`expression` via safe runtime calls or fallbacks
- Universal formatter that covers common Foundation classes and custom classes
- Synthetic children for arrays, dictionaries, sets, and ivars in custom classes
- Expression evaluator enablement via a DeclVendor + ExternalASTSource
- Class/ivar/method discovery using direct libobjc2 runtime APIs (no fragile DWARF parsing)

## Architecture overview
Core components live in this folder. Each has a clear responsibility and they collaborate at runtime.

- `GNUstepObjCRuntime.{h,cpp}`
  - The main plugin class, registered as `gnu-objc-v2`.
  - Detects the runtime, wires into LLDB’s ObjC language runtime, and exposes:
    - `GetObjectDescription` – summaries for `po` using tagged‑pointer decoding, fast runtime calls (`objc_msgSend`/`UTF8String`), expression fallback, and a universal formatter fallback.
    - `GetDynamicTypeAndAddress` – resolves dynamic class names (special‑cases tagged pointers), sets `ValueType` correctly, and asks the `DeclVendor` for a proper `CompilerType` so formatters work reliably.
    - `UpdateISAToDescriptorMapIfNeeded` – populates LLDB’s ISA→descriptor cache using runtime enumeration.
    - `GetStepThroughTrampolinePlan` – basic step‑out plan when stopped in `objc_msgSend` and similar trampolines (see “Differences vs Apple” below).
    - `CalculateHasNewLiteralsAndIndexing` – probes whether modern subscripting symbols exist in the current process.

- `GNUstepObjCRuntimeIntrospector.{h,cpp}`
  - Direct bridge to libobjc2: calls runtime functions (e.g., `objc_getClass`, `class_copyMethodList`, `class_getSuperclass`, `object_getClass`, `ivar_*`, `objc_copyClassList`) and reads critical object/class fields in target memory.
  - Tagged pointers: quick checks, tiny‐string decode, and class resolution via `object_getClass`.
  - Caches symbol lookups and ISA→name mappings for performance.

- `GNUstepObjCRuntimeUtilities.{h,cpp}`
  - Safe evaluation options tuned for Windows/Linux (timeouts, exception trapping)
  - `TargetStringAllocator` for argument strings in target memory
  - `RuntimeFunctionCaller` that centralizes symbol resolution and function calls (`objc_msgSend`, `sel_getUid`, etc.) with ABI‑correct call plans and thread‑safe execution.

- `GNUstepClassDescriptorV2.{h,cpp}`
  - Implements `ObjCLanguageRuntime::ClassDescriptor` using the Introspector.
  - Supplies superclass, metaclass, instance size, method lists (instance/class), and ivars (including inherited) – the backbone for both expression evaluation and synthetic views.

- `GNUstepObjCDeclVendor.{h,cpp}`
  - Creates Objective‑C interface declarations on demand for the expression parser (Clang AST).
  - Uses `ExternalASTSource` to lazily “finish” interfaces: pulls superclass, methods, and ivars from the runtime via the `ClassDescriptor` and installs them into the scratch AST.
  - Ensures `CompilerType` is available for dynamic objects so LLDB’s formatter matching works (critical for collection children like `dict[0].key`).

- `GNUstepUniversalFormatter.{h,cpp}`
  - A universal summary and synthetic provider registered from `ObjCLanguage.cpp` (see `LoadGNUstepFormatters`).
  - Summary path: prefer `[obj description]` → `UTF8String` via runtime calls; fall back to class+address or collection count.
  - Synthetic path: supports arrays, dictionaries, sets via direct calls (e.g., `objectAtIndex:`, `allKeys`, `objectForKey:`, `allObjects`) and shows custom classes’ ivars (via Introspector + ClassDescriptor).

## How data flows at runtime
- Printing/summary (`po`):
  1) If the pointer is tagged, decode immediately (tiny NSString or small NSNumber) and return.
  2) Otherwise, call `[obj description]` using `RuntimeFunctionCaller`, then `UTF8String` and read memory back for the printable C string.
  3) If that fails, evaluate a minimal expression as a fallback.
  4) If still unresolved, ask the universal formatter; as a last resort, emit `<Class: 0xaddr>`.

- Dynamic typing:
  1) Check `CouldHaveDynamicValue` and pointer validity via Introspector.
  2) Tagged pointers: keep `ValueType=Scalar` (never dereference), set name only.
  3) Regular objects: read ISA, fetch class name from Introspector, set `LoadAddress` and ask `DeclVendor` to produce a proper `CompilerType*` for the class pointer.

- Expression evaluation:
  - `DeclVendor` creates/finishes ObjC interfaces in the scratch Clang AST, driven by `ClassDescriptor::Describe` (superclass, methods, ivars). This breaks the recursion trap where expression evaluation needs type info that only the runtime knows.

## Differences from Apple’s reference runtime plugin
This plugin targets GNUstep/libobjc2 on non‑Apple platforms and intentionally diverges in a few areas to be robust cross‑platform.

- Trampoline stepping
  - Apple: dedicated, architecture‑aware “step through ObjC trampoline” logic that lands directly in the real IMP, with nuanced breakpoint/resolver support.
  - Here: pragmatic “step‑out of trampoline” via `ThreadPlanStepOut` when stopped in `objc_msgSend` (or friends). It reliably gets you from the runtime dispatch frame back to user code, but it’s not a full Apple‑style trampoline stepper. Upgrading to a full step‑through plan is tracked in “Future work”.

- Formatters
  - Apple ships many specialized Cocoa formatters. We provide a universal formatter that covers common Foundation types and custom classes by dispatching to the runtime and inspecting classes dynamically. This keeps maintenance low and works for GNUstep variants (e.g., GS* classes) without per‑class hardcoding.

- Tagged pointers
  - Implementation is tailored to libobjc2’s tagging (common tags observed: 1=NSNumber, 2=NSDate, 4=NSString/tiny string). Apple’s exact tagging semantics and masks differ.

- Runtime function bridge
  - We use a consolidated, thread‑safe `RuntimeFunctionCaller` that builds ABI‑correct call plans, allocates argument strings in target memory, and applies safe evaluation timeouts (important on Windows). Apple’s implementation can rely on different platform facilities and tighter integration.

- Expression integration hooks
  - When Apple‑specific helpers (`gdb_object_getClass`) exist, we’ll opportunistically use them; otherwise we fall back to portable runtime calls. The object checker paths are adapted accordingly.

- Modern literals and subscripting
  - Apple’s runtime/SDKs guarantee modern literals/subscript availability. GNUstep/libobjc2 may not implement all modern subscripting selectors in the target. `CalculateHasNewLiteralsAndIndexing` probes for selectors like `-[NSDictionary objectForKeyedSubscript:]` and `-[NSArray objectAtIndexedSubscript:]` to decide feature availability.

## Current limitations and known gaps
- Trampoline step‑through
  - Only a basic “step out” plan is implemented. A real step‑through trampoline plan that resolves the target IMP (and handles super/caching/fast‑path lookups) would improve the single‑step experience.

- Subscripting and modern literals
  - If the target’s Foundation does not implement keyed/indexed subscripting methods, expression sugar and some formatter conveniences won’t be available. This is a runtime capability, not just a debugger feature.

- Exception breakpoints
  - `CreateExceptionResolver` is currently a stub.

- Coverage and polish in formatters
  - The universal approach is robust, but specific edge cases (e.g., large dictionaries, unusual class clusters, proxies) can benefit from targeted providers.

- Performance
  - Most hot paths are cached, but first‑time class enumeration and AST population can be noticeable. This is expected and limited to initial queries.

## Build and try it
- Build just the plugin:
  - VS Code Task: `build-plugin-only`
  - Terminal:
    - `cd /home/robk/code/llvm-project/build`
    - `ninja lldbPluginGNUstepObjCRuntime`
- Build LLDB components and examples:
  - VS Code Task: `build-plugin-and-tests` or `build-all-test-programs`
- Run with example programs in `lldb/examples/` (ensure you compiled with correct GNUstep flags; see `GNUSTEP_DEBUGGING.md`).

## File map (quick reference)
- `GNUstepObjCRuntime.*` – Plugin entry, dynamic typing, object description, module hooks
- `GNUstepObjCRuntimeIntrospector.*` – Direct runtime/memory bridge, tagged pointers, class/method/ivar enumeration
- `GNUstepObjCRuntimeUtilities.*` – Safe expression opts, ABI‑correct runtime calls, module lookup helpers
- `GNUstepClassDescriptorV2.*` – Class metadata adapter for LLDB
- `GNUstepObjCDeclVendor.*` – Clang AST synthesis (interfaces/methods/ivars) for expression parser
- `GNUstepUniversalFormatter.*` – Universal summary and synthetic children
- `CMakeLists.txt` – Plugin build target `lldbPluginGNUstepObjCRuntime`

## Future work
- Implement a full step‑through trampoline plan
  - Recognize `objc_msgSend*`, `objc_msgLookup*`, block trampolines, and land in the resolved IMP with architecture‑aware logic and cache hints.
- Expand formatter coverage
  - Targeted providers for tricky class clusters (e.g., bridged collections), better large‑collection pagination, and stable dictionary enumeration without allocating.
- Improve exception integration
  - Real exception resolver support on GNUstep.
- Harden subscripting support
  - Optional method‑forwarding shims in DeclVendor when a modern subscript isn’t present but a legacy method is (when safe and observable at runtime).
- More tagged pointer cases
  - Confirm and handle additional libobjc2 tags and encodings across architectures.

## Notes and gotchas
- Many operations require the process to be in `stopped` state; the plugin guards against running‑state calls and timeouts.
- Tagged pointers are treated as immediate values. We never dereference them; their `ValueType` is kept as `Scalar` to avoid bad memory reads.
- The plugin is careful with recursion: reentrancy guards prevent nested calls from running the same expensive paths.

---

Completion summary
- Overview of purpose: done
- Component descriptions and interactions: done
- Differences from Apple’s runtime plugin and rationale: done
- Known gaps and future work: done
- Minimal build/try notes: done

If you want, I can add an index from `CLAUDE.md` and `GNUSTEP_DEBUGGING.md` back to this README and wire up cross‑references.
