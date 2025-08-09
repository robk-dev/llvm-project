# CallRuntimeFunction Implementation Specification
## GNUstep LLDB Plugin Runtime Function Calling

**Date**: 2025-08-08  
**Author**: Agent Alpha (Analyst)  
**Status**: Implementation Blueprint  
**Priority**: CRITICAL - Blocking ISA Resolution and Custom Class Support

---

## Executive Summary

The `CallRuntimeFunction()` method in GNUstepObjCRuntimeIntrospector is currently a stub returning `LLDB_INVALID_ADDRESS`. This prevents critical functionality including:
- Runtime class lookup via `objc_lookup_class`
- ISA resolution for custom objects
- Dynamic property introspection
- Method invocation for debugging

This specification provides a complete implementation blueprint based on proven patterns from Apple's ObjC runtime, adapted for GNUstep's libobjc2.

---

## 1. Architecture Overview

### 1.1 LLDB Function Calling Stack

```
┌─────────────────────────────────────┐
│     GNUstepObjCRuntimeIntrospector  │ <- Our implementation layer
├─────────────────────────────────────┤
│         UtilityFunction              │ <- Code generation
├─────────────────────────────────────┤
│         FunctionCaller               │ <- Function wrapper
├─────────────────────────────────────┤
│     ThreadPlanCallFunction           │ <- Execution control
├─────────────────────────────────────┤
│      Process/Thread/Target           │ <- LLDB core infrastructure
└─────────────────────────────────────┘
```

### 1.2 Key Components

1. **UtilityFunction**: Generates wrapper code for calling runtime functions
2. **FunctionCaller**: Manages argument marshalling and return values
3. **ThreadPlanCallFunction**: Controls execution flow on target thread
4. **ExecutionContext**: Provides process/thread/frame context for execution

---

## 2. Implementation Pattern Analysis

### 2.1 Apple's Implementation Pattern

From analysis of `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/AppleObjCRuntime/AppleObjCRuntime.cpp`:

```cpp
// Apple's pattern for calling runtime functions:
// 1. Get or create a FunctionCaller
m_print_object_caller_up.reset(
    exe_scope->CalculateTarget()->GetFunctionCallerForLanguage(
        eLanguageTypeObjC, return_compiler_type, *function_address,
        arg_value_list, "objc-object-description", error));

// 2. Insert the function into the target
m_print_object_caller_up->InsertFunction(exe_ctx, wrapper_struct_addr,
                                         diagnostics);

// 3. Set execution options
EvaluateExpressionOptions options;
options.SetUnwindOnError(true);
options.SetTryAllThreads(true);
options.SetStopOthers(true);
options.SetIgnoreBreakpoints(true);
options.SetTimeout(process->GetUtilityExpressionTimeout());
options.SetIsForUtilityExpr(true);

// 4. Execute the function
ExpressionResults results = m_print_object_caller_up->ExecuteFunction(
    exe_ctx, &wrapper_struct_addr, options, diagnostics, ret);
```

### 2.2 UtilityFunction Pattern

From `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/AppleObjCRuntime/AppleObjCRuntimeV2.cpp`:

```cpp
// Creating utility functions for runtime introspection:
auto utility_fn_or_error = exe_ctx.GetTargetRef().CreateUtilityFunction(
    std::move(code), std::move(name), eLanguageTypeC, exe_ctx);

// Making a function caller from utility function:
utility_fn->MakeFunctionCaller(clang_uint32_t_type, arguments,
                              exe_ctx.GetThreadSP(), error);
```

---

## 3. GNUstep CallRuntimeFunction Implementation

### 3.1 Header Modifications

**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.h`

```cpp
class GNUstepObjCRuntimeIntrospector : public ObjCRuntimeIntrospector {
private:
  // Cache for function callers to avoid repeated compilation
  struct FunctionCallerCache {
    std::unique_ptr<FunctionCaller> objc_lookup_class_caller;
    std::unique_ptr<FunctionCaller> class_getName_caller;
    std::unique_ptr<FunctionCaller> object_getClass_caller;
    std::unique_ptr<FunctionCaller> class_getSuperclass_caller;
  };
  
