# Agent Context: Runtime Core Bridge Hardening (P0)

**Objective:** Harden the GNUstep/libobjc2 runtime integration by improving dynamic class discovery, ISA/descriptor mapping, tagged pointer detection, and ensuring safe memory introspection.

**Analysis & Key Issues:**
-   **Class Discovery:** The current implementation relies on `objc_copyClassList`, but lacks robust error handling for cases where it returns 0 or a very large number of classes. There is no caching mechanism, leading to repeated, expensive queries.
    -   *File:* `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`
-   **Tagged Pointers:** Tagged pointer detection is a critical performance optimization but is not yet implemented, forcing all pointers through a slower, more general inspection path.
-   **Memory Safety:** Introspection logic may not be fully read-only and could potentially alter the state of the debugged process (e.g., by calling methods that affect reference counts).

**Requirements & Implementation Plan:**
1.  **Implement Robust Class Discovery:**
    -   Wrap `objc_copyClassList` calls in a RAII-style unique pointer to prevent memory leaks.
    -   Add a simple cache (e.g., `std::vector<ClassInfo>`) to the `GNUstepObjCRuntime` instance, invalidated on module load/unload events.
    -   **Code Suggestion (`GNUstepObjCRuntime.cpp`):**
        ```cpp
        // In GetClassList or a similar method
        if (!m_class_list_cache.empty()) {
            return m_class_list_cache;
        }
        
        unsigned int count = 0;
        std::unique_ptr<Class[], decltype(&free)> classes(
            m_v2_api.objc_copyClassList(&count), &free);

        if (!classes || count == 0) {
            return {}; // Handle error gracefully
        }
        // Populate cache...
        ```
2.  **Add Tagged Pointer Detection:**
    -   Implement a function `IsTaggedPointer(lldb::addr_t addr)` that checks for the tagged pointer bit, as defined by the libobjc2 ABI.
    -   Integrate this check early in the object inspection path to quickly identify and handle tagged pointers (e.g., for strings and numbers).
    -   Add new unit tests in `unittests/Language/ObjC/GNUstep/Core/GNUstepTaggedPointerTest.cpp`.
3.  **Ensure Safe Introspection:**
    -   Audit `GNUstepObjCRuntimeIntrospector.cpp` to ensure it uses `process->ReadMemory()` instead of invoking methods on the target that could have side effects.
    -   All introspection should be strictly read-only.

**Acceptance Criteria:**
-   New unit tests for class discovery (with 0, 10, and 1000 classes) and tagged pointer detection pass.
-   The `StressTest.cpp` in `unittests/Language/ObjC/GNUstep/Formatters/Performance/` runs without memory leaks or crashes.
-   No regressions in existing formatter or API tests.

