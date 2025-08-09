# GNUstep LLDB Plugin - Comprehensive Unit Test Backlog

## Executive Summary

This document provides a comprehensive testing backlog for the GNUstep LLDB plugin implementation. The analysis identifies **42 formatter classes**, **15 core components**, and over **500 specific test scenarios** that require coverage. Current test implementation is at **0% functional coverage** with only stub tests present.

## Test Coverage Gap Analysis

### Current State
- **Existing Tests**: 3 test files with placeholder implementations only
  - `GNUstepFormattersTest.cpp` - 13 test stubs, no functional implementation
  - `GNUstepIntrospectorTest.cpp` - 10 test stubs, no functional implementation  
  - `GNUstepTaggedPointerTest.cpp` - 10 partially implemented tests (tagged pointer logic only)
- **Functional Coverage**: 0% (all tests are placeholders except tagged pointer tests)
- **Test Infrastructure**: Basic GoogleTest setup exists but lacks mock framework

### Required Coverage
- **42 Formatter Classes** requiring unit tests
- **15 Core Runtime Components** requiring integration tests
- **500+ Test Scenarios** covering positive, negative, edge cases, and performance
- **100% Code Coverage Goal** for production readiness

---

## Detailed Component Testing Requirements

### 1. Core Runtime Components

#### 1.1 GNUstepObjCRuntime (Main Plugin)
**Priority: CRITICAL**
- [ ] Plugin initialization and registration
- [ ] Runtime detection (libobjc.so.2, libgnustep-base.so)
- [ ] Category creation and management
- [ ] Formatter registration workflow
- [ ] Plugin teardown and cleanup
- [ ] Multiple process attachment
- [ ] Runtime version detection
- [ ] Error handling during initialization

#### 1.2 GNUstepObjCRuntimeIntrospector
**Priority: CRITICAL**
- [ ] ISA pointer resolution
- [ ] Class name extraction from memory
- [ ] Method list traversal
- [ ] Ivar extraction and type encoding
- [ ] Protocol conformance checking
- [ ] Superclass chain walking
- [ ] Memory bounds validation
- [ ] Corrupted ISA handling
- [ ] Tagged pointer detection
- [ ] Small object optimization detection

#### 1.3 GNUstepObjCDeclVendor
**Priority: HIGH**
- [ ] Type synthesis for runtime classes
- [ ] Method signature creation
- [ ] Property synthesis
- [ ] Protocol type creation
- [ ] Category handling
- [ ] Type caching mechanisms
- [ ] Invalid type handling

#### 1.4 GNUstepRuntimeV2API
**Priority: HIGH**
- [ ] Runtime function resolution
- [ ] Symbol lookup fallbacks
- [ ] Function calling via expression evaluator
- [ ] Parameter marshalling
- [ ] Return value extraction
- [ ] Error handling for missing symbols

#### 1.5 GNUstepClassDescriptor
**Priority: MEDIUM**
- [ ] Class metadata extraction
- [ ] Instance size calculation
- [ ] Method table parsing
- [ ] Property list parsing
- [ ] Weak ivar layout
- [ ] Strong ivar layout

---

### 2. Formatter Base Infrastructure

#### 2.1 GNUstepFormattersBase
**Priority: CRITICAL**
- [ ] Base summary provider functionality
- [ ] Base synthetic provider functionality
- [ ] Memory reading utilities
- [ ] String extraction helpers
- [ ] Error propagation
- [ ] Null checking utilities
- [ ] Performance timing helpers

#### 2.2 GNUstepFormattersRegistry
**Priority: CRITICAL**
- [ ] Formatter registration workflow
- [ ] Type matching patterns
- [ ] Category enablement
- [ ] Registration order dependencies
- [ ] Duplicate registration handling
- [ ] Unregistration workflow
- [ ] Category state persistence

#### 2.3 GNUstepIdDispatcher
**Priority: CRITICAL**
- [ ] Dynamic type resolution for 'id' type
- [ ] Formatter dispatch logic
- [ ] Class name to formatter mapping
- [ ] Fallback to generic formatter
- [ ] Performance of dispatch
- [ ] Caching of dispatch results