  mutable FunctionCallerCache m_function_cache;
  
  // Helper methods for function calling
  lldb::addr_t CallRuntimeFunctionImpl(
      const char *function_name,
      const CompilerType &return_type,
      const ValueList &args,
      ExecutionContext &exe_ctx,
      Status &error) const;
      
  std::unique_ptr<FunctionCaller> GetOrCreateFunctionCaller(
      const char *function_name,
      const CompilerType &return_type,
      const ValueList &arg_types,
      ExecutionContext &exe_ctx,
      Status &error) const;
      
  bool SetupExecutionContext(ExecutionContext &exe_ctx) const;
  
public:
  // Main interface (replace stub)
  lldb::addr_t CallRuntimeFunction(const char *function_name,
                                   lldb::addr_t arg) override;
                                   
  // Extended interface for multiple arguments
  lldb::addr_t CallRuntimeFunction(const char *function_name,
                                   const ValueList &args,
                                   const CompilerType &return_type);
};
```

### 3.2 Core Implementation

**File**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.cpp`

```cpp
#include "lldb/Expression/FunctionCaller.h"
#include "lldb/Expression/UtilityFunction.h"
#include "lldb/Expression/DiagnosticManager.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/ThreadPlanCallFunction.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

lldb::addr_t GNUstepObjCRuntimeIntrospector::CallRuntimeFunction(
    const char *function_name, lldb::addr_t arg) {
  
  if (!m_process || !function_name) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Setup execution context
  ExecutionContext exe_ctx;
  if (!SetupExecutionContext(exe_ctx)) {
    LLDB_LOG(GetLog(LLDBLog::Language),
             "[GNUstep] Failed to setup execution context for {0}",
             function_name);
    return LLDB_INVALID_ADDRESS;
  }
  
  // Get scratch type system for argument and return types
  TypeSystemClangSP scratch_ts_sp = 
      ScratchTypeSystemClang::GetForTarget(exe_ctx.GetTargetRef());
  if (!scratch_ts_sp) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Setup argument value
  ValueList args;
  Value arg_value;
  
  // For objc_lookup_class, argument is a C string pointer
  // For other functions, might be an object pointer
  if (strcmp(function_name, "objc_lookup_class") == 0) {
    // Argument is a const char* (C string)
    CompilerType char_ptr_type = scratch_ts_sp->GetCStringType(true);
    arg_value.SetValueType(Value::ValueType::HostAddress);
    arg_value.SetCompilerType(char_ptr_type);
  } else {
    // Argument is a pointer (void* or id)
    CompilerType void_ptr_type = 
        scratch_ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType();
    arg_value.SetValueType(Value::ValueType::Scalar);
    arg_value.SetCompilerType(void_ptr_type);
  }
  
  arg_value.GetScalar() = arg;
  args.PushValue(arg_value);
  
  // Return type is typically a pointer
  CompilerType return_type = 
      scratch_ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType();
  
  // Call the implementation
  Status error;
  lldb::addr_t result = CallRuntimeFunctionImpl(
      function_name, return_type, args, exe_ctx, error);
      
  if (error.Fail()) {
    LLDB_LOG(GetLog(LLDBLog::Language),
             "[GNUstep] Failed to call {0}: {1}",
             function_name, error.AsCString());
    return LLDB_INVALID_ADDRESS;
  }
  
  return result;
}

lldb::addr_t GNUstepObjCRuntimeIntrospector::CallRuntimeFunctionImpl(
    const char *function_name,
    const CompilerType &return_type,
    const ValueList &args,
    ExecutionContext &exe_ctx,
    Status &error) const {
    
  // Get or create the function caller
  std::unique_ptr<FunctionCaller> &caller = 
      GetOrCreateFunctionCaller(function_name, return_type, args, 
                                exe_ctx, error);
  if (!caller || error.Fail()) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Prepare for execution
  DiagnosticManager diagnostics;
  lldb::addr_t wrapper_struct_addr = LLDB_INVALID_ADDRESS;
  
  // Insert function arguments
  if (!caller->WriteFunctionArguments(exe_ctx, wrapper_struct_addr, 
                                      args, diagnostics)) {
    error = Status::FromError(diagnostics.GetAsError(
        lldb::eExpressionSetupError,
        "Failed to write function arguments"));
    return LLDB_INVALID_ADDRESS;
  }
  
  // Setup execution options
  EvaluateExpressionOptions options;
  options.SetUnwindOnError(true);
  options.SetTryAllThreads(false); // Use current thread
  options.SetStopOthers(true);
  options.SetIgnoreBreakpoints(true);
  options.SetTimeout(std::chrono::milliseconds(1000)); // 1 second timeout
  options.SetIsForUtilityExpr(true);
  
  // Execute the function
  Value result_value;
  ExpressionResults results = caller->ExecuteFunction(
      exe_ctx, &wrapper_struct_addr, options, diagnostics, result_value);
      
  // Clean up arguments
  if (wrapper_struct_addr != LLDB_INVALID_ADDRESS) {
    caller->DeallocateFunctionResults(exe_ctx, wrapper_struct_addr);
  }
  
  // Check execution results
  if (results != eExpressionCompleted) {
    error = Status::FromError(diagnostics.GetAsError(
        lldb::eExpressionParseError,
        "Function execution failed"));
    return LLDB_INVALID_ADDRESS;
  }
  
  // Extract return value
  lldb::addr_t return_addr = result_value.GetScalar().ULongLong(
      LLDB_INVALID_ADDRESS);
      
  LLDB_LOG(GetLog(LLDBLog::Language),
           "[GNUstep] Called {0}({1:x}) = {2:x}",
           function_name, args.GetValueAtIndex(0)->GetScalar().ULongLong(),
           return_addr);
           
  return return_addr;
}

std::unique_ptr<FunctionCaller>& 
GNUstepObjCRuntimeIntrospector::GetOrCreateFunctionCaller(
    const char *function_name,
    const CompilerType &return_type,
    const ValueList &arg_types,
    ExecutionContext &exe_ctx,
    Status &error) const {
    
  // Check cache first
  std::unique_ptr<FunctionCaller> *cached_caller = nullptr;
  
  if (strcmp(function_name, "objc_lookup_class") == 0) {
    cached_caller = &m_function_cache.objc_lookup_class_caller;
  } else if (strcmp(function_name, "class_getName") == 0) {
    cached_caller = &m_function_cache.class_getName_caller;
  } else if (strcmp(function_name, "object_getClass") == 0) {
    cached_caller = &m_function_cache.object_getClass_caller;
  } else if (strcmp(function_name, "class_getSuperclass") == 0) {
    cached_caller = &m_function_cache.class_getSuperclass_caller;
  }
  
  // Return cached caller if available
  if (cached_caller && *cached_caller) {
    return *cached_caller;
  }
  
  // Resolve function address
  Address function_address;
  const Symbol *symbol = nullptr;
  
  // Try libobjc2 first
  ModuleSP objc_module = GetObjCModule();
  if (objc_module) {
    symbol = objc_module->FindFirstSymbolWithNameAndType(
        ConstString(function_name), eSymbolTypeCode);
  }
  
  // Fallback to Foundation if not found
  if (!symbol) {
    ModuleSP foundation_module = GetFoundationModule();
    if (foundation_module) {
      symbol = foundation_module->FindFirstSymbolWithNameAndType(
          ConstString(function_name), eSymbolTypeCode);
    }
  }
  
  if (!symbol) {
    error = Status::FromErrorStringWithFormat(
        "Could not find symbol for function '%s'", function_name);
    return *cached_caller; // Return empty unique_ptr
  }
  
  function_address = symbol->GetAddress();
  
  // Create the function caller
  std::string caller_name = std::string(function_name) + "_caller";
  std::unique_ptr<FunctionCaller> new_caller(
      exe_ctx.GetTargetRef().GetFunctionCallerForLanguage(
          eLanguageTypeC, return_type, function_address,
          arg_types, caller_name.c_str(), error));
          
  if (error.Fail() || !new_caller) {
    return *cached_caller; // Return empty unique_ptr
  }
  
  // Compile the wrapper function
  DiagnosticManager diagnostics;
  ThreadSP thread_sp = exe_ctx.GetThreadSP();
  
  unsigned num_errors = new_caller->CompileFunction(thread_sp, diagnostics);
  if (num_errors > 0) {
    error = Status::FromError(diagnostics.GetAsError(
        lldb::eExpressionParseError,
        "Failed to compile function wrapper"));
    return *cached_caller;
  }
  
  // Insert the wrapper into the target
  if (!new_caller->WriteFunctionWrapper(exe_ctx, diagnostics)) {
    error = Status::FromError(diagnostics.GetAsError(
        lldb::eExpressionSetupError,
        "Failed to insert function wrapper"));
    return *cached_caller;
  }
  
  // Cache and return
  if (cached_caller) {
    *cached_caller = std::move(new_caller);
    return *cached_caller;
  }
  
  // For non-cached functions, we need to store it somewhere
  // This is a simplified approach - in production, use a map
  static std::unique_ptr<FunctionCaller> temp_caller;
  temp_caller = std::move(new_caller);
  return temp_caller;
}

bool GNUstepObjCRuntimeIntrospector::SetupExecutionContext(
    ExecutionContext &exe_ctx) const {
    
  if (!m_process) {
    return false;
  }
  
  // Get a thread suitable for expression execution
  ThreadSP thread_sp = m_process->GetThreadList()
      .GetExpressionExecutionThread();
  if (!thread_sp) {
    // Fallback to selected thread
    thread_sp = m_process->GetThreadList().GetSelectedThread();
  }
  
  if (!thread_sp) {
    return false;
  }
  
  // Ensure thread is stopped and safe for function calls
  if (!thread_sp->SafeToCallFunctions()) {
    LLDB_LOG(GetLog(LLDBLog::Language),
             "[GNUstep] Thread not safe for function calls");
    return false;
  }
  
  // Build execution context
  thread_sp->CalculateExecutionContext(exe_ctx);
  
  // Ensure we have a frame
  if (!exe_ctx.GetFramePtr()) {
    StackFrameSP frame_sp = thread_sp->GetSelectedFrame(
        DoNoSelectMostRelevantFrame);
    if (!frame_sp) {
      frame_sp = thread_sp->GetStackFrameAtIndex(0);
    }
    exe_ctx.SetFrameSP(frame_sp);
  }
  
  return exe_ctx.HasThreadScope() && exe_ctx.HasProcessScope();
}
```

