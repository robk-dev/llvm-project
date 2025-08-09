# Implementation Requirements for GNUstep LLDB Plugin Issues

## Issue 1: Dictionary Formatter Display Enhancement

### Current Behavior
```lldb
(lldb) v personInfo
personInfo = 2 key/value pairs @{...}
  count = 2
  [0].key = "name"
  [0].value = "John Doe2"
  [1].key = "occupation"
  [1].value = "Developer"
```

### Desired Behavior
```lldb
(lldb) v personInfo
personInfo = 2 key/value pairs @{name = "John Doe2", occupation = "Developer"}
  name = "John Doe2"
  occupation = "Developer"
```

### Implementation Requirements

#### 1. Modify Synthetic Children Naming
**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDictionaryFormatters.cpp`

**Changes Required at lines 1045-1051**:
```cpp
// CURRENT CODE:
StreamString name_stream;
if (is_key) {
  name_stream.Printf("[%zu].key", pair_idx);
} else {
  name_stream.Printf("[%zu].value", pair_idx);
}

// REQUIRED CHANGE:
StreamString name_stream;
if (is_key) {
  // For keys, we want to show them as direct children
  // Read the key object to get its string representation
  std::string key_name = ExtractKeyName(pair.key_addr);
  if (!key_name.empty()) {
    name_stream.Printf("%s", key_name.c_str());
  } else {
    name_stream.Printf("[%zu]", pair_idx);
  }
  // Store this for value pairing
  m_key_names[pair_idx] = key_name;
} else {
  // Skip value children - we'll show them inline with keys
  return nullptr;
}
```

#### 2. Implement Key-Value Pair Display
**New Method Required**:
```cpp
std::string GNUstepNSDictionarySyntheticProvider::ExtractKeyName(lldb::addr_t key_addr) {
  // Extract string representation of key for use as child name
  // Must handle NSString keys primarily
  // Should return clean string without quotes for use as identifier
}
```

#### 3. Restructure Children Calculation
```cpp
llvm::Expected<uint32_t> CalculateNumChildren() {
  // Return number of pairs + 1 for count (not 2x pairs)
  return static_cast<uint32_t>(m_pairs.size() + 1);
}
```

### Testing Requirements
1. Test with string keys
2. Test with NSNumber keys
3. Test with nested dictionaries
4. Test with nil values
5. Verify performance remains <50ms

---

## Issue 2: BankAccount ISA Lookup and Custom Class Introspection

### Current Behavior
```lldb
(lldb) po account
BankAccount(isa=unknown type, _accountNumber=invalid object, _ownerName=invalid object, _balance=1100.00, _transactions=invalid object)
```

### Desired Behavior
```lldb
(lldb) po account
<BankAccount: 0x555555872b70>
  accountNumber = "ACC-001"
  ownerName = "John Doe2"
  balance = 1100.00
  transactions = 3
```

### Implementation Requirements

#### 1. Fix ISA Resolution
**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.cpp`

**Enhancement at GetClassName() - line 61-112**:
```cpp
std::string GNUstepObjCRuntimeIntrospector::GetClassName(lldb::addr_t isa_addr) {
  // Add validation for ISA pointer
  if (isa_addr < 0x1000) { // Too low to be valid
    return "";
  }
  
  // For metaclasses, need to follow one more indirection
  Status error;
  lldb::addr_t potential_metaclass = m_process->ReadPointerFromMemory(isa_addr, error);
  if (!error.Fail() && potential_metaclass == isa_addr) {
    // This is a metaclass, the name is still at offset 2*address_size
  }
  
  // Continue with existing implementation...
}
```

