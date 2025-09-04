# Agent Context: Test Suite Reactivation & Coverage (P0)

**Objective:** Reactivate and stabilize the unit and API test suites to provide a reliable validation gate for all code changes.

**Analysis & Key Issues:**
-   **Disabled Tests:** A significant number of tests are in the `unittests/Language/ObjC/GNUstep/disabled_tests` directory, indicating they are failing or flaky. This represents a major gap in test coverage.
-   **API Test Runner:** The API tests in `test/API/lang/objc/gnustep/` rely on a series of shell scripts and Python test files (`TestGNUstep*.py`) that may have broken dependencies or environment assumptions.
-   **Missing Coverage:** Critical new features like tagged pointer detection and class discovery caching have no corresponding tests.

**Requirements & Implementation Plan:**
1.  **Re-enable Unit Tests:**
    -   Move tests from the `disabled_tests` directory back into the active test suite one by one.
    -   Fix any build errors or assertion failures. If a test is genuinely flaky, it should be marked as such with a clear `FIXME` comment explaining the issue, but it should remain in the active suite if possible.
    -   **File to Modify:** `unittests/Language/ObjC/GNUstep/CMakeLists.txt` (to re-add the test files).
2.  **Stabilize API Tests:**
    -   Audit the `run_lldb_test.sh` and `Makefile` in `test/API/lang/objc/gnustep/` to ensure they correctly locate the locally-built `lldb` and `clang`.
    -   Fix any failing Python-based API tests (`TestGNUstepFormatters.py`, etc.).
3.  **Add New Test Coverage:**
    -   Create `unittests/Language/ObjC/GNUstep/Core/GNUstepTaggedPointerTest.cpp` to test tagged pointer detection logic.
    -   Create `unittests/Language/ObjC/GNUstep/Core/GNUstepClassDiscoveryTest.cpp` to test class caching and discovery.
    -   Add new API tests for the recently added formatters (`NSCharacterSet`, `NSIndexSet`, etc.) to validate their output against golden reference files.

**Acceptance Criteria:**
-   The `LanguageObjCGNUstepTests` target builds and runs with `ninja check-lldb-unit`, with all previously disabled tests now passing.
-   Running `lit -v --filter=gnustep` in the build directory passes without failures.
-   Code coverage metrics (if available) show that new features are tested.

