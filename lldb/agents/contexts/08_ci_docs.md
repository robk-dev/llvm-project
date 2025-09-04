# Agent Context: CI and Documentation (P2)

**Objective:** Establish a local continuous integration (CI) script and document the process for contributing new features, particularly data formatters.

**Analysis & Key Issues:**
-   **No Automated Local Run:** There is no single script to run all the necessary checks (build, unit tests, API tests), which makes it difficult to validate changes locally before committing.
-   **Lack of Contributor Docs:** The process for adding a new data formatter is not documented, which creates a barrier for new contributors. Key information, such as where to add files and which tests are required, is missing.
    -   *File:* `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/README.md` (currently minimal).

**Requirements & Implementation Plan:**
1.  **Create a Local CI Script:**
    -   Develop a new script, `scripts2/local_ci.sh`, that automates the following steps:
        1.  Performs a clean build of the plugin and LLDB (`./dev.sh clean_build`).
        2.  Runs all unit tests (`ninja check-lldb-unit`).
        3.  Runs the GNUstep-specific API tests (`lit -v --filter=gnustep`).
        4.  Collects all logs into a timestamped directory under `agents/reports/`.
        5.  Generates a simple `summary.md` file indicating the pass/fail status of each stage.
2.  **Write Contributor Documentation:**
    -   Significantly expand the `README.md` in the `GNUstepObjCRuntime` plugin directory.
    -   Add a "Contributing a New Formatter" section that includes:
        -   A checklist for creating the formatter (e.g., `GNUstepMyClassFormatter.cpp/.h`).
        -   Instructions on how to register the new formatter in `GNUstepFormattersRegistry.cpp`.
        -   A requirement to add a new unit test for the formatter's summary string.
        -   A requirement to update an API test to validate the formatter's output in a live debug session.
    -   **Code Suggestion (`README.md`):**
        ```markdown
        ### Contributing a New Formatter
        1.  **Create Formatter Files:** Create `formatters/GNUstepMyClassFormatter.cpp` and `.h`.
        2.  **Implement Logic:** Write the summary provider function.
        3.  **Register Formatter:** Add your formatter to the registry in `GNUstepFormattersRegistry.cpp`.
        4.  **Add Unit Test:** Add a test case to `unittests/.../Formatters/Foundation/` to check the summary string.
        5.  **Add API Test:** Update `test/API/.../test_new_formatters.m` to include your class.
        ```

**Acceptance Criteria:**
-   `./scripts2/local_ci.sh` runs successfully and produces a report directory with logs and a summary.
-   The `README.md` in the plugin directory contains clear, actionable instructions for adding a new formatter.

