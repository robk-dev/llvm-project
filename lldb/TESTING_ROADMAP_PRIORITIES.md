# GNUstep LLDB Plugin Testing Roadmap and Priorities

## Current Status: 45-50% Coverage → Target: 90% Meaningful Coverage

### Immediate Critical Priorities (Week 1-2)

#### 1. **ISA Lookup and Resolution Testing** (CRITICAL - Blocking Issue)
**Current Coverage:** ~0%  
**Target Coverage:** 95%  
**Files:** `GNUstepObjCRuntimeIntrospector.cpp` (660 lines)

**Missing Tests:**
- `GetISAFromObject()` with various object types
- Tagged pointer detection and classification
- Regular object ISA extraction
- Cross-architecture pointer size handling
- Memory alignment validation

**Test Implementation Priority:**
```cpp
// High Priority Test Cases:
- Basic object ISA lookup
- Tagged NSNumber ISA handling
- Tagged NSString ISA handling  
- Custom class ISA resolution
- Corrupted object ISA handling
- NULL/invalid pointer handling
```

**Estimated Effort:** 3-4 days  
**Risk if Not Fixed:** Custom class debugging completely broken

#### 2. **Runtime API Symbol Resolution** (CRITICAL)
**Current Coverage:** ~0%  
**Target Coverage:** 90%  
**Files:** `GNUstepRuntimeV2API.cpp` (1,272 lines)

**Missing Tests:**
- `CallRuntimeFunction()` implementation and testing
- Symbol lookup across different GNUstep versions
- Function pointer validation
- Runtime library loading verification
- ABI compatibility testing

**Implementation Strategy:**
- Mock runtime environment for testing
- Symbol resolution stress testing  
- Error handling for missing symbols
- Version compatibility matrix testing

**Estimated Effort:** 5-6 days  
**Risk if Not Fixed:** Plugin fails with runtime updates

#### 3. **Plugin Lifecycle and Error Handling** (HIGH)
**Current Coverage:** ~20%  
**Target Coverage:** 85%  
**Files:** `GNUstepObjCRuntime.cpp` (634 lines)

**Missing Tests:**
- Plugin initialization failure scenarios
- Runtime detection edge cases
- Formatter registration error handling  
- Graceful degradation when runtime unavailable
- Memory cleanup on plugin unload

### Medium-term Development (Week 3-4)

#### 4. **Foundation Type Completion** (MEDIUM)
**Current Coverage:** ~30%  
**Target Coverage:** 80%

**Priority Order:**
1. **NSError/NSException** (153 lines) - Most commonly needed for debugging
2. **NSData/NSMutableData** (159 lines) - Binary data inspection crucial
3. **NSUUID** (105 lines) - Simple but missing completely
4. **NSIndexPath** (161 lines) - UI/navigation debugging
5. **NSAttributedString** (141 lines) - Text rendering debugging

**Test Requirements:**
```objective-c
// NSError testing priorities:
- Domain and code extraction
- UserInfo dictionary formatting  
- Localized description handling
- Error chaining/underlying errors
- Custom error subclasses

// NSData testing priorities:  
- Binary data display (hex/ASCII)
- Large data truncation
- Empty/null data handling
- Different data sources (file, network, memory)
- Mutable vs immutable differences
```

#### 5. **Memory Safety and Corruption Handling** (HIGH)
**Current Coverage:** ~15%  
**Target Coverage:** 75%

**Test Scenarios:**
- Invalid object pointers (0xDEADBEEF patterns)
- Partially corrupted objects (valid ISA, corrupted data)
- Memory access violations during formatting
- Race conditions in multi-threaded debugging
- Stack overflow prevention in recursive formatting

### Long-term Quality Assurance (Week 5-6)

#### 6. **Performance and Stress Testing** (MEDIUM)
**Current Coverage:** ~25%  
**Target Coverage:** 70%

**Performance Targets:**
- Collections with 100K+ elements: <200ms formatting time
- Nested structures 20+ levels deep: <500ms formatting time  
- Memory usage growth: Linear, not exponential
- Concurrent debugging sessions: No interference

**Stress Test Scenarios:**
- Massive collections (NSArray with 1M elements)
- Deep nesting (dictionary in array in dictionary...)
- Rapid debugging session creation/destruction
- Memory pressure situations
- Cross-thread object access patterns

#### 7. **Integration and Compatibility Testing** (LOW)
**Current Coverage:** ~40%  
**Target Coverage:** 65%

**Compatibility Matrix:**
- GNUstep versions: 1.26, 1.27, 1.28, 1.29+
- libobjc2 versions: 2.0, 2.1, 2.2+  
- Architecture support: x86_64, ARM64, i386
- Linux distributions: Ubuntu 20.04+, Fedora 35+, Debian 11+

