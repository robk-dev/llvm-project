//===-- GNUstepObjCRuntimeIntrospector.cpp ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCRuntimeIntrospector.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/ThreadList.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Expression/FunctionCaller.h"
#include "lldb/Expression/DiagnosticManager.h"
#include "lldb/Expression/UtilityFunction.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/ModuleList.h"
#include "lldb/Core/Value.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"
#include "lldb/ValueObject/ValueObject.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

using namespace lldb;
using namespace lldb_private;

GNUstepObjCRuntimeIntrospector::GNUstepObjCRuntimeIntrospector(Process *process)
    : m_process(process) {
  // Cache some runtime constants
  if (m_process) {
    m_address_size = m_process->GetAddressByteSize();
    m_byte_order = m_process->GetByteOrder();
  } else {
    m_address_size = 0;
    m_byte_order = lldb::eByteOrderInvalid;
  }
}

lldb::addr_t GNUstepObjCRuntimeIntrospector::GetISAFromObject(ValueObject &valobj) {
  if (!m_process) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Get the object address
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Check if this is a tagged pointer first
  if (IsTaggedPointer(obj_addr)) {
    // For tagged pointers, we need to extract the class information differently
    // GNUstep tagged pointers encode class information in the lower bits
    return obj_addr; // Return the tagged pointer itself for now
  }
  
  // For regular objects, the ISA is the first pointer-sized value
  Status error;
  lldb::addr_t isa_addr = m_process->ReadPointerFromMemory(obj_addr, error);
  
  if (error.Fail()) {
    return LLDB_INVALID_ADDRESS;
  }
  
  return isa_addr;
}

std::string GNUstepObjCRuntimeIntrospector::GetClassName(lldb::addr_t isa_addr) {
  if (!m_process || isa_addr == LLDB_INVALID_ADDRESS) {
    // printf("[DEBUG] GetClassName: Invalid process or ISA address\n");
    return "";
  }
  
  // printf("[DEBUG] GetClassName: Looking up class name for ISA 0x%llx\n", (unsigned long long)isa_addr);
  
  // Check if this is a tagged pointer
  if (IsTaggedPointer(isa_addr)) {
    // For tagged pointers, decode the class based on the tag
    uint64_t tag = isa_addr & 0x7; // Lower 3 bits are the tag
    switch (tag) {
    case 1: // Tagged number
      return "NSNumber";
    case 2: // Tagged date
      return "NSDate";
    case 4: // Tagged string (GNUstep uses tag 4 for strings based on our observation)
      return "NSString";
    default:
      return "<tagged[" + std::to_string(tag) + "]>";
    }
  }

  // Based on libobjc2/class.h, the structure of an objc_class is:
  // struct objc_class {
  //   Class isa;          // 0 * address_size
  //   Class super_class;  // 1 * address_size
  //   const char *name;   // 2 * address_size  <- This is what we want!
  //   ...
  // };
  // We need to read the 'name' pointer, which is the third pointer in the
  // structure.

  Status error;

  // The 'name' field is at an offset of 2 * address_size from the start of the
  // class structure.
  const lldb::addr_t name_ptr_addr = isa_addr + (2 * m_address_size);
  
  // printf("[DEBUG] GetClassName: Reading name pointer from address 0x%llx\n", (unsigned long long)name_ptr_addr);

  const lldb::addr_t name_addr = m_process->ReadPointerFromMemory(name_ptr_addr, error);

  if (error.Fail() || name_addr == LLDB_INVALID_ADDRESS) {
    // printf("[DEBUG] GetClassName: Failed to read name pointer: %s\n", error.AsCString());
    return "";
  }

  // Now read the C-string from the 'name' pointer.
  // printf("[DEBUG] GetClassName: Reading C-string from address 0x%llx\n", (unsigned long long)name_addr);
  
  std::string class_name;
  m_process->ReadCStringFromMemory(name_addr, class_name, error);

  if (error.Fail()) {
    // printf("[DEBUG] GetClassName: Failed to read class name string: %s\n", error.AsCString());
    return "";
  }

  // printf("[DEBUG] GetClassName: Found class name: %s\n", class_name.c_str());
  return class_name;
}

