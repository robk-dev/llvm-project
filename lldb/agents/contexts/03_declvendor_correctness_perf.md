# Agent Context 03 — DeclVendor Correctness & Performance (P1)

## Objective
Ensure robust name lookup/import for Objective‑C declarations with proper caching and safe AST lifecycle.

## Scope
- `GNUstepObjCDeclVendor.{h,cpp}`

## Observations
- DeclVendor commonly suffers from lifecycle issues around AST contexts and caching invalidation.
- Expression evaluation can interleave with module loads; ensure thread-safety.

## Requirements
- Support lookup by: class, category, selector; import necessary headers/modules as needed.
- Caches with explicit invalidation triggers on module load or target changes.
- Clear diagnostics when lookups fail; no silent nulls.

## Acceptance Criteria
- Unit tests exercising sequences of lookups with and without debug info.
- Concurrency test simulating repeated expression evaluations; no UAFs.

## Approach
1) Audit constructor/destructor ownership of `ASTContext`, `ClangImporter` and related.
2) Introduce small RAII helpers for AST session; guard with mutex or lldb’s thread-safety primitives.
3) Add a cache keyed by mangled/objc names with generation counter; invalidate on module updates.
4) Improve error reporting via `Status` in vendor methods.
5) Add gtests covering success/failure and perf (budgeted loops).

## Validation
- Unit tests pass consistently; run under `ASAN_OPTIONS=detect_leaks=1` if available.

## Agent prompt
You are an expert in LLDB ClangImporter and DeclVendor. Steps:
1) Map out DeclVendor data flow: how lookups are requested and how AST/Importer are created and owned.
2) Add a small RAII session object for the AST state, ensure proper locking and destruction order.
3) Add a name cache keyed by ObjC identifiers (class/category/selector) with a generation counter; invalidate on module load notifications.
4) Improve diagnostics: return `Status` with precise reasons for lookup misses.
5) Implement gtests that perform sequences of lookups with/without debug info, and a small concurrency loop.

Constraints:
- No global statics with non-trivial destructors; prefer members with clear ownership.
- Keep perf in check; measure with a simple loop in tests.

Done when:
- Tests pass, performance is acceptable, and logs show clear diagnostics in failure cases.
