# GNUstep LLDB Integration - Task Plan & Status

## 🎯 **Project Objective**
Finalize GNUstep/libobjc2 debugger integration for Objective-C on Windows/Linux to create a minimal release ready for LLVM project PR submission.

## ✅ **COMPLETED TASKS**

### Task 01: Language Type Handling ✅
- **Status**: COMPLETE
- **What was done**: Fixed NSUInteger type mapping from `L` to `Q` for Windows x64
- **Location**: `GNUstepObjCDeclVendor.cpp` - type encoding corrections
- **Result**: Basic type introspection now works correctly

### Task 02: ObjC Literals Detection ✅  
- **Status**: COMPLETE
- **What was done**: Enhanced literal detection in expression evaluation pipeline
- **Location**: Various runtime components
- **Result**: String literals (@"Hello") now work properly

### Task 03: Windows Calling Conventions ✅
- **Status**: COMPLETE  
- **What was done**: Implemented proper x64 calling convention handling
- **Location**: Runtime introspection components
- **Result**: Method calls work correctly on Windows

### Task 04: CFStringCreateWithBytes Implementation ✅
- **Status**: COMPLETE
- **What was done**: Created CFString fallback utility function with full AST body
- **Location**: `GNUstepObjCDeclVendor.cpp` - `DefineCFStringCreateWithBytesBody()`
- **Result**: String literals (@"Test") evaluate successfully

## 🔧 **CURRENT ISSUE: ARCHITECTURAL PROBLEM IDENTIFIED**

### **Root Cause Analysis**
Our implementation has a **fundamental architectural flaw**:

1. **❌ Fighting Symptoms**: We maintain 500+ lines of hardcoded method signatures
2. **❌ Manual Type Encoding**: Hand-maintaining Windows/Linux type differences  
3. **❌ Missing Metaclass Support**: Not discovering class methods properly
4. **❌ Ignoring Runtime**: Not using GNUstep's built-in introspection capabilities

### **What Should Work But Doesn't**
- ✅ String literals: `@"Hello"` 
- ✅ Method calls: `[greeting length]`
- ❌ Array literals: `@[@"test1", @"test2"]`
- ❌ Dictionary literals: `@{@"key": @"value"}`
- ❌ Class method calls: `[NSArray arrayWithObjects:...]`

## 🚨 **CRITICAL DISCOVERY: WE HAVE ALL NEEDED RUNTIME FUNCTIONS**

Analysis of GNUstep libobjc2 runtime.h shows we have:
```c
Method * class_copyMethodList(Class cls, unsigned int *outCount);
const char * method_getTypeEncoding(Method method);
Class objc_getMetaClass(const char *name);        // ← KEY MISSING PIECE
Method class_getClassMethod(Class aClass, SEL aSelector);
SEL sel_registerName(const char *selName);
```

**The real problem**: We're not using `objc_getMetaClass()` to discover class methods!

## 📋 **IMMEDIATE NEXT TASKS**

### Task 05: Fix Metaclass Method Discovery 🔥 **HIGH PRIORITY**
- **Problem**: Array/dictionary literals fail because we can't find `arrayWithObjects:count:`
- **Root Cause**: We only query instance methods, not class methods from metaclass
- **Solution**: Use `objc_getMetaClass("NSArray")` + `class_copyMethodList()`
- **Impact**: Will fix array/dictionary literals completely

### Task 06: Eliminate Hardcoded Method Tables 🔥 **ARCHITECTURAL**  
- **Problem**: 500+ lines of manually maintained method signatures
- **Solution**: Replace with pure runtime introspection using `class_copyMethodList()`
- **Benefit**: Automatic type encoding discovery, no manual maintenance

### Task 07: Trust Runtime Type Encodings 🔥 **SIMPLIFICATION**
- **Problem**: Manual type encoding fixes (e.g., `@32@0:8r^@16Q24`)
- **Solution**: Use `method_getTypeEncoding()` directly from runtime
- **Benefit**: No more Windows/Linux encoding differences to maintain

