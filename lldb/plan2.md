# Research agent plan

🧠: I am an expert in LLDB internals, Objective-C runtimes (Apple & GNUstep/libobjc2), Clang/LLVM codegen, and cross-platform (Windows x64 / COFF) debugging.

I know your current LLDB fork adds `GNUstepObjCRuntime` under `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/`, with a working POC for Foundation visualizers and class descriptors, but expression evaluation fails for some ObjC constructs on Windows (e.g., `NSNumber` creation/usage, ObjC subscripting). I also know LLDB already upstreamed basic GNUstep hooks and testing switches, so we can align with that path. ([reviews.llvm.org][1])

I will reason step-by-step to determine the best course of action to achieve a minimal, upstream-ready contribution that reliably enables a **minimum set of expression-evaluation features** for GNUstep/libobjc2 on Windows:

* NSNumber creation & scalar extraction
* NSArray/NSDictionary literals/subscripting (compiler maps these to selectors)
* Class/selector/runtime calls via `objc_msgSend` with correct calling convention on Win64
* Declarative AST scaffolding for a *very* small Foundation surface (NSString/NSNumber/NSArray/NSDictionary)
* Basic, stable data formatters you already have

I can use LLDB’s LanguageRuntime/DeclVendor extension points, Clang expression parser hooks, and minimal runtime symbol discovery. Relevant frameworks: LLDB `ObjCLanguageRuntime` interface, DeclVendor, Clang `LangOptions.ObjCRuntime`, and ORC JIT linking behavior. (Clang maps ObjC subscripting to specific selectors; GNUstep Base exposes these selectors). ([LLDB][2], [Clang][3], [GNUstep][4])

I will help you accomplish your goal by following these steps:

1. **Root-cause triage** (tight scope): identify why your three failing expressions break on Windows x64 with libobjc2.
2. **Minimal feature target & acceptance criteria** for an upstream-friendly MR.
3. **Precise code changes** (file-by-file) to:

   * enable ObjC “new literals & subscripting” in your runtime,
   * fix Windows calling conventions for `objc_msgSend` cast thunks,
   * harden runtime symbol resolution for COFF/`__imp_` symbols,
   * keep tiny Foundation AST shims for expressions (NSNumber/NSArray/NSDictionary/NSString).
4. **Tests** (API + lit) that align with `LLDB_TEST_OBJC_GNUSTEP`.
5. **MR draft** (summary, scope, risk, perf notes).
6. **Verification playbook** on Windows.
7. **Cut branch & commit plan** with a tight, reviewable diff.

My task ends when: the minimal expressions below evaluate correctly on Windows x64 against libobjc2 + GNUstep Base:

* `(id)[NSNumber numberWithInt:7]`
* `(int)[(id)[NSNumber numberWithInt:7] intValue]`
* `id arr=(id)[NSArray arrayWithObjects:@"a",@"b",nil]; (id)arr[0]`

First step, question: would you like the branch name `wip/gnustep-objc-eval-mvp` and the MR titled **“\[lldb]\[gnustep]\[windows] Minimal ObjC expression-evaluation MVP (NSNumber + subscripting)”**?

---

## 🧙🏾‍♂️: Aligning on your goal

You want a **minimum viable, upstream-palatable** patch that makes ObjC expression evaluation actually work for the most common cases on Windows with GNUstep/libobjc2 — without blowing scope.

### Why those expressions likely fail (tight diagnosis)

1. **ObjC subscripting/literals aren’t enabled** in your runtime, so Clang doesn’t map `arr[0]` to the correct selectors. For LLDB, this is gated by `HasNewLiteralsAndIndexing()` on the runtime; Apple’s runtime enables it by checking for `objectForKeyedSubscript:` in images. GNUstep **does** implement subscripting methods in Base (e.g., `NSDictionary -objectForKeyedSubscript:`), so your runtime should expose this too. ([Clang][3], [GNUstep][4])

2. **Windows x64 calling convention** for `objc_msgSend` (varargs) must be correct. On Win64 there’s a single calling convention (RCX/RDX/R8/R9, etc.), and `objc_msgSend` export/imp name handling differs (COFF `__imp_` thunks). Incorrect CC or symbol resolution => mis-calls/crashes or JIT linkage failures. There’s historical context that `objc_msgSend` on Windows has been tricky in libobjc2 — we should set the calling convention explicitly when building the typed function pointer casts LLDB uses in expressions, **and** make symbol resolution robust to `__imp_` decoration. ([Microsoft Learn][5], [Wikipedia][6], [lists.gnu.org][7])

