# Agent Context: Build & Dev Tooling Stabilization (P0)

**Objective:** Fix shell scripts and VS Code tasks so developers can reliably rebuild and run examples/tests with one click or one command.

**Analysis & Key Issues:**
-   **`dev.sh` is broken:** The script suffers from multiple critical shell script errors, including mismatched `if`/`fi` blocks and empty `if` conditions. This makes it unreliable for any development workflow.
    -   *File:* `lldb/dev.sh`, Lines 130-135 (broken `if/fi`), 155-158 (empty `if`).
-   **Inconsistent Environment:** `LD_LIBRARY_PATH` and `PATH` are set inconsistently across `examples/Makefile`, `test/API/lang/objc/gnustep/Makefile`, and `.vscode/launch.json`, leading to build and runtime failures.
-   **Task Dependencies:** VS Code tasks for building examples depend on a pre-existing `build` directory but don't enforce it, causing confusing failures for new contributors.

**Requirements & Implementation Plan:**
1.  **Fix `dev.sh`:**
    -   Correct all `shellcheck` errors, focusing on control flow (`if`/`fi`, `if/else`).
    -   Implement robust error handling using `set -euo pipefail`.
    -   Add timing for build commands to track performance.
    -   **Code Suggestion (`dev.sh`):**
        ```bash
        # Correct timing and error handling
        local start_time=$(date +%s)
        if ninja $BUILD_TARGETS -j$(nproc); then
            local end_time=$(date +%s)
            local duration=$((end_time - start_time))
            log_success "Quick build completed in ${duration}s"
        else
            log_error "Quick build failed."
            exit 1
        fi
        ```
2.  **Centralize Environment Setup:**
    -   Create a helper function or a sourced script (e.g., in `scripts2/helpers/common.sh`) to export a consistent `LD_LIBRARY_PATH` and `PATH`.
    -   Update all `Makefile`s and `launch.json` to use this single source of truth.
3.  **Improve VS Code Tasks:**
    -   Ensure the `build-all-examples` task in `tasks.json` depends on the `build-full-development` task to guarantee the build directory and binaries exist.
    -   Add comments to `launch.json` clarifying the purpose of each configuration and its dependencies.

**Acceptance Criteria:**
-   `shellcheck lldb/dev.sh` passes without critical errors.
-   `./dev.sh quick_build` and `./dev.sh clean_build` complete successfully and report timing.
-   The "build-all-examples" VS Code task runs successfully from a clean state.
-   The "🚀 GNUstep/libobjc2 - Debug" launch configuration starts without "file not found" or "library not loaded" errors.