#### 2. Implement Runtime Function Calls
**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepRuntimeV2API.cpp`

**Complete CallRuntimeFunction() - line 172-210**:
```cpp
lldb::addr_t GNUstepRuntimeV2API::CallRuntimeFunction(
    const std::string &function_name, const std::vector<lldb::addr_t> &args) {
  
  // Get execution context
  ExecutionContext exe_ctx = m_process->GetThreadList().GetSelectedThread()->GetExecutionContext();
  
  // Create expression to call runtime function
  StreamString expr;
  expr.Printf("((void*)%s)(", function_name.c_str());
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) expr.Printf(", ");
    expr.Printf("0x%" PRIx64, args[i]);
  }
  expr.Printf(")");
  
  // Evaluate expression
  EvaluateExpressionOptions options;
  options.SetIgnoreBreakpoints(true);
  options.SetUnwindOnError(true);
  options.SetTimeout(std::chrono::seconds(1));
  
  ValueObjectSP result_sp;
  ExpressionResults result = m_process->GetTarget().EvaluateExpression(
      expr.GetString(), exe_ctx.GetFrameSP().get(), result_sp, options);
  
  if (result == eExpressionCompleted && result_sp) {
    return result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
  }
  
  return LLDB_INVALID_ADDRESS;
}
```

#### 3. Add Ivar Extraction
**New Method in GNUstepObjCRuntimeIntrospector**:
```cpp
std::vector<IvarInfo> GNUstepObjCRuntimeIntrospector::GetIvarsForClass(lldb::addr_t class_addr) {
  std::vector<IvarInfo> ivars;
  
  // Use runtime API to get ivar list
  unsigned int count = 0;
  Ivar *ivar_list = m_runtime_api->class_copyIvarList(class_addr, &count);
  
  for (unsigned int i = 0; i < count; ++i) {
    IvarInfo info;
    info.name = m_runtime_api->ivar_getName(ivar_list[i]);
    info.type = m_runtime_api->ivar_getTypeEncoding(ivar_list[i]);
    info.offset = m_runtime_api->ivar_getOffset(ivar_list[i]);
    ivars.push_back(info);
  }
  
  if (ivar_list) {
    m_runtime_api->free(ivar_list);
  }
  
  return ivars;
}
```

### Testing Requirements
1. Test with BankAccount custom class
2. Test with inheritance hierarchies
3. Test with categories
4. Test with properties vs ivars
5. Verify no crashes on invalid ISA

---

## Issue 3: Runtime Symbol Linking

### Implementation Requirements

#### 1. Enhance Symbol Resolution
**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepRuntimeV2API.cpp`

**Improve ResolveRuntimeSymbol() - line 193**:
```cpp
lldb::addr_t GNUstepRuntimeV2API::ResolveRuntimeSymbol(const char *name) {
  // Try multiple resolution strategies
  
  // 1. Try symbol table first
  const Symbol *symbol = m_objc_module->FindFirstSymbolWithNameAndType(
      ConstString(name), eSymbolTypeCode);
  
  if (symbol) {
    lldb::addr_t addr = symbol->GetLoadAddress(&m_process->GetTarget());
    if (addr != LLDB_INVALID_ADDRESS) {
      return addr;
    }
  }
  
  // 2. Try dynamic loader
  ModuleSP module = m_objc_module;
  if (module) {
    SymbolContextList sc_list;
    module->FindFunctions(ConstString(name), CompilerDeclContext(), 
                          eFunctionNameTypeAuto, sc_list);
    if (sc_list.GetSize() > 0) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(0, sc);
      if (sc.symbol) {
        return sc.symbol->GetLoadAddress(&m_process->GetTarget());
      }
    }
  }
  
  // 3. Fall back to dlsym if available
  // This requires expression evaluation
  
  return LLDB_INVALID_ADDRESS;
}
```

#### 2. Add Runtime Validation
```cpp
bool GNUstepRuntimeV2API::ValidateRuntimeFunctions() {
  // Test that resolved functions actually work
  
  // Try to get NSObject class
  Class nsobject = m_runtime.objc_getClass("NSObject");
  if (!nsobject) {
    return false;
  }
  
  // Try to get class name
  const char *name = m_runtime.class_getName(nsobject);
  if (!name || strcmp(name, "NSObject") != 0) {
    return false;
  }
  
  return true;
}
```

### Environment Configuration Requirements

#### Launch Configuration Updates
**File**: `.vscode/launch.json` (or equivalent)
```json
{
  "environment": [
    {
      "name": "LD_LIBRARY_PATH",
      "value": "${workspaceFolder}/gnustep-install/lib:/usr/local/lib:/usr/lib/x86_64-linux-gnu:${env:LD_LIBRARY_PATH}"
    },
    {
      "name": "GNUSTEP_RUNTIME_ROOT",
      "value": "${workspaceFolder}/gnustep-install"
    }
  ]
}
```

---

## Priority Implementation Order

### Week 1
1. **Day 1-2**: Fix dictionary formatter display (Low complexity, High impact)
2. **Day 3-4**: Debug and fix ISA resolution (Medium complexity, High impact)
3. **Day 5**: Add runtime validation and logging

### Week 2
1. **Day 1-3**: Implement CallRuntimeFunction properly
2. **Day 4-5**: Add ivar extraction for custom classes
3. **Day 5**: Integration testing

### Success Metrics
- [ ] Dictionary shows concise key=value format
- [ ] BankAccount properties are visible
- [ ] Runtime functions return valid results
- [ ] No crashes during introspection
- [ ] Performance remains <50ms

---

## Code Review Checklist
- [ ] No memory leaks in string extraction
- [ ] Proper error handling for invalid pointers
- [ ] Logging added for debugging
- [ ] Tests cover edge cases
- [ ] Documentation updated
- [ ] Performance profiled

---
*Requirements Document Generated: 2025-08-08*
*Implementation Target: 2025-08-15*