## Testing Infrastructure Improvements

### 1. **Automated Test Framework** (Week 1)
**Priority:** HIGH  
**Estimated Effort:** 2-3 days

**Components Needed:**
- Automated test program compilation with proper flags
- LLDB session automation for regression testing  
- Performance benchmark automation
- Test result comparison and reporting
- CI/CD integration hooks

**Implementation:**
```bash
# Test automation script structure:
lldb/scripts2/
├── automated_testing/
│   ├── run_all_tests.sh
│   ├── performance_benchmarks.py
│   ├── regression_detection.py
│   └── coverage_measurement.py
```

### 2. **Coverage Measurement Integration** (Week 2)
**Priority:** MEDIUM  
**Estimated Effort:** 2 days

**Tools Integration:**
- LLVM coverage tools (llvm-profdata, llvm-cov)
- Coverage report generation  
- Trend tracking and regression detection
- Per-component coverage breakdown

### 3. **Test Data Generation** (Week 3)
**Priority:** MEDIUM  
**Estimated Effort:** 3 days

**Generators Needed:**
- Large collection test data (arrays, dictionaries, sets)
- Complex nested structure generators
- Corrupted object simulators  
- Performance stress test data
- Multi-threaded test scenario generators

## Specific Next Actions (This Week)

### Day 1-2: ISA Lookup Emergency Fix
1. **Implement `CallRuntimeFunction()` stub replacement**
   - Focus on basic object_getClass() functionality
   - Test with simple NSString and NSNumber objects
   - Validate tagged pointer detection works

2. **Add Basic ISA Tests**
   ```python
   # Add to TestGNUstepIntrospector.py:
   def test_isa_lookup_basic_objects(self):
       # Test ISA lookup for NSString
       # Test ISA lookup for NSNumber  
       # Test ISA lookup for NSArray
       # Verify class names are correct
   ```

### Day 3-4: Runtime Symbol Resolution  
1. **Implement Symbol Lookup Testing**
   - Test objc_getClass symbol resolution
   - Test object_getClass symbol resolution
   - Add fallback strategies for missing symbols

2. **Add Runtime API Tests**
   ```python
   # Add to TestGNUstepRuntime.py:
   def test_runtime_symbol_resolution(self):
       # Verify required symbols are found
       # Test symbol resolution with different GNUstep versions
       # Test graceful handling of missing symbols
   ```

### Day 5-7: Foundation Type Completion
1. **Implement NSError Formatter Testing**
   - Create comprehensive NSError test objects
   - Test domain, code, and userInfo extraction
   - Verify localized description handling

2. **Implement NSData Formatter Testing**  
   - Test binary data display formats
   - Test large data truncation behavior
   - Test empty/null data edge cases

## Quality Gates and Success Metrics

### Phase 1 Success Criteria (Week 2):
- [ ] ISA lookup works for all basic Foundation types
- [ ] Custom class properties are accessible in debugger  
- [ ] Runtime symbol resolution robust across GNUstep versions
- [ ] Zero crashes in formatter error scenarios
- [ ] Test coverage measurement baseline established

### Phase 2 Success Criteria (Week 4):
- [ ] All 13 formatter types have comprehensive test coverage
- [ ] Memory corruption scenarios handled gracefully
- [ ] Performance targets met for large collections
- [ ] Automated test execution integrated into build process

### Phase 3 Success Criteria (Week 6):
- [ ] 90%+ coverage of all CRITICAL components
- [ ] 85%+ coverage of all HIGH PRIORITY components  
- [ ] Comprehensive error handling test suite
- [ ] Performance regression detection system
- [ ] Production-ready plugin suitable for upstream submission

## Resource Requirements and Timeline

### Development Resources:
- **Primary Developer:** 6 weeks full-time equivalent
- **Test Infrastructure:** 1 week additional setup time
- **Documentation and Integration:** 1 week additional

### Infrastructure Requirements:
- **Test Environments:** Multiple Linux distributions with GNUstep
- **CI/CD Integration:** Automated testing pipeline
- **Performance Monitoring:** Benchmarking and trending tools
- **Coverage Tools:** LLVM coverage measurement integration

### Risk Mitigation:
- **Parallel Development:** ISA lookup and symbol resolution can be developed concurrently
- **Incremental Testing:** Each component can be tested independently  
- **Fallback Strategies:** Graceful degradation for untested edge cases
- **Community Feedback:** Early testing with real applications

This roadmap provides a realistic path to 90% meaningful test coverage while prioritizing the most critical functionality first. The focus on ISA lookup and runtime symbol resolution addresses the current blocking issues that prevent full debugging functionality.