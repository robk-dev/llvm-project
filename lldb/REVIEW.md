🧙🏾‍♂️: Excellent. I’ll now begin a comprehensive chain-of-thought code review of your `GNUstepObjCRuntime` module and associated formatters for x86\_64 GNUstep on Linux/Windows.

🧩: I will audit the full module including all formatters and helpers, identify fragile patterns, layout assumptions, runtime integration issues, and produce a structured roadmap with:

* Enumerated issues
* Precise file\:line citations
* Safer/portable code snippets to fix them
* Grouped work packages for your coding agents

I’ll get started now and return with the first annotated roadmap and patch proposals.


# Code Review and Improvement Roadmap for GNUstepObjCRuntime Module

This document analyzes the current implementation of the **GNUstepObjCRuntime** LLDB plugin (for the GNUstep libobjc2 Objective-C runtime on x86\_64) and enumerates its shortcomings. Each issue is described with code references and proposed solutions. By addressing these issues step by step, we can enhance the module to achieve feature parity with LLDB's Apple ObjC runtime support.

## 1. Missing Dynamic Type Resolution (`GetDynamicTypeAndAddress`)

**Issue:** The `GetDynamicTypeAndAddress` function is not implemented – it simply returns `false` unconditionally. This means LLDB cannot determine an object's dynamic (runtime) class type when evaluating expressions or printing objects, undermining the debugging experience for Objective-C objects.

```cpp
bool GNUstepObjCRuntime::GetDynamicTypeAndAddress(...,
                                                  TypeAndOrName &class_type_or_name,
                                                  Address &address,
                                                  Value::ValueType &value_type, ...) {
    return false;  // Stub: dynamic type resolution not implemented
}:contentReference[oaicite:1]{index=1}
```

**Impact:** Without dynamic type resolution, an `id` or base class pointer will not be properly identified as an instance of its actual subclass. For example, an `NSObject *obj` pointing to a `Student` instance will still appear as `NSObject` in LLDB, rather than `Student`. This breaks features like `expr obj` or `po obj` that rely on the debugger knowing the true class of an object.

**Solution:** Implement `GetDynamicTypeAndAddress` to determine the object's class at runtime using the GNUstep libobjc2 runtime information:

* **Read the ISA Pointer:** When LLDB has a `ValueObject` that could be an Objective-C object (checked via `CouldHaveDynamicValue`), read the object's ISA. On 64-bit ObjC, the ISA is typically at offset 0 of the object’s memory (barring tagged pointers; see Issue 3). Use `m_process->ReadPointerFromMemory` (as wrapped by the helper `ReadPointer`) to read the ISA value from the object’s address.

* **Update Class Map if Needed:** Ensure the ISA-to-descriptor map is up-to-date by calling `UpdateISAToDescriptorMapIfNeeded()`. This will populate the internal map of class ISA addresses to class descriptors (see Issue 7 for ensuring this covers all classes).

* **Lookup Class Descriptor:** Using the ISA value as a key, find the corresponding class descriptor in `m_isa_to_descriptor`. The patch already stores discovered classes in this map via `AddClass()`. If the ISA is found, retrieve the `ClassDescriptorSP`.

* **Set Dynamic Type Info:** Use the class descriptor to identify the class name and, if possible, the debug type:

  * Set `class_type_or_name` to the class’s name (via `descriptor->GetClassName()`).
  * Attempt to find a `TypeSP` for this class. If debug info for the class exists (e.g. the program included the @interface), the descriptor’s `GetType()` may return it. Otherwise, consider creating a synthetic type:

    * Query the LLDB `DeclVendor` or search the target’s symbols for the class name to create an incomplete `Struct` type for the class. (In Apple’s runtime, LLDB tries to find or create a `Type` for the dynamic class if one isn’t already present.)
  * Set the `address` to the original object’s address (the location of the actual object in memory).
  * Set `value_type` to indicate a loaded pointer (e.g. `Value::eValueTypeLoadAddress`), since we have the object’s address in memory.

* **Return true if resolved:** If we found a class descriptor (or at least a class name), return true to indicate the dynamic type was identified. Otherwise, return false to let LLDB fall back to static type.

**Example Implementation Snippet:**

```cpp
bool GNUstepObjCRuntime::GetDynamicTypeAndAddress(ValueObject &in_value,
                                                 DynamicValueType use_dynamic,
                                                 TypeAndOrName &class_type_or_name,
                                                 Address &address,
                                                 Value::ValueType &value_type, 
                                                 llvm::ArrayRef<uint8_t> &local_buffer) {
    if (!CouldHaveDynamicValue(in_value))
        return false;
    // Get the pointer value of the object
    lldb::addr_t obj_addr = in_value.GetPointerValue();
    if (!obj_addr)
        return false;
    // Handle tagged pointers separately (see Issue 3)
    if (IsTaggedPointer(obj_addr)) {
        // (Potential handling for tagged pointer objects)
        // For now, skip dynamic resolution if it's a tagged pointer
        return false;
    }
    // Read ISA from object memory
    lldb::addr_t isa = ReadPointer(obj_addr);
    if (isa == LLDB_INVALID_ADDRESS)
        return false;
    UpdateISAToDescriptorMapIfNeeded();  // ensure class map is populated
    auto it = m_isa_to_descriptor.find(isa);
    if (it == m_isa_to_descriptor.end())
        return false; // unknown class ISA
    ClassDescriptorSP desc = it->second;
    ConstString class_name = desc->GetClassName();
    if (!class_name) 
        return false;
    // Set the dynamic type name
    class_type_or_name.SetName(class_name);
    // Try to get or create a Type for this class
    if (TypeSP type_sp = desc->GetType()) {
        class_type_or_name.SetTypeSP(type_sp);
    } else {
        // Attempt to find type by name in debug info or create a forward declaration
        if (auto *vendor = ObjCLanguageRuntime::GetDeclVendor()) {
            if (auto types = vendor->FindTypes(class_name, /*max_matches*/1); !types.empty()) {
                class_type_or_name.SetCompilerType(types.front());
            }
        }
        // (Else, could create a minimal CompilerType for the class in the scratch AST)
    }
    // Use the original object address as the location
    address.SetRawAddress(obj_addr);
    value_type = Value::eValueTypeLoadAddress;
    return true;
}
```

This implementation uses the runtime to get the ISA pointer and then the class descriptor. It also illustrates how we might integrate with LLDB's type system (via `DeclVendor` or AST) to obtain a `CompilerType` for the dynamic class. With this in place, expressions and printouts in LLDB will show the correct dynamic class names and types.