#### 2.4 GNUstepGenericFormatter
**Priority: HIGH**
- [ ] Generic object display
- [ ] ISA extraction for unknown types
- [ ] Basic property enumeration
- [ ] Fallback formatting
- [ ] Custom class handling

---

### 3. String Formatters

#### 3.1 GNUstepStringFormatters (NSString & variants)
**Priority: CRITICAL**
**Test Scenarios:**
- [ ] ASCII string extraction
- [ ] UTF-8 string with emojis
- [ ] UTF-16 string handling
- [ ] UTF-32 string handling
- [ ] Empty string ("")
- [ ] Nil string (0x0)
- [ ] Single character string
- [ ] Maximum length string (2GB)
- [ ] String with null bytes
- [ ] Corrupted string pointer
- [ ] Tagged string (small string optimization)
- [ ] Constant strings (compile-time)
- [ ] Mutable string variants
- [ ] String with special characters (\n, \t, etc.)
- [ ] Non-printable characters
- [ ] Memory-mapped strings
- [ ] String slices/substrings
- [ ] Performance with 10MB+ strings

**Edge Cases:**
- [ ] Invalid encoding marker
- [ ] Truncated string data
- [ ] Cyclic string references
- [ ] String in unmapped memory

---

### 4. Number Formatters

#### 4.1 GNUstepNumberFormatters (NSNumber & variants)
**Priority: CRITICAL**
**Test Scenarios:**
- [ ] Integer values (int8, int16, int32, int64)
- [ ] Unsigned integers (uint8, uint16, uint32, uint64)
- [ ] Float values (normal, denormal, infinity, NaN)
- [ ] Double values (normal, denormal, infinity, NaN)
- [ ] Boolean values (YES/NO, true/false, 1/0)
- [ ] Decimal numbers
- [ ] Tagged integer pointers
- [ ] Tagged float pointers
- [ ] Nil NSNumber
- [ ] NSNumber with custom type encoding
- [ ] Large integers (> 2^53)
- [ ] Negative zero (-0.0)
- [ ] Extreme values (INT_MAX, INT_MIN)

**Edge Cases:**
- [ ] Corrupted type encoding
- [ ] Invalid tagged pointer format
- [ ] Unaligned memory access
- [ ] Type mismatch (float data, int type)

---

### 5. Collection Formatters

#### 5.1 GNUstepArrayFormatters (NSArray/NSMutableArray)
**Priority: CRITICAL**
**Test Scenarios:**
- [ ] Empty array ([])
- [ ] Single element array
- [ ] Arrays with 10, 100, 1000, 10000 elements
- [ ] Nested arrays (array of arrays)
- [ ] Mixed type arrays
- [ ] Arrays with nil elements
- [ ] Circular reference arrays
- [ ] Immutable vs mutable arrays
- [ ] GSInlineArray optimization
- [ ] GSPlaceholderArray
- [ ] Array with custom objects
- [ ] Performance with 1M+ elements
- [ ] Child enumeration (GetChildAtIndex)
- [ ] Summary truncation for large arrays

**Edge Cases:**
- [ ] Corrupted count field
- [ ] Invalid element pointers
- [ ] Array during mutation
- [ ] Thread safety during access

#### 5.2 GNUstepDictionaryFormatters (NSDictionary/NSMutableDictionary)
**Priority: CRITICAL**
**Test Scenarios:**
- [ ] Empty dictionary ({})
- [ ] Single key-value pair
- [ ] Dictionaries with 10, 100, 1000 pairs
- [ ] String keys vs object keys
- [ ] Nested dictionaries
- [ ] Dictionaries with nil values
- [ ] Circular reference dictionaries
- [ ] Immutable vs mutable dictionaries
- [ ] Hash table internal structure
- [ ] Key-value pair display format
- [ ] Custom key objects
- [ ] Performance with 100K+ pairs
- [ ] Child naming ([0].key vs key name)

