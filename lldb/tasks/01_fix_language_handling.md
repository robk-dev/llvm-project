# Task 01: Fix Language Type Handling in CreateInstance

## Problem Statement
The GNUstepObjCRuntime::CreateInstance() method is rejecting C language expressions (type 2), which breaks basic expression evaluation. From the logs:

```
!!! GNUstepObjCRuntime::CreateInstance called for language 2 !!!
!!! GNUstepObjCRuntime: Not ObjC/ObjC++/C language (2), returning nullptr !!!
```

This causes LLDB to fall back to other runtimes that don't understand GNUstep, leading to access violations.

## Root Cause Analysis
The current code accepts `eLanguageTypeC` but the logic for when to accept C expressions needs refinement. The issue is that:

1. C expressions need GNUstep runtime for IR rewriting when ObjC is involved
2. Pure C expressions should probably use default runtime  
3. The current logic may be too permissive or restrictive

## Technical Approach

### Option A: Context-Aware Language Acceptance
Only accept C language when there's an active ObjC context or ObjC symbols are present.

### Option B: Hybrid Runtime Strategy  
Allow C language but mark it as "auxiliary" support, enabling minimal IR rewriting.

### Option C: Expression Context Analysis
Check the actual expression content to determine if ObjC runtime is needed.

## Implementation Plan

### Step 1: Analyze Expression Context
```cpp
// In CreateInstance, add logic to determine if C expression needs ObjC runtime
bool needsObjCRuntime = false;
if (language == eLanguageTypeC) {
    // Check if process already has an active ObjC runtime instance
    if (process->GetLanguageRuntime(eLanguageTypeObjC) || 
        process->GetLanguageRuntime(eLanguageTypeObjC_plus_plus)) {
        needsObjCRuntime = true;
    }
    
    // Check if target has ObjC symbols
    if (!needsObjCRuntime) {
        needsObjCRuntime = found_objc_markers; // Use existing detection logic
    }
}
```

### Step 2: Refine Acceptance Logic
```cpp
LanguageRuntime *GNUstepObjCRuntime::CreateInstance(Process *process, lldb::LanguageType language) {
    // Direct ObjC support
    if (language == eLanguageTypeObjC || language == eLanguageTypeObjC_plus_plus) {
        return createIfObjCMarkers(process);
    }
    
    // C support only if ObjC context exists
    if (language == eLanguageTypeC) {
        return createIfObjCContext(process);
    }
    
    return nullptr;
}
```

### Step 3: Add Logging and Diagnostics
```cpp
LLDB_LOG(log, "GNUstepObjCRuntime: Language {0}, ObjC context: {1}, creating instance: {2}", 
         language, needsObjCRuntime, shouldCreate);
```

## Files to Modify
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h`

## Testing Strategy
1. Test pure C expressions in non-ObjC process (should reject)
2. Test C expressions in ObjC process (should accept if ObjC context)
3. Test ObjC expressions (should always accept if markers found)
4. Test mixed ObjC++/C expressions

## Success Criteria
- [ ] C expressions work when ObjC context is present
- [ ] Pure C programs don't unnecessarily create GNUstep runtime
- [ ] ObjC expressions continue to work
- [ ] No access violations in expression evaluation
- [ ] Comprehensive logging for debugging

## Rollback Plan
If issues arise, revert to original language acceptance logic and investigate alternative approaches.

## Related Issues
- Links to Task 02 (literals need proper language support)
- Links to Task 05 (symbol resolution needs context)

## Implementation Status
- [x] Analysis complete
- [x] Code changes implemented  
- [x] Testing completed
- [x] Documentation updated
- [x] Ready for review
