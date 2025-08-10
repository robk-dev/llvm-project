# GNUstep LLDB Plugin - Development Action Plan

**Date**: 2025-08-10  
**Status**: ACTIVE  
**Goal**: Achieve production-ready GNUstep LLDB debugging by fixing critical bugs and implementing missing features

## Executive Summary

Based on delta analysis, the GNUstep LLDB plugin is 48-80% complete but has **7 critical bugs** preventing production use. This action plan organizes development into **3 parallel work streams** with clear priorities and dependencies.

**Critical Path**: Fix bugs (2 days) → Activate disabled formatters (1 day) → Test coverage (3 days) → New formatters (2 weeks)

## Current State Assessment

### Working Features (15 formatters)
✅ NSString, NSNumber, NSArray, NSDictionary, NSSet  
✅ NSDate, NSURL, NSError, NSData, NSUUID  
✅ NSNull, NSException, NSAttributedString  
✅ Generic Object, Id Dispatcher

### Critical Issues (Must Fix)
❌ Array first element shows `<NSConstantString>` instead of value  
❌ Custom class string ivars show `<invalid object>`  
❌ Dictionary keys corrupted ("namerr" instead of "name")  
❌ ISA resolution fails for custom classes  
❌ Dictionary format verbose (`[0].key` instead of `key = value`)  
❌ NSIndexPath formatter causes recursion/crash (disabled)  
❌ NSNotification formatter causes crash (disabled)

### Missing Features
🔧 7 formatters lack test programs  
🔧 0% unit test coverage  
🔧 20+ high-priority formatters not implemented  
🔧 Custom class property introspection not working

## Priority System

- **P0**: Production blockers - Must fix immediately
- **P1**: Core functionality - Required for basic debugging  
- **P2**: Important features - Significantly improve experience
- **P3**: Nice to have - Complete coverage

## Parallel Work Streams

### Stream A: Bug Fix Team (Critical Path)
**Goal**: Fix all P0 bugs blocking production use  
**Duration**: 2-3 days  
**Dependencies**: None - can start immediately

#### Agent 1: Memory Access Bug Fixes
**Assignment**: Fix array, dictionary, and custom class display issues  
**Tasks**:
- [ ] Fix array first element NSConstantString display (GNUstepArrayFormatters.cpp:467)
- [ ] Fix custom class string ivar double indirection (GNUstepGenericFormatter.cpp:429)
- [ ] Fix dictionary key buffer overflow (GNUstepDictionaryFormatters.cpp:1207)
- [ ] Fix dictionary display format to show `key = value` (lines 1047-1050)
**Success Criteria**: All test cases in custom_class_test.m pass

#### Agent 2: Runtime Introspection Fixes
**Assignment**: Fix ISA resolution and implement CallRuntimeFunction  
**Tasks**:
- [ ] Implement CallRuntimeFunction in GNUstepObjCRuntime.cpp
- [ ] Implement GetDynamicTypeAndAddress for custom classes
- [ ] Fix ISA resolution in GNUstepObjCRuntimeIntrospector.cpp
- [ ] Add ISA caching mechanism
**Success Criteria**: Custom classes show correct type and properties

#### Agent 3: Disabled Formatter Recovery
**Assignment**: Debug and re-enable NSIndexPath and NSNotification  
**Tasks**:
- [ ] Debug NSIndexPath recursion issue
- [ ] Fix NSNotification memory access crash
- [ ] Add cycle detection and validation
- [ ] Re-enable both formatters in registry
**Success Criteria**: Both formatters work without crashes

### Stream B: Formatter Implementation Team
**Goal**: Implement missing high-priority formatters  
**Duration**: 2 weeks  
**Dependencies**: Start after Stream A completes P0 fixes

#### Agent 4: Collection Enhancement Formatters
**Assignment**: NSIndexSet, NSDecimalNumber, NSCharacterSet  
**Tasks**:
- [ ] Implement NSIndexSet with range display ("3 indexes in [0-2]")
- [ ] Implement NSDecimalNumber with precision handling
- [ ] Implement NSCharacterSet with predefined set detection
- [ ] Create test programs for each formatter
**Deliverables**: 3 formatters + 3 test programs

