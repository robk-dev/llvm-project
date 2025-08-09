# Compiler Bug: Array Literals with GNUstep Runtime

## Issue Description

When using Objective-C array literal syntax `@[...]` with our locally built LLVM/Clang targeting the GNUstep runtime, the compiler fails to generate proper NSConstantString instances for string literals within the array. Instead, it stores the NSConstantString CLASS object pointer in array positions where string instances should be.

## Affected Version

- **Compiler**: Locally built LLVM/Clang from /home/robk/code/llvm-project/build/bin/clang
- **Target Runtime**: GNUstep 2.1 (-fobjc-runtime=gnustep-2.1)
- **Date Discovered**: 2025-08-09

## Symptoms

When creating arrays using literal syntax:
```objc
NSArray *languages = @[@"Objective-C", @"Swift", @"Python"];
```

In LLDB, instead of seeing string values, the array shows:
```
(NSArray *) languages = 0x0000000000418110 [
  "Objective-C",
  0x00007ffff7d9f6f8,  // Should be "Swift"
  0x00007ffff7d9f6f8   // Should be "Python"
]
```

The address `0x00007ffff7d9f6f8` is the NSConstantString CLASS object, not a string instance.

## Root Cause Analysis

### 1. Assembly Generation Issue

Examining the generated assembly (`custom_class_test.s`), only the first string literal is generated:

```asm
.L.objc_str_Objective-C:
	.asciz	"Objective-C"
```

The strings "Swift" and "Python" are completely missing from the assembly output. No `.objc_str_Swift` or `.objc_str_Python` symbols are generated.

### 2. Array Initialization

The array initialization code incorrectly references the NSConstantString class object instead of string instances:

```asm
# Array initialization for languages array
leaq	.L.objc_str_init.33(%rip), %rdi  # First string (works)
movq	%rdi, 128(%rsp)
leaq	__NSConstantString(%rip), %rdi    # Class object (wrong!)
movq	%rdi, 136(%rsp)
leaq	__NSConstantString(%rip), %rdi    # Class object (wrong!)
movq	%rdi, 144(%rsp)
```

### 3. Verification

Running the test program confirms the issue:
- First element: Tagged pointer `0x8da7973e8000002c` (correct, decodes to "Objective-C")
- Second element: `0x00007ffff7d9f6f8` (NSConstantString class object)
- Third element: `0x00007ffff7d9f6f8` (NSConstantString class object)

## Impact

1. **Debugging**: Arrays show incorrect values in LLDB
2. **Runtime Behavior**: Likely crashes or undefined behavior when accessing these "strings"
3. **Data Loss**: String literals are completely missing from the compiled binary

## Workaround

Use traditional array creation methods instead of literal syntax:
```objc
// Don't use:
NSArray *languages = @[@"Objective-C", @"Swift", @"Python"];

// Use instead:
NSArray *languages = [NSArray arrayWithObjects:
    @"Objective-C", @"Swift", @"Python", nil];
```

## Formatter Mitigation

The GNUstepArrayFormatter has been updated to detect this case and display `<NSConstantString class>` instead of the raw hex address, making the issue more apparent during debugging.

```cpp
// In GNUstepArrayFormatters.cpp
if (test_name.find("METACLASS") != std::string::npos || test_name.empty()) {
    std::string class_name = introspector.GetClassName(obj_addr);
    if (class_name == "NSConstantString" || class_name == "__NSConstantString") {
        return "<NSConstantString class>";
    }
    return "<class object>";
}
```

## Test Case

```objc
#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        // This triggers the bug
        NSArray *buggy = @[@"First", @"Second", @"Third"];
        
        // This works correctly
        NSArray *working = [NSArray arrayWithObjects:
            @"First", @"Second", @"Third", nil];
        
        // Breakpoint here and inspect both arrays
        NSLog(@"Buggy: %@", buggy);
        NSLog(@"Working: %@", working);
    }
    return 0;
}
```

## Next Steps

1. Create minimal reproducible test case
2. Check if this affects upstream LLVM or is specific to our build
3. File bug report with LLVM project
4. Investigate clang's Objective-C literal lowering for GNUstep runtime
5. Potential fix location: `clang/lib/CodeGen/CGObjC.cpp` or `CGObjCGNU.cpp`

## Related Files

- `/home/robk/code/llvm-project/lldb/examples/custom_class_test.m` - Exhibits the bug
- `/home/robk/code/llvm-project/lldb/examples/test_constant_string.m` - Minimal test case
- `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepArrayFormatters.cpp` - Formatter mitigation