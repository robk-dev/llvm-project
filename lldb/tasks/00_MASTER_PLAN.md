# GNUstep LLDB Expression Evaluation MVP - Master Plan

## Project Goals
Create a minimal, upstream-ready patch for LLDB that enables Objective-C expression evaluation on Windows/Linux with GNUstep/libobjc2. Target the most critical use cases while maintaining code quality suitable for LLVM project submission.

## Current State Analysis

### ✅ What's Working
- Basic GNUstep runtime detection and instantiation
- Data formatters for NSString, NSNumber, NSArray, NSDictionary
- Class descriptor introspection
- Runtime symbol resolution infrastructure
- Basic DeclVendor framework

### ❌ Critical Issues Identified

1. **Expression Evaluation Failures**: C language (type 2) expressions are rejected, causing access violations
2. **Missing ObjC Literals/Subscripting**: No `CalculateHasNewLiteralsAndIndexing()` implementation
3. **Windows Calling Convention Issues**: `objc_msgSend` casts may not use correct Win64 CC
4. **Symbol Resolution Gaps**: Missing `__imp_` prefix handling for COFF imports
5. **CFString Creation Missing**: No `CFStringCreateWithBytes` implementation for @"" literals
6. **Runtime Declaration Gaps**: Missing key function declarations in DeclVendor

### 🎯 MVP Success Criteria

These expressions must work on Windows x64 + GNUstep:
```objc
expr -l objc++ -O -- (id)[NSNumber numberWithInt:7]
expr -l objc++ -O -- (int)[(id)[NSNumber numberWithInt:7] intValue]  
expr -l objc++ -O -- id arr=(id)[NSArray arrayWithObjects:@"a",@"b",nil]; (id)arr[0]
expr -l objc++ -O -- @"hello"
```

## Task Breakdown

### Phase 1: Core Infrastructure (Tasks 01-03)
- [ ] **Task 01**: Fix language type handling in CreateInstance
- [ ] **Task 02**: Implement ObjC literals and subscripting support  
- [ ] **Task 03**: Fix Windows calling convention for objc_msgSend

### Phase 2: Runtime Functions (Tasks 04-06)
- [ ] **Task 04**: Implement CFStringCreateWithBytes for @"" literals
- [ ] **Task 05**: Enhance runtime symbol resolution (COFF support)
- [ ] **Task 06**: Complete DeclVendor runtime function declarations

### Phase 3: Testing & Validation (Tasks 07-08)
- [ ] **Task 07**: Create comprehensive test suite
- [ ] **Task 08**: Performance optimization and cleanup

### Phase 4: Documentation & Submission (Task 09)
- [ ] **Task 09**: Prepare upstream submission

## Architecture Principles

1. **Runtime Introspection First**: Use GNUstep runtime queries rather than hardcoding
2. **Minimal Surface Area**: Only expose essential Foundation classes/methods
3. **Windows-First Design**: Ensure Windows x64 calling conventions are correct
4. **Upstream Compatible**: Follow LLVM coding standards and review patterns
5. **Performance Conscious**: Avoid expensive operations during CreateInstance

## Risk Mitigation

- Each task includes rollback instructions
- Changes are isolated to GNUstep plugin only
- Comprehensive logging for field debugging
- Test coverage for regressions

## Timeline Estimate
- Phase 1: 3-4 tasks (core fixes)
- Phase 2: 3 tasks (runtime completion) 
- Phase 3: 2 tasks (testing)
- Phase 4: 1 task (submission)

**Total: ~8-10 focused development sessions**
