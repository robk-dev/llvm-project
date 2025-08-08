# Task 01: String Formatter Activation

## Epic: [003_DATA_FORMATTERS](../../epics/003_DATA_FORMATTERS.md)
**Priority**: P0 (Critical)  
**Effort Estimate**: 3 days  
**Assignee**: TBD  
**Status**: Ready for Development

## Overview
Activate the existing string formatter infrastructure that is currently disabled in the GNUstepObjCRuntime initialization. The formatter code exists but is not being registered with LLDB's DataVisualization system.

## Background
During the recent development session, we implemented complete string formatter infrastructure:
- String extraction logic exists in `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.cpp`
- Formatter registration system is implemented in `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepFormattersRegistry.cpp`
- However, formatter registration is currently disabled/problematic in the Initialize() method

## Technical Requirements

### Current State Analysis
1. **Formatter Infrastructure**: ✅ Complete
   - `GNUstepNSStringFormatterFunction()` implemented
   - `ExtractStringContent()` logic exists
   - `RegisterStringFormatters()` method ready

2. **Registration System**: ⚠️ Needs Fixing
   - Code exists in `GNUstepObjCRuntime::Initialize()` (lines 36-49)
   - API mismatches need resolution
   - TypeCategory creation and enabling needs validation

3. **Integration**: ❌ Not Working
   - Formatters not appearing in LLDB sessions
   - `po` commands still show raw addresses

### Implementation Details

#### File: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`

**Current Implementation (lines 36-49):**
```cpp
// Register our formatters with LLDB (simplified approach)
printf("[DEBUG] GNUstepObjCRuntime: Registering formatters...\n");

// Try to get or create the GNUstep type category
TypeCategoryImplSP category_sp;
if (DataVisualization::Categories::GetCategory(ConstString("gnustep"), category_sp, true)) {
  if (category_sp) {
    printf("[DEBUG] GNUstepObjCRuntime: Got category, registering formatters\n");
    GNUstepFormattersRegistry::RegisterFormatters(*category_sp);
    // Enable the category using the proper API
    DataVisualization::Categories::Enable(ConstString("gnustep"), TypeCategoryMap::Default);
    printf("[DEBUG] GNUstepObjCRuntime: Formatters registered and enabled\n");
  }
}
```

**Issues to Address:**
1. Verify `GetCategory()` API usage and parameters
2. Confirm `Enable()` API parameters and functionality
3. Add error handling and diagnostics
4. Validate category creation if it doesn't exist

#### File: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepFormattersRegistry.cpp`

**Current Implementation:**
```cpp
void GNUstepFormattersRegistry::RegisterStringFormatters(TypeCategoryImpl &category) {
  // Register NSString summary provider
  TypeSummaryImpl::Flags string_flags;
  string_flags.SetCascades(true)
             .SetSkipPointers(false)
             .SetSkipReferences(false)
             .SetDontShowChildren(true)
             .SetDontShowValue(true)
             .SetShowMembersOneLiner(false)
             .SetHideItemNames(true);

  // Create the summary provider
  auto string_summary = std::make_shared<CXXFunctionSummaryFormat>(
      string_flags, GNUstepNSStringFormatterFunction, "NSString summary provider");

  // Register for various NSString type names
  category.AddTypeSummary("NSString", eFormatterMatchExact, string_summary);
  // ... multiple type registrations
}
```

**Validation Needed:**
1. Verify `CXXFunctionSummaryFormat` parameters
2. Confirm type name matching strategies
3. Test flag combinations for optimal display

## Acceptance Criteria

### Must Have
1. **Formatter Registration Success**
   - [ ] Initialize() method successfully registers string formatters
   - [ ] No errors or exceptions during registration
   - [ ] Debug output confirms successful registration
   - [ ] LLDB `type summary list` shows GNUstep formatters

2. **Basic String Formatting**
   - [ ] `po greeting2` shows string content instead of address
   - [ ] String content extracted correctly from GNUstep NSString objects
   - [ ] ASCII strings display properly in quotes: `@"Hello World"`
   - [ ] Empty strings handled correctly: `@""`