3. **AST scaffolding for Foundation**: Expressions like `[NSNumber numberWithInt:]` and `intValue` must compile in the expression parser even when headers aren’t present. That’s what your `GNUstepObjCDeclVendor` is for — but we must ensure the *right minimal method set* is injected and that our cast thunks (when needed) use the correct CC & return types.

Net: enable subscripting, fix CC + symbol resolution, and keep the AST surface tiny & deterministic.

---

## 🧠: Actionable plan (file-by-file)

### A) Enable ObjC literals/subscripting in runtime (critical)

**Files:**

* `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h/.cpp`

**Change:** Implement `CalculateHasNewLiteralsAndIndexing()` analogously to Apple’s runtime, but targeting GNUstep Base:

```cpp
// GNUstepObjCRuntime.cpp
bool GNUstepObjCRuntime::CalculateHasNewLiteralsAndIndexing() {
  if (!m_process) return false;
  Target &T = m_process->GetTarget();
  // Presence of subscripting is the canonical detector:
  //  -[NSDictionary objectForKeyedSubscript:]
  SymbolContextList sc_list;
  T.GetImages().FindSymbolsWithNameAndType(
    ConstString("-[NSDictionary objectForKeyedSubscript:]"),
    eSymbolTypeCode, sc_list);

  // Fallback: NSArray subscripting
  if (sc_list.IsEmpty()) {
    T.GetImages().FindSymbolsWithNameAndType(
      ConstString("-[NSArray objectAtIndexedSubscript:]"),
      eSymbolTypeCode, sc_list);
  }
  return !sc_list.IsEmpty();
}
```

This mirrors Clang’s mapping of subscripting to selectors and uses what GNUstep Base exposes. This flips `LangOptions.DebuggerObjCLiteral = true` for expressions. ([Clang][3], [GNUstep][4])

**Acceptance:** `id arr=(id)[NSArray arrayWithObjects:@"a",@"b",nil]; (id)arr[0]` compiles & runs (selector mapping kicks in).

---

### B) Fix `objc_msgSend` calling convention & symbol resolution on Windows

**Files:**

* `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCDeclVendor.cpp` (where you build function prototypes & casts)
* `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp` (runtime symbol resolution)

**1) Calling convention:** When you build typed casts for `objc_msgSend` (and friends), use the **target’s proper CC**:

```cpp
// In GNUstepObjCDeclVendor.cpp when constructing FunctionProtoType:
// Choose the right CC for Windows x64:
auto &TI = ctx.getTargetInfo();
auto Triple = TI.getTriple();
llvm::CallingConv::ID CC = llvm::CallingConv::C;
if (Triple.isOSWindows() && Triple.getArch() == llvm::Triple::x86_64) {
  CC = llvm::CallingConv::X86_64_Win64;
}
FunctionProtoType::ExtProtoInfo epi;
epi.ExtInfo = epi.ExtInfo.withCallingConv(CC);
QualType fTy = ctx.getFunctionType(retTy, paramTys, epi);
```

Use **variadic** prototypes for `objc_msgSend` when needed (`epi.Variadic = true`), and **precise return types** for numeric returns (e.g., `int`, `double`). On Win64, the ABI dictates register usage and float returns in XMM0; the cast signature must match so the JIT emits correct calls. ([Microsoft Learn][5])

**2) Varargs methods:** For `+arrayWithObjects:` ensure the decl allows varargs (`...`) so expressions like `[@"a", @"b", nil]` via `arrayWithObjects:` lower correctly:

```cpp
// Declare objc_msgSend(id, SEL, ...) and objc_msgSendSuper where applicable.
AddCFunctionDecl(ctx, TU, "objc_msgSend", GetIdTy(ctx),
                 { GetIdTy(ctx), GetSelTy(ctx) }, /*isVariadic*/true);
```

**3) COFF `__imp_` imports:** Make runtime symbol lookup resilient:

```cpp
// GNUstepObjCRuntime.cpp
static const char *kNames[] = {
  "objc_msgSend", "__imp_objc_msgSend", "_objc_msgSend", "__imp__objc_msgSend"
};
for (auto *n : kNames) {
  if (TryResolveAndCache(n)) break;
}
```

If you already have a resolver, extend it similarly for `objc_getClass`, `sel_getUid`, and (if used) stret/fpret variants (often unnecessary on x86\_64, but be defensive). Historical notes show Windows/libobjc2 needed special care here. ([lists.gnu.org][7])

**Acceptance:**

* `(id)[NSNumber numberWithInt:7]` returns a valid object.
* `(int)[(id)[NSNumber numberWithInt:7] intValue]` returns `7`.

---

### C) Keep AST scaffolding tiny & deterministic

