# Task 09: Prepare Upstream Submission

## Problem Statement
Prepare the GNUstep LLDB integration for submission to the LLVM project. This requires comprehensive documentation, proper git history, adherence to LLVM contribution guidelines, and creation of a compelling case for upstream acceptance.

## Submission Strategy

### LLVM Review Process Understanding
1. **Phabricator Review**: Use LLVM's review system (reviews.llvm.org)
2. **Incremental Reviews**: Break large changes into reviewable chunks
3. **Community Engagement**: Engage with LLDB maintainers early
4. **Test Coverage**: Demonstrate comprehensive testing
5. **Documentation**: Provide clear rationale and usage instructions

### Upstream Value Proposition
- **Cross-platform Objective-C debugging**: Enables ObjC debugging on Linux/Windows
- **GNUstep ecosystem support**: Supports growing GNUstep developer community  
- **Standards compliance**: Follows existing LLDB plugin patterns
- **Minimal risk**: Self-contained plugin with no impact on existing functionality

## Implementation Plan

### Step 1: Git History Cleanup

#### Create Clean Feature Branch
```bash
# Create clean branch for upstream submission
git checkout main
git pull upstream main
git checkout -b feature/gnustep-objc-runtime-mvp

# Cherry-pick clean commits from development branch
git cherry-pick <commit-hash-01>  # Language handling fix
git cherry-pick <commit-hash-02>  # Literals/subscripting support
git cherry-pick <commit-hash-03>  # Windows calling convention fix
git cherry-pick <commit-hash-04>  # CFString implementation
git cherry-pick <commit-hash-05>  # Symbol resolution enhancement
git cherry-pick <commit-hash-06>  # DeclVendor completion
git cherry-pick <commit-hash-07>  # Test suite addition
git cherry-pick <commit-hash-08>  # Performance optimization

# Squash related commits if needed
git rebase -i HEAD~8
```

#### Commit Message Standards
```
[lldb][gnustep] Add minimal Objective-C expression evaluation support

This patch implements basic Objective-C expression evaluation support
for GNUstep/libobjc2 runtime on Windows and Linux platforms.

Key features:
- NSNumber creation and value extraction: [NSNumber numberWithInt:7]
- String literals: @"hello"  
- Array subscripting: arr[0] (when supported by GNUstep Base)
- Proper Windows x64 calling conventions for objc_msgSend

The implementation follows the existing LLDB language runtime plugin
pattern and is isolated to avoid impact on Apple Objective-C runtime
support.

Tested on:
- Windows 10 x64 with GNUstep/libobjc2 via MSYS2
- Ubuntu 20.04 with GNUstep development environment

Fixes: rdar://problem/expression-evaluation-gnustep

Differential Revision: https://reviews.llvm.org/D[number]
```

### Step 2: Documentation Creation

#### Plugin Documentation
```markdown
# File: lldb/docs/use/tutorial/objc-gnustep.md
# Debugging Objective-C with GNUstep

This document describes LLDB's support for debugging Objective-C applications
built with the GNUstep runtime (libobjc2) on Windows and Linux platforms.

## Prerequisites

### Windows (MSYS2)
```bash
pacman -S mingw-w64-ucrt-x86_64-gnustep-base
pacman -S mingw-w64-ucrt-x86_64-gnustep-make
```

### Linux (Ubuntu/Debian)
```bash
apt-get install gnustep-devel gnustep-base-common
```

## Basic Usage

### Starting a Debug Session
```bash
lldb my_gnustep_program.exe
(lldb) settings set target.language objc++
(lldb) breakpoint set -f main.m -l 10
(lldb) process launch
```

### Expression Evaluation
```bash
# NSNumber operations
(lldb) expr -l objc++ -- (id)[NSNumber numberWithInt:42]
(lldb) expr -l objc++ -- (int)[(id)[NSNumber numberWithInt:42] intValue]

# String literals
(lldb) expr -l objc++ -- @"Hello GNUstep"
(lldb) expr -l objc++ -- [@"test" length]

# Array operations (GNUstep Base 1.24+)
(lldb) expr -l objc++ -- id arr = [NSArray arrayWithObjects:@"a", @"b", nil]
(lldb) expr -l objc++ -- (id)[arr objectAtIndex:0]
(lldb) expr -l objc++ -- (id)arr[0]  // If subscripting supported
```

## Limitations

- Limited to basic Foundation classes (NSString, NSNumber, NSArray, NSDictionary)
- Modern ObjC syntax support depends on GNUstep Base version
- ARC expressions may not work correctly
- Custom class introspection is limited

## Troubleshooting

### Expression Evaluation Fails
1. Verify GNUstep runtime libraries are loaded
2. Check that target language is set to objc++
3. Enable expression logging: `log enable lldb expr`

### Symbol Resolution Issues
```bash
(lldb) image lookup -s objc_msgSend
(lldb) image lookup -s objc_getClass
```

If symbols are not found, ensure libobjc is properly linked.
```

