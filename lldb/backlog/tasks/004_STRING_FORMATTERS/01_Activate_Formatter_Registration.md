# Task 01: Activate Formatter Registration

## Overview
Enable string formatter registration in the GNUstepObjCRuntime initialization process by removing the TODO comment and fixing LLDB DataVisualization API compatibility issues.

## Priority: P0 (Critical)
**Effort**: 2 days  
**Epic**: [004_STRING_FORMATTERS](../../epics/004_STRING_FORMATTERS.md)  
**Assignee**: TBD  
**Status**: Ready for Development

## Business Value
- **Immediate Impact**: Unblocks all formatter development
- **Developer Visibility**: First visible improvement in debugging experience
- **Technical Foundation**: Enables subsequent formatter features

## Technical Scope

### Current State Analysis
The string formatter infrastructure is completely implemented but disabled by a TODO comment in the Initialize() method:

**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`
**Lines 37-38**: 
```cpp
// TODO: Register our formatters with LLDB once we get the basic runtime working  
printf("[DEBUG] GNUstepObjCRuntime: Formatter registration disabled for now\n");
```

### Required Changes

1. **Replace TODO Section** (Primary Implementation)
   - Remove the TODO comment and debug print
   - Add proper formatter registration call
   - Handle LLDB API compatibility issues

2. **Fix DataVisualization API Issues** (Compatibility Layer)
   - Address `GetCategory()` function signature mismatch
   - Fix `Enable()` method parameter requirements  
   - Add proper error handling for registration failures

3. **Add Missing Includes** (Build Dependencies)
   - Ensure all required LLDB headers are included
   - Add missing TypeCategory and DataVisualization includes
   - Verify forward declarations are complete

### Implementation Details

#### Step 1: API Compatibility Research
**Files to Analyze**:
- `/home/robk/code/llvm-project/lldb/include/lldb/DataFormatters/DataVisualization.h`
- `/home/robk/code/llvm-project/lldb/include/lldb/DataFormatters/TypeCategory.h`

**Research Questions**:
- What is the correct signature for `GetCategory()`?
- How should categories be enabled in current LLDB versions?
- Are there API changes between LLDB versions we need to handle?

#### Step 2: Implementation Strategy
```cpp
// Proposed implementation approach:
void GNUstepObjCRuntime::Initialize() {
  PluginManager::RegisterPlugin(
      "gnu-objc-v2", "GNUstep Objective-C V2 Runtime",
      CreateInstance, nullptr);
  
  printf("[DEBUG] GNUstepObjCRuntime::Initialize() called\n");
  
  // Register our formatters with LLDB
  printf("[DEBUG] GNUstepObjCRuntime: Registering formatters...\n");
  
  TypeCategoryImplSP category_sp;
  if (DataVisualization::Categories::GetCategory(ConstString("gnustep"), category_sp, true)) {
    if (category_sp) {
      printf("[DEBUG] GNUstepObjCRuntime: Registering string formatters\n");
      GNUstepFormattersRegistry::RegisterFormatters(*category_sp);
      DataVisualization::Categories::Enable(ConstString("gnustep"), TypeCategoryMap::Default);
      printf("[DEBUG] GNUstepObjCRuntime: Formatters enabled\n");
    }
  } else {
    printf("[DEBUG] GNUstepObjCRuntime: Failed to get/create category\n");
  }
}
```

#### Step 3: Error Handling
- Graceful degradation if formatter registration fails
- Clear debug messages for troubleshooting
- Ensure runtime continues working even if formatters fail to load

## Acceptance Criteria

### Must Have
1. **Formatter Registration Active**
   - TODO comment removed from Initialize() method
   - GNUstepFormattersRegistry::RegisterFormatters() called successfully
   - String formatters visible in LLDB type summary list

2. **Build Success**
   - No compilation errors related to DataVisualization APIs
   - No linking errors for formatter registration code
   - All required headers properly included

3. **Runtime Stability**
   - LLDB runtime loads without errors
   - Formatter registration doesn't cause crashes
   - Basic debugging functionality remains intact

4. **Debug Visibility**
   - Debug messages confirm formatter registration attempt
   - Clear indication of success/failure in debug output
   - Formatter registration status visible in LLDB logs

### Should Have
1. **Error Handling**
   - Graceful handling of API compatibility issues
   - Clear error messages for registration failures
   - Fallback behavior if formatters can't be registered

2. **API Compatibility**
   - Works with current LLDB version in development environment
   - Handles potential API variations gracefully
   - Future-proof implementation for LLDB API changes

### Testing Requirements
1. **Build Verification**
   - Clean compilation with no warnings
   - Successful linking of all formatter components
   - No regressions in existing build process

2. **Runtime Testing**
   - LLDB loads GNUstep runtime plugin successfully
   - Debug messages appear in LLDB console
   - No crashes during initialization

3. **Formatter Availability**
   - String formatters appear in `type summary list`
   - NSString type has associated summary provider
   - Basic string formatting attempt (even if content extraction fails)

## Implementation Tasks

### Day 1: Research and API Compatibility
**Morning (4 hours)**:
- Analyze current LLDB DataVisualization API
- Research `GetCategory()` and `Enable()` correct signatures
- Test minimal formatter registration example
- Document API compatibility requirements

**Afternoon (4 hours)**:
- Design error handling strategy
- Create compatibility layer if needed
- Plan implementation approach
- Set up testing methodology

### Day 2: Implementation and Testing
**Morning (4 hours)**:
- Implement formatter registration code
- Add proper includes and dependencies
- Handle API compatibility issues
- Add debug logging and error handling

**Afternoon (4 hours)**:
- Build and test implementation
- Verify formatter registration success
- Debug any API issues
- Validate no regressions in runtime functionality

## Risk Assessment

### High Risk
- **LLDB API Incompatibility**: Current APIs may differ from implementation expectations
- **Build Failures**: Missing includes or API changes could break compilation

### Medium Risk
- **Runtime Crashes**: Incorrect API usage could cause LLDB crashes
- **Formatter Conflicts**: Registration might conflict with existing formatters

### Low Risk
- **Debug Message Issues**: Non-critical debug output problems
- **Performance Impact**: Formatter registration overhead

### Mitigation Strategies
- Start with minimal implementation, expand incrementally
- Extensive testing before committing changes
- Maintain debug logging for troubleshooting
- Keep fallback options if formatter registration fails

## Dependencies
- **Epic 001**: Core runtime foundation (provides Initialize() method)
- **Existing Formatter Code**: GNUstepFormattersRegistry must be complete
- **LLDB Headers**: DataVisualization and TypeCategory headers accessible
- **Build System**: CMake configuration for formatter dependencies

## Definition of Done
- [ ] TODO comment removed from Initialize() method
- [ ] GNUstepFormattersRegistry::RegisterFormatters() called in Initialize()
- [ ] Code compiles without errors or warnings
- [ ] LLDB runtime loads successfully with formatter registration
- [ ] Debug messages confirm formatter registration attempt
- [ ] String formatters visible in `type summary list` output
- [ ] No regressions in existing LLDB runtime functionality
- [ ] Error handling present for registration failures
- [ ] Code reviewed and approved
- [ ] Changes committed to version control

## Success Metrics
- **Build Time**: No increase in compilation time
- **Registration Time**: <100ms to register string formatters
- **Stability**: 0 crashes during formatter registration
- **Visibility**: String formatters appear in LLDB type system

---

*Task Owner*: Development Team  
*Created*: August 2025  
*Target Completion*: August 9, 2025 (2 days from project start)
