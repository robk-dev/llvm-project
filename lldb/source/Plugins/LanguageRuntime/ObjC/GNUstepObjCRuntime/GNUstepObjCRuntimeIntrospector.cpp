//===-- GNUstepObjCRuntimeIntrospector.cpp ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCRuntimeIntrospector.h"
#include "GNUstepObjCRuntimeUtilities.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/ModuleList.h"
#include "lldb/Core/Section.h"
#include "lldb/Core/Value.h"
#include "lldb/Expression/DiagnosticManager.h"
#include "lldb/Expression/FunctionCaller.h"
#include "lldb/Expression/UtilityFunction.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Symbol/SymbolFile.h"
#include "lldb/Symbol/Symtab.h"
#include "lldb/Target/ABI.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Platform.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/ThreadList.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/StreamString.h"
#include "lldb/ValueObject/ValueObject.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::gnustep_objc_runtime_utilities;

GNUstepObjCRuntimeIntrospector::GNUstepObjCRuntimeIntrospector(Process *process)
    : m_process(process) {
  // Cache some runtime constants
  if (m_process) {
    m_address_size = m_process->GetAddressByteSize();
    m_byte_order = m_process->GetByteOrder();

    // Create the consolidated runtime function caller
    m_runtime_caller = std::make_unique<RuntimeFunctionCaller>(m_process);

    // Runtime symbols are handled by RuntimeFunctionCaller on-demand
  } else {
    m_address_size = 0;
    m_byte_order = lldb::eByteOrderInvalid;
  }
}

lldb::addr_t
GNUstepObjCRuntimeIntrospector::GetISAFromObject(ValueObject &valobj) {
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
    // For tagged pointers, we need to use the runtime's classForObject function
    // to get the correct class pointer from the SmallObjectClasses array
    return GetTaggedPointerClass(obj_addr);
  }

  // For regular objects, the ISA is the first pointer-sized value
  Status error;
  lldb::addr_t isa_addr = m_process->ReadPointerFromMemory(obj_addr, error);

  if (error.Fail()) {
    return LLDB_INVALID_ADDRESS;
  }

  return isa_addr;
}