std::string GNUstepObjCRuntimeIntrospector::GetClassNameFromObject(ValueObject &valobj) {
  lldb::addr_t isa_addr = GetISAFromObject(valobj);
  if (isa_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  return GetClassName(isa_addr);
}

ConstString GNUstepObjCRuntimeIntrospector::GetClassNameFromISA(lldb::addr_t isa_addr) {
  // Handle nil ISA
  if (!isa_addr || isa_addr == LLDB_INVALID_ADDRESS) {
    return ConstString();
  }
  
  // Check cache first
  auto it = m_isa_to_name_cache.find(isa_addr);
  if (it != m_isa_to_name_cache.end()) {
    return it->second;
  }
  
  // Read class structure to get the name
  if (!m_process) {
    return ConstString();
  }
  
  Status error;
  
  // GNUstep class layout (from libobjc2/runtime.h):
  // struct objc_class {
  //     Class isa;          // offset 0: Metaclass pointer
  //     Class super_class;  // offset 8: Superclass pointer  
  //     const char *name;   // offset 16: Class name
  //     ...
  // };
  
  // Read the class name pointer at offset 16
  lldb::addr_t name_ptr_addr = isa_addr + 16;
  lldb::addr_t name_ptr = m_process->ReadPointerFromMemory(name_ptr_addr, error);
  
  if (error.Fail() || name_ptr == 0 || name_ptr == LLDB_INVALID_ADDRESS) {
    return ConstString();
  }
  
  // Read the class name string
  char name_buffer[256];
  size_t bytes_read = m_process->ReadCStringFromMemory(
      name_ptr, name_buffer, sizeof(name_buffer), error);
  
  if (error.Fail() || bytes_read == 0) {
    return ConstString();
  }
  
  // Cache and return the result
  ConstString class_name(name_buffer);
  m_isa_to_name_cache[isa_addr] = class_name;
  
  return class_name;
}

lldb::addr_t GNUstepObjCRuntimeIntrospector::FindClass(const std::string &class_name) {
  if (!m_process || class_name.empty()) {
    return LLDB_INVALID_ADDRESS;
  }

  // Try to call objc_lookup_class function in the target
  // This is more reliable than trying to parse the class table ourselves
  std::vector<lldb::addr_t> args;
  
  // First, we need to create a string in the target process memory
  Status error;
  lldb::addr_t string_addr = m_process->AllocateMemory(class_name.length() + 1, 
                                                       lldb::ePermissionsReadable, error);
  if (error.Fail() || string_addr == LLDB_INVALID_ADDRESS) {
    return LLDB_INVALID_ADDRESS;
  }

  // Write the class name to target memory
  size_t bytes_written = m_process->WriteMemory(string_addr, class_name.c_str(), 
                                               class_name.length() + 1, error);
  if (error.Fail() || bytes_written != class_name.length() + 1) {
    m_process->DeallocateMemory(string_addr);
    return LLDB_INVALID_ADDRESS;
  }

  args.push_back(string_addr);
  lldb::addr_t class_addr = CallRuntimeFunction("objc_lookup_class", args);

  // Clean up the allocated string
  m_process->DeallocateMemory(string_addr);

  return class_addr;
}

bool GNUstepObjCRuntimeIntrospector::IsValidGNUstepRuntime() {
  if (!m_process) {
    return false;
  }

  // Try to find objc_lookup_class function - this indicates GNUstep runtime
  Target &target = m_process->GetTarget();
  SymbolContextList sc_list;
  target.GetImages().FindSymbolsWithNameAndType(ConstString("objc_lookup_class"),
                                               lldb::eSymbolTypeCode, sc_list);
  
  return sc_list.GetSize() > 0;
}

lldb::addr_t GNUstepObjCRuntimeIntrospector::CallRuntimeFunction(
    const std::string &function_name, const std::vector<lldb::addr_t> &args) {
  
  if (!m_process || function_name.empty()) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Setup execution context
  ExecutionContext exe_ctx;
  if (!SetupExecutionContext(exe_ctx)) {
    Log *log = GetLog(LLDBLog::Language);
    LLDB_LOG(log, "[GNUstep] Failed to setup execution context for {0}",
             function_name);
    return LLDB_INVALID_ADDRESS;
  }
  
  // Get scratch type system for argument and return types
  TypeSystemClangSP scratch_ts_sp = 
      ScratchTypeSystemClang::GetForTarget(exe_ctx.GetTargetRef());
  if (!scratch_ts_sp) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Build argument list
  ValueList arg_values;
  for (lldb::addr_t arg : args) {
    Value arg_value;
    
    // Determine argument type based on function name
    if (function_name == "objc_lookup_class") {
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
    arg_values.PushValue(arg_value);
  }
  
  // Return type is typically a pointer
  CompilerType return_type = 
      scratch_ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType();
  
  // Call the implementation
  Status error;
  lldb::addr_t result = CallRuntimeFunctionImpl(
      function_name.c_str(), return_type, arg_values, exe_ctx, error);
      
  if (error.Fail()) {
    Log *log = GetLog(LLDBLog::Language);
    LLDB_LOG(log, "[GNUstep] Failed to call {0}: {1}",
             function_name, error.AsCString());
    return LLDB_INVALID_ADDRESS;
  }
  
  return result;
}

bool GNUstepObjCRuntimeIntrospector::IsTaggedPointer(lldb::addr_t obj_addr) {
  if (!m_process || obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // GNUstep uses tagged pointers for small objects to avoid allocations.
  // The tag is in the lower 3 bits:
  // - Tag 4: Tiny strings (up to 8 characters)
  // - Tag 1: Small integers (NSNumber)
  // - Tag 2: Dates or other small objects
  // 
  // Check if any of the lower 3 bits are set (indicating a tagged pointer)
  uint64_t tag = obj_addr & 0x7;
  
  // Valid tags are 1, 2, 4 (not 0, 3, 5, 6, 7)
  if (tag == 1 || tag == 2 || tag == 4) {
    return true;
  }
  
  return false;
}

std::string GNUstepObjCRuntimeIntrospector::DecodeTaggedString(lldb::addr_t obj_addr) {
  // GNUstep uses "tiny strings" with tag value 4 for short compile-time constants.
  // Based on analysis of GNUstep source code (GSString.m):
  //
  // Bit layout for 64-bit systems:
  // - Bits 0-2: Tag (must be 4 for tiny strings)
  // - Bits 3-7: Length (5 bits, can store 0-31 but max is 9 characters)
  // - Bits 8-56: Unused/padding
  // - Bits 57-63, 50-56, 43-49, etc: Characters stored from high bits down
  //   Each character uses 7 bits, stored at bit position (57 - i*7)
  //
  // The macro from GNUstep source:
  // #define TINY_STRING_CHAR(s, x) ((s & (0xFE00000000000000 >> (x*7))) >> (57-(x*7)))
  // #define TINY_STRING_LENGTH_SHIFT 3
  // #define TINY_STRING_LENGTH_MASK 0x1f
  //
  // Note: The runtime may set additional high bits (e.g., 0xc instead of 0x8 prefix)
  // for metadata. We mask these off when decoding.
  
  // Verify this is a tagged string (tag = 4)
  if ((obj_addr & 0x7) != 4) {
    return "";
  }
  
  // Don't mask the address - the character extraction already handles the right bits
  // Extract length from bits 3-7 (after the tag)
  int length = (obj_addr >> 3) & 0x1f;
  
  // Sanity check - tiny strings can't be longer than 9 characters
  if (length > 9 || length == 0) {
    return "";
  }
  
  // Decode characters - each uses 7 bits, stored from bit 57 downward
  std::string result;
  result.reserve(length);
  
  for (int i = 0; i < length; i++) {
    // Extract character at position i using the GNUstep formula
    // Characters are stored at bits (57 - i*7) for 7 bits each
    // The mask 0xFE means 7 bits (1111110 in binary), shifted to the right position
    uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
    char c = (obj_addr & mask) >> (57 - (i * 7));
    
    // Validate it's a printable ASCII character
    if (c >= 0x20 && c <= 0x7e) {
      result += c;
    } else if (c == 0) {
      // Unexpected null in the middle - stop
      break;
    } else {
      // Non-printable character - this shouldn't happen with valid tiny strings
      // Return what we have so far or indicate error
      if (result.empty()) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "<tagged_%llx...>", 
                 (unsigned long long)(obj_addr & 0xffffffffffff));
        return buffer;
      }
      break;
    }
  }
  
  // Return the decoded string
  return result;
}

bool GNUstepObjCRuntimeIntrospector::IsValidObjectPointer(lldb::addr_t obj_addr) {
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Tagged pointers are always valid if they have the right tag bits
  if (IsTaggedPointer(obj_addr)) {
    return true;
  }
  
  // For regular pointers, do some basic sanity checks
  if (m_address_size == 8) {
    // On 64-bit systems, valid object pointers should be:
    // - Aligned to at least 8 bytes
    // - In a reasonable memory range
    if ((obj_addr & 0x7) != 0) {
      return false;
    }
    // Very low addresses are likely invalid
    if (obj_addr < 0x1000) {
      return false;
    }
  } else {
    // On 32-bit systems, align to 4 bytes
    if ((obj_addr & 0x3) != 0) {
      return false;
    }
    if (obj_addr < 0x1000) {
      return false;
    }
  }
  
  // Try to read the ISA pointer - if this fails, it's not a valid object
  Status error;
  lldb::addr_t isa = m_process->ReadPointerFromMemory(obj_addr, error);
  if (error.Fail() || isa == 0 || isa == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  return true;
}

// Implementation of CallRuntimeFunctionImpl
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
    Log *log = GetLog(LLDBLog::Language);
    LLDB_LOG(log, "[GNUstep] Failed to get/create function caller for {0}: {1}",
             function_name, error.Fail() ? error.AsCString() : "null caller");
    return LLDB_INVALID_ADDRESS;
  }
  
  // Prepare for execution
  DiagnosticManager diagnostics;
  lldb::addr_t wrapper_struct_addr = LLDB_INVALID_ADDRESS;
  
  // Make a mutable copy of args for WriteFunctionArguments
  ValueList mutable_args(args);
  
  // Insert function arguments
  if (!caller->WriteFunctionArguments(exe_ctx, wrapper_struct_addr, 
                                      mutable_args, diagnostics)) {
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
    Log *log = GetLog(LLDBLog::Language);
    LLDB_LOG(log, "[GNUstep] Function execution failed for {0}: result={1}",
             function_name, results);
    
    // Log diagnostics for debugging
    std::string diagnostic_str;
    for (const auto &diag : diagnostics.Diagnostics()) {
      diagnostic_str += diag->GetMessage();
      diagnostic_str += "; ";
    }
    if (!diagnostic_str.empty()) {
      LLDB_LOG(log, "[GNUstep] Diagnostics: {0}", diagnostic_str);
    }
    
    error = Status::FromError(diagnostics.GetAsError(
        lldb::eExpressionParseError,
        "Function execution failed"));
    return LLDB_INVALID_ADDRESS;
  }
  
  // Extract return value
  lldb::addr_t return_addr = result_value.GetScalar().ULongLong(
      LLDB_INVALID_ADDRESS);
      
  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(log, "[GNUstep] Called {0}({1:x}) = {2:x}",
           function_name, 
           mutable_args.GetSize() > 0 ? mutable_args.GetValueAtIndex(0)->GetScalar().ULongLong() : 0,
           return_addr);
           
  return return_addr;
}