**Edge Cases:**
- [ ] Hash collision handling
- [ ] Corrupted hash table
- [ ] Dictionary during rehashing
- [ ] Invalid key/value pairs

#### 5.3 GNUstepSetFormatters (NSSet/NSMutableSet)
**Priority: HIGH**
**Test Scenarios:**
- [ ] Empty set
- [ ] Single element set
- [ ] Sets with 10, 100, 1000 elements
- [ ] Sets with duplicate attempts
- [ ] Mixed type sets
- [ ] Nested sets
- [ ] Circular reference sets
- [ ] Immutable vs mutable sets
- [ ] NSCountedSet variants
- [ ] Performance with 100K+ elements
- [ ] Element enumeration

**Edge Cases:**
- [ ] Hash table corruption
- [ ] Set during mutation
- [ ] Invalid element pointers

---

### 6. Foundation Type Formatters

#### 6.1 GNUstepDateFormatters (NSDate/NSCalendarDate)
**Priority: MEDIUM**
**Test Scenarios:**
- [ ] Current date/time
- [ ] Reference date (2001-01-01)
- [ ] Distant past/future
- [ ] Nil date
- [ ] Invalid time intervals
- [ ] Timezone handling
- [ ] Daylight saving transitions
- [ ] Leap seconds
- [ ] Calendar date components
- [ ] Date formatting patterns

#### 6.2 GNUstepURLFormatters (NSURL)
**Priority: MEDIUM**
**Test Scenarios:**
- [ ] HTTP/HTTPS URLs
- [ ] File URLs
- [ ] FTP URLs
- [ ] Custom schemes
- [ ] URLs with query parameters
- [ ] URLs with fragments
- [ ] Encoded URLs
- [ ] International domain names
- [ ] Relative URLs
- [ ] Nil URL

#### 6.3 GNUstepDataFormatters (NSData/NSMutableData)
**Priority: MEDIUM**
**Test Scenarios:**
- [ ] Empty data
- [ ] Small data (< 1KB)
- [ ] Large data (> 1MB)
- [ ] Binary data display
- [ ] Hex dump format
- [ ] ASCII interpretation
- [ ] Data slices
- [ ] Memory-mapped data
- [ ] Compressed data

#### 6.4 GNUstepUUIDFormatters (NSUUID)
**Priority: LOW**
**Test Scenarios:**
- [ ] Valid UUID
- [ ] Nil UUID
- [ ] Zero UUID
- [ ] Random UUID
- [ ] UUID string formatting
- [ ] Byte representation

#### 6.5 GNUstepErrorFormatters (NSError)
**Priority: MEDIUM**
**Test Scenarios:**
- [ ] Error with domain and code
- [ ] Error with userInfo
- [ ] Nested errors
- [ ] Recovery options
- [ ] Localized descriptions
- [ ] Custom error domains

#### 6.6 GNUstepNullFormatter (NSNull)
**Priority: LOW**
**Test Scenarios:**
- [ ] NSNull singleton
- [ ] Display format
- [ ] Distinction from nil

#### 6.7 GNUstepExceptionFormatter (NSException)
**Priority: MEDIUM**
**Test Scenarios:**
- [ ] Exception name and reason
- [ ] Stack trace
- [ ] User info
- [ ] Nested exceptions

#### 6.8 GNUstepAttributedStringFormatter (NSAttributedString)
**Priority: LOW**
**Test Scenarios:**
- [ ] Plain attributed string
- [ ] String with attributes
- [ ] Attribute ranges
- [ ] Complex formatting

#### 6.9 GNUstepIndexPathFormatter (NSIndexPath)
**Priority: LOW**
**Test Scenarios:**
- [ ] Empty index path
- [ ] Single index
- [ ] Multiple indices
- [ ] Display format

#### 6.10 GNUstepNotificationFormatter (NSNotification)
**Priority: LOW**
**Test Scenarios:**
- [ ] Notification name
- [ ] Notification object
- [ ] User info dictionary
- [ ] Nil components