### 3.3 Integration with ISA Resolution

Update the ISA resolution to use the new CallRuntimeFunction:

```cpp
std::string GNUstepObjCRuntimeIntrospector::GetClassName(
    lldb::addr_t object_addr) {
    
  if (!m_process || object_addr == 0 || 
      object_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // First, try direct memory reading (fast path)
  std::string class_name = GetClassNameDirect(object_addr);
  if (!class_name.empty()) {
    return class_name;
  }
  
  // Fallback to runtime function call (slow path but more reliable)
  // Call object_getClass(object) to get the class pointer
  lldb::addr_t class_addr = CallRuntimeFunction("object_getClass", 
                                                object_addr);
  if (class_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Call class_getName(class) to get the name
  lldb::addr_t name_addr = CallRuntimeFunction("class_getName", 
                                               class_addr);
  if (name_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read the C string from memory
  char buffer[256];
  Status error;
  size_t bytes_read = m_process->ReadCStringFromMemory(
      name_addr, buffer, sizeof(buffer), error);
      
  if (error.Success() && bytes_read > 0) {
    return std::string(buffer);
  }
  
  return "";
}
```

---

## 4. Alternative Approach: Using UtilityFunction

For more complex runtime introspection, we can use UtilityFunction to inject custom code:

```cpp
llvm::Expected<std::unique_ptr<UtilityFunction>>
GNUstepObjCRuntimeIntrospector::CreateClassInfoExtractor(
    ExecutionContext &exe_ctx) {
    
  // Generate C code that calls multiple runtime functions
  std::string extractor_code = R"(
    extern "C" {
      void *objc_lookup_class(const char *name);
      const char *class_getName(void *cls);
      void *class_getSuperclass(void *cls);
      
      struct ClassInfo {
        void *isa;
        void *superclass;
        const char *name;
        const char *super_name;
      };
      
      void __lldb_gnustep_get_class_info(const char *class_name,
                                         struct ClassInfo *info) {
        if (!class_name || !info) return;
        
        void *cls = objc_lookup_class(class_name);
        if (!cls) return;
        
        info->isa = cls;
        info->name = class_getName(cls);
        info->superclass = class_getSuperclass(cls);
        if (info->superclass) {
          info->super_name = class_getName(info->superclass);
        }
      }
    }
  )";
  
  return exe_ctx.GetTargetRef().CreateUtilityFunction(
      std::move(extractor_code),
      "__lldb_gnustep_class_info_extractor",
      eLanguageTypeC,
      exe_ctx);
}
```