// Implementation of GetOrCreateFunctionCaller
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
  } else {
    // Use generic cache for other functions
    auto it = m_function_cache.generic_callers.find(function_name);
    if (it != m_function_cache.generic_callers.end()) {
      return it->second;
    }
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
    static std::unique_ptr<FunctionCaller> empty_ptr;
    return empty_ptr;
  }
  
  function_address = symbol->GetAddress();
  
  // Create the function caller
  std::string caller_name = std::string(function_name) + "_caller";
  std::unique_ptr<FunctionCaller> new_caller(
      exe_ctx.GetTargetRef().GetFunctionCallerForLanguage(
          eLanguageTypeC, return_type, function_address,
          arg_types, caller_name.c_str(), error));
          
  if (error.Fail() || !new_caller) {
    static std::unique_ptr<FunctionCaller> empty_ptr;
    return empty_ptr;
  }
  
  // Compile the wrapper function
  DiagnosticManager diagnostics;
  ThreadSP thread_sp = exe_ctx.GetThreadSP();
  
  unsigned num_errors = new_caller->CompileFunction(thread_sp, diagnostics);
  if (num_errors > 0) {
    error = Status::FromError(diagnostics.GetAsError(
        lldb::eExpressionParseError,
        "Failed to compile function wrapper"));
    static std::unique_ptr<FunctionCaller> empty_ptr;
    return empty_ptr;
  }
  
  // Insert the wrapper into the target
  if (!new_caller->WriteFunctionWrapper(exe_ctx, diagnostics)) {
    error = Status::FromError(diagnostics.GetAsError(
        lldb::eExpressionSetupError,
        "Failed to insert function wrapper"));
    static std::unique_ptr<FunctionCaller> empty_ptr;
    return empty_ptr;
  }
  
  // Cache and return
  if (cached_caller) {
    *cached_caller = std::move(new_caller);
    return *cached_caller;
  } else {
    // Store in generic cache
    auto result = m_function_cache.generic_callers.emplace(
        function_name, std::move(new_caller));
    return result.first->second;
  }
}