---

### 7. Performance Testing

**Priority: HIGH**

#### 7.1 Formatter Response Time
- [ ] All formatters complete in < 50ms
- [ ] Large collection handling (100K+ elements)
- [ ] Deep nesting performance (100+ levels)
- [ ] Memory usage profiling
- [ ] Cache effectiveness

#### 7.2 Memory Safety
- [ ] Out-of-bounds access handling
- [ ] Invalid pointer dereferencing
- [ ] Stack overflow prevention
- [ ] Memory leak detection
- [ ] Thread safety validation

#### 7.3 Scalability
- [ ] Multiple simultaneous debug sessions
- [ ] Large process memory spaces (> 10GB)
- [ ] Remote debugging performance
- [ ] Network latency handling

---

## Proposed Test File Structure

```
lldb/unittests/Language/ObjC/GNUstep/
├── CMakeLists.txt
├── TestUtilities/
│   ├── MockProcess.h
│   ├── MockProcess.cpp
│   ├── MockTarget.h
│   ├── MockTarget.cpp
│   ├── MockValueObject.h
│   ├── MockValueObject.cpp
│   ├── TestDataGenerator.h
│   └── TestDataGenerator.cpp
├── Core/
│   ├── GNUstepObjCRuntimeTest.cpp
│   ├── GNUstepIntrospectorTest.cpp
│   ├── GNUstepDeclVendorTest.cpp
│   ├── GNUstepRuntimeAPITest.cpp
│   └── GNUstepClassDescriptorTest.cpp
├── Formatters/
│   ├── Base/
│   │   ├── FormattersBaseTest.cpp
│   │   ├── FormattersRegistryTest.cpp
│   │   ├── IdDispatcherTest.cpp
│   │   └── GenericFormatterTest.cpp
│   ├── Primitives/
│   │   ├── StringFormattersTest.cpp
│   │   ├── NumberFormattersTest.cpp
│   │   └── TaggedPointerTest.cpp
│   ├── Collections/
│   │   ├── ArrayFormattersTest.cpp
│   │   ├── DictionaryFormattersTest.cpp
│   │   └── SetFormattersTest.cpp
│   ├── Foundation/
│   │   ├── DateFormattersTest.cpp
│   │   ├── URLFormattersTest.cpp
│   │   ├── DataFormattersTest.cpp
│   │   ├── UUIDFormattersTest.cpp
│   │   ├── ErrorFormattersTest.cpp
│   │   ├── NullFormatterTest.cpp
│   │   ├── ExceptionFormatterTest.cpp
│   │   ├── AttributedStringFormatterTest.cpp
│   │   ├── IndexPathFormatterTest.cpp
│   │   └── NotificationFormatterTest.cpp
│   └── Performance/
│       ├── FormatterPerformanceTest.cpp
│       ├── MemorySafetyTest.cpp
│       └── ScalabilityTest.cpp
└── Integration/
    ├── EndToEndFormatterTest.cpp
    ├── CustomClassTest.cpp
    ├── RuntimeIntrospectionTest.cpp
    └── ExpressionEvaluationTest.cpp
```

---

## Test Implementation Priority

### Phase 1: Critical Core (Week 1)
1. **Mock Infrastructure Setup**
   - Create MockProcess, MockTarget, MockValueObject
   - Setup test data generators
   - Configure CMake for test building

2. **Core Runtime Tests**
   - GNUstepObjCRuntime initialization
   - Introspector basic functionality
   - Formatters registry

3. **Essential Formatter Tests**
   - NSString basic cases
   - NSNumber basic cases
   - NSArray basic cases

### Phase 2: Complete Primitives (Week 2)
1. **String Formatter Comprehensive**
   - All encoding types
   - Tagged strings
   - Edge cases

2. **Number Formatter Comprehensive**
   - All numeric types
   - Tagged pointers
   - Special values (NaN, Inf)

3. **Id Dispatcher**
   - Dynamic type resolution
   - Dispatch performance