**Files:**

* `GNUstepObjCDeclVendor.{h,cpp}`

**What to expose (only):**

* **NSString**: `+stringWithUTF8String:`, `-UTF8String`, `-length`, `-description`.
* **NSNumber**: `+numberWithInt:`, `+numberWithDouble:`, `+numberWithBool:`, `-intValue`, `-doubleValue`, `-boolValue`.
* **NSArray**: `+arrayWithObjects:`, `-objectAtIndex:`, `-objectAtIndexedSubscript:`.
* **NSDictionary**: `-objectForKey:`, `-objectForKeyedSubscript:`.

That’s enough for your three failing patterns; Clang maps subscripts to the subscript selectors. ([Clang][3], [GNUstep][4])

**Implementation notes:**

* Keep your existing dynamic discovery path, but **hard-fallback** to the small static signatures above when runtime reflection isn’t available at expression parse time.
* Make sure numeric return types are exact so the JIT generates correct call sequences on Win64.
* Limit class list to these four to keep runtime queries fast (perf matters; there was an LLDB perf regression related to GNUstep probes). ([reviews.llvm.org][1])

---

### D) Don’t touch generic Cocoa formatters in `Plugins/Language/ObjC/`

You already have dedicated GNUstep formatters. Keep these isolated so the MR remains safe and reviewable.

---

### E) Hook up `HasNewLiteralsAndIndexing()` to Clang expression options

This happens automatically once (A) is implemented; LLDB sets `LangOptions.ObjCRuntime = GNUstep` when your runtime advertises as such, which unlocks literal/subscript lowering during expression compilation. ([LLDB][2])

---

## ✅ Minimal acceptance criteria (MVP)

On **Windows x64 + libobjc2 + GNUstep Base** target:

```
expr -l objc++ -O -- (id)[NSNumber numberWithInt:7]                 // non-null id
expr -l objc++ -O -- (int)[(id)[NSNumber numberWithInt:7] intValue] // 7
expr -l objc++ -O -- id arr=(id)[NSArray arrayWithObjects:@"a",@"b",nil]; (id)arr[0] // @"a"
```

…and `po`/visualizers continue to work for NSString/NSNumber/NSArray/NSDictionary.

---

## 🧠: Tests & CI hooks (aligned with upstream)

1. **Enable GNUstep tests:** Use (or mimic) `LLDB_TEST_OBJC_GNUSTEP` switch already upstreamed for the test suite. Add a Windows-guarded test directory with a tiny ObjC sample binary linked to GNUstep Base. ([reviews.llvm.org][8])
2. **API tests** (lit / python):

   * `TestExprNSNumber.py` – covers the two NSNumber expressions above.
   * `TestExprSubscripting.py` – covers `NSArray` and `NSDictionary` subscripting.
3. **Symbol resolution test** (Windows only): Validate `__imp_objc_msgSend` fallback by dumping the bound address (skip if symbol not present).
4. **No perf regression**: Ensure we don’t probe symbols at `CreateInstance()` (see prior regression) — resolve only on `ModulesDidLoad` and cache. ([reviews.llvm.org][1])

---

## 🧠: MR skeleton (ready-to-paste)

**Title:** `[lldb][gnustep][windows] Minimal ObjC expression-evaluation MVP (NSNumber + subscripting)`
**Summary:**

* Implements `GNUstepObjCRuntime::CalculateHasNewLiteralsAndIndexing()` so Clang maps ObjC subscripting to selectors under GNUstep.
* Corrects Windows x64 calling conventions for `objc_msgSend` casts in `GNUstepObjCDeclVendor` and hardens COFF symbol resolution (`__imp_` / decorated names).
* Limits DeclVendor’s “Foundation shim” to NSString/NSNumber/NSArray/NSDictionary with precise signatures.
* Adds Windows-focused API tests under `LLDB_TEST_OBJC_GNUSTEP`.

**Scope out of MVP:** stepping trampolines, super-sends edge cases, tagged pointers, ARC nuances, full Foundation surface.

**Risk:** low, GNUstep-gated paths only. Apple runtime paths untouched.
**Perf:** no expensive probes at create-time; resolve once on `ModulesDidLoad`, then cache. ([reviews.llvm.org][1])

---

## 🧠: Implementation checklist for Claude Code (do in order)

1. **Branch**

```bash
git checkout -b wip/gnustep-objc-eval-mvp
```

2. **Runtime literals/subscripting**

* Add `CalculateHasNewLiteralsAndIndexing()` to `GNUstepObjCRuntime.{h,cpp}` as above. Ensure it returns true when either `-[NSDictionary objectForKeyedSubscript:]` or `-[NSArray objectAtIndexedSubscript:]` is found. ([Clang][3], [GNUstep][4])