// Implementation of SetupExecutionContext
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
    Log *log = GetLog(LLDBLog::Language);
    LLDB_LOG(log, "[GNUstep] Thread not safe for function calls");
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

// Implementation of GetObjCModule
lldb::ModuleSP GNUstepObjCRuntimeIntrospector::GetObjCModule() const {
  if (!m_process) {
    return ModuleSP();
  }
  
  Target &target = m_process->GetTarget();
  const ModuleList &modules = target.GetImages();
  
  for (uint32_t idx = 0; idx < modules.GetSize(); idx++) {
    ModuleSP module_sp = modules.GetModuleAtIndex(idx);
    if (module_sp) {
      const char *module_name = module_sp->GetFileSpec().GetFilename().GetCString();
      if (module_name && 
          (strstr(module_name, "libobjc.so") || 
           strstr(module_name, "libobjc2"))) {
        return module_sp;
      }
    }
  }
  
  return ModuleSP();
}

// Implementation of GetFoundationModule
lldb::ModuleSP GNUstepObjCRuntimeIntrospector::GetFoundationModule() const {
  if (!m_process) {
    return ModuleSP();
  }
  
  Target &target = m_process->GetTarget();
  const ModuleList &modules = target.GetImages();
  
  for (uint32_t idx = 0; idx < modules.GetSize(); idx++) {
    ModuleSP module_sp = modules.GetModuleAtIndex(idx);
    if (module_sp) {
      const char *module_name = module_sp->GetFileSpec().GetFilename().GetCString();
      if (module_name && strstr(module_name, "libgnustep-base.so")) {
        return module_sp;
      }
    }
  }
  
  return ModuleSP();
}