## 2. Incomplete Class Descriptor (Superclass, IVars, etc.)

**Issue:** The nested `GNUstepClassDescriptor` class currently provides only basic information (ISA and class name) and leaves many methods unimplemented or returning dummy values. For example, `GetSuperclass()` returns an empty `ClassDescriptorSP` (no superclass), `GetMetaclass()` is unimplemented, and `GetInstanceSize()` always returns 0. Additionally, `GetTaggedPointerInfo()` is hard-coded to `false`. This means the debugger lacks important class metadata:

* **Superclass hierarchy:** LLDB cannot determine if one class is a subclass of another.
* **Instance size and ivars:** The size of instances and their instance variables are not exposed, which can be useful in debugging (e.g., in the `language objc class-table dump` command or for memory layout inspection).
* **Metaclass info:** The relationship for `Class` objects (the metaclass) is not available.
* **Tagged pointer identification:** (See Issue 3 for details on tagged pointers.)

**Impact:** Without superclass info, LLDB cannot correctly report an object's inheritance chain. For instance, if you dump the Objective-C class table in LLDB, every class will show no superclass. Similarly, instance size being 0 is incorrect and could mislead any feature that relies on it (like calculating object offsets for ivars, if that were implemented). While these may not directly break core functionality, they reduce the completeness and accuracy of the debugger's data.

**Solution:** Extend `GNUstepClassDescriptor` to capture more class metadata, utilizing libobjc2 runtime functions or direct memory reads:

* **Superclass:** Use the Objective-C runtime API `class_getSuperclass()` to find a class’s superclass at runtime. The `LoadRuntimeSymbols()` function should be extended to locate the `class_getSuperclass` symbol similarly to how it finds `class_getName`. After dynamic class enumeration, we can iterate over each discovered class and call `class_getSuperclass` in the target process to get its superclass pointer:

  * If a superclass exists (and is not one of the root classes like `NSObject`), find or create the corresponding `ClassDescriptorSP` and store it. We might add a member in `GNUstepClassDescriptor` (e.g., `ObjCISA m_super_isa` or a cached `ClassDescriptorSP m_superclass_sp`).
  * Implement `GetSuperclass()` to return the cached superclass descriptor. For example:

    ```cpp
    ClassDescriptorSP GNUstepClassDescriptor::GetSuperclass() override {
        if (!m_super_fetched) { // lazy-load once
            ObjCISA super_isa = ReadClassSuperclass(m_isa); // uses runtime to get superclass ISA
            if (super_isa && super_isa != m_isa) {
                m_superclass = m_runtime->GetClassDescriptor(super_isa);
            }
            m_super_fetched = true;
        }
        return m_superclass;
    }
    ```

    Here, `m_runtime->GetClassDescriptor(isa)` would look up `isa` in the global map and return the descriptor (or create one if needed). This requires that `GNUstepClassDescriptor` holds a reference or pointer to the runtime (`m_runtime`) to query the map, or the runtime could fill in superclass info during the update phase.

* **Instance Size:** The Objective-C runtime often provides `class_getInstanceSize()` to get the instance size of a class. We should confirm if libobjc2 implements this (likely it does for Apple API compatibility). If so, we can load `class_getInstanceSize` symbol and call it for each class similar to above. If not available, instance size might also be obtainable via the class structure in memory (libobjc2’s class struct likely contains size or it could be computed from ivar list). Implement `GetInstanceSize()` to return the correct size. This will allow LLDB to report the memory footprint of objects, and commands like class table dump can display a non-zero instance size.

