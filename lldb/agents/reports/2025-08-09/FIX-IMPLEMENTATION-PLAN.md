# GNUstep LLDB Formatter - Fix Implementation Plan

**Date**: 2025-08-09
**Author**: Agent Alpha (Analyst)
**Target**: Immediate resolution of 4 critical formatter issues

## Implementation Priority and Order

### Phase 1: String Extraction Fixes (2 hours)
**Priority**: CRITICAL
**Risk**: LOW
**Impact**: HIGH

#### 1.1 Fix Array First Element (30 minutes)

**File**: `lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepArrayFormatters.cpp`

**Changes Required**:

1. **Line 462-467**: Add robust length reading with fallback
```cpp
// BEFORE (line 462-467):
lldb::addr_t len_addr = obj_addr + 16;
uint32_t string_length = 0;
GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length));

// AFTER:
lldb::addr_t len_addr = obj_addr + 16;
uint32_t string_length = 0;
bool length_read_success = GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length));

// Validate length
if (!length_read_success || string_length == 0 || string_length > 10000) {
    // Try reading without length - null terminated
    std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 256);
    if (!result.empty()) {
        return result;
    }
    string_length = 256; // Fallback to reasonable default
}
```

2. **Line 467**: Ensure non-zero preview length
```cpp
// BEFORE:
uint32_t preview_length = (string_length > 0 && string_length < 100) ? string_length : 100;

// AFTER:
uint32_t preview_length = std::max(1u, std::min(string_length, 100u));
```

#### 1.2 Fix Dictionary Key Buffer Overflow (30 minutes)

**File**: `lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDictionaryFormatters.cpp`

**Changes Required**:

1. **Lines 1206-1214**: Fix buffer management
```cpp
// BEFORE:
char buffer[257] = {0};
size_t bytes_read = m_process->ReadMemory(string_ptr, buffer, 256, error);
if (!error.Fail() && bytes_read > 0) {
    buffer[256] = '\0';
    size_t len = strnlen(buffer, bytes_read);
    if (len > 0) {
        return std::string(buffer, len);
    }
}

// AFTER:
char buffer[257] = {0};
size_t to_read = 256;
size_t bytes_read = m_process->ReadMemory(string_ptr, buffer, to_read, error);
if (!error.Fail() && bytes_read > 0) {
    // Ensure null termination at actual read position
    buffer[bytes_read] = '\0';
    
    // Find first null or use bytes_read
    size_t actual_len = strnlen(buffer, bytes_read);
    
    // Clean any non-printable characters
    for (size_t i = 0; i < actual_len; i++) {
        if (buffer[i] < 0x20 || buffer[i] > 0x7E) {
            if (buffer[i] != '\0') {
                actual_len = i; // Truncate at first non-printable
                break;
            }
        }
    }
    
    return std::string(buffer, actual_len);
}
```

### Phase 2: Object Ivar Resolution (1 hour)
**Priority**: CRITICAL
**Risk**: MEDIUM
**Impact**: HIGH

#### 2.1 Fix Custom Class String Ivars (45 minutes)

**File**: `lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepGenericFormatter.cpp`

**Changes Required**:

1. **Lines 420-432**: Fix double-indirection issue
```cpp
// BEFORE:
std::string GNUstepGenericFormatter::FormatObjectIvar(Process *process, 
                                                      lldb::addr_t obj_addr) {
  if (!process || !process->IsValid())
    return "nil";
  
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS)
    return "nil";
  
  Status error;
  lldb::addr_t obj_ptr = GNUstepRuntimeHelper::ReadPointer(process, obj_addr, error);
  
  if (error.Fail() || obj_ptr == 0 || obj_ptr == LLDB_INVALID_ADDRESS)
    return "nil";

// AFTER:
std::string GNUstepGenericFormatter::FormatObjectIvar(Process *process, 
                                                      lldb::addr_t obj_addr) {
  if (!process || !process->IsValid())
    return "nil";
  
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS)
    return "nil";
  
  // For object ivars, obj_addr IS the object address, not a pointer to it
  lldb::addr_t obj_ptr = obj_addr;
  
  // Verify it's a valid object by checking for ISA at offset 0
  Status error;
  lldb::addr_t isa = GNUstepRuntimeHelper::ReadPointer(process, obj_ptr, error);
  
  if (error.Fail() || isa == 0 || isa == LLDB_INVALID_ADDRESS) {
    // Not a valid object at this address
    // Try reading it as a pointer to an object (legacy case)
    obj_ptr = GNUstepRuntimeHelper::ReadPointer(process, obj_addr, error);
    if (error.Fail() || obj_ptr == 0 || obj_ptr == LLDB_INVALID_ADDRESS) {
      return "nil";
    }
    
    // Verify the pointed-to address has a valid ISA
    isa = GNUstepRuntimeHelper::ReadPointer(process, obj_ptr, error);
    if (error.Fail() || isa == 0 || isa == LLDB_INVALID_ADDRESS) {
      return "nil";
    }
  }
  
  // Now obj_ptr definitely points to a valid object
  // Continue with existing logic using obj_ptr...
```

2. **Lines 444-454**: Improve NSString extraction for ivars
```cpp
// Add special handling for NSConstantString in ivars
if (class_name.find("NSConstantString") != std::string::npos || 
    class_name.find("__NSConstantString") != std::string::npos) {
  // NSConstantString layout
  lldb::addr_t str_ptr_addr = obj_ptr + 8;  // char* at offset 8
  lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  
  if (!error.Fail() && str_data_addr != 0) {
    // Read the string directly
    std::string str_content = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 256);
    if (!str_content.empty()) {
      // Truncate for display
      if (str_content.length() > 64) {
        str_content = str_content.substr(0, 61) + "...";
      }
      return "\"" + str_content + "\"";
    }
  }
}
```

### Phase 3: ISA Display Resolution (1 hour)
**Priority**: MEDIUM
**Risk**: MEDIUM
**Impact**: MEDIUM

#### 3.1 Implement GetDynamicTypeAndAddress (45 minutes)

**File**: `lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`

**Changes Required**:

1. Add proper dynamic type resolution:
```cpp
// Add to GNUstepObjCRuntime class:
bool GNUstepObjCRuntime::GetDynamicTypeAndAddress(
    ValueObject &in_value, lldb::DynamicValueType use_dynamic,
    TypeAndOrName &class_type_or_name, Address &address,
    Value::ValueType &value_type) {
  
  // Get the address of the object
  lldb::addr_t obj_addr = in_value.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Read the ISA pointer
  Process *process = GetProcess();
  if (!process) {
    return false;
  }
  
  Status error;
  lldb::addr_t isa = process->ReadPointerFromMemory(obj_addr, error);
  if (error.Fail() || isa == 0) {
    return false;
  }
  
  // Get the class name using our introspector
  GNUstepObjCRuntimeIntrospector introspector(process);
  std::string class_name = introspector.GetClassName(isa);
  
  if (!class_name.empty()) {
    // Set the class name
    class_type_or_name.SetName(ConstString(class_name));
    
    // Set the address
    address.SetRawAddress(obj_addr);
    
    // Set value type
    value_type = Value::ValueType::LoadAddress;
    
    return true;
  }
  
  return false;
}
```

### Phase 4: Testing and Validation (2 hours)
**Priority**: CRITICAL
**Risk**: LOW
**Impact**: HIGH

#### 4.1 Create Comprehensive Test Script (30 minutes)

**File**: `lldb/examples/test_formatter_fixes.lldb`

```lldb
# Test script for formatter fixes
file formatter_integration_test
b formatter_integration_test.m:180
run

# Test 1: Array first element
po programmingLanguages
# EXPECTED: @["Objective-C", "Swift", "Python"]
# NOT: @[<NSConstantString>, "Swift", "Python"]

# Test 2: Custom class string ivars  
po account
# EXPECTED: _accountNumber="ACC-001", _ownerName="John Doe"
# NOT: _accountNumber=<invalid object>

# Test 3: Dictionary keys
po personInfo
# EXPECTED: {name = "John Doe"; occupation = "Developer"}
# NOT: {namerr = ...; name = ...}

# Test 4: ISA display
frame variable account
# EXPECTED: (BankAccount *) account = ...
# NOT: (<unknown type> *) account = ...

continue
```