3. **Calling convention fix**

* In `GNUstepObjCDeclVendor.cpp`, **when creating function prototype types for msgSend casts**, compute CC from `ASTContext::getTargetInfo().getTriple()` and set `CallingConv::X86_64_Win64` on Windows x64; otherwise leave `CallingConv::C`. Mark varargs where appropriate. ([Microsoft Learn][5])

4. **Symbol resolution hardening**

* In `GNUstepObjCRuntime.cpp`, expand `ResolveAndCacheRuntimeSymbols()` to try `{"objc_msgSend","__imp_objc_msgSend","_objc_msgSend","__imp__objc_msgSend"}` and same style for any other runtime calls you depend on. Cache once; avoid repeated lookups. ([lists.gnu.org][7])

5. **Trim & verify DeclVendor surface**

* Keep just the small method sets listed above for `NSString/NSNumber/NSArray/NSDictionary`. Ensure signatures/encodings match Clang expectations (e.g., `@#@:i` for `+numberWithInt:` and `i@:` for `-intValue`). (Clang maps subscripting to selectors; don’t over-declare.) ([Clang][3])

6. **Tests**

* Add small API tests under the GNUstep test switch, asserting the three expressions work and returning the expected types/values.
* On Windows runners, log the final bound address you call for `objc_msgSend` so we can diagnose import thunk issues (non-fatal).

7. **Docs & MR**

* Add a short note in `lldb/docs` (or in the MR description) describing GNUstep-Windows MVP coverage and future work.

---

## 🧙🏾‍♂️: Verification playbook (manual)

* Launch a tiny ObjC sample linked to GNUstep Base (Windows x64).
* In LLDB:

  * `settings set target.language objective-c++` (or use `-l objc++` per expression).
  * Run the three acceptance expressions.
  * Verify `po` formatting still works for the four classes.
  * Optional: dump symbol resolution logs to confirm `objc_msgSend` address (import thunk vs direct).

---

## 📚 References (for reviewers)

* **Upstream GNUstep support & perf note** – LLDB review (Aug 2023). ([reviews.llvm.org][1])
* **Enable GNUstep tests flag** – `LLDB_TEST_OBJC_GNUSTEP`. ([reviews.llvm.org][8])
* **Clang mapping for ObjC subscripting** – docs (selectors used). ([Clang][3])
* **GNUstep Base provides subscript selectors** – e.g., `NSDictionary -objectForKeyedSubscript:`. ([GNUstep][4])
* **Windows x64 calling convention** – MSVC docs (registers/returns). ([Microsoft Learn][5])
* **Historical note on `objc_msgSend` on Windows/libobjc2** – gnustep-dev ML. ([lists.gnu.org][7])
* **LLDB `ObjCLanguageRuntime` interface** – where our hooks attach. ([LLDB][2])

---

🧙🏾‍♂️: \[aligning on my goal] If you’re happy with this exact MVP cut, I’ll proceed with that branch name and MR title.

🧠: Would you like me to (1) generate the concrete patch snippets for `CalculateHasNewLiteralsAndIndexing()`, the Windows CC block, and the symbol-resolver trylist exactly against your uploaded tree, or (2) include a tiny test program + lldb test files in the same pass?

[1]: https://reviews.llvm.org/D158205?utm_source=chatgpt.com "[lldb] Fix performance regression after adding GNUstep ..."
[2]: https://lldb.llvm.org/cpp_reference/classlldb__private_1_1ObjCLanguageRuntime.html?utm_source=chatgpt.com "lldb_private::ObjCLanguageRuntime Class Reference - LLVM"
[3]: https://clang.llvm.org/docs/ObjectiveCLiterals.html?utm_source=chatgpt.com "Objective-C Literals — Clang 22.0.0git documentation"
[4]: https://www.gnustep.org/resources/documentation/Developer/Base/Reference/NSDictionary.html?utm_source=chatgpt.com "NSDictionary class reference"
[5]: https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170&utm_source=chatgpt.com "x64 Calling Convention"
[6]: https://en.wikipedia.org/wiki/X86_calling_conventions?utm_source=chatgpt.com "X86 calling conventions"
[7]: https://lists.gnu.org/archive/html/gnustep-dev/2013-07/msg00054.html?utm_source=chatgpt.com "Re: libobjc compile issue on mingw - GNU mailing lists"
[8]: https://reviews.llvm.org/D146058?utm_source=chatgpt.com "⚙ D146058 [lldb][gnustep] Add basic test and infrastructure ..."