* **Ivars and Methods:** While not absolutely necessary for basic functionality, providing ivar information would improve the debugging experience (e.g., letting LLDB inspect an object's ivar values). We could use `class_copyIvarList` in the target process to get ivar names and types. This is a more involved enhancement:

  * Load the symbol for `class_copyIvarList` (and `ivar_getName`/`ivar_getTypeEncoding`) if available.
  * In `Describe()` or a similar mechanism, query the target for ivars. Alternatively, populate ivar info during the class enumeration (though that could slow down attach time if done for every class).
  * At minimum, implementing `GetNumIVars()` and `GetIVarAtIndex()` in `GNUstepClassDescriptor` would allow LLDB’s class dump to list ivars. For now, this can be a lower priority (not needed unless users explicitly dump class details).

* **Metaclass:** In Objective-C, each class has a metaclass (the class object of the class itself). Typically, the metaclass’s ISA points to `NSObject`’s metaclass or itself. To support debugging of `Class` objects, we could:

  * Provide a `GetMetaclass()` implementation that returns a descriptor for the metaclass. The runtime API does not directly give a metaclass, but one approach is: if an ISA points to a metaclass (usually an ISA of a class object is its metaclass, whose ISA in turn is the root metaclass). This gets complex and may not be immediately necessary to implement. For now, returning an empty `ClassDescriptorSP` (as currently) is acceptable, but it can be noted for future improvement.
  * Alternatively, if libobjc2 has an internal flag for metaclass or if the OBJC symbols differentiate metaclasses (sometimes symbols like `OBJC_METACLASS_$_ClassName` exist), we could gather those similarly and mark descriptors as metaclass as needed.

**Example Superclass Integration:** After dynamic class discovery in `UpdateISAToDescriptorMap()`, we could add:

```cpp
// After populating m_isa_to_descriptor with all classes:
if (m_class_getSuperclass_addr != LLDB_INVALID_ADDRESS) {
    for (auto &entry : m_isa_to_descriptor) {
        ObjCISA isa = entry.first;
        ClassDescriptorSP desc = entry.second;
        // Evaluate class_getSuperclass for this class
        std::string expr = llvm::formatv("(void*)class_getSuperclass((void*)0x{0:x})", isa).str();
        ValueObjectSP result;
        Status error;
        UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result, error);
        if (error.Success() && result && result->GetValueAsUnsigned(0) != 0) {
            lldb::addr_t super_isa = result->GetValueAsUnsigned(0);
            if (super_isa != LLDB_INVALID_ADDRESS) {
                if (ClassDescriptorSP super_desc = GetClassDescriptor(super_isa)) {
                    // Link the superclass in the descriptor (assuming GNUstepClassDescriptor has a setter)
                    std::static_pointer_cast<GNUstepClassDescriptor>(desc)->SetSuperclass(super_desc);
                }
            }
        }
    }
}
```

This pseudocode calls `class_getSuperclass` for each known class and links the descriptor to its superclass descriptor. We would need to have `m_class_getSuperclass_addr` resolved in `LoadRuntimeSymbols()` (e.g., find symbol `"class_getSuperclass"` similarly to how `class_getName` is found).

By implementing these enhancements, the class descriptors will carry much more information. This benefits commands like `language objc class-table dump` (which will show superclass names and instance sizes properly) and potentially allows LLDB to better pretty-print objects. It also sets the stage for future features (like conditional breakpoints on classes or improved summaries) to utilize this metadata.

## 3. No Handling of Tagged Pointers

**Issue:** The current implementation does not account for **tagged pointer** objects. Tagged pointers are small Objective-C objects (like small NSNumber, NSDate, etc.) that Apple’s runtime encodes directly in the pointer value (usually using certain bits to store data instead of an address). In our `GNUstepClassDescriptor`, the methods `GetTaggedPointerInfo()` are stubbed to return false, and `CheckPointer(...)` in the base class is not overridden, meaning the runtime assumes no pointers are tagged.

**Impact:** If the GNUstep libobjc2 runtime uses any form of tagged pointers for efficiency (it's designed to be a drop-in replacement, so it might implement tagged pointers similarly on 64-bit), LLDB will misidentify such objects. Specifically:

* A tagged pointer does not point to a real ISA in memory, so reading memory at that “address” will fail or yield incorrect data.
* The current `GetDynamicTypeAndAddress` implementation (once added) might treat a tagged pointer as a normal address and fail to find it in the ISA map or cause memory read errors.
* This could manifest as certain objects not being recognized at all (e.g., an `NSNumber` might not show a dynamic type or might not be printable via `po` because its pointer isn't handled).

**Solution:** Introduce logic to detect and handle tagged pointers in the runtime:

* **Detecting Tagged Pointers:** Determine the scheme used by libobjc2 for tagged pointers. On Apple x86\_64, pointers with the lowest bit set (and a specific high-bit pattern) are tagged. We need to find if libobjc2 has similar constants (perhaps documented in libobjc2 or by inspecting its source). If libobjc2 does not implement tagged pointers, this can be a no-op (but we should verify this to future-proof the code).

  * If a pointer is within certain small ranges or aligns with known tag bit patterns, consider it a tagged pointer.
  * Provide a utility (possibly a `TaggedPointerVendor` implementation or a simple check function in our runtime) to test a pointer.

* **ClassDescriptor for Tagged Pointers:** In Apple’s LLDB, there is a concept of a `TaggedPointerVendor` which can produce a `ClassDescriptor` for a tagged pointer by decoding it (e.g., an NSNumber tagged pointer might encode the value and the class kind in the bits). For GNUstep, if such pointers exist, we might implement a simplified approach:

  * If `IsPossibleTaggedPointer(ptr)` returns true, then `GetDynamicTypeAndAddress` should not read memory at that address. Instead, it should determine the class via the pointer itself.
  * For example, Apple encodes certain small NSNumber instances in the pointer with a specific class (like `__NSCFNumber`). We would need to map the pointer to a class name. Without deep knowledge of libobjc2’s scheme, one approach is to call an Objective-C runtime function on the pointer to get its class. For instance, `object_getClass((id)obj)` called on a tagged pointer in a well-functioning runtime should return a valid `Class` pointer (for Apple, this works; for libobjc2, it likely does too if they support tagged pointers).
  * We could invoke `object_getClass` via expression evaluation for tagged pointers to retrieve the class, then proceed as usual to find the descriptor by that ISA.

* **Prevent Misuse:** At minimum, add a guard in `GetDynamicTypeAndAddress` (as shown in Issue 1 snippet) to detect a tagged pointer and avoid treating it as an address. Instead of immediately returning false, a more advanced implementation would handle it as described. But initially, we could simply return false for tagged pointers to avoid erroneous memory reads, and log a warning that a tagged pointer was encountered and not handled.

**Implementation Plan:**

1. **Tagged Pointer Detection:** Implement a function, e.g., `bool IsTaggedPointer(lldb::addr_t ptr)`, in `GNUstepObjCRuntime` or as part of a `TaggedPointerVendor` subclass. This function might check something like:

   ```cpp
   bool GNUstepTaggedPointerVendor::IsPossibleTaggedPointer(lldb::addr_t ptr) override {
       // Example condition: pointer is not aligned to 16 bytes (as real pointers might be) 
       // and has a certain high-bit pattern.
       // Apple uses 0x800...0001 as a mask indicator, but GNUstep might differ.
       return (ptr & 0x1) != 0; // simplistic check: least significant bit set
   }
   ```

   This is a placeholder; actual conditions need verification against libobjc2. If uncertain, we might initially only use a very conservative check or none at all, and document this as a gap.

2. **Tagged Class Resolution:** If we identify a pointer as tagged, use runtime to get its class:

   ```cpp
   if (IsTaggedPointer(obj_addr)) {
       // call object_getClass on the object
       std::string expr = llvm::formatv("(void*)object_getClass((void*)0x{0:x})", obj_addr).str();
       UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, error);
       if (error.Success() && result_sp) {
           lldb::addr_t class_isa = result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
           // proceed to find class descriptor for class_isa
       }
   }
   ```

   Ensure `object_getClass` symbol is loaded (we might add it in `LoadRuntimeSymbols()` if available in libobjc2).

3. **Special-case Known Tagged Classes:** Optionally, if we know certain bit patterns correspond to specific classes (like in Apple's case, certain high bits indicate an NSNumber or NSDate), we could directly map them. This requires libobjc2 details, so it may be beyond scope without additional research.

For now, implementing the detection and using `object_getClass` as a fallback would significantly improve handling. If libobjc2 does not use tagged pointers, these checks will rarely trigger, but having them ensures future compatibility.

**Conclusion:** Adding tagged pointer support ensures that small objects encoded in pointers are not ignored. This will allow commands like `po` or expression evaluation on these objects to work (for example, printing an `NSNumber` that is actually a tagged pointer will yield its value instead of an error or blank result).

## 4. `GetObjectDescription` Not Implemented

**Issue:** Both overloads of `GetObjectDescription` in `GNUstepObjCRuntime` simply return an error stating that the feature is not supported. This function is supposed to produce an Objective-C object's description string (akin to what `po object` does) at the runtime layer.

```cpp
llvm::Error GNUstepObjCRuntime::GetObjectDescription(Stream &str, ValueObject &valobj) {
    return llvm::createStringError("LLDB's GNUStep runtime does not support object description");
}:contentReference[oaicite:13]{index=13}
```

**Impact:** When a user tries to print an object in LLDB using the `po` command or when the debugger internally tries to fetch an object's description (for example, for logging or some summaries), this unimplemented method means the runtime plugin provides no help. In practice, LLDB might fall back to other means (like calling `description` via the expression evaluator), but it could be less efficient or fail if scripting is disabled. The user sees an error or no meaningful output for `po` if the fallback isn't working.

**Solution:** Implement `GetObjectDescription` using the target’s Objective-C runtime to call the `-description` method of objects:

* The approach can be similar to how we already call runtime functions via `UserExpression::Evaluate`. We can invoke `[obj description]` on the target and retrieve the result. Typically, `-description` returns an `NSString`. To get a C-string from it, we might then call `[description UTF8String]`.
* Since this is potentially unsafe to call arbitrary code on the target (the description method could have side effects or be overridden by user code), LLDB’s typical approach is to do this with care. However, given we are creating a thorough support, providing this feature behind the same safety mechanisms (timeouts, ignoring breakpoints, unwinding on error) is acceptable.

**Proposed Implementation Outline:**

```cpp
llvm::Error GNUstepObjCRuntime::GetObjectDescription(Stream &str, ValueObject &object) {
    if (!m_process)
        return llvm::createStringError("No process available");
    lldb::addr_t obj_addr = object.GetPointerValue();
    if (!obj_addr)
        return llvm::createStringError("Invalid object pointer");
    // Prepare an expression to call [object description]
    ExecutionContext exe_ctx;
    m_process->CalculateExecutionContext(exe_ctx);
    EvaluateExpressionOptions options;
    options.SetUnwindOnError(true);
    options.SetIgnoreBreakpoints(true);
    options.SetTimeout(std::chrono::seconds(5));
    // Call -[obj description] and get NSString result
    std::string expr = llvm::formatv("(id)[(id)0x{0:x} description]", obj_addr).str();
    ValueObjectSP result;
    Status error;
    UserExpression::Evaluate(exe_ctx, options, expr.c_str(), /*expr_prefix*/"", result, error);
    if (error.Fail() || !result) {
        return llvm::createStringError("Failed to call description: %s", error.AsCString());
    }
    lldb::addr_t nsstr_addr = result->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
    if (nsstr_addr == LLDB_INVALID_ADDRESS || nsstr_addr == 0) {
        return llvm::createStringError("description returned nil");
    }
    // Now get UTF8String from the NSString
    expr = llvm::formatv("(const char*)[(id)0x{0:x} UTF8String]", nsstr_addr).str();
    UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result, error);
    if (error.Fail() || !result) {
        return llvm::createStringError("Failed to get UTF8String: %s", error.AsCString());
    }
    lldb::addr_t cstr_addr = result->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
    if (cstr_addr == LLDB_INVALID_ADDRESS || cstr_addr == 0) {
        return llvm::createStringError("UTF8String returned null");
    }
    // Read C-string from target memory
    std::string desc = ReadCString(cstr_addr, 1024);
    str.Printf("%s", desc.c_str());
    return llvm::Error::success();
}
```

This example flow does the following:

1. Calls `[object description]` on the target, obtaining an `NSString *` (or whatever the Foundation equivalent is in GNUstep).
2. Calls `UTF8String` on that NSString to get a C-string pointer.
3. Reads the C-string from target memory into our debugger memory.
4. Writes it into the provided `Stream` for output.

We should ensure that the target’s Foundation (GNUstep Base library) is loaded; otherwise, `description` may not be present (for pure libobjc2 without Foundation, `description` is defined in `NSObject`, so likely Foundation is present in most Objective-C programs). The code above uses Objective-C syntax in the expression; LLDB’s expression evaluator should handle that since the language is set to ObjC++ automatically when ObjC runtime is present.

**Safety:** We use `options.SetIgnoreBreakpoints(true)` and a timeout to avoid getting stuck in case `description` does something problematic (like waiting on input or hitting a breakpoint). This mirrors how LLDB handles `po` internally. We might also consider using a shorter expression that directly calls a runtime function if available (Apple’s runtime has `objc_msgSend` to call description, but that’s more complex to set up manually).

By implementing this, the user will be able to use `po someObject` and get a meaningful description string, as long as the object's `-description` is well-behaved. This greatly improves the usability of the debugger for Objective-C objects.

## 5. No Step-Through Trampoline Plan (`GetStepThroughTrampolinePlan`)

**Issue:** The method `GetStepThroughTrampolinePlan` currently returns `nullptr` and is marked TODO. In LLDB, this is used to skip over certain library "trampoline" functions when single-stepping. For Objective-C, trampolines typically refer to message dispatch functions like `objc_msgSend` and its variants, which are not actual user code but assembly stubs that jump to the real method implementation. The Apple runtime plugin implements logic to detect when the thread is about to step into `objc_msgSend` and instead provides a plan to step through (over) it so the debugger lands in the target method directly.

**Impact:** Without a step-through trampoline plan, stepping in Objective-C code can be very tedious:

* If a user steps into a message send (e.g., `obj.methodCall`), LLDB will go into the assembly of `objc_msgSend` or libobjc’s dispatch functions, since it has no instructions to skip them. This is confusing and not useful to the developer.
* The user has to manually step out or continue until the actual implementation is reached.
* It can also interfere with stepping behavior if the debugger lands inside the runtime's functions (which may not have source code or symbols the user cares about).

**Solution:** Implement a trampoline stepping strategy for GNUstep’s message dispatch. Likely, libobjc2 uses similar function names (`objc_msgSend`, `objc_msgSend_fpret`, `objc_msgSend_stret`, etc.) for message sending on x86\_64 (since it aims to be compatible with Apple’s ABI). We should confirm the symbols:

* The GNUstep libobjc2 source or symbols list would show if `objc_msgSend` is present. It's highly probable, as Clang generates calls to `objc_msgSend` for message expressions.

**Plan:**

1. **Identify Trampoline Functions:** Determine the set of function names (or addresses) that should be considered trampolines:

   * Common ones: `objc_msgSend`, `objc_msgSendSuper`, `objc_msgSend_stret`, `objc_msgSendSuper_stret`, and possibly `objc_msgSend_fixedup` or others depending on libobjc2 internals. Also, on x86-64, stret variants exist for structures.
   * libobjc2 might also use some PLT stubs or intermediate thunks; but focusing on known names is a good start.

2. **Implement Detection:** In `GetStepThroughTrampolinePlan`, examine the current instruction pointer or symbol of the thread:

   * Get the thread’s PC (`Thread &thread` is passed in). We can use `thread.GetRegisterContext()->GetPC()` to get the address.
   * Resolve that address to a symbol or function name (`Target::GetModuleAtAddress` -> `SymbolContext`).
   * If the symbol name matches one of the known trampolines (e.g., `"objc_msgSend"`), then we want to step through it.

3. **Provide a Plan:** LLDB provides a `ThreadPlan` API. We can create a `ThreadPlanStepOut` (to step out of the trampoline function) or a custom plan:

   * E.g.,

     ```cpp
     return thread.QueueThreadPlanForStepThrough(/*range list for trampolines*/, target_func_addr, stop_others);
     ```

     However, implementing a custom `ThreadPlan` might be overkill if a simpler approach works.

   * Apple’s `AppleObjCRuntimeV2` uses an `AppleObjCTrampolineHandler` which sets breakpoints on the return address of `objc_msgSend` and continues execution until that point (effectively skipping the trampoline).

   * For our purposes, a simpler approach: if we detect the PC is in `objc_msgSend`, we can create a `ThreadPlanStepOut` of the current function:

     ```cpp
     if (is_objc_msgSend_function) {
         return std::make_shared<ThreadPlanStepOut>(&thread, nullptr, false, stop_others, eVoteNoOpinion, eVoteNoOpinion);
     }
     ```

     A `ThreadPlanStepOut` with no specific frame will step out of the current stack frame, which is exactly what we want (to get out of the message send and back to the caller’s context, which should then immediately be in the actual implementation of the method call).

   * Alternatively, use `ThreadPlanStepThrough` if available for trampolines: LLDB may have a specialized function to handle step-through by setting breakpoints at possible return addresses.

4. **Edge Cases:** Ensure that we do this only when appropriate:

   * We should verify the module of the symbol is the Objective-C runtime (libobjc). We don’t want to treat a user function that coincidentally is named `objc_msgSend` (unlikely) as a trampoline.
   * Possibly check that the address lies within the `objc` library module loaded (we can check the module of the SymbolContext).
   * Only apply to known trampolines.

**Example Implementation:**

```cpp
ThreadPlanSP GNUstepObjCRuntime::GetStepThroughTrampolinePlan(Thread &thread, bool stop_others) {
    const SymbolContext *sym_ctx = thread.GetStackFrameAtIndex(0)->GetSymbolContextPtr();
    if (sym_ctx && sym_ctx->GetSymbol()) {
        ConstString sym_name = sym_ctx->GetSymbol()->GetName();
        if (sym_name && sym_name.GetStringRef().startswith("objc_msgSend")) {
            // We've identified a trampoline function
            return thread.QueueThreadPlanForStepOut(/*current frame*/nullptr, false, stop_others);
        }
    }
    return ThreadPlanSP(); // no special plan
}
```

In the above:

* We get the current frame's symbol. If it starts with `"objc_msgSend"` (covers `objc_msgSend`, `objc_msgSendSuper`, etc. due to prefix match), we queue a step-out plan on the current frame.
* `QueueThreadPlanForStepOut` is a convenience that creates and enqueues a `ThreadPlanStepOut`. The `nullptr` for frame means use the current frame.

This simplistic approach would cause the debugger to step out of `objc_msgSend` immediately, effectively skipping over it. The next execution stop should be at the instruction after the call to objc\_msgSend in the caller, which *should* be the beginning of the actual method implementation (because the return from objc\_msgSend will land in the method).

**Further Refinement:** The above might not handle all cases, e.g., stepping *over* a message send vs stepping *in*. Another approach is to intercept at the call site. Apple's trampoline handler uses breakpoints in the `objc_msgSend` implementations to redirect stepping. However, implementing that fully might require deeper integration. The step-out strategy is a reasonable start that addresses the most common scenario of accidentally stepping into the assembly stub.

By implementing `GetStepThroughTrampolinePlan`, stepping through Objective-C messages becomes smoother. Users can step *in* to a message send and LLDB will take them directly to the target method code rather than into the bowels of the runtime. This quality-of-life improvement makes debugging ObjC code on GNUstep runtime much more pleasant.

## 6. `CreateObjectChecker` Is a Stub (No Selector Checking)

**Issue:** The `CreateObjectChecker` function is left as a no-op stub that simply returns an empty utility function. In LLDB’s design, `CreateObjectChecker` is meant to produce a piece of code that can be injected into the target process to check some property of an object at runtime (for example, to test if an object implements a certain selector, often used in conditionals for breakpoints or to validate an object’s ISA).

Currently, the implementation constructs an empty function that does nothing and returns that as a `UtilityFunction`:

```cpp
const char *function_template = R"(
    extern "C" void
    %s(void *$__lldb_arg_obj, void *$__lldb_arg_selector) {}
)";
...
return GetTargetRef().CreateUtilityFunction(empty_function_code, name,
                                            eLanguageTypeC, exe_ctx);
```



**Impact:** Any LLDB feature or user script that relies on `CreateObjectChecker` will not work correctly. For instance, LLDB might use this for its internal checks or future enhancements (Apple's runtime uses a similar mechanism to check if a class is a certain kind or if an object can respond to a selector). Because our version is a dummy, those will always succeed or do nothing, potentially leading to incorrect debugger behavior in edge cases:

* If a user sets a breakpoint condition like `[(id)obj respondsToSelector:@selector(foo)]`, the LLDB runtime might try to use an object checker to evaluate that safely. Our stubbed function does nothing, so it wouldn't properly tell if `foo` is implemented.
* Similarly, some internal dynamic type resolution safety checks might be planned to use this (though currently we haven’t implemented such, Apple LLDB uses a checker to verify an ISA is valid).

**Solution:** Implement the object checker function or a reasonable approximation:

* The purpose is to return a `UtilityFunction` that can be invoked with an object and a selector and return whether the object responds to that selector (or some other condition). Apple’s implementation for V2 checks if a given selector is in the class’s method list.
* A simpler approach: use the Objective-C runtime function `class_respondsToSelector(Class, SEL)` or `-[NSObject respondsToSelector:]`. Since calling Objective-C methods from injected C code is complex (would need to call objc\_msgSend, which itself might not be trivial to call from inserted code without setting up Objective-C context), using runtime C APIs is better.
* If libobjc2 provides `class_getMethodImplementation` or `class_respondsToSelector`, we can leverage those:

  * `BOOL class_respondsToSelector(Class cls, SEL sel)` – not sure if this exists, but we can simulate it by checking if `class_getMethodImplementation(cls, sel) != NULL`.
  * We have the object and selector as arguments in the generated function signature (see the template with `$__lldb_arg_obj` and `$__lldb_arg_selector`). We can cast the object to `id` and selector to `SEL` and perform the check.

**Revised UtilityFunction Code Generation:** We can dynamically generate a small C function that does something like:

```c
extern "C" bool FunctionName(void *$__lldb_arg_obj, void *$__lldb_arg_selector) {
    id obj = (id)$__lldb_arg_obj;
    SEL sel = (SEL)$__lldb_arg_selector;
    if (!obj || !sel) return false;
    Class cls = object_getClass(obj);
    if (!cls) return false;
    // Use class_respondsToSelector if available, otherwise:
    bool result = false;
    if (class_respondsToSelector) {
        result = class_respondsToSelector(cls, sel);
    } else {
        // Fallback: try to get method IMP
        if (class_getMethodImplementation) {
            result = (class_getMethodImplementation(cls, sel) != NULL);
        } else {
            // naive fallback: iterate class_getInstanceMethod if available (too heavy for a utility function possibly)
        }
    }
    return result;
}
```

However, embedding this in a string with proper casting is a bit involved. We might simplify to assume the presence of `class_getMethodImplementation` which is a commonly exported symbol in ObjC runtime:

* We should load `class_getMethodImplementation` in `LoadRuntimeSymbols` similarly if we plan to use it.
* Our generated function can call it directly, as it’s just a C function in the runtime.

So, the string template could be:

```cpp
const char *check_function_template = R"(
    #include <objc/runtime.h>
    extern "C" bool
    {func_name}(void *$__lldb_arg_obj, void *$__lldb_arg_selector) {{
        if (!$__lldb_arg_obj || !$__lldb_arg_selector) return false;
        Class cls = object_getClass((id)$__lldb_arg_obj);
        if (!cls) return false;
        return class_getMethodImplementation(cls, (SEL)$__lldb_arg_selector) != 0;
    }}
)";
```

This uses `object_getClass` and `class_getMethodImplementation` to determine if the class (or its superclasses) has an implementation for the given selector. It returns a boolean.

We then use `GetTargetRef().CreateUtilityFunction` to compile this function in the target process. LLDB can then call this utility function with an object and selector. The returned `UtilityFunction` can be stored or used in conditions.

**Use Cases:** After implementing, LLDB could use it for:

* Breakpoints that stop when an object receives a certain message (ensuring the selector exists to avoid sending unknown selector).
* Validating an object’s ISA by checking if querying it triggers something (this is more Apple-specific).

Even if LLDB core isn't currently calling our `CreateObjectChecker`, having it correctly implemented is valuable for completeness and potential future uses. It essentially gives the debugger a safe way to ask the target runtime questions about objects.

**Testing the Checker:** We should test that calling the returned function works. For example:

```lldb
(lldb) expr (bool)$checker($obj, @selector(fooMethod))
```

should return true if `$obj` responds to `fooMethod`.

Implementing this fully may require careful string construction and includes, but once done, it will close the loop on the last unimplemented major method in this plugin.

## 7. Potential Memory Leak in `CallObjCCopyClassList` (Dynamic Class Enumeration)

**Issue:** The `CallObjCCopyClassList` method that dynamically discovers all classes has a subtle memory management problem. It calls `objc_copyClassList` twice but frees the allocated class list only once. Specifically:

* First call: `(void*)objc_copyClassList(NULL, &count)` to get the class count (casting the result to `(void*)` and ignoring it).
* Second call: same call again to get the actual class list pointer.
* After the second call, it stores the returned pointer in `class_array_addr` and eventually frees that memory by calling `free()` in the target.
* **Problem:** The memory returned by the first call is never freed. The first call to `objc_copyClassList` with a non-null count pointer will allocate an array of class pointers internally and return it (according to the Cocoa API contract). Since the result of the first call is not saved, that array is leaked in the target process.

**Impact:** This leak means that whenever `UpdateISAToDescriptorMap()` runs (typically when a debug session starts or modules are loaded), it will allocate an array of class pointers and never release it. If a program has many classes, this could be a significant amount of memory. If `UpdateISAToDescriptorMapIfNeeded` runs multiple times (it is marked complete after one run unless new libraries load, but new library loads set the flag to false, potentially causing multiple updates over a long session), multiple leaks could accumulate.

While not catastrophic for short debugging sessions, in a long-running debugging scenario or repeated attach/detach cycles, the target could accumulate leaked memory. This is especially problematic because the target might be an application where memory is critical.

**Solution:** Modify `CallObjCCopyClassList` to avoid the double allocation. We have a few options:

* **Single Call to `objc_copyClassList`:** The function returns the class list and populates the count. We can do one call and use both the returned pointer and the count:

  1. Allocate memory for `count` (as done).
  2. Evaluate `objc_copyClassList((unsigned int*)count_addr)` (notice no `NULL` for first param; the Apple signature is actually `objc_copyClassList(unsigned int *outCount)` with one parameter. It appears libobjc2 might have the same signature since we call it as if two parameters where the first is unused `NULL`).
  3. This single call will give us a pointer (list of classes) in `result_sp` and also write the count to `count_addr`.
  4. Use both results. If count is 0, free the returned pointer immediately (it could be `NULL` or a valid pointer—Apple’s function returns `NULL` if count is 0).
  5. If count > 0, proceed to read the class pointers and then free the returned list via `free()`.

* **Use `objc_getClassList`:** Alternatively, some runtimes provide `int objc_getClassList(Class *buffer, int bufferCount)`. This function fills a provided buffer with class definitions and returns the number of classes (if buffer is NULL, it returns the total class count without allocating). If libobjc2 has this (likely for compatibility), we could use it:

  * Call `objc_getClassList(NULL, 0)` to get count.
  * Allocate our own buffer of that size.
  * Call `objc_getClassList(buffer_ptr, count)` to fill it.
  * Then *we* free our buffer (no need to call `free` in the target, since the buffer is in target memory that we allocated via `AllocateMemory` and can free via `DeallocateMemory`).
  * This approach avoids using `objc_copyClassList` altogether and gives more control. It also avoids the leak by design.
  * However, using `objc_getClassList` requires two calls and managing memory ourselves, whereas fixing the current approach only needs tweaking one call.

Given our current code, the simplest fix is to make one call to `objc_copyClassList` and capture the pointer:

**Corrected Code Sketch:**

```cpp
// Allocate memory for count as before...
std::string expr = llvm::formatv("(void*)objc_copyClassList((unsigned int*)0x{0:x})", count_addr).str();
UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, error);
if (error.Fail() || !result_sp) {
    // handle error
}
uint32_t count = ReadMemoryUnsigned(count_addr, sizeof(uint32_t));
if (count == 0) {
    // No classes; free any returned pointer if non-null
    lldb::addr_t temp_ptr = result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
    if (temp_ptr != LLDB_INVALID_ADDRESS && temp_ptr != 0 && m_free_addr != LLDB_INVALID_ADDRESS) {
        // Free the allocated empty list
        std::string free_expr = llvm::formatv("free((void*)0x{0:x})", temp_ptr).str();
        UserExpression::Evaluate(exe_ctx, options, free_expr.c_str(), "", result_sp, error);
    }
    m_process->DeallocateMemory(count_addr);
    return false;
}
lldb::addr_t class_array_addr = result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
if (class_array_addr == LLDB_INVALID_ADDRESS) {
    // handle error
}
// Now proceed to read `count` class pointers from class_array_addr...
```

This way, we call `objc_copyClassList` once, get both results at once. We ensure to free the allocated list at the end (which the existing code already does for the second call’s result). We need to also handle the scenario of 0 classes by freeing the (likely `NULL` or tiny) pointer returned.

**Double-Check Signature:** It's worth verifying the actual signature of `objc_copyClassList` in libobjc2. The code calls it with an extra `NULL` argument (as if the function were `objc_copyClassList(void *unused, unsigned int *outCount)`). If that's the case, perhaps libobjc2’s `objc_copyClassList` expects two parameters (maybe for backward compatibility with OS X 10.4 or GNU runtime). We might need to adjust accordingly:

* If indeed `objc_copyClassList(void *filter, unsigned int *outCount)` exists (where filter could filter classes by image, but passing NULL means all), then our calls are correct. In that case, combining into one call still works (just include the NULL).
* For example, do `objc_copyClassList((void*)0, (unsigned int*)count_addr)` in one call.

Given the current code uses `(void*)0` as first arg, we'll preserve that in the single call expression.

**Testing the Fix:** After this change, use the verification steps:

* Run the dynamic class enumeration and monitor the target’s memory usage or check that no additional allocations persist. If possible, instrument the libobjc2 to log allocations or run under a tool to detect leaks. However, simply reasoning about it or checking that we free what we allocate should suffice.
* The `verify_dynamic_discovery.md` test mentioned custom classes being discovered. We should ensure those still appear, indicating our single call still works as intended.

By fixing this memory leak, we make the class discovery process more robust for repeated use and long sessions, and we adhere to good memory hygiene in the target process.

## 8. Missing Runtime Introspection for Superclasses and Metadata

*(This item is partly an extension of Issue 2 but focused on leveraging runtime API to enhance introspection beyond just class names.)*

**Issue:** The current implementation focuses on discovering class names and ISAs, but does not use all available runtime information to enrich the debugger. Aside from superclass (already discussed), other metadata like protocols, properties, and methods are not touched. Moreover, `LoadRuntimeSymbols()` only loads a few symbols (`objc_copyClassList`, `class_getName`, `free`), leaving out useful ones like `object_getClass`, `class_getSuperclass`, `class_getInstanceSize`, etc. The result is that our debugger plugin operates with minimal information, whereas it could take advantage of libobjc2’s introspection functions.

**Impact:** Not fully utilizing the runtime’s introspection capabilities means the debugger might be doing more work than necessary or missing opportunities to present data:

* Some tasks we implemented above (like finding superclasses or checking selectors) require runtime functions which we did not initially load (`class_getSuperclass`, `object_getClass`, `class_getMethodImplementation`, etc.). If these symbols are not loaded/resolved, our attempts to call them via expressions could fail or be less efficient (expressions that call them by name rely on symbol lookup at runtime each time).
* Without pre-loaded addresses, each expression evaluation involving these functions does a symbol lookup internally. Better to resolve once and possibly even call via `FunctionCaller` for efficiency (though that might be micro-optimization).

**Solution:** Expand `LoadRuntimeSymbols()` to gather addresses of additional Objective-C runtime functions that we plan to use or that could be useful:

* **`object_getClass`** – to get an object’s class (`objc_copyClassList` indirectly uses internal data, but `object_getClass` is explicitly useful for tagged pointers as noted, and for any scenario needing the actual class pointer).
* **`class_getSuperclass`** – for superclass chaining.
* **`class_getInstanceSize`** – to get instance size (populating `GetInstanceSize()` in descriptors).
* **`class_getMethodImplementation`** – for the object checker logic.
* **`class_copyIvarList` and `ivar_getName/type`** – if we decide to implement ivar listing (maybe future).
* **`objc_msgSend` (and variants)** – not for calling, but possibly to set breakpoints or analyze (though we can get those from symbols by regex if needed, similar to how Apple sets breakpoint on `objc_exception_throw` via CreateExceptionResolver).

We should add find-symbol calls for these in `LoadRuntimeSymbols()`. For example:

```cpp
// In LoadRuntimeSymbols:
FindSymbolInModule(modules, "object_getClass", m_object_getClass_addr);
FindSymbolInModule(modules, "class_getSuperclass", m_class_getSuperclass_addr);
FindSymbolInModule(modules, "class_getInstanceSize", m_class_getInstanceSize_addr);
FindSymbolInModule(modules, "class_getMethodImplementation", m_class_getMethodImpl_addr);
FindSymbolInModule(modules, "objc_msgSend", m_objc_msgSend_addr); // maybe for reference
```

(where `FindSymbolInModule` is a helper that does the `FindSymbolsWithNameAndType` and sets the address if found, similar to the existing code).

By resolving these at startup (when the ObjC runtime library is loaded), we avoid repeated lookups. We can then use `m_object_getClass_addr` etc. in our code:

* For instance, in tagged pointer handling, instead of constructing an expression string each time for `object_getClass`, we could use LLDB's `FunctionCaller` to call the function pointer at `m_object_getClass_addr` with the object pointer. However, using `UserExpression` is fine; knowing the address just ensures the symbol exists.
* For `class_getSuperclass`, we can call it via expression or potentially write a small snippet with `FunctionCaller` as well. But since we already do expressions elsewhere, consistency is okay.

**Advantage:** Loading these symbols upfront also acts as a check – if any of them are missing (for example, if libobjc2 didn't implement one), we can adjust. Libobjc2 is pretty complete with Objective-C API, so it likely has all of these.

Additionally, consider using these functions to fill in data:

* After discovering classes, use `m_class_getInstanceSize_addr` with an expression for each class to record instance sizes in the descriptor (similar loop as for superclasses).
* Possibly also use `m_object_getClass_addr` in `GetDynamicTypeAndAddress` for reliability: Instead of reading ISA via memory (which might be complicated if pointer masking is in effect), calling `object_getClass(obj)` on the target might be a more direct and future-proof way to get the class pointer. This is slightly heavier (requires target execution) but given we already pause the process and have a context, it might be acceptable. We could make this an optional path if direct memory read fails or if a pointer appears tagged.

**Example:** using `object_getClass` in dynamic resolution:

```cpp
if (m_object_getClass_addr != LLDB_INVALID_ADDRESS) {
    // Evaluate object_getClass(obj) in target
    std::string expr = llvm::formatv("(void*)object_getClass((void*)0x{0:x})", obj_addr).str();
    UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, error);
    if (error.Success() && result_sp) {
        isa = result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
    }
}
```

This could replace the manual `ReadPointer` for ISA, ensuring that if libobjc2 ever changes ISA representation, the runtime call still yields the correct class.

**Conclusion:** By broadening the use of runtime introspection APIs, we make the plugin more robust against runtime implementation details and potentially simpler (fewer hard-coded offsets or assumptions). It leverages the runtime to do the heavy lifting of introspection safely. Each added piece (superclass, instance size, etc.) contributes to a richer debugging experience:

* Superclass chains visible,
* Correct instance sizes,
* Possibly ability to list ivars in the future,
* More reliable dynamic type resolution via `object_getClass`,
* Ability to safely verify selectors via `class_getMethodImplementation`.

All these improvements align with how the Apple runtime plugin works (using internal runtime functions and data structures to gather info) but using public API where possible for maintainability.

## 9. Exception Catch Breakpoints Not Handled

**Issue:** The `CreateExceptionResolver` implementation only sets up a breakpoint for exception throwing (`objc_exception_throw`) when `throw_bp` is true, and ignores the `catch_bp` parameter. This means LLDB can break when an Objective-C exception is thrown, but cannot break on the moment an exception is caught (i.e., when an `@catch` clause is entered).

**Impact:** If a user or script tries to enable breaking on exception catch in an Objective-C context, it will silently not work. For example, a user might configure the debugger to break on Objective-C exceptions (both throw and catch). With the current code:

* Break on throw works (it uses a `BreakpointResolverName` for `objc_exception_throw`).
* Break on catch is not set at all, so exceptions caught by an `@catch` block will not pause execution.

This is a minor issue because Objective-C exceptions are not frequently used (they're slower and less common than other error handling techniques), and even LLDB on Apple systems typically only breaks on throws by default. There's no straightforward way to break exactly at the point of a catch because the runtime does not have a distinct "catch handler" function to break on (unlike C++ which can break on catch because of C++ exception handling runtime).

**Solution:** We can acknowledge this limitation or attempt a best-effort implementation:

* The Apple runtime plugin doesn’t set a catch breakpoint either (it only breaks on throw). In fact, `catch_bp` might be a legacy parameter mostly relevant to C++ or other language runtimes.
* One possible way to catch an Objective-C exception catch would be to set a breakpoint on the user’s catch block addresses via metadata, but LLDB doesn’t easily provide that from the runtime side.

Given this, it might be acceptable to document that catch breakpoints are not supported for ObjC, or simply implement `CreateExceptionResolver` to ignore `catch_bp` (as it does now) but perhaps log a warning if `catch_bp` is requested.

If we did want to attempt something:

* When `catch_bp` is true, we could set breakpoints on the function that handles unwinding after catch. In ObjC, exceptions use the same mechanism as C++ (DWARF EH or SEH on Windows). Possibly, one could break on the end of `_objc_exception_throw` or some phase 2 personality function. This is complex and likely unnecessary.

**Recommendation:** Leave `CreateExceptionResolver` as is (break on throw only), but make sure this is clearly communicated. Since the code is already doing the right thing for throw:

```cpp
if (throw_bp)
    resolver_sp = std::make_shared<BreakpointResolverName>(
        bkpt, "objc_exception_throw", eFunctionNameTypeBase,
        eLanguageTypeUnknown, Breakpoint::Exact, 0, eLazyBoolNo);
```



We might just add:

```cpp
if (catch_bp) {
    // ObjC catch breakpoints are not supported; no resolver for catch.
}
```

Or even set a breakpoint on `objc_exception_throw` for catch as well (which doesn’t make sense, so better not).

**Impact of leaving as-is:** Practically none for typical usage. When users do “break on exception”, LLDB (even on Apple) only stops on throw. We can mirror that behavior. If a user specifically asks for catch and we do nothing, they might be confused, but this is an edge scenario.

We can also ensure our documentation or logs reflect that catch breakpoints are not available in GNUstep runtime. Perhaps outputting a log warning if `catch_bp` is true:

```cpp
LLDB_LOG(GetLog(LLDBLog::Breakpoints), "GNUstepObjCRuntime: catch exceptions not supported");
```

**Conclusion:** This is a minor shortcoming and likely acceptable to postpone or document rather than actively implement. It does not significantly hinder debugging, since catching an exception means the program is continuing normally (which usually doesn’t require breaking in a debugger; breaking on throw is usually enough to inspect the state at the time of error).

---

By addressing the issues above, we will have a much more robust and feature-complete GNUstepObjCRuntime plugin:

* **Dynamic types** will be correctly identified.
* **Class descriptors** will carry essential class info (names, hierarchy, sizes).
* **Tagged pointers** will not confound the debugger.
* **Object descriptions** (`po`) will work seamlessly.
* **Stepping** through Objective-C calls will bypass the runtime internals.
* **Selector checks** and other utility functions will be available for internal use.
* **Memory usage** during class enumeration will be efficient and leak-free.
* **Additional introspection** (superclasses, etc.) will enrich the debugging data.
* **Exceptions** handling will match typical expectations (break on throw).

Each of these improvements can be implemented and tested in isolation, which is ideal for an incremental development approach. The coding team can take on each item, implement it, and add or update tests as appropriate (ensuring existing tests for dynamic values, `po` printing, stepping, etc., pass with the new code). This roadmap will bring the GNUstep Objective-C debugging on par with what developers expect from LLDB on Apple’s runtime, but tailored to the GNUstep libobjc2 environment.

**Sources:**

* GNUstepObjCRuntime.cpp and .h (current implementation)
* LLDB Apple ObjC runtime implementation (for reference behaviors) (dynamic type resolution), Apple exception handling (throws).
