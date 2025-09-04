# Agent Context: Foundation Formatters Coverage (P1)

**Objective:** Broaden and stabilize Foundation object summaries for `NSCharacterSet`, `NSIndexSet`, `NSDecimalNumber`, and other key classes to provide rich, useful information in the debugger.

**Analysis & Key Issues:**
-   **`NSCharacterSet`:** The current formatter is likely too verbose, printing the entire internal bitmap. A concise summary (e.g., "[a-z]", "decimal digits") is needed.
    -   *File:* `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepCharacterSetFormatters.cpp`
-   **`NSIndexSet`:** Does not efficiently display large, contiguous ranges. It should summarize them (e.g., "100 indexes in 2 ranges") instead of listing every index.
    -   *File:* `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepIndexSetFormatters.cpp`
-   **`NSDecimalNumber`:** Formatting may be locale-dependent, leading to inconsistent output. It also needs to handle special values like NaN gracefully.
    -   *File:* `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDecimalNumberFormatters.cpp`
-   **Missing Formatters:** There is no formatter for `NSAttributedString`, a commonly used text class.

**Requirements & Implementation Plan:**
1.  **Improve `NSCharacterSet` Formatter:**
    -   Check for well-known character sets (e.g., `letterCharacterSet`, `decimalDigitCharacterSet`) by comparing against singleton instances and provide a descriptive summary.
    -   For custom sets, provide a compact representation of the contents.
2.  **Enhance `NSIndexSet` Formatter:**
    -   Implement logic to detect and display contiguous ranges compactly (e.g., `[0-100, 200-250]`).
    -   For very large or fragmented sets, provide a summary with the total count and a few sample ranges.
3.  **Harden `NSDecimalNumber` Formatter:**
    -   Ensure all numeric formatting is done in a locale-independent way.
    -   Add explicit checks for `notANumber` and other special NSDecimalNumber values and display them clearly.
4.  **Add `NSAttributedString` Formatter:**
    -   Create `GNUstepAttributedStringFormatter.cpp` and `.h`.
    -   The summary should display the string content, similar to `NSString`, but perhaps with an indicator of its attributed nature (e.g., `@"Hello World" (attributed)`).
5.  **Add Comprehensive Tests:**
    -   For each formatter, add new unit tests in `unittests/Language/ObjC/GNUstep/Formatters/Foundation/` that cover edge cases (nil, empty, large values) and validate the summary strings.

**Acceptance Criteria:**
-   The `frame variable` output for `NSCharacterSet`, `NSIndexSet`, and `NSDecimalNumber` is concise, stable, and informative.
-   A new, working formatter for `NSAttributedString` is available.
-   All new and existing formatter unit tests pass.