#### API Documentation  
```cpp
// File: lldb/include/lldb/API/SBGNUstepRuntime.h (if public API needed)
/**
 * @file SBGNUstepRuntime.h
 * @brief GNUstep Objective-C runtime support for LLDB
 * 
 * This header provides minimal public API for GNUstep runtime introspection.
 * Most functionality is internal to LLDB's expression evaluation system.
 */

namespace lldb {

/**
 * @brief Check if target supports GNUstep Objective-C runtime
 * @param target The target to check
 * @return true if GNUstep runtime symbols are detected
 */
LLDB_API bool SBTargetSupportsGNUstepObjC(SBTarget target);

/**
 * @brief Get GNUstep runtime version information if available
 * @param target The target to query
 * @return Version string or empty if not available
 */
LLDB_API const char* SBTargetGetGNUstepVersion(SBTarget target);

} // namespace lldb
```

### Step 3: Test Documentation and Examples

#### Test Program for Documentation
```objc
// File: lldb/test/API/lang/objc/gnustep/TestPrograms/documentation_example.m
/**
 * Simple GNUstep Objective-C program for LLDB documentation examples
 * 
 * Compile with:
 * clang -fobjc-runtime=gnustep-2.0 -fblocks -fconstant-string-class=NSConstantString \
 *       -I/usr/include/GNUstep -L/usr/lib/GNUstep -lgnustep-base -lobjc \
 *       -o documentation_example documentation_example.m
 */

#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        NSLog(@"GNUstep LLDB Documentation Example");
        
        // Number operations
        NSNumber *answer = [NSNumber numberWithInt:42];
        int value = [answer intValue];
        NSLog(@"The answer is %d", value);
        
        // String operations
        NSString *greeting = @"Hello GNUstep";
        NSUInteger length = [greeting length];
        NSLog(@"Greeting '%@' has %lu characters", greeting, length);
        
        // Array operations
        NSArray *colors = [NSArray arrayWithObjects:@"red", @"green", @"blue", nil];
        NSString *firstColor = [colors objectAtIndex:0];
        NSLog(@"First color: %@", firstColor);
        
        // Modern subscripting (if supported)
        if ([colors respondsToSelector:@selector(objectAtIndexedSubscript:)]) {
            NSString *secondColor = colors[1];
            NSLog(@"Second color (subscripting): %@", secondColor);
        }
        
        return 0; // <- Set breakpoint here for testing
    }
}
```

### Step 4: Review Preparation

#### Self-Review Checklist
```markdown
# GNUstep LLDB Integration - Pre-Submission Checklist

## Code Quality
- [ ] All code follows LLVM coding standards
- [ ] No compiler warnings with -Wall -Wextra -Werror
- [ ] Static analysis (clang-static-analyzer) passes clean
- [ ] Thread safety verified
- [ ] Memory leaks checked with valgrind/ASan

## Functionality
- [ ] Core MVP expressions work: NSNumber, @"", array subscripting
- [ ] Windows x64 calling conventions correct
- [ ] Symbol resolution handles COFF imports
- [ ] CFString literals work without crashes
- [ ] No regressions in existing Apple ObjC runtime

## Testing
- [ ] Unit tests pass on all supported platforms
- [ ] Integration tests validate MVP functionality  
- [ ] Performance tests show acceptable overhead
- [ ] Regression tests prevent known issues
- [ ] Test coverage > 80% for new code

## Documentation
- [ ] User-facing documentation complete
- [ ] API documentation for public interfaces
- [ ] Example programs and test cases
- [ ] Troubleshooting guide
- [ ] Installation instructions

## Platform Support
- [ ] Windows 10 x64 with MSYS2/GNUstep
- [ ] Ubuntu 20.04+ with GNUstep
- [ ] No impact on unsupported platforms
- [ ] Graceful degradation when GNUstep unavailable

## Review Readiness
- [ ] Git history is clean and logical
- [ ] Commit messages follow LLVM standards
- [ ] Patch size is reasonable for review
- [ ] Related work referenced appropriately
- [ ] Breaking changes documented (none expected)
```

### Step 5: Phabricator Submission

#### Create Review Request
```bash
# Upload to Phabricator
arc diff --create

# Or use git if arc not available
git format-patch HEAD~8 --stdout | \
  curl -X POST \
       -H "Content-Type: text/plain" \
       -d @- \
       "https://reviews.llvm.org/differential/upload/"
```