## 🧪 **DIAGNOSTIC TEST COMMANDS**

### Current State Verification:
```bash
# Test metaclass discovery
source /c/code/llvm-project/debug_setup.sh && cd c:/code/llvm-project/build && /ucrt64/bin/lldb.exe /c/code/llvm-project/lldb/examples/simple_test.exe -o "b main" -o "run" -o "expr (void*)objc_getMetaClass(\"NSArray\")" -o "quit"

# Test class method detection  
source /c/code/llvm-project/debug_setup.sh && cd c:/code/llvm-project/build && /ucrt64/bin/lldb.exe /c/code/llvm-project/lldb/examples/simple_test.exe -o "b main" -o "run" -o "expr (void*)class_getClassMethod((Class)objc_getMetaClass(\"NSArray\"), (SEL)sel_registerName(\"arrayWithObjects:count:\"))" -o "quit"

# Test array literals (currently failing)
source /c/code/llvm-project/debug_setup.sh && cd c:/code/llvm-project/build && /ucrt64/bin/lldb.exe /c/code/llvm-project/lldb/examples/simple_test.exe -o "b main" -o "run" -o "expr @[@\"test1\", @\"test2\"]" -o "quit"
```

## 📁 **KEY FILES TO UNDERSTAND**

### Primary Implementation:
- `GNUstepObjCDeclVendor.cpp` - Main declaration vendor (1712 lines)
- `GNUstepObjCRuntime.cpp` - Runtime plugin core
- `GNUstepRuntimeV2API.h` - Runtime API interface

### Method Discovery Logic:
- Line 1020: `GetAllMethodsIncludingInherited()` - Only gets instance methods
- Line 1604: `GetObjectClass()` - Attempts metaclass but incomplete
- Lines 466-520: Hardcoded method signature tables (TO BE ELIMINATED)

### Critical Functions:
- `FinishDecl()` - Where class interfaces are populated
- `CreateMethodDecl()` - Where method declarations are created
- `EnsureRuntimeDecls()` - Runtime function declarations

## 🔄 **ARCHITECTURAL COMPARISON**

### Apple LLDB (Simple & Correct):
```cpp
// Pseudocode - what Apple does
Class metaclass = objc_getMetaClass("NSArray");
Method *methods = class_copyMethodList(metaclass, &count);
for (each method) {
    const char *encoding = method_getTypeEncoding(method);
    // Create method declaration using runtime encoding
}
```

### Our Current Implementation (Complex & Fragile):
```cpp
// What we do now - 500+ lines of hardcoded tables
static const FoundationMethodSignature NSArray_methods[] = {
  {"arrayWithObjects:count:", "@32@0:8r^@16Q24", false},  // Manual encoding!
  // ... hundreds more
};
```

## 🎯 **SUCCESS CRITERIA**

When complete, these should all work:
```objc
@"Hello"                           // ✅ Already works
[greeting length]                  // ✅ Already works  
@[@"test1", @"test2"]             // ❌ Target to fix
@{@"key": @"value"}               // ❌ Target to fix
[NSArray arrayWithObjects:...]     // ❌ Target to fix
```

## 📊 **PROGRESS TRACKING**

- **Overall Progress**: 60% complete
- **Core Runtime**: ✅ Working
- **String Literals**: ✅ Working  
- **Method Calls**: ✅ Working
- **Class Method Discovery**: ❌ Broken (next target)
- **Array/Dict Literals**: ❌ Blocked on class methods

## 🚀 **NEXT SESSION PRIORITIES**

1. **Immediate**: Fix metaclass method discovery (Task 05)
2. **Architectural**: Replace hardcoded tables with runtime introspection (Task 06)  
3. **Validation**: Comprehensive testing of all literal types
4. **Polish**: Code cleanup and documentation for PR submission

---

**Last Updated**: 2025-09-01
**Status**: Ready for Task 05 - Metaclass Method Discovery