---

## 5. Error Handling and Edge Cases

### 5.1 Thread Safety
- Ensure process is stopped before calling functions
- Use appropriate thread for execution
- Handle thread state restoration after calls

### 5.2 Memory Management
- Properly allocate and deallocate wrapper structures
- Clean up after function execution
- Handle out-of-memory conditions

### 5.3 Symbol Resolution
- Try multiple modules (libobjc2, Foundation)
- Handle mangled vs unmangled symbols
- Fallback strategies for missing symbols

### 5.4 Timeout Protection
- Set reasonable timeouts (1 second default)
- Allow user configuration of timeout values
- Graceful handling of timeout conditions

---

## 6. Testing Strategy

### 6.1 Unit Tests

```cpp
TEST(GNUstepIntrospector, CallRuntimeFunction_ObjcLookupClass) {
  // Setup mock process and runtime
  MockProcess process;
  GNUstepObjCRuntimeIntrospector introspector(&process);
  
  // Test successful lookup
  lldb::addr_t class_addr = introspector.CallRuntimeFunction(
      "objc_lookup_class", (lldb::addr_t)"NSString");
  EXPECT_NE(class_addr, LLDB_INVALID_ADDRESS);
  
  // Test failed lookup
  lldb::addr_t invalid_class = introspector.CallRuntimeFunction(
      "objc_lookup_class", (lldb::addr_t)"NonExistentClass");
  EXPECT_EQ(invalid_class, 0);
}
```