#### Review Description Template
```markdown
# Summary

This patch adds minimal Objective-C expression evaluation support for GNUstep/libobjc2 runtime on Windows and Linux platforms.

## Motivation

GNUstep provides a free, cross-platform implementation of the OpenStep/Cocoa APIs, enabling Objective-C development on non-Apple platforms. However, LLDB currently only supports Apple's Objective-C runtime, leaving GNUstep developers without expression evaluation capabilities during debugging.

This patch addresses the most critical debugging scenarios:
- Creating and inspecting NSNumber objects
- Working with string literals (@"")  
- Basic array/dictionary operations
- Modern subscripting syntax (when supported by GNUstep Base)

## Implementation Approach

The implementation follows LLDB's existing language runtime plugin pattern:

1. **GNUstepObjCRuntime**: Main plugin class extending ObjCLanguageRuntime
2. **GNUstepObjCDeclVendor**: AST declaration provider for Foundation classes
3. **Symbol Resolution**: Cross-platform symbol lookup with Windows COFF support
4. **Calling Conventions**: Correct Win64 ABI for objc_msgSend calls
5. **CFString Fallback**: Custom implementation for @"" literals

## Testing

- Unit tests for core functionality
- Integration tests on Windows (MSYS2) and Linux  
- Performance benchmarks to ensure no regression
- Cross-platform symbol resolution validation

## Future Work (Out of Scope)

- Full Foundation API surface
- ARC expression support
- Advanced debugging features (stepping, trampolines)
- Custom class introspection

## Related Work

- D158205: Previous GNUstep support infrastructure
- D146058: Basic GNUstep test framework

Test Plan:
- Build LLDB with patch applied
- Run test suite: `ninja check-lldb-api-lang-objc-gnustep`
- Manual testing with provided example program

Reviewers: jingham, labath, clayborg, DavidSpickett
```

### Step 6: Community Engagement

#### Mailing List Discussion
```markdown
Subject: [lldb-dev] GNUstep Objective-C Expression Evaluation Support

Hi LLDB developers,

I've been working on adding basic Objective-C expression evaluation support 
for the GNUstep runtime (libobjc2) on Windows and Linux platforms. 

The motivation is to enable debugging for the growing GNUstep developer 
community who currently lack expression evaluation capabilities in LLDB.

Before submitting for review, I wanted to gauge community interest and 
get early feedback on the approach:

1. Is there interest in supporting non-Apple Objective-C runtimes?
2. Does the plugin-based approach seem reasonable?
3. Are there specific concerns about Windows/cross-platform support?
4. What would be the preferred scope for an initial contribution?

The current implementation enables these core scenarios:
- (id)[NSNumber numberWithInt:42]
- @"string literals"  
- basic array/dictionary operations
- modern subscripting when supported

Implementation follows existing patterns and is isolated to avoid impact
on Apple runtime support.

I plan to submit a patch in the next few days if there's interest.

Best regards,
[Your Name]
```

#### Response to Review Feedback Template
```markdown
Thank you for the detailed review! I've addressed the feedback:

> Concern about Windows calling convention handling

Fixed in latest revision. Now using target-specific calling convention 
detection with explicit CC_X86_64_Win64 for Windows x64. Added test 
coverage to verify no access violations.

> Symbol resolution seems complex

Simplified the approach based on your suggestion. Now using a cached
lookup table with platform-specific symbol name variants. Performance
benchmarks show < 1ms for cached lookups.

> Test coverage could be better

Added comprehensive unit tests covering:
- Runtime detection logic
- Symbol resolution with various naming schemes  
- AST generation for runtime functions
- Cross-platform compatibility

Coverage is now >85% for new code.

> Documentation needs improvement

Added user-facing documentation with:
- Installation instructions for Windows/Linux
- Usage examples for common scenarios
- Troubleshooting guide for common issues
- API documentation for public interfaces

I believe this addresses the major concerns. Please let me know if you'd
like me to iterate on any specific areas.
```

## Files to Create/Modify

### Documentation Files
- `lldb/docs/use/tutorial/objc-gnustep.md`
- `lldb/test/API/lang/objc/gnustep/README.md`
- `lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/README.md`

### Example Programs
- `lldb/test/API/lang/objc/gnustep/TestPrograms/documentation_example.m`
- `lldb/test/API/lang/objc/gnustep/TestPrograms/Makefile`

### Review Materials
- `SUBMISSION_CHECKLIST.md`
- `REVIEW_TEMPLATE.md`
- `PHABRICATOR_DESCRIPTION.md`

## Success Criteria
- [ ] Clean git history ready for upstream
- [ ] Comprehensive documentation complete
- [ ] Example programs demonstrate functionality
- [ ] Review request created in Phabricator
- [ ] Community engagement initiated
- [ ] Pre-submission checklist completed
- [ ] Positive initial feedback from reviewers
- [ ] Patch ready for iterative review process

## Timeline
- **Week 1**: Git cleanup and documentation
- **Week 2**: Example programs and test validation
- **Week 3**: Community engagement and review creation
- **Week 4+**: Iterative review process

## Risk Mitigation
- **Review fatigue**: Break large changes into smaller, focused reviews
- **Platform concerns**: Emphasize isolated nature and no impact on existing code
- **Maintenance burden**: Highlight self-contained nature and comprehensive tests
- **Use case validity**: Provide concrete examples and user testimonials

## Implementation Status
- [ ] Git history cleaned and prepared
- [ ] Documentation written and reviewed
- [ ] Example programs created and tested
- [ ] Community engagement completed
- [ ] Phabricator review created
- [ ] Initial feedback incorporated
- [ ] Ready for final submission

## Dependencies
- Requires all tasks 01-08 to be completed and validated
- May require iterations based on community feedback
