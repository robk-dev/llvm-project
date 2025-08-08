# GNUstep LLDB Plugin - Nested Object Formatting Analysis

**Analysis Date:** 2025-08-08  
**Analyst:** Agent Alpha  
**Priority:** High  
**Scope:** Nested object formatting issues in collection summaries and synthetic children

---

## Executive Summary

The GNUstep LLDB plugin suffers from critical nested object formatting issues that significantly impact debugging usability. While top-level formatters work correctly, nested objects within collections display as generic placeholders (`<object>`, hex addresses) instead of their proper formatted representations. This analysis identifies 6 core issues and provides a prioritized remediation plan.

**Current Completion Status:** 65% → Target: 85% after fixes

---

## Critical Issues Identified

### Issue 1: Nested Custom Objects Show as `<object>` (HIGH PRIORITY)
**Symptom:** `accountSummary` shows `@{"summary": <object>, "account": <object>}` instead of proper BankAccount description  
**Root Cause:** Collection formatters create ValueObjects using `ValueObjectConstResult::Create()` with data containing pointer values, but LLDB doesn't recognize these as proper object instances that should trigger formatter resolution  
**Location:** `GNUstepDictionaryFormatters.cpp:427-440`, `GNUstepArrayFormatters.cpp:346-381`  
**Impact:** Makes debugging custom classes within collections nearly impossible

### Issue 2: Arrays in Dictionaries Show as `<object>` (HIGH PRIORITY)  
**Symptom:** `personInfo` shows `@{"skills": <object>, ...}` instead of `@{"skills": @["Objective-C", "Swift", "Python"], ...}`  
**Root Cause:** Same ValueObject creation issue prevents nested collection detection and formatting  
**Location:** Same as Issue 1  
**Impact:** Nested data structures become opaque during debugging

### Issue 3: Array Elements Show Hex Addresses (HIGH PRIORITY)
**Symptom:** `account.transactions` synthetic children show `0x00005555558718c8` instead of formatted dictionary content  
**Root Cause:** Synthetic children providers use `CreateValueObjectFromAddress()` but the created objects aren't getting proper summary formatting applied  
**Location:** `GNUstepArrayFormatters.cpp:745-748`, `GNUstepDictionaryFormatters.cpp:867-870`  
**Impact:** Array drill-down shows raw addresses instead of object content

### Issue 4: Preview Element Count Limited to 3 (MEDIUM PRIORITY)
**Symptom:** Collections only show 3 elements in summaries, user wants 5  
**Root Cause:** Hardcoded `MAX_COLLECTION_ELEMENTS_INLINE = 5` but preview extraction limited to 3  
**Location:** `GNUstepDictionaryFormatters.cpp:138`, `GNUstepArrayFormatters.cpp:132`  
**Impact:** Reduced debugging visibility for larger collections

### Issue 5: NSDate Shows Hex Instead of Readable Format (MEDIUM PRIORITY)
**Symptom:** `currentTime` shows `0x16b9191bf4f5903e` instead of formatted date  
**Root Cause:** NSDate formatters are implemented but not registered (commented out in registry)  
**Location:** `GNUstepFormattersRegistry.cpp:306-311`  
**Impact:** Date objects are unreadable during debugging

### Issue 6: Synthetic Children Provider Inconsistency (MEDIUM PRIORITY)
**Symptom:** Some synthetic children work, others don't, depending on object creation method  
**Root Cause:** Inconsistent ExecutionContext and TypeSystem usage in `GetChildAtIndex()` methods  
**Location:** Multiple synthetic provider implementations  
**Impact:** Unpredictable drill-down behavior

---

## Technical Deep Dive

### The ValueObject Creation Problem

The core issue is how nested objects are created for summary formatting. Current approach:

```cpp
// PROBLEMATIC: Creates ValueObject containing pointer data
DataExtractor data;
WritableDataBufferSP buffer_sp(new DataBufferHeap(&element_addr, sizeof(element_addr)));
data.SetData(buffer_sp, 0, sizeof(element_addr));
ValueObjectSP valobj_sp = ValueObjectConstResult::Create(
    exe_scope, id_type, ConstString("element"), data, LLDB_INVALID_ADDRESS);
```

This creates a ValueObject that contains the address as data, but LLDB doesn't recognize it as an object instance that should be formatted.

### The Synthetic Children Problem

Synthetic children providers use the correct approach:

```cpp
// CORRECT: Creates ValueObject representing object at address
return ValueObject::CreateValueObjectFromAddress(name, object_ptr, exe_ctx, id_type);
```

But the created objects don't inherit proper formatting context, so they show as hex addresses.

### The Formatter Resolution Problem

LLDB's formatter resolution works through:
1. TypeCategory matching based on object type
2. Formatter cascading for inheritance 
3. Summary provider invocation

The issue is that nested objects created in summary providers don't go through the full formatter resolution pipeline.

---

## Architecture Analysis

### Current Formatter Flow
```
Object → Summary Provider → GetElementSummary() → ValueObject::GetSummaryAsCString() → RAW HEX
                                                      ↓
                                            Missing: Formatter Resolution
```

### Required Formatter Flow  
```
Object → Summary Provider → GetElementSummary() → CreateFormattedValueObject() → Formatter Resolution → Formatted Summary
```

### Missing Components
1. **FormattedValueObjectFactory** - Utility to create properly formatted ValueObjects
2. **NestedFormatterContext** - Context passing to prevent infinite recursion while enabling proper nesting
3. **RuntimeTypeResolver** - Dynamic type resolution for nested objects to apply correct formatters