std::string
GNUstepObjCRuntimeIntrospector::GetClassName(lldb::addr_t isa_addr) {
  if (!m_process || isa_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }

  // Check if this is a tagged pointer
  if (IsTaggedPointer(isa_addr)) {
    // For tagged pointers, decode the class based on the tag
    uint64_t tag = isa_addr & 0x7; // Lower 3 bits are the tag
    switch (tag) {
    case 1: // Tagged number
      return "NSNumber";
    case 2: // Tagged date
      return "NSDate";
    case 4: // Tagged string (GNUstep uses tag 4 for strings based on our
            // observation)
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

  const lldb::addr_t name_addr =
      m_process->ReadPointerFromMemory(name_ptr_addr, error);

  if (error.Fail() || name_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }

  // Now read the C-string from the 'name' pointer.
  std::string class_name;
  m_process->ReadCStringFromMemory(name_addr, class_name, error);

  if (error.Fail()) {
    return "";
  }

  return class_name;
}

std::string
GNUstepObjCRuntimeIntrospector::GetClassNameFromObject(ValueObject &valobj) {
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }

  // Check for tagged pointers first - they need special handling
  if (IsTaggedPointer(obj_addr)) {
    return GetTaggedPointerClassName(obj_addr);
  }

  // For regular objects, use normal ISA resolution
  lldb::addr_t isa_addr = GetISAFromObject(valobj);
  if (isa_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }

  return GetClassName(isa_addr);
}

ConstString
GNUstepObjCRuntimeIntrospector::GetClassNameFromISA(lldb::addr_t isa_addr) {
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
  lldb::addr_t name_ptr =
      m_process->ReadPointerFromMemory(name_ptr_addr, error);

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

lldb::addr_t
GNUstepObjCRuntimeIntrospector::FindClass(const std::string &class_name) {
  // Consolidated implementation: use GetClassPointer which has better error
  // handling
  return GetClassPointer(class_name);
}

bool GNUstepObjCRuntimeIntrospector::IsValidGNUstepRuntime() {
  if (!m_process) {
    return false;
  }

  // Try to find objc_lookup_class function - this indicates GNUstep runtime
  Target &target = m_process->GetTarget();
  SymbolContextList sc_list;
  target.GetImages().FindSymbolsWithNameAndType(
      ConstString("objc_lookup_class"), lldb::eSymbolTypeCode, sc_list);

  return sc_list.GetSize() > 0;
}

bool GNUstepObjCRuntimeIntrospector::IsValidObjectPointer(
    lldb::addr_t obj_addr) {
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

bool GNUstepObjCRuntimeIntrospector::IsTaggedPointer(lldb::addr_t obj_addr) {
  // GNUstep uses the lower 3 bits for tagging on 64-bit systems
  // Valid tags are: 1 (NSNumber), 2 (NSDate), 4 (NSString)
  // Tag 0 means regular object pointer (must be aligned)
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }

  if (m_address_size == 8) {
    // 64-bit: Check lower 3 bits for valid tags
    uint8_t tag = obj_addr & 0x7;
    return (tag == 1 || tag == 2 || tag == 4);
  } else {
    // 32-bit: Use lower bit only
    return (obj_addr & 0x1) != 0;
  }
}

std::string
GNUstepObjCRuntimeIntrospector::DecodeTaggedString(lldb::addr_t obj_addr) {
  // Decode GNUstep tagged strings (tag = 4)
  // Bit layout for 64-bit systems:
  // - Bits 0-2: Tag (must be 4 for tiny strings)
  // - Bits 3-7: Length (5 bits, can store 0-31 but max is 9 characters)
  // - Bits 8-56: Unused/padding
  // - Bits 57-63, 50-56, 43-49, etc: Characters stored from high bits down
  //   Each character uses 7 bits, stored at bit position (57 - i*7)

  // Verify this is a tagged string (tag = 4)
  if ((obj_addr & 0x7) != 4) {
    return "";
  }

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

  return result;
}

// Implementation of CallRuntimeFunctionImpl
lldb::addr_t GNUstepObjCRuntimeIntrospector::CallRuntimeFunctionImpl(
    const char *function_name, const CompilerType &return_type,
    const ValueList &args, ExecutionContext &exe_ctx, Status &error) const {
  using namespace gnustep_objc_runtime_utilities;

  // Get or create the function caller
  std::unique_ptr<FunctionCaller> &caller = GetOrCreateFunctionCaller(
      function_name, return_type, args, exe_ctx, error);
  if (!caller || error.Fail()) {
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
        lldb::eExpressionSetupError, "Failed to write function arguments"));
    return LLDB_INVALID_ADDRESS;
  }

  // Setup execution options
  EvaluateExpressionOptions options = MakeSafeExpressionOptions(true);
  options.SetStopOthers(true);
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
        lldb::eExpressionParseError, "Function execution failed"));
    return LLDB_INVALID_ADDRESS;
  }

  // Extract return value
  lldb::addr_t return_addr =
      result_value.GetScalar().ULongLong(LLDB_INVALID_ADDRESS);

  return return_addr;
}