### 6.2 Integration Tests

Test with real GNUstep binaries:
1. Load `/home/robk/code/llvm-project/lldb/examples/custom_class_test`
2. Set breakpoint after object creation
3. Call `po account` to trigger ISA resolution
4. Verify correct class name extraction

### 6.3 Performance Tests

Measure function call overhead:
- Target: <50ms per runtime function call
- Batch operations should use caching
- Monitor memory usage during repeated calls

---

## 7. Files to Modify

### Core Implementation Files
1. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.h`
   - Add function caller cache
   - Declare new methods

2. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.cpp`
   - Implement CallRuntimeFunction
   - Add helper methods
   - Update GetClassName to use runtime calls

### Supporting Files
3. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`
   - Ensure module references are available
   - Add logging for debugging

4. `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/CMakeLists.txt`
   - Add any new dependencies if needed

---

## 8. Risk Assessment

### High Risk Items
1. **Symbol Resolution Failures**
   - Mitigation: Multiple fallback strategies
   - Test with stripped and unstripped binaries

2. **Thread State Corruption**
   - Mitigation: Proper state save/restore
   - Use LLDB's built-in mechanisms

3. **Memory Leaks**
   - Mitigation: RAII patterns, proper cleanup
   - Valgrind testing

### Medium Risk Items
1. **Performance Impact**
   - Mitigation: Caching, lazy evaluation
   - Profile common use cases

2. **Platform Differences**
   - Mitigation: Test on multiple platforms
   - Abstract platform-specific code

---

## 9. Implementation Timeline

### Phase 1: Basic Infrastructure (2 days)
- Setup ExecutionContext handling
- Implement basic CallRuntimeFunction
- Add logging and error handling

### Phase 2: Function Caller Cache (1 day)
- Implement caching mechanism
- Add common runtime functions
- Optimize repeated calls

### Phase 3: Integration (2 days)
- Update ISA resolution
- Fix custom class support
- Update formatters to use new capability

### Phase 4: Testing and Refinement (2 days)
- Comprehensive testing
- Performance optimization
- Documentation

---

## 10. Success Criteria

### Functional Requirements
- ✅ `objc_lookup_class` works correctly
- ✅ `class_getName` returns valid strings
- ✅ Custom classes (BankAccount) resolve properly
- ✅ No crashes with invalid inputs

### Performance Requirements
- ✅ <50ms per function call
- ✅ Caching reduces repeated call overhead by 90%
- ✅ Memory usage <10MB for cache

### Quality Requirements
- ✅ 95% test coverage
- ✅ No memory leaks
- ✅ Thread-safe implementation
- ✅ Proper error handling

---

## Appendix A: Key LLDB APIs

### Required Headers
```cpp
#include "lldb/Expression/FunctionCaller.h"
#include "lldb/Expression/UtilityFunction.h"
#include "lldb/Expression/DiagnosticManager.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Core/Value.h"
#include "lldb/Core/ValueList.h"
```

### Key Classes
- `FunctionCaller`: Manages function execution
- `UtilityFunction`: Generates wrapper code
- `ExecutionContext`: Provides execution environment
- `DiagnosticManager`: Collects errors and warnings
- `EvaluateExpressionOptions`: Controls execution behavior

---

## Appendix B: GNUstep Runtime Functions

### Essential Functions
```c
// Class lookup
Class objc_lookup_class(const char *name);
Class objc_getClass(const char *name);