3. **Error Handling**
   - [ ] Invalid objects don't crash the formatter
   - [ ] Corrupted string objects show error message instead of garbage
   - [ ] Non-string objects passed to string formatter are handled gracefully

### Should Have
1. **Multiple String Types**
   - [ ] NSString formatting works
   - [ ] NSMutableString formatting works
   - [ ] NSConstantString formatting works
   - [ ] Pointer vs non-pointer types handled correctly

2. **Debug Information**
   - [ ] Clear debug output during formatter registration
   - [ ] Diagnostic messages for formatter failures
   - [ ] Ability to disable formatters for troubleshooting

### Could Have
1. **Advanced Features**
   - [ ] Unicode string support (UTF-8, UTF-16)
   - [ ] Large string truncation with ellipsis
   - [ ] String encoding detection and display

## Implementation Plan

### Step 1: API Validation (4 hours)
1. Research current LLDB DataVisualization APIs
2. Verify `GetCategory()` and `Enable()` method signatures
3. Check for API changes in target LLDB version
4. Update method calls if necessary

### Step 2: Registration Debugging (8 hours)
1. Add comprehensive debug logging to registration process
2. Implement step-by-step validation of each registration stage
3. Test with minimal formatter to isolate issues
4. Verify category creation and activation

### Step 3: Formatter Testing (8 hours)
1. Create test harness for formatter validation
2. Test with simple NSString objects from `/home/robk/code/llvm-project/lldb/examples/simple_test.m`
3. Validate `po` command integration
4. Test edge cases (nil, empty strings, corrupted objects)

### Step 4: Integration Validation (4 hours)
1. Full debugging session with formatted output
2. Performance impact assessment
3. Compatibility testing with existing LLDB features
4. Documentation of successful formatter activation

## Test Cases

### Test Case 1: Basic String Formatting
```objc
// From: /home/robk/code/llvm-project/lldb/examples/simple_test.m
NSString *greeting2 = [NSString stringWithUTF8String:"Hello, GNUstep World!"];

// Expected LLDB output:
(lldb) po greeting2
@"Hello, GNUstep World!"

// Instead of current:
(lldb) po greeting2
0x7ffff7abc123
```

### Test Case 2: Multiple String Types
```objc
NSString *regular = @"Regular string";
NSMutableString *mutable = [NSMutableString stringWithString:@"Mutable"];
NSString *empty = @"";

// All should show formatted content, not addresses
```

### Test Case 3: Error Handling
```objc
NSString *corrupted = (NSString *)0xdeadbeef;  // Invalid pointer
NSString *nil_string = nil;

// Should show error messages, not crash
```

## Definition of Done

- [ ] String formatters are successfully registered in LLDB
- [ ] `po` command shows formatted string content for GNUstep NSString objects
- [ ] No crashes or errors during formatter registration or execution
- [ ] Debug logging confirms successful formatter activation
- [ ] Integration test passes with real GNUstep application
- [ ] Code changes reviewed and approved
- [ ] Documentation updated with formatter activation confirmation

## Success Metrics

- **Activation Rate**: 100% successful formatter registration
- **Functionality**: String content visible in 95% of NSString objects
- **Performance**: <10ms additional overhead per string formatting operation
- **Stability**: 0 crashes during 100 consecutive string formatting operations

## Dependencies

- **Epic 001**: Basic runtime detection must be working
- **Test Environment**: Ability to build and test with GNUstep applications
- **LLDB Version**: Compatible LLDB version with required DataVisualization APIs

## Risks and Mitigation

### Risk: LLDB API Incompatibility
**Likelihood**: Medium  
**Impact**: High  
**Mitigation**: Research current APIs, implement fallback mechanisms, version detection

### Risk: GNUstep String Layout Changes
**Likelihood**: Low  
**Impact**: Medium  
**Mitigation**: Test with multiple GNUstep versions, implement version-specific logic

---

*Created*: December 2024  
*Last Updated*: December 2024  
*Epic*: 003_DATA_FORMATTERS  
*Dependencies*: Epic 001 completion