// Implementation of GetOrCreateFunctionCaller
std::unique_ptr<FunctionCaller> &
GNUstepObjCRuntimeIntrospector::GetOrCreateFunctionCaller(
    const char *function_name, const CompilerType &return_type,
    const ValueList &arg_types, ExecutionContext &exe_ctx,
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

  // Fallback to target-wide search if not found in specific modules
  if (!symbol) {
    // Search all modules by name across target as final fallback
    SymbolContextList sc_list;
    m_process->GetTarget().GetImages().FindSymbolsWithNameAndType(
        ConstString(function_name), eSymbolTypeCode, sc_list);
    if (sc_list.GetSize() > 0) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(0, sc);
      symbol = sc.symbol;
    }
  }

  // Windows/libobjc2 sometimes exposes these as aliases; add minimal remaps
  if (!symbol) {
    const char *alias_name = nullptr;
    if (strcmp(function_name, "sel_getUid") == 0) {
      alias_name = "sel_registerName";
    }

    if (alias_name) {
      SymbolContextList sc_list;
      m_process->GetTarget().GetImages().FindSymbolsWithNameAndType(
          ConstString(alias_name), eSymbolTypeCode, sc_list);
      if (sc_list.GetSize() > 0) {
        SymbolContext sc;
        sc_list.GetContextAtIndex(0, sc);
        symbol = sc.symbol;
      }
    }
  }

  if (!symbol) {
    error = Status::FromErrorStringWithFormat(
        "Could not find symbol for function '%s'", function_name);
    static std::unique_ptr<FunctionCaller> empty_ptr;
    return empty_ptr;
  }

  // Log which module we found the symbol in for diagnostics
  const char *module_name = "unknown";
  if (symbol->GetAddress().GetModule()) {
    const char *name = symbol->GetAddress()
                           .GetModule()
                           ->GetFileSpec()
                           .GetFilename()
                           .GetCString();
    if (name)
      module_name = name;
  }

  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(
      log,
      "[GNUstep] GetOrCreateFunctionCaller: Found {0} in module {1} at 0x{2:x}",
      function_name, module_name,
      symbol->GetAddress().GetLoadAddress(&m_process->GetTarget()));

  function_address = symbol->GetAddress(); // Create the function caller
  std::string caller_name = std::string(function_name) + "_caller";
  std::unique_ptr<FunctionCaller> new_caller(
      exe_ctx.GetTargetRef().GetFunctionCallerForLanguage(
          eLanguageTypeC, return_type, function_address, arg_types,
          caller_name.c_str(), error));

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
        lldb::eExpressionParseError, "Failed to compile function wrapper"));
    static std::unique_ptr<FunctionCaller> empty_ptr;
    return empty_ptr;
  }

  // Insert the wrapper into the target
  if (!new_caller->WriteFunctionWrapper(exe_ctx, diagnostics)) {
    error = Status::FromError(diagnostics.GetAsError(
        lldb::eExpressionSetupError, "Failed to insert function wrapper"));
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

// Implementation of GetObjCModule
lldb::ModuleSP GNUstepObjCRuntimeIntrospector::GetObjCModule() const {
  if (!m_runtime_caller) {
    return ModuleSP();
  }

  return m_runtime_caller->FindObjCModule();
}

// Implementation of GetFoundationModule
lldb::ModuleSP GNUstepObjCRuntimeIntrospector::GetFoundationModule() const {
  if (!m_runtime_caller) {
    return ModuleSP();
  }

  return m_runtime_caller->FindFoundationModule();
}

lldb::addr_t GNUstepObjCRuntimeIntrospector::GetRuntimeFunctionAddress(
    const char *function_name) {
  if (!m_runtime_caller) {
    return LLDB_INVALID_ADDRESS;
  }

  // Delegate to the consolidated runtime function caller
  return m_runtime_caller->GetRuntimeFunctionAddress(function_name);
}

lldb::addr_t
GNUstepObjCRuntimeIntrospector::GetTaggedPointerClass(lldb::addr_t obj_addr) {
  if (!IsTaggedPointer(obj_addr)) {
    return LLDB_INVALID_ADDRESS;
  }

  // Use the runtime's object_getClass function to get the proper class
  // This will use classForObject() internally which handles SmallObjectClasses
  // lookup
  std::vector<lldb::addr_t> args;
  args.push_back(obj_addr);

  lldb::addr_t class_addr = CallRuntimeFunction("object_getClass", args);
  if (class_addr == LLDB_INVALID_ADDRESS) {
    // Fallback: Try to resolve manually using the tag bits
    // This is less reliable but better than nothing
    uint64_t tag_index;
    if (m_address_size == 4) {
      tag_index = 0; // 32-bit has only one small object class
    } else {
      tag_index = obj_addr & 0x7; // 64-bit uses lower 3 bits as index
    }

    Log *log = GetLog(LLDBLog::Language);
    LLDB_LOG(log,
             "[GNUstep] Failed to get tagged pointer class via runtime, "
             "tag_index={0}",
             tag_index);
  }

  return class_addr;
}

std::string GNUstepObjCRuntimeIntrospector::GetTaggedPointerClassName(
    lldb::addr_t obj_addr) {
  if (!IsTaggedPointer(obj_addr)) {
    return "";
  }

  // Get the class pointer first
  lldb::addr_t class_addr = GetTaggedPointerClass(obj_addr);
  if (class_addr == LLDB_INVALID_ADDRESS) {
    // Fallback to educated guessing based on common GNUstep patterns
    uint64_t tag;
    if (m_address_size == 4) {
      tag = obj_addr & 1;
    } else {
      tag = obj_addr & 7;
    }

    // Common small object classes in GNUstep (based on libobjc2 source)
    switch (tag) {
    case 1:
      return "NSNumber"; // Most common tagged pointer
    case 2:
      return "NSDate"; // Sometimes used for dates
    case 4:
      return "NSString"; // Tiny strings
    default:
      return "UnknownTaggedObject";
    }
  }

  // Get the class name using standard ISA resolution
  ConstString class_name = GetClassNameFromISA(class_addr);
  return class_name.GetCString() ? class_name.GetCString() : "";
}

// ================================================================================================
// CRITICAL: Direct Method Introspection Implementation
// Apple's approach: Use direct runtime function calls to avoid expression
// evaluation recursion
// ================================================================================================

lldb::addr_t
GNUstepObjCRuntimeIntrospector::GetClassPointer(const std::string &class_name) {
  if (!m_process || class_name.empty()) {
    return LLDB_INVALID_ADDRESS;
  }

  // Runtime symbols handled by RuntimeFunctionCaller on-demand
  if (!GetRuntimeFunctionAddress("objc_getClass")) {
    return LLDB_INVALID_ADDRESS;
  }

  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(log,
           "[GNUstepIntrospector] GetClassPointer: Looking for class '{0}'",
           class_name);

  // Allocate class name string in target process memory using RAII helper
  TargetStringAllocator string_alloc(m_process, class_name);
  if (!string_alloc.IsValid()) {
    LLDB_LOG(log,
             "[GNUstepIntrospector] GetClassPointer: Failed to allocate memory "
             "for class name '{0}': {1}",
             class_name, string_alloc.GetError().AsCString());
    return LLDB_INVALID_ADDRESS;
  }

  // Call objc_getClass(class_name) with properly allocated string
  std::vector<lldb::addr_t> args = {string_alloc.GetAddress()};
  lldb::addr_t result = CallRuntimeFunction("objc_getClass", args);

  LLDB_LOG(
      log,
      "[GNUstepIntrospector] GetClassPointer: objc_getClass('{0}') = 0x{1:x}",
      class_name, result);
  return result;
}

std::vector<GNUstepObjCRuntimeIntrospector::MethodInfo>
GNUstepObjCRuntimeIntrospector::GetInstanceMethods(lldb::addr_t class_ptr) {
  std::vector<MethodInfo> methods;

  if (!m_process || class_ptr == LLDB_INVALID_ADDRESS) {
    return methods;
  }

  // Runtime symbols handled by RuntimeFunctionCaller on-demand
  if (!GetRuntimeFunctionAddress("class_copyMethodList") ||
      !GetRuntimeFunctionAddress("method_getName") ||
      !GetRuntimeFunctionAddress("method_getTypeEncoding") ||
      !GetRuntimeFunctionAddress("sel_getName") ||
      !GetRuntimeFunctionAddress("free")) {
    return methods;
  }

  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(log,
           "[GNUstepIntrospector] GetInstanceMethods: Getting methods for "
           "class 0x{0:x}",
           class_ptr);

  // Call class_copyMethodList(class_ptr, &count) directly
  uint32_t count = 0;
  std::vector<lldb::addr_t> args = {class_ptr, (lldb::addr_t)&count};
  lldb::addr_t method_list = CallRuntimeFunction("class_copyMethodList", args);

  if (method_list == LLDB_INVALID_ADDRESS || count == 0) {
    LLDB_LOG(log, "[GNUstepIntrospector] GetInstanceMethods: No methods found "
                  "or call failed");
    return methods;
  }

  LLDB_LOG(log, "[GNUstepIntrospector] GetInstanceMethods: Found {0} methods",
           count);

  // Read the method array from memory
  Status error;
  const size_t method_ptr_size = m_address_size;

  for (uint32_t i = 0; i < count; i++) {
    lldb::addr_t method_ptr_addr = method_list + (i * method_ptr_size);
    lldb::addr_t method_ptr =
        m_process->ReadPointerFromMemory(method_ptr_addr, error);

    if (error.Fail() || method_ptr == LLDB_INVALID_ADDRESS) {
      continue;
    }

    // Get method name: SEL method_getName(Method method)
    std::vector<lldb::addr_t> method_args = {method_ptr};
    lldb::addr_t sel_ptr = CallRuntimeFunction("method_getName", method_args);

    if (sel_ptr == LLDB_INVALID_ADDRESS) {
      continue;
    }

    // Get selector name: const char *sel_getName(SEL sel)
    std::vector<lldb::addr_t> sel_args = {sel_ptr};
    lldb::addr_t name_ptr = CallRuntimeFunction("sel_getName", sel_args);

    if (name_ptr == LLDB_INVALID_ADDRESS) {
      continue;
    }

    // Read selector name string
    std::string selector_name;
    m_process->ReadCStringFromMemory(name_ptr, selector_name, error);

    if (error.Fail() || selector_name.empty()) {
      continue;
    }

    // Get type encoding: const char *method_getTypeEncoding(Method method)
    lldb::addr_t encoding_ptr =
        CallRuntimeFunction("method_getTypeEncoding", method_args);

    std::string type_encoding;
    if (encoding_ptr != LLDB_INVALID_ADDRESS) {
      m_process->ReadCStringFromMemory(encoding_ptr, type_encoding, error);
      if (error.Fail()) {
        type_encoding = "@:"; // Default ObjC method signature
      }
    } else {
      type_encoding = "@:"; // Default ObjC method signature
    }

    // Create method info
    MethodInfo method_info;
    method_info.selector_name = selector_name;
    method_info.type_encoding = type_encoding;
    method_info.implementation = 0; // Not needed for declaration purposes

    methods.push_back(method_info);

    LLDB_LOG(log, "[GNUstepIntrospector] GetInstanceMethods:   -{0} ({1})",
             selector_name, type_encoding);
  }

  // Free the method list
  std::vector<lldb::addr_t> free_args = {method_list};
  CallRuntimeFunction("free", free_args);

  LLDB_LOG(log,
           "[GNUstepIntrospector] GetInstanceMethods: Returning {0} methods",
           methods.size());
  return methods;
}

std::vector<GNUstepObjCRuntimeIntrospector::MethodInfo>
GNUstepObjCRuntimeIntrospector::GetClassMethods(lldb::addr_t class_ptr) {
  std::vector<MethodInfo> methods;

  if (!m_process || class_ptr == LLDB_INVALID_ADDRESS) {
    return methods;
  }

  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(log,
           "[GNUstepIntrospector] GetClassMethods: Getting class methods for "
           "class 0x{0:x}",
           class_ptr);

  // KEY INSIGHT: Class methods are stored in the metaclass as instance methods
  // So we need to get the metaclass ISA from the class pointer and call
  // GetInstanceMethods on it

  Status error;
  lldb::addr_t metaclass_ptr =
      m_process->ReadPointerFromMemory(class_ptr, error);

  if (error.Fail() || metaclass_ptr == LLDB_INVALID_ADDRESS) {
    LLDB_LOG(log, "[GNUstepIntrospector] GetClassMethods: Failed to read "
                  "metaclass pointer");
    return methods;
  }

  LLDB_LOG(log,
           "[GNUstepIntrospector] GetClassMethods: Metaclass pointer: 0x{0:x}",
           metaclass_ptr);

  // Get instance methods from the metaclass (which are the class methods of the
  // original class)
  methods = GetInstanceMethods(metaclass_ptr);

  // Log them as class methods
  for (const auto &method : methods) {
    LLDB_LOG(log, "[GNUstepIntrospector] GetClassMethods:   +{0} ({1})",
             method.selector_name, method.type_encoding);
  }

  LLDB_LOG(log,
           "[GNUstepIntrospector] GetClassMethods: Returning {0} class methods",
           methods.size());
  return methods;
}

lldb::addr_t GNUstepObjCRuntimeIntrospector::CallRuntimeFunction(
    const std::string &function_name, const std::vector<lldb::addr_t> &args) {

  if (!m_runtime_caller) {
    return LLDB_INVALID_ADDRESS;
  }

  // Delegate to the consolidated runtime function caller
  lldb::addr_t result =
      m_runtime_caller->CallRuntimeFunction(function_name, args);
  return result;
}

// Runtime class enumeration methods for dynamic formatter registration

// === Enhanced Runtime API Implementation (merged from GNUstepRuntimeV2API) ===

namespace {

// Helper to create error messages
llvm::Error CreateError(const char *format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  return llvm::createStringError(llvm::inconvertibleErrorCode(), buffer);
}

} // anonymous namespace

llvm::Expected<std::string>
GNUstepObjCRuntimeIntrospector::ReadCStringFromTarget(lldb::addr_t addr) {
  if (addr == 0 || addr == LLDB_INVALID_ADDRESS) {
    return CreateError("Invalid address");
  }

  char buffer[1024];
  Status error;
  m_process->ReadCStringFromMemory(addr, buffer, sizeof(buffer), error);

  if (error.Fail()) {
    return CreateError("Failed to read string: %s", error.AsCString());
  }

  return std::string(buffer);
}

llvm::Expected<std::vector<uint8_t>>
GNUstepObjCRuntimeIntrospector::ReadMemory(lldb::addr_t addr, size_t size) {
  if (addr == 0 || addr == LLDB_INVALID_ADDRESS) {
    return CreateError("Invalid address");
  }

  std::vector<uint8_t> buffer(size);
  Status error;
  size_t bytes_read = m_process->ReadMemory(addr, buffer.data(), size, error);

  if (error.Fail() || bytes_read != size) {
    return CreateError("Failed to read memory: %s", error.AsCString());
  }

  return buffer;
}

llvm::Expected<std::vector<GNUstepObjCRuntimeIntrospector::IvarInfo>>
GNUstepObjCRuntimeIntrospector::GetAllIvarsIncludingInherited(Class cls) {
  std::vector<IvarInfo> all_ivars;

  if (!cls || cls == (void *)LLDB_INVALID_ADDRESS) {
    return CreateError("Invalid class pointer");
  }

  // Check if we have the runtime functions available
  // Runtime symbols handled by RuntimeFunctionCaller on-demand
  if (!GetRuntimeFunctionAddress("class_copyIvarList") ||
      !GetRuntimeFunctionAddress("ivar_getName") ||
      !GetRuntimeFunctionAddress("ivar_getTypeEncoding") ||
      !GetRuntimeFunctionAddress("ivar_getOffset")) {
    return CreateError("Runtime functions not available");
  }

  // Walk up the class hierarchy and collect ivars
  lldb::addr_t current_class = (lldb::addr_t)cls;

  while (current_class && current_class != LLDB_INVALID_ADDRESS) {

    // Call class_copyIvarList through runtime function pointer
    // We need to allocate memory in the target process for the output count
    Status error;
    lldb::addr_t count_addr = m_process->AllocateMemory(
        sizeof(unsigned int), ePermissionsReadable | ePermissionsWritable,
        error);
    if (error.Fail() || count_addr == LLDB_INVALID_ADDRESS) {
      continue;
    }

    // Initialize count to 0
    unsigned int zero = 0;
    m_process->WriteMemory(count_addr, &zero, sizeof(unsigned int), error);

    std::vector<lldb::addr_t> args = {current_class, count_addr};

    // We need to use CallRuntimeFunction to get the ivar list
    lldb::addr_t ivar_list_ptr =
        CallRuntimeFunction("class_copyIvarList", args);

    // Read the count back from target memory
    unsigned int ivar_count = 0;
    m_process->ReadMemory(count_addr, &ivar_count, sizeof(unsigned int), error);

    // Free the count memory
    m_process->DeallocateMemory(count_addr);

    if (ivar_list_ptr && ivar_list_ptr != LLDB_INVALID_ADDRESS &&
        ivar_count > 0) {

      // Read the ivar array
      for (unsigned int i = 0; i < ivar_count; i++) {
        // Read the Ivar pointer from the array
        Status error;
        lldb::addr_t ivar_ptr = m_process->ReadPointerFromMemory(
            ivar_list_ptr + (i * m_address_size), error);

        if (error.Fail() || ivar_ptr == 0 || ivar_ptr == LLDB_INVALID_ADDRESS) {
          continue;
        }

        // Get ivar name
        args = {ivar_ptr};
        lldb::addr_t name_ptr = CallRuntimeFunction("ivar_getName", args);
        std::string ivar_name;
        if (name_ptr && name_ptr != LLDB_INVALID_ADDRESS) {
          auto name_result = ReadCStringFromTarget(name_ptr);
          if (name_result) {
            ivar_name = *name_result;
          }
        }

        // Get ivar type encoding
        lldb::addr_t type_ptr =
            CallRuntimeFunction("ivar_getTypeEncoding", args);
        std::string type_encoding;
        if (type_ptr && type_ptr != LLDB_INVALID_ADDRESS) {
          auto type_result = ReadCStringFromTarget(type_ptr);
          if (type_result) {
            type_encoding = *type_result;
          }
        }

        // Get ivar offset
        lldb::addr_t offset = CallRuntimeFunction("ivar_getOffset", args);

        IvarInfo info;
        info.name = ivar_name;
        info.type_encoding = type_encoding;
        info.offset = (ptrdiff_t)offset;
        all_ivars.push_back(info);
      }

      // Free the ivar list
      if (GetRuntimeFunctionAddress("free")) {
        args = {ivar_list_ptr};
        CallRuntimeFunction("free", args);
      }
    }

    // Get superclass
    args = {current_class};
    current_class = CallRuntimeFunction("class_getSuperclass", args);
  }

  return all_ivars;
}

llvm::Expected<std::vector<GNUstepObjCRuntimeIntrospector::Class>>
GNUstepObjCRuntimeIntrospector::GetAllClasses() {
  std::vector<Class> classes;

  if (!m_process) {
    return CreateError("No process available");
  }

  // Runtime symbols handled by RuntimeFunctionCaller on-demand
  if (!GetRuntimeFunctionAddress("objc_copyClassList")) {
    return CreateError("objc_copyClassList not available");
  }

  // Allocate memory for count
  Status error;
  lldb::addr_t count_addr = m_process->AllocateMemory(
      sizeof(unsigned int), ePermissionsReadable | ePermissionsWritable, error);
  if (error.Fail() || count_addr == LLDB_INVALID_ADDRESS) {
    return CreateError("Failed to allocate memory for count");
  }

  // Initialize count to 0
  unsigned int zero = 0;
  m_process->WriteMemory(count_addr, &zero, sizeof(unsigned int), error);

  // Call objc_copyClassList
  std::vector<lldb::addr_t> args = {count_addr};
  lldb::addr_t class_list_ptr = CallRuntimeFunction("objc_copyClassList", args);

  // Read the count
  unsigned int count = 0;
  m_process->ReadMemory(count_addr, &count, sizeof(unsigned int), error);

  // Free the count memory
  m_process->DeallocateMemory(count_addr);

  if (class_list_ptr && class_list_ptr != LLDB_INVALID_ADDRESS && count > 0) {
    // Read the class pointers
    for (unsigned int i = 0; i < count; i++) {
      lldb::addr_t class_ptr = m_process->ReadPointerFromMemory(
          class_list_ptr + (i * m_address_size), error);
      if (!error.Fail() && class_ptr && class_ptr != LLDB_INVALID_ADDRESS) {
        classes.push_back((Class)class_ptr);
      }
    }

    // Free the class list
    if (GetRuntimeFunctionAddress("free")) {
      args = {class_list_ptr};
      CallRuntimeFunction("free", args);
    }
  }

  return classes;
}

llvm::Expected<std::vector<std::pair<lldb::addr_t, std::string>>>
GNUstepObjCRuntimeIntrospector::GetAllClassesWithISAs() {
  // Use session-based caching since the class list doesn't change during
  // debugging unless new classes are dynamically loaded (which is rare)
  if (m_all_classes_cached) {
    // Cache is valid for the entire session, return cached data
    static int cache_hits = 0;
    cache_hits++;
    if (cache_hits % 100 == 1) { // Only log every 100th hit to reduce noise
    }
    return m_all_classes_cache;
  }

  // Cache doesn't exist, enumerate classes once for the session
  std::vector<std::pair<lldb::addr_t, std::string>> class_info;

  auto classes_result = GetAllClasses();
  if (!classes_result) {
    return classes_result.takeError();
  }

  for (Class cls : *classes_result) {
    lldb::addr_t isa = (lldb::addr_t)cls;
    std::string name = GetClassName(isa);
    if (!name.empty()) {
      class_info.push_back({isa, name});
    }
  }

  // Update cache - valid for entire session
  m_all_classes_cache = class_info;
  m_all_classes_cached = true;

  return class_info;
}
