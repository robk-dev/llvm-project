# 🎯 GNUstep LLDB Integration - Next Session Instructions

## 📋 **IMMEDIATE CONTEXT**

You are continuing work on **finalizing GNUstep/libobjc2 LLDB integration** for Objective-C debugging on Windows/Linux. This is a **mature project** nearing completion for LLVM project PR submission.

### **Current Status**: 60% Complete ✅
- ✅ String literals work: `@"Hello"`
- ✅ Method calls work: `[greeting length]`  
- ✅ Runtime introspection works
- ❌ Array literals fail: `@[@"test1", @"test2"]`
- ❌ Dictionary literals fail: `@{@"key": @"value"}`

## 🚨 **CRITICAL DISCOVERY MADE**

**ROOT CAUSE IDENTIFIED**: We have a **fundamental architectural flaw** - we're not using GNUstep's metaclass to discover class methods properly.

### **The Problem**:
Array/dictionary literals require class methods like `[NSArray arrayWithObjects:count:]`, but our code only discovers **instance methods**. We need **metaclass method discovery**.

### **The Solution**:
GNUstep runtime provides `objc_getMetaClass()` + `class_copyMethodList()` - we just need to use them correctly.

## 🔧 **IMMEDIATE TASK: Fix Metaclass Method Discovery**

### **What to Fix**:
File: `c:\code\llvm-project\lldb\source\Plugins\LanguageRuntime\ObjC\GNUstepObjCRuntime\GNUstepObjCDeclVendor.cpp`

**Current broken code** (around line 1020):
```cpp
// Only gets INSTANCE methods - missing CLASS methods!
auto methods_or_err = m_runtime_api->GetAllMethodsIncludingInherited(class_info.class_ptr);
```

**Need to add metaclass discovery**:
```cpp
// Get metaclass for class methods
Class metaclass = objc_getMetaClass(class_name.c_str());
Method *class_methods = class_copyMethodList(metaclass, &count);
// Process class methods and add to interface
```

### **Key Functions in GNUstep Runtime**:
```c
Class objc_getMetaClass(const char *name);           // Get metaclass  
Method * class_copyMethodList(Class cls, unsigned int *outCount);  // Get methods
const char * method_getTypeEncoding(Method method);   // Get type encoding
SEL method_getName(Method method);                    // Get selector name
```

## 📁 **KEY FILES TO WORK WITH**

### **Primary File**:
- `GNUstepObjCDeclVendor.cpp` (1712 lines) - Main implementation

### **Critical Functions to Modify**:
1. **`FinishDecl()`** (line ~1000) - Where class interfaces are populated
2. **`PopulateInterfaceFromRuntime()`** (line ~1543) - Runtime method discovery
3. **Hardcoded method tables** (lines 466-520) - THESE SHOULD BE ELIMINATED

### **Supporting Files**:
- `GNUstepRuntimeV2API.h` - Runtime API definitions
- `GNUstepObjCRuntime.cpp` - Runtime plugin core

## 🧪 **DIAGNOSTIC TEST COMMANDS** 

### **Environment Setup**:
```bash
# Always run this first
source /c/code/llvm-project/debug_setup.sh
cd c:/code/llvm-project/build
```

### **Test Metaclass Discovery**:
```bash
/ucrt64/bin/lldb.exe /c/code/llvm-project/lldb/examples/simple_test.exe -o "b main" -o "run" -o "expr (void*)objc_getMetaClass(\"NSArray\")" -o "quit"
```

### **Test Class Method Detection**:
```bash  
/ucrt64/bin/lldb.exe /c/code/llvm-project/lldb/examples/simple_test.exe -o "b main" -o "run" -o "expr (void*)class_getClassMethod((Class)objc_getMetaClass(\"NSArray\"), (SEL)sel_registerName(\"arrayWithObjects:count:\"))" -o "quit"
```

### **Test Array Literals** (should fail initially):
```bash
/ucrt64/bin/lldb.exe /c/code/llvm-project/lldb/examples/simple_test.exe -o "b main" -o "run" -o "expr @[@\"test1\", @\"test2\"]" -o "quit"
```