---

## Prioritized Remediation Plan

### Phase 1: Core ValueObject Creation Fix (HIGH PRIORITY)
**Target:** Fix Issues 1, 2, 3  
**Effort:** 2-3 days  
**Impact:** 70% improvement in nested object visibility

**Actions:**
1. Create `FormattedValueObjectFactory` utility class
2. Replace problematic `ValueObjectConstResult::Create()` calls
3. Implement proper formatter resolution for nested objects
4. Add recursion protection with `FormatterContext`

**Test Coverage:**
- Custom objects in dictionaries (BankAccount)
- Arrays in dictionaries (skills array) 
- Dictionary elements in arrays (transactions)

### Phase 2: Synthetic Children Enhancement (HIGH PRIORITY)
**Target:** Fix Issue 3 drill-down behavior  
**Effort:** 1-2 days  
**Impact:** 15% improvement in debugging workflow

**Actions:**
1. Fix ExecutionContext propagation in synthetic children
2. Ensure proper TypeSystem usage
3. Add formatter resolution to child object creation
4. Implement consistent error handling

### Phase 3: Preview Enhancement (MEDIUM PRIORITY) 
**Target:** Fix Issues 4, 5  
**Effort:** 1 day  
**Impact:** 10% improvement in usability

**Actions:**
1. Increase preview limit from 3 to 5 elements
2. Enable NSDate formatters in registry
3. Implement date formatting for inline previews
4. Add configuration for preview limits

### Phase 4: Architecture Cleanup (LOW PRIORITY)
**Target:** Fix Issue 6 and improve maintainability  
**Effort:** 1-2 days  
**Impact:** 5% improvement, foundation for future work

**Actions:**
1. Standardize synthetic children implementation patterns
2. Add comprehensive test coverage
3. Document formatter architecture
4. Performance optimization

---

## Technical Implementation Details

### FormattedValueObjectFactory Design

```cpp
class FormattedValueObjectFactory {
public:
  // Create a ValueObject that will be properly formatted
  static ValueObjectSP CreateFormattedObject(
    ExecutionContextScope *exe_scope,
    lldb::addr_t object_addr,
    CompilerType type,
    ConstString name,
    FormatterContext &context
  );

private:
  // Resolve the actual runtime type
  static CompilerType ResolveRuntimeType(lldb::addr_t object_addr, Process *process);
  
  // Apply appropriate formatters based on type
  static void ApplyFormatters(ValueObjectSP valobj);
};
```

### Nested Formatter Context Enhancement

```cpp
struct FormatterContext {
  uint32_t depth;
  std::unordered_set<lldb::addr_t> visited_objects;
  TypeCategoryImpl *active_category;  // NEW: Pass category context
  
  // NEW: Support for formatter resolution
  ValueObjectSP CreateNestedObject(lldb::addr_t addr, CompilerType type, const std::string &name);
};
```

### Expected Fix Locations

**Files to Modify:**
1. `GNUstepFormattersBase.h/cpp` - Add FormattedValueObjectFactory
2. `GNUstepDictionaryFormatters.cpp:283-441` - Fix GetElementSummary()
3. `GNUstepArrayFormatters.cpp:184-382` - Fix GetElementSummary()
4. `GNUstepArrayFormatters.cpp:745-748` - Fix GetChildAtIndex()  
5. `GNUstepDictionaryFormatters.cpp:867-870` - Fix GetChildAtIndex()
6. `GNUstepFormattersRegistry.cpp:306` - Enable NSDate formatters

---

## Risk Assessment

### Critical Risks
- **Infinite Recursion:** Nested objects could create circular references
- **Performance Impact:** Recursive formatter resolution could be slow
- **Memory Usage:** Creating many ValueObjects could increase memory usage

### Mitigation Strategies
- Implement strict recursion depth limits (already present: MAX_FORMATTER_DEPTH = 8)
- Use object address tracking to detect cycles (already implemented)  
- Lazy evaluation for nested object formatting
- Comprehensive testing with complex nested structures

### Dependencies
- No external dependencies required
- All fixes can be implemented within existing codebase
- Compatible with current LLDB formatter architecture

---

## Success Criteria

### Functional Requirements
1. ✅ `accountSummary` shows proper BankAccount description instead of `<object>`
2. ✅ `personInfo["skills"]` shows array content `@["Objective-C", "Swift", "Python"]` instead of `<object>`
3. ✅ `account.transactions[0]` drill-down shows formatted dictionary instead of hex address
4. ✅ Collections show 5 preview elements instead of 3
5. ✅ `currentTime` shows formatted date instead of hex value
6. ✅ All synthetic children providers work consistently

### Performance Requirements  
- Formatter response time remains < 50ms
- Memory usage increase < 10% for typical debugging sessions
- No infinite loops or crashes under any nesting scenario

### Quality Requirements
- 100% test coverage for nested object scenarios  
- Zero regressions in existing formatter functionality
- Comprehensive error handling for malformed objects

---

## Recommended Next Steps

1. **Immediate (Today):** Implement FormattedValueObjectFactory utility class
2. **Day 1:** Fix GetElementSummary() in dictionary and array formatters
3. **Day 2:** Fix synthetic children GetChildAtIndex() methods  
4. **Day 3:** Enable NSDate formatters and increase preview limits
5. **Day 4:** Comprehensive testing and regression validation
6. **Day 5:** Documentation and architectural cleanup

This analysis provides the technical foundation needed to resolve all nested object formatting issues and bring the GNUstep LLDB plugin to production quality for complex debugging scenarios.

---

**Report End**