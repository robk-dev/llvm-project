# LLDB GNUstep Project - Immediate Action Plan

## Current Status: Build Successfully Completed ✅

The LLDB GNUstep Objective-C runtime plugin now compiles and links successfully with all virtual methods implemented. We have a solid foundation to build upon.

## Next Priority Actions (Next 2 Weeks)

### Week 1: String Formatter Activation
**Epic**: [004_STRING_FORMATTERS](epics/004_STRING_FORMATTERS.md)

#### Task 1: [Activate Formatter Registration](tasks/004_STRING_FORMATTERS/01_Activate_Formatter_Registration.md) 
**Duration**: 2 days  
**Priority**: P0 (Critical)

**Objective**: Remove the TODO comment blocking formatter registration and fix LLDB API compatibility issues.

**Key Deliverables**:
- Remove TODO comment in `GNUstepObjCRuntime::Initialize()`
- Fix DataVisualization API compatibility
- Enable string formatter registration
- Validate formatters appear in LLDB type system

**Files to Modify**:
- `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp` (lines 37-38)

#### Task 2: [Basic String Content Extraction](tasks/004_STRING_FORMATTERS/02_Basic_String_Content_Extraction.md)
**Duration**: 4 days  
**Priority**: P0 (Critical)  
**Blocked by**: Task 1

**Objective**: Implement actual string content extraction so `po myString` shows "Hello World" instead of pointer addresses.

**Key Deliverables**:
- Complete `ExtractStringContent()` implementation
- NSString memory layout analysis and reading
- UTF-8 character support
- Memory safety and error handling

**Files to Modify**:
- `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.cpp`

### Week 2: Validation and Foundation Expansion
**Epic**: [001_CORE_RUNTIME_FOUNDATION](epics/001_CORE_RUNTIME_FOUNDATION.md) + [006_FOUNDATION_FORMATTERS](epics/006_FOUNDATION_FORMATTERS.md)

#### Task 3: ISA Resolution Enhancement (3 days)
**Objective**: Complete ISA-to-class name resolution for robust object type detection.

#### Task 4: NSNumber Basic Formatting (2 days)  
**Objective**: Implement NSNumber value extraction to show "42" instead of pointer addresses.

## Success Criteria for Next 2 Weeks

### Functional Goals
1. **String Debugging Works**: `po myString` displays actual string content
2. **Number Debugging Works**: `po myNumber` displays actual numeric value  
3. **Type Detection Works**: Runtime correctly identifies NSString vs NSNumber vs other objects
4. **No Regressions**: Existing LLDB functionality remains intact

### Technical Goals
1. **Build Stability**: No compilation or linking errors
2. **Runtime Stability**: No crashes during formatter registration or usage
3. **Performance**: String/number formatting completes in <10ms
4. **Memory Safety**: Robust handling of invalid/corrupted objects

## Specialist Dev Agent Handoff

### For String Formatter Specialist
**Focus**: Epic 004 - String Formatters  
**Immediate Task**: [01_Activate_Formatter_Registration](tasks/004_STRING_FORMATTERS/01_Activate_Formatter_Registration.md)

**Key Context**:
- String formatter infrastructure is 100% complete but disabled
- Main blocker: TODO comment in `GNUstepObjCRuntime.cpp:37-38`
- LLDB API compatibility issues need resolution
- Success metric: String formatters appear in `type summary list`

**Technical Focus Areas**:
- LLDB DataVisualization API research
- TypeCategory registration and enablement
- Error handling for registration failures
- Debug logging for troubleshooting

### For Foundation Type Specialist  
**Focus**: Epic 006 - Foundation Formatters  
**Preparation Task**: Research NSNumber memory layout

**Key Context**:
- NSNumber formatting should follow same pattern as strings
- Need to understand GNUstep NSNumber internal structure
- Integration with existing introspector system
- Support for int, float, bool NSNumber variants

### For Runtime Introspection Specialist
**Focus**: Epic 001 - Core Runtime Foundation  
**Preparation Task**: ISA resolution completion

**Key Context**:
- ISA resolution currently stubbed in `GetDynamicTypeAndAddress()`
- Class name resolution works but needs optimization
- Memory layout understanding critical for formatters
- Performance optimization opportunities exist

## Testing Strategy

### Immediate Validation (Daily)
1. **Build Test**: `./build_scripts/build_lldb.sh` completes successfully
2. **Load Test**: LLDB loads GNUstep runtime without errors
3. **Detection Test**: Runtime detects GNUstep applications correctly
4. **Formatter Test**: String formatters registered (visible in type system)

### Integration Testing (Weekly)
1. **Real App Testing**: Test with `/home/robk/code/llvm-project/lldb/examples/simple_test`
2. **Memory Safety**: Test with corrupted/invalid objects
3. **Performance**: Measure formatting time for various object types
4. **Cross-Platform**: Validate on different Linux distributions

## Risk Mitigation

### High Priority Risks
1. **LLDB API Compatibility**: Different LLDB versions may have incompatible APIs
   - *Mitigation*: Start with current environment, add compatibility layers as needed

2. **GNUstep Memory Layout**: Object layouts may vary between GNUstep versions
   - *Mitigation*: Test with multiple GNUstep versions, implement layout detection

3. **Performance Impact**: Formatter overhead could slow debugging significantly
   - *Mitigation*: Early performance monitoring, optimization, and caching strategies

### Medium Priority Risks
1. **String Encoding Complexity**: UTF-8/UTF-16 handling edge cases
   - *Mitigation*: Start with ASCII, add encoding complexity incrementally

2. **Memory Safety**: Reading corrupted objects could crash LLDB
   - *Mitigation*: Extensive bounds checking and validation

## Communication Plan

### Daily Standups
- Progress on current tasks
- Blockers and dependencies  
- Test results and validation
- Risk identification and mitigation

### Weekly Reviews
- Epic progress assessment
- Technical architecture decisions
- Performance metrics review
- Roadmap adjustments if needed

### Milestone Celebrations
- **Week 1 Success**: String formatters working end-to-end
- **Week 2 Success**: Basic Foundation type coverage complete
- **Month 1 Success**: Production-ready string and number debugging

---

## Quick Reference

### Key Files for Immediate Work
- **Main Runtime**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`
- **String Formatters**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepStringFormatters.cpp`
- **Formatter Registry**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepFormattersRegistry.cpp`
- **Test Application**: `/home/robk/code/llvm-project/lldb/examples/simple_test.m`

### Build Commands
```bash
# Full build
./build_scripts/build_lldb.sh

# Quick formatter-only build  
cd /home/robk/code/llvm-project && ninja -C build tools/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/CMakeFiles/lldbPluginGNUstepObjCRuntime.dir/GNUstepObjCRuntime.cpp.o
```

### Debugging Commands
```bash
# Test with simple app
cd /home/robk/code/llvm-project/lldb/examples
lldb ./simple_test
(lldb) b main
(lldb) run  
(lldb) po greeting2  # Should show string content, not pointer
```

---

*Created*: August 7, 2025  
*Next Review*: August 14, 2025  
*Owner*: Product Owner + Development Team