#### 4.2 Memory Validation Tests (30 minutes)

```lldb
# Validate memory layout
memory read -fx -c32 account
# Check ISA at offset 0
# Check string pointers at offsets 8, 16

# Validate string content
script
import lldb
process = lldb.debugger.GetSelectedTarget().GetProcess()
thread = process.GetSelectedThread()
frame = thread.GetSelectedFrame()
account = frame.FindVariable("account")
print(f"Account: {account.GetSummary()}")
```

#### 4.3 Performance Validation (30 minutes)

```python
# Performance test script
import time
import lldb

def test_formatter_performance():
    target = lldb.debugger.GetSelectedTarget()
    process = target.GetProcess()
    thread = process.GetSelectedThread()
    frame = thread.GetSelectedFrame()
    
    # Test array formatting speed
    start = time.time()
    for i in range(100):
        array = frame.FindVariable("programmingLanguages")
        summary = array.GetSummary()
    elapsed = time.time() - start
    print(f"Array formatting: {elapsed/100*1000:.2f}ms per operation")
    
    # Test dictionary formatting speed
    start = time.time()
    for i in range(100):
        dict_var = frame.FindVariable("personInfo")
        summary = dict_var.GetSummary()
    elapsed = time.time() - start
    print(f"Dictionary formatting: {elapsed/100*1000:.2f}ms per operation")

test_formatter_performance()
```

#### 4.4 Regression Tests (30 minutes)

Ensure existing functionality still works:
- NSNumber formatting
- NSSet formatting
- Nested collections
- Tagged pointers
- Empty collections
- Nil objects

## Build and Deployment

### Build Commands
```bash
cd /home/robk/code/llvm-project/build
ninja lldbPluginGNUstepObjCRuntime -j$(nproc)
```

### Test Execution
```bash
cd /home/robk/code/llvm-project/lldb/examples
./test_formatter_fixes.sh
```

## Risk Mitigation

### Rollback Plan
1. Keep backup of original files
2. Git commit before changes
3. Test in isolated environment first

### Error Handling
- Add null checks for all pointer operations
- Validate memory reads before use
- Provide meaningful fallback values
- Log errors for debugging

## Success Criteria

### Functional Requirements
- [ ] Array first element shows actual string value
- [ ] Custom class string ivars display correctly
- [ ] Dictionary keys are not corrupted
- [ ] ISA shows actual class name

### Performance Requirements
- [ ] All formatters complete in <50ms
- [ ] No memory leaks
- [ ] No crashes on invalid input

### Quality Requirements
- [ ] All existing tests pass
- [ ] New tests added for fixes
- [ ] Code follows LLVM style
- [ ] Comments explain complex logic

## Timeline

| Phase | Task | Duration | Dependencies |
|-------|------|----------|-------------|
| 1 | String extraction fixes | 2 hours | None |
| 2 | Object ivar resolution | 1 hour | Phase 1 |
| 3 | ISA display resolution | 1 hour | Phase 2 |
| 4 | Testing and validation | 2 hours | Phase 3 |
| 5 | Documentation update | 30 minutes | Phase 4 |
| **Total** | **Complete fix implementation** | **6.5 hours** | |

## Post-Implementation

### Documentation Updates
1. Update CLAUDE.md with fixed issues
2. Update formatter documentation
3. Add test case documentation
4. Update backlog status

### Monitoring
1. Watch for user reports of formatting issues
2. Monitor performance metrics
3. Check for memory usage patterns
4. Validate with real-world code

## Conclusion

This implementation plan provides a clear, step-by-step approach to fixing all four remaining critical issues in the GNUstep LLDB formatters. The fixes are well-understood, low-risk, and can be implemented incrementally with validation at each step.

The total estimated time of 6.5 hours includes implementation, testing, and documentation. Each fix is independent, allowing for parallel development if multiple developers are available.

Success will be measured by all test cases passing and user confirmation that the debugging experience matches expectations for GNUstep development.