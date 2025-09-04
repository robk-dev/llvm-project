# Agent Context: Fix and Enhance Examples (P1)

**Objective:** Fix the incomplete and broken Objective-C examples to provide a reliable way to test, demonstrate, and debug the GNUstep plugin.

**Analysis & Key Issues:**
-   **`custom_class_test.m` is Incomplete:** The primary test file for formatters is syntactically incorrect and cannot be compiled.
    -   *File:* `examples/custom_class_test.m`
    -   **Line 68:** The `description` method is missing its format string and arguments.
    -   **Line 120:** The `NSException` object is not fully initialized.
    -   **Line 160:** The `NSAttributedString` is not fully initialized.
    -   **Line 163:** The `NSIndexPath` is not fully initialized.
    -   The `main` function is not properly closed.
-   **Makefile Brittleness:** The `examples/Makefile` may have hardcoded paths or assume a specific environment, making it difficult for new contributors to build the examples.

**Requirements & Implementation Plan:**
1.  **Complete `custom_class_test.m`:**
    -   Fix all syntax errors and incomplete object initializations.
    -   Implement the `BankAccount`'s `description` method to provide a useful summary.
    -   Ensure the program compiles without warnings using the local Clang build.
    -   **Code Suggestion (`custom_class_test.m`):**
        ```objectivec
        // In BankAccount implementation
        - (NSString *)description {
            return [NSString stringWithFormat:@"<BankAccount number: %@, owner: %@, balance: %.2f>",
                                             self.accountNumber, self.ownerName, self.balance];
        }

        // In main function
        NSException *testException = [NSException exceptionWithName:@"TestException" 
                                                             reason:@"A test reason" 
                                                           userInfo:nil];
        
        NSAttributedString *attrString = [[NSAttributedString alloc] initWithString:@"Attributed String"
                                                                         attributes:@{NSForegroundColorAttributeName:[UIColor redColor]}];
        ```
2.  **Harden the `Makefile`:**
    -   Ensure the `Makefile` in `examples/` uses relative paths and environment variables (`$(CC)`, `$(LD_LIBRARY_PATH)`) passed in from the build system or developer scripts.
    -   Remove any hardcoded paths.

**Acceptance Criteria:**
-   `make all -C examples` completes successfully without errors.
-   The `custom_class_test` binary runs and exits cleanly.
-   Launching the "🚀 GNUstep/libobjc2 - Debug" configuration in VS Code successfully starts the debugger and hits the breakpoint in `main`.