### **Rebuild After Changes**:
```bash
cd c:/code/llvm-project/build && ninja lldbPluginGNUstepObjCRuntime
```

## 🎯 **SUCCESS METRICS**

### **When Fixed, These Should Work**:
```objc
@"Hello"                           // ✅ Already works
[greeting length]                  // ✅ Already works
@[@"test1", @"test2"]             // 🎯 TARGET: Should work
@{@"key": @"value"}               // 🎯 TARGET: Should work  
[NSArray arrayWithObjects:...]     // 🎯 TARGET: Should work
```

## 📊 **IMPLEMENTATION STRATEGY**

### **Phase 1: Add Metaclass Support** 🔥
1. Locate `FinishDecl()` function in `GNUstepObjCDeclVendor.cpp`
2. Add metaclass discovery using `objc_getMetaClass()`
3. Use `class_copyMethodList()` to get class methods
4. Add class methods to interface declarations

### **Phase 2: Eliminate Hardcoded Tables** 
1. Remove static method signature arrays (lines 466-520)
2. Replace with pure runtime introspection
3. Trust GNUstep's type encodings completely

### **Phase 3: Validation & Testing**
1. Test all literal types work
2. Verify performance is acceptable  
3. Clean up code for PR submission

## 🚨 **ARCHITECTURAL INSIGHTS**

### **Why Our Current Approach is Wrong**:
- We maintain **500+ lines** of hardcoded method signatures
- We manually fix type encodings like `"@32@0:8r^@16Q24"`
- We fight against the runtime instead of using it

### **Why Apple LLDB is Simpler**:
- Uses **pure runtime introspection**
- Trusts the runtime's type encodings
- Automatically discovers all methods

### **What We Should Do**:
- Use `objc_getMetaClass()` for class methods
- Use `class_copyMethodList()` for dynamic discovery
- Use `method_getTypeEncoding()` for type information
- **Eliminate most hardcoded tables**

## 🔍 **DEBUGGING HINTS**

### **Common Issues**:
1. **Metaclass returns NULL** - Check class name spelling
2. **Method not found** - Verify selector name format  
3. **Type encoding wrong** - Use runtime encoding, don't hardcode
4. **Build failures** - Rebuild entire plugin after changes

### **Logging Commands**:
```bash
# Enable detailed logging
expr (void)printf("Metaclass: %p\n", objc_getMetaClass("NSArray"))
```

### **Verification Commands**:
```bash
# Check if method exists at runtime
expr (void*)class_getClassMethod((Class)objc_getClass("NSArray"), (SEL)sel_registerName("arrayWithObjects:count:"))
```

## 📋 **COMPLETION CRITERIA**

### **Minimum Viable Release**:
- ✅ String literals work
- ✅ Method calls work
- 🎯 Array literals work
- 🎯 Dictionary literals work
- 🎯 Class method calls work

### **Code Quality for PR**:
- Remove hardcoded method tables where possible
- Use runtime introspection consistently
- Add comprehensive test coverage
- Document Windows/Linux differences

## 🚀 **START HERE**

1. **Read the task file**: `c:\code\llvm-project\lldb\tasks.md`
2. **Run diagnostic tests** to confirm current state
3. **Locate `FinishDecl()` function** in `GNUstepObjCDeclVendor.cpp`
4. **Add metaclass method discovery** using GNUstep runtime functions
5. **Test array literals** to verify the fix works

## 🎯 **EXPECTED OUTCOME**

After fixing metaclass method discovery, array and dictionary literals should work completely, bringing the project to ~90% completion and ready for final polish before LLVM PR submission.

---

**Session Objective**: Fix metaclass method discovery to enable array/dictionary literals
**Primary File**: `GNUstepObjCDeclVendor.cpp`  
**Key Function**: `FinishDecl()` around line 1000
**Success Test**: `@[@"test1", @"test2"]` should evaluate successfully