#### Agent 5: System Class Formatters
**Assignment**: NSTimeZone, NSLocale, NSCalendar  
**Tasks**:
- [ ] Implement NSTimeZone ("America/New_York (GMT-5)")
- [ ] Implement NSLocale ("en_US (English, United States)")
- [ ] Implement NSCalendar with components
- [ ] Create comprehensive test suite
**Deliverables**: 3 formatters + test programs

#### Agent 6: Process/Bundle Formatters
**Assignment**: NSBundle, NSUserDefaults, NSProcessInfo  
**Tasks**:
- [ ] Implement NSBundle ("com.example.app at /path")
- [ ] Implement NSUserDefaults with key count
- [ ] Implement NSProcessInfo ("MyApp (PID: 12345)")
- [ ] Add synthetic children support where needed
**Deliverables**: 3 formatters + test programs

### Stream C: Testing and Quality Team
**Goal**: Achieve 100% test coverage for all formatters  
**Duration**: 1 week initially, then ongoing  
**Dependencies**: Can start immediately for existing formatters

#### Agent 7: Test Program Development
**Assignment**: Create missing test programs  
**Tasks**:
- [ ] Create test_attributedstring.m
- [ ] Create test_indexpath.m
- [ ] Create test_null.m
- [ ] Create test_exception.m
- [ ] Create test_notification.m
- [ ] Create test_uuid.m
- [ ] Create test_data.m
**Deliverables**: 7 test programs with validation scripts

#### Agent 8: Unit Test Implementation
**Assignment**: Build unit test framework  
**Tasks**:
- [ ] Create GNUstepFormatterUnitTests.cpp
- [ ] Implement memory access pattern tests
- [ ] Add string extraction tests
- [ ] Add ISA resolution tests
- [ ] Add tagged pointer tests
- [ ] Add performance benchmarks
**Deliverables**: 50+ unit tests, >80% code coverage

#### Agent 9: Integration Testing
**Assignment**: Build integration test suite  
**Tasks**:
- [ ] Create TestGNUstepFormatters.py for LLDB
- [ ] Write interactive test scripts (.lldb files)
- [ ] Build regression test suite
- [ ] Setup performance monitoring
- [ ] Create CI/CD pipeline configuration
**Deliverables**: Automated test suite, CI integration

## Implementation Timeline

### Week 1: Critical Fixes and Testing
**Days 1-2**: Stream A fixes all P0 bugs  
**Days 3-4**: Stream C creates missing test programs  
**Day 5**: Validate all fixes, re-enable disabled formatters  
**Goal**: Production-ready with existing formatters

### Week 2: Core Formatter Implementation
**Days 6-7**: Stream B implements NSIndexSet, NSDecimalNumber, NSCharacterSet  
**Days 8-9**: Stream B implements system classes (NSTimeZone, NSLocale, NSCalendar)  
**Day 10**: Stream C builds unit test framework  
**Goal**: 9 new formatters with tests

### Week 3: Extended Features
**Days 11-12**: Stream B implements process/bundle formatters  
**Days 13-14**: Complete integration testing  
**Day 15**: Performance optimization and documentation  
**Goal**: Full P1/P2 feature set complete

### Week 4: Polish and Upstream
**Days 16-17**: Fix any remaining issues  
**Days 18-19**: Complete documentation  
**Day 20**: Prepare upstream LLVM patch  
**Goal**: Production-ready for upstream submission

## Success Metrics

### Quality Gates
Each phase must meet these criteria before proceeding:

#### Phase 1 Complete (End of Week 1)
- [ ] All 7 P0 bugs fixed
- [ ] All existing formatters have test programs
- [ ] No crashes in 100 test runs
- [ ] Custom classes display correctly

#### Phase 2 Complete (End of Week 2)
- [ ] 9 new formatters implemented
- [ ] Unit test coverage > 50%
- [ ] All formatters < 50ms response time
- [ ] Integration tests passing

#### Phase 3 Complete (End of Week 3)
- [ ] All P1/P2 formatters implemented
- [ ] Unit test coverage > 80%
- [ ] Full regression test suite
- [ ] Performance benchmarks met

#### Production Ready (End of Week 4)
- [ ] Zero known crashes
- [ ] All tests passing
- [ ] Documentation complete
- [ ] Code review approved
- [ ] Upstream patch submitted

## Risk Management