### Phase 3: Collections (Week 3)
1. **Array Tests**
   - All array variants
   - Large arrays
   - Nested structures

2. **Dictionary Tests**
   - Key-value formatting
   - Hash table internals
   - Performance

3. **Set Tests**
   - Set operations
   - Uniqueness validation

### Phase 4: Foundation Types (Week 4)
1. **Date/Time Formatters**
2. **URL/Data Formatters**
3. **Error/Exception Formatters**
4. **Remaining Foundation types**

### Phase 5: Integration & Performance (Week 5)
1. **End-to-end scenarios**
2. **Custom class support**
3. **Performance benchmarks**
4. **Memory safety validation**
5. **Documentation**

---

## Test Patterns and Best Practices

### Unit Test Pattern
```cpp
TEST_F(GNUstepFormatterTest, TestName) {
  // Arrange
  auto mock_process = CreateMockProcess();
  auto mock_value = CreateMockValueObject(mock_process, test_data);
  
  // Act
  auto result = formatter->GetSummary(mock_value);
  
  // Assert
  EXPECT_EQ(result, expected_output);
  EXPECT_TRUE(result.Success());
  EXPECT_LT(execution_time, 50ms);
}
```

### Edge Case Pattern
```cpp
TEST_F(GNUstepFormatterTest, HandlesCorruptedData) {
  // Arrange
  auto corrupted_data = GenerateCorruptedData();
  
  // Act
  auto result = formatter->GetSummary(corrupted_data);
  
  // Assert
  EXPECT_TRUE(result.HasError());
  EXPECT_THAT(result.GetError(), HasSubstr("corrupted"));
  EXPECT_NO_CRASH();
}
```

### Performance Test Pattern
```cpp
TEST_F(GNUstepPerformanceTest, LargeCollectionPerformance) {
  // Arrange
  auto large_array = GenerateArray(1000000);
  
  // Act
  auto start = std::chrono::high_resolution_clock::now();
  auto result = formatter->GetSummary(large_array);
  auto duration = std::chrono::high_resolution_clock::now() - start;
  
  // Assert
  EXPECT_LT(duration, 50ms);
  EXPECT_TRUE(result.Success());
}
```

---

## Success Metrics

### Coverage Goals
- **Line Coverage**: > 90%
- **Branch Coverage**: > 85%
- **Function Coverage**: 100%
- **Edge Case Coverage**: 100%

### Performance Goals
- **Formatter Response**: < 50ms for 99% of operations
- **Memory Usage**: < 100MB overhead per debug session
- **Scalability**: Support 10+ simultaneous debug sessions

### Quality Goals
- **Test Stability**: 0% flaky tests
- **Documentation**: 100% of public APIs documented
- **Maintainability**: All tests follow consistent patterns

---

## Risk Mitigation

### Technical Risks
1. **Mock Complexity**: Creating realistic mocks for LLDB internals
   - Mitigation: Start with minimal mocks, iterate based on needs
   
2. **Test Data Generation**: Creating valid GNUstep object layouts
   - Mitigation: Extract real data from running GNUstep processes

3. **Performance Testing**: Accurate performance measurement
   - Mitigation: Use LLDB's built-in profiling tools

### Schedule Risks
1. **Scope Creep**: Test requirements expanding
   - Mitigation: Strict prioritization, defer nice-to-haves

2. **Integration Issues**: Tests failing in CI
   - Mitigation: Early CI integration, incremental commits

---

## Conclusion

This comprehensive testing backlog identifies **500+ test scenarios** across **42 formatter classes** and **15 core components**. The current implementation has **0% functional test coverage**, representing a critical gap for production readiness.

Implementation of this test suite will require approximately **5 weeks** of focused development, resulting in a robust, maintainable, and comprehensive test framework that ensures the GNUstep LLDB plugin meets production quality standards.

The prioritized approach focuses on critical formatters first (NSString, NSNumber, Collections) before expanding to complete Foundation type coverage. This ensures that the most commonly used debugging scenarios are thoroughly tested early in the development cycle.