// Class introspection  
const char *class_getName(Class cls);
Class class_getSuperclass(Class cls);
BOOL class_isMetaClass(Class cls);

// Object introspection
Class object_getClass(id obj);
const char *object_getClassName(id obj);

// Method introspection
Method *class_copyMethodList(Class cls, unsigned int *outCount);
const char *sel_getName(SEL sel);
```

### Memory Layout Constants
```c
// Object structure offsets
#define ISA_OFFSET 0  // First field in object
#define CLASS_NAME_OFFSET 16  // Offset to name in class struct (verify)
#define SUPERCLASS_OFFSET 8   // Offset to superclass pointer
```

---

## Appendix C: References

1. LLDB Source Code:
   - Apple ObjC Runtime: `lldb/source/Plugins/LanguageRuntime/ObjC/AppleObjCRuntime/`
   - Expression Evaluation: `lldb/source/Expression/`
   - Target Infrastructure: `lldb/source/Target/`

2. GNUstep Runtime:
   - libobjc2 source: `/home/robk/code/llvm-project/lldb/libobjc2/`
   - Runtime headers: `/usr/local/include/objc/`

3. LLDB Documentation:
   - Expression evaluation: https://lldb.llvm.org/use/expression.html
   - Plugin development: https://lldb.llvm.org/resources/plugins.html

---

*End of Specification*