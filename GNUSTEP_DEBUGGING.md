# GNUstep Objective-C Debugging with LLDB on Windows

This document describes how to set up and use LLDB with custom GNUstep formatters for debugging Objective-C code on Windows.

## 🎯 **BREAKTHROUGH SOLUTION: Correct GNUstep Compilation Flags**

After extensive debugging, we discovered that **proper GNUstep method dispatch requires specific compilation flags**. The key differences from standard configurations:

### **Critical Compilation Flags** 🔧


```makefile
# WORKING GNUstep compilation flags - Required for proper method dispatch
CFLAGS = -MMD -MP \
         -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -DGNU_GUI_LIBRARY=1 \
         -DGNUSTEP_RUNTIME=1 -D_NONFRAGILE_ABI=1 -DGNUSTEP_WITH_DLL \
         -fno-strict-aliasing -fexceptions -fobjc-exceptions \
         -D_NATIVE_OBJC_EXCEPTIONS -fno-omit-frame-pointer \
         -DGSWARN -DGSDIAGNOSE -Wno-import \
         -g -O0 -fobjc-runtime=gnustep-2.0 -fblocks \
         -fconstant-string-class=NSConstantString


# source debug_setup.sh

# cd /c/code/llvm-project/lldb/examples && clang -MMD -MP -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -DGNU_GUI_LIBRARY=1 -DGNUSTEP_RUNTIME=1 -D_NONFRAGILE_ABI=1 -DGNUSTEP_BASE_LIBRARY=1 -DGNUSTEP_WITH_DLL -fno-strict-aliasing -fexceptions -fobjc-exceptions -D_NATIVE_OBJC_EXCEPTIONS -fno-omit-frame-pointer -DGSWARN -DGSDIAGNOSE -Wno-import -g -O0 -fobjc-runtime=gnustep-2.0 -fblocks -fconstant-string-class=NSConstantString -IC:/.conan/24ea94/1/include -IC:/.conan/24ea94/1/GNUstep/System/Library/Headers -include /c/code/llvm-project/lldb/examples/gnustep_fix.h -Wl,--enable-auto-import -fuse-ld=lld -lstdc++ -lgcc_s -LC:/.conan/24ea94/1/lib -LC:/.conan/24ea94/1/GNUstep/System/Library/Libraries -lgnustep-base -lobjc -lws2_32 -ladvapi32 -lcomctl32 -luser32 -lcomdlg32 -lmpr -lnetapi32 -lm -o minimal_fail_test minimal_fail_test.m


LDFLAGS = -Wl,--enable-auto-import -fuse-ld=lld -lstdc++ -lgcc_s

LIBS = -lgnustep-base -lobjc -lws2_32 -ladvapi32 -lcomctl32 \
       -luser32 -lcomdlg32 -lmpr -lnetapi32 -lm -lpthread
```

### **Key Discoveries** 💡

1. **`-fobjc-runtime=gnustep-2.0`** (NOT 2.1!) - Critical for method dispatch
2. **`-fuse-ld=lld`** - Must use LLD linker instead of default LD
3. **`-D_NONFRAGILE_ABI=1 -DGNUSTEP_RUNTIME=1`** - Essential runtime flags
4. **Proper Windows libraries** - Required for GNUstep DLL support

## 🚀 **What Now Works**

### **Successful Compilation & Execution**
- ✅ **NSString literals**: `@"Hello"`
- ✅ **NSArray literals**: `@[ @"apple", @"banana" ]`
- ✅ **NSDictionary literals**: `@{ @"name": @"John", @"age": @30 }`
- ✅ **NSNumber literals**: `@30`, `@3.14`, `@YES`

### **LLDB Debugging Features**
- ✅ **GNUstep plugin loads**: Accepts languages 2 (C), 16 (ObjC), 17 (ObjC++)
- ✅ **Array formatters work**: `po fruits` shows `(apple, banana)`
- ✅ **Clean output**: No terminal corruption
- ✅ **Breakpoints work**: Full debugging workflow functional

## 🔧 **Environment Setup**

### **Prerequisites**
- Windows 10/11 with MSYS2 UCRT64
- Clang 20.1.8+
- Conan package manager
- GNUstep distribution via Conan

### **Quick Setup**
```bash
# 1. Source the environment setup
source /c/code/llvm-project/debug_setup.sh

# 2. Compile example with working flags
cd /c/code/llvm-project/lldb/examples
make simple_test

# 3. Debug with LLDB
winpty /c/code/llvm-project/build/bin/lldb ./simple_test.exe
```

### **LLDB Debug Session Example**
```
(lldb) target create "./simple_test.exe" 
(lldb) b main
(lldb) run
(lldb) br set -l 16  # Set breakpoint after dictionary creation
(lldb) c
(lldb) po fruits     # Shows: (apple, banana) 
(lldb) po personInfo # Shows dictionary contents
```

## 🎉 **Success Metrics**

### **Before Fix**
```
❌ undefined reference to `.objc_selector_alloc_@16@0:8'
❌ undefined reference to `.objc_selector_initWithInt:_@20@0:8i16'
❌ undefined reference to `.objc_selector_dictionaryWithObjects:forKeys:count:`
```

### **After Fix**
```
✅ 2025-08-15 15:29:56.534 simple_test[5988:8700] Testing: Hello and (apple, banana)
✅ (lldb) po fruits
✅ (apple, banana)
```

## 🧠 **Technical Deep Dive**

### **Root Cause Analysis**
The issue was that **GNUstep method dispatch requires specific runtime initialization flags**. Without:
- `-D_NONFRAGILE_ABI=1`: Modern ObjC ABI support
- `-DGNUSTEP_RUNTIME=1`: GNUstep runtime activation  
- `-fobjc-runtime=gnustep-2.0`: Correct runtime version
- `-fuse-ld=lld`: Compatible linker

The compiler generates method calls but **runtime method dispatch fails** at link time.

### **Why Literals Work But Method Calls Don't**
- **Literals** (`@"string"`, `@[]`, `@{}`) use **compiler built-ins**
- **Method calls** (`alloc`, `init`, `numberWithInt:`) need **runtime dispatch**

## 🔄 **Development Workflow**

### **Compile and Debug** 
```bash
# Quick compile
compile_example simple_test

# Quick debug  
run_debug simple_test

# Rebuild LLDB after changes
rebuild_lldb
```

### **Adding New Test Cases**
1. Create `.m` file in `lldb/examples/`
2. Add target to Makefile
3. Use the **exact compilation flags** above
4. Test with `winpty lldb`

## 🎯 **Current Status & Limitations**

### **Working** ✅
- Basic Objective-C debugging
- NSString, NSArray, NSDictionary literals
- LLDB GNUstep plugin integration
- Clean formatter output
- Complex nested data structures

### **Known Limitations** ⚠️
- Instance method calls (`alloc`/`init`) work only with correct runtime flags
- Some terminal formatting issues may persist
- Windows-specific GNUstep quirks

### **Future Enhancements** 🔮
- NSDictionary/NSNumber custom formatters
- Better method introspection
- Enhanced Windows compatibility
- Performance optimizations

---

**🏆 Achievement**: Successfully implemented working Objective-C debugging on Windows with LLDB and GNUstep, solving critical method dispatch linking issues through proper runtime configuration.