### High Risk Items
1. **CallRuntimeFunction complexity** - May require LLVM expression evaluator expertise
   - Mitigation: Consult LLVM forums, study Apple implementation
   
2. **Memory layout changes** - GNUstep updates could break formatters
   - Mitigation: Version detection, compatibility layer
   
3. **Performance degradation** - Complex formatters may be slow
   - Mitigation: Caching, lazy loading, profiling

### Contingency Plans
- If Stream A takes longer: Delay Stream B start
- If performance issues: Add caching layer
- If upstream changes needed: Fork and maintain separately

## Communication Protocol

### Daily Sync Points
- Stream leads report progress at 10am
- Blockers escalated immediately
- Test results shared at 5pm

### Deliverable Tracking
Use TODO comments in code:
```cpp
// TODO(Stream-A): Fix array first element bug
// FIXED(Agent-1): Array displays correctly now
```

### Progress Reporting
Update this document daily with:
- [x] Completed tasks
- [ ] Pending tasks
- 🔄 In progress (with agent assignment)

## Agent Assignments

### Recommended Specializations
- **Agent 1-3**: C++ experts with LLDB experience (Stream A)
- **Agent 4-6**: Objective-C and Foundation framework experts (Stream B)  
- **Agent 7-9**: Testing and Python scripting experts (Stream C)

### Coordination Points
- Stream A must complete before Stream B starts major work
- Stream C can start immediately on existing formatters
- Daily merge of fixes to avoid conflicts

## File Locations Reference

### Implementation Files
- Formatters: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/`
- Runtime: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/`
- Tests: `/home/robk/code/llvm-project/lldb/examples/`

### Instruction Documents
- Formatter Instructions: `/home/robk/code/llvm-project/lldb/AGENT_INSTRUCTIONS_FORMATTERS.md`
- Runtime Instructions: `/home/robk/code/llvm-project/lldb/AGENT_INSTRUCTIONS_RUNTIME.md`
- Testing Instructions: `/home/robk/code/llvm-project/lldb/AGENT_INSTRUCTIONS_TESTING.md`

### Build Commands
```bash
# Quick rebuild after changes
cd /home/robk/code/llvm-project/build
ninja lldbPluginGNUstepObjCRuntime

# Full LLDB build
ninja lldb lldb-server lldb-argdumper

# Run tests
cd /home/robk/code/llvm-project/lldb/examples
make all
/home/robk/code/llvm-project/build/bin/lldb custom_class_test
```

## Next Actions

### Immediate (Today)
1. Stream A begins fixing P0 bugs
2. Stream C creates first test program
3. Update project status tracking

### Tomorrow
1. Complete first bug fixes
2. Begin unit test framework
3. Re-enable disabled formatters

### This Week
1. All P0 bugs fixed
2. All test programs created
3. Production validation complete

## Appendix: Quick Reference

### Bug Fix Locations
| Bug | File | Line | Priority |
|-----|------|------|----------|
| Array first element | GNUstepArrayFormatters.cpp | 467 | P0 |
| Custom class ivars | GNUstepGenericFormatter.cpp | 429 | P0 |
| Dictionary keys | GNUstepDictionaryFormatters.cpp | 1207 | P0 |
| Dictionary format | GNUstepDictionaryFormatters.cpp | 1047-1050 | P0 |
| ISA resolution | GNUstepObjCRuntimeIntrospector.cpp | Multiple | P0 |
| NSIndexPath crash | GNUstepFormattersRegistry.cpp | 342 | P0 |
| NSNotification crash | GNUstepFormattersRegistry.cpp | 343 | P0 |

### Missing Formatters Priority List
| Formatter | Priority | Effort | Stream |
|-----------|----------|--------|--------|
| NSIndexSet | P1 | 3 days | B |
| NSDecimalNumber | P1 | 3 days | B |
| NSCharacterSet | P1 | 3 days | B |
| NSTimeZone | P2 | 3 days | B |
| NSLocale | P2 | 3 days | B |
| NSCalendar | P2 | 5 days | B |
| NSBundle | P2 | 3 days | B |
| NSUserDefaults | P2 | 4 days | B |
| NSProcessInfo | P2 | 3 days | B |

---

**Document Status**: ACTIVE  
**Last Updated**: 2025-08-10  
**Next Review**: Daily at 5pm  
**Owner**: GNUstep LLDB Development Team