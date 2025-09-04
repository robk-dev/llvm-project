# Agent Context: Windows/WSL Enablement (P2)

**Objective:** Compile and run the GNUstep runtime plugin on Windows (using MSYS2/MinGW) and ensure its functionality is on par with the Linux/WSL environment.

**Analysis & Key Issues:**
-   **Platform-Specific APIs:** The codebase currently uses POSIX-specific APIs like `dlopen` for dynamic library interaction, which are not available on Windows.
    -   *File:* `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepRuntimeV2API.cpp`
-   **CMake Configuration:** The `CMakeLists.txt` for the plugin does not have the necessary logic to detect a Windows build environment and link against the required Windows libraries (e.g., `kernel32.dll` for `LoadLibrary` and `GetProcAddress`).
    -   *File:* `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/CMakeLists.txt`
-   **Path Handling:** Windows uses backslashes (`\`) for path separators, which can cause issues if paths are not correctly normalized.

**Requirements & Implementation Plan:**
1.  **Update CMake for Windows:**
    -   Add a conditional block in `CMakeLists.txt` to check for `WIN32`.
    -   If on Windows, link against the necessary system libraries.
    -   Define a preprocessor macro (e.g., `_WIN32`) to be used in the C++ code.
2.  **Abstract Dynamic Loading:**
    -   Create a small C++ shim or use preprocessor directives (`#ifdef _WIN32`) to replace `dlopen` with `LoadLibrary` and `dlsym` with `GetProcAddress`.
    -   **Code Suggestion (`GNUstepRuntimeV2API.cpp`):**
        ```cpp
        #ifdef _WIN32
        #include <windows.h>
        #define LOAD_LIBRARY(path) LoadLibrary(path)
        #define GET_SYMBOL(handle, name) GetProcAddress(handle, name)
        #else
        #include <dlfcn.h>
        #define LOAD_LIBRARY(path) dlopen(path, RTLD_LAZY)
        #define GET_SYMBOL(handle, name) dlsym(handle, name)
        #endif
        ```
3.  **Normalize Paths:**
    -   Use LLVM's `llvm::sys::path::convert_to_slash` or a similar utility to ensure all paths are handled consistently across platforms.
4.  **Document Windows Setup:**
    -   Add a section to the main `README.md` detailing the required MSYS2/MinGW packages and any specific steps needed to build the plugin on Windows.

**Acceptance Criteria:**
-   The plugin compiles successfully on a Windows machine using MSYS2/MinGW and Clang.
-   A simple Objective-C program can be launched and inspected with the plugin loaded, without crashes.
-   The documentation is clear enough for a new contributor to set up a Windows build environment.

