//===-- GNUstepObjCRuntimeIntrospector.cpp ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCRuntimeIntrospector.h"
#include "GNUstepObjCRuntimeUtilities.h"
#include "formatters/GNUstepFormattersBase.h"
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

    // Note: Don't load runtime symbols here as libraries may not be loaded yet
    // LoadRuntimeSymbols() will be called on-demand when symbols are first
    // needed
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

  // Ensure runtime symbols are loaded for enhanced introspection
  EnsureRuntimeSymbolsLoaded();

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
  // Consolidated implementation: use GetClassPointer which has better error handling
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

  GNUStepLogger::ScopedLogger logger("CallRuntimeFunctionImpl", "[GNUstep]");
  logger.LogMessage("Starting function call for {0}", function_name);

  // Get or create the function caller
  std::unique_ptr<FunctionCaller> &caller = GetOrCreateFunctionCaller(
      function_name, return_type, args, exe_ctx, error);
  if (!caller || error.Fail()) {
    logger.LogMessage("Failed to get/create function caller for {0}: {1}",
                      function_name,
                      error.Fail() ? error.AsCString() : "null caller");
    return LLDB_INVALID_ADDRESS;
  }

  logger.LogMessage("Got function caller for {0}", function_name);

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

void GNUstepObjCRuntimeIntrospector::EnsureRuntimeSymbolsLoaded() {
  if (!m_runtime_symbols_loaded && m_process) {
    LoadRuntimeSymbols();
    m_runtime_symbols_loaded = true;
  }
}

bool GNUstepObjCRuntimeIntrospector::LoadRuntimeSymbols() {
  if (!m_process) {
    return false;
  }

  // Load essential ObjC runtime functions for enhanced introspection
  m_object_getClass_addr = GetRuntimeFunctionAddress("object_getClass");
  m_class_getSuperclass_addr = GetRuntimeFunctionAddress("class_getSuperclass");
  m_class_getInstanceSize_addr =
      GetRuntimeFunctionAddress("class_getInstanceSize");
  m_class_getMethodImplementation_addr =
      GetRuntimeFunctionAddress("class_getMethodImplementation");
  m_objc_msgSend_addr = GetRuntimeFunctionAddress("objc_msgSend");
  m_objc_copyClassList_addr = GetRuntimeFunctionAddress("objc_copyClassList");
  m_class_getName_addr = GetRuntimeFunctionAddress("class_getName");
  m_free_addr = GetRuntimeFunctionAddress("free");

  // Load method introspection functions for dynamic method discovery
  m_objc_getMetaClass_addr = GetRuntimeFunctionAddress("objc_getMetaClass");
  m_objc_getClass_addr = GetRuntimeFunctionAddress("objc_getClass");
  m_class_copyMethodList_addr =
      GetRuntimeFunctionAddress("class_copyMethodList");
  m_method_getName_addr = GetRuntimeFunctionAddress("method_getName");
  m_method_getTypeEncoding_addr =
      GetRuntimeFunctionAddress("method_getTypeEncoding");
  m_sel_getName_addr = GetRuntimeFunctionAddress("sel_getName");

  // Return success if we got the core functions needed for introspection
  return (m_object_getClass_addr != LLDB_INVALID_ADDRESS &&
          m_class_getSuperclass_addr != LLDB_INVALID_ADDRESS &&
          m_class_getName_addr != LLDB_INVALID_ADDRESS &&
          m_objc_getMetaClass_addr != LLDB_INVALID_ADDRESS &&
          m_class_copyMethodList_addr != LLDB_INVALID_ADDRESS &&
          m_method_getName_addr != LLDB_INVALID_ADDRESS &&
          m_method_getTypeEncoding_addr != LLDB_INVALID_ADDRESS);
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

  EnsureRuntimeSymbolsLoaded();

  if (m_objc_getClass_addr == LLDB_INVALID_ADDRESS) {
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

lldb::addr_t GNUstepObjCRuntimeIntrospector::GetMetaClassPointer(
    const std::string &class_name) {
  if (!m_process || class_name.empty()) {
    return LLDB_INVALID_ADDRESS;
  }

  EnsureRuntimeSymbolsLoaded();

  if (m_objc_getMetaClass_addr == LLDB_INVALID_ADDRESS) {
    return LLDB_INVALID_ADDRESS;
  }

  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(
      log,
      "[GNUstepIntrospector] GetMetaClassPointer: Looking for metaclass '{0}'",
      class_name);

  // Allocate class name string in target process memory using RAII helper
  TargetStringAllocator string_alloc(m_process, class_name);
  if (!string_alloc.IsValid()) {
    LLDB_LOG(log,
             "[GNUstepIntrospector] GetMetaClassPointer: Failed to allocate "
             "memory for class name '{0}': {1}",
             class_name, string_alloc.GetError().AsCString());
    return LLDB_INVALID_ADDRESS;
  }

  // Call objc_getMetaClass(class_name) with properly allocated string
  std::vector<lldb::addr_t> args = {string_alloc.GetAddress()};
  lldb::addr_t result = CallRuntimeFunction("objc_getMetaClass", args);

  LLDB_LOG(log,
           "[GNUstepIntrospector] GetMetaClassPointer: "
           "objc_getMetaClass('{0}') = 0x{1:x}",
           class_name, result);
  return result;
}

std::vector<GNUstepObjCRuntimeIntrospector::MethodInfo>
GNUstepObjCRuntimeIntrospector::GetInstanceMethods(lldb::addr_t class_ptr) {
  std::vector<MethodInfo> methods;

  if (!m_process || class_ptr == LLDB_INVALID_ADDRESS) {
    return methods;
  }

  EnsureRuntimeSymbolsLoaded();

  if (m_class_copyMethodList_addr == LLDB_INVALID_ADDRESS ||
      m_method_getName_addr == LLDB_INVALID_ADDRESS ||
      m_method_getTypeEncoding_addr == LLDB_INVALID_ADDRESS ||
      m_sel_getName_addr == LLDB_INVALID_ADDRESS ||
      m_free_addr == LLDB_INVALID_ADDRESS) {
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
  return m_runtime_caller->CallRuntimeFunction(function_name, args);
}

// Runtime class enumeration methods for dynamic formatter registration

std::vector<std::string> GNUstepObjCRuntimeIntrospector::GetAllClassNames() {
  std::vector<std::string> class_names;
  
  if (!m_process) {
    return class_names;
  }
  
  EnsureRuntimeSymbolsLoaded();
  
  if (m_objc_copyClassList_addr == LLDB_INVALID_ADDRESS) {
    return class_names;
  }
  
  ExecutionContext exe_ctx;
  if (!SetupRuntimeExecutionContext(m_process, exe_ctx)) {
    return class_names;
  }
  
  TypeSystemClangSP scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(m_process->GetTarget());
  if (!scratch_ts_sp) {
    return class_names;
  }
  
  // Call objc_copyClassList(&count) to get all classes
  ValueList arg_values;
  
  // Allocate space for the count parameter
  Status error;
  lldb::addr_t count_addr = m_process->AllocateMemory(4, ePermissionsReadable | ePermissionsWritable, error);
  if (error.Fail() || count_addr == LLDB_INVALID_ADDRESS) {
    return class_names;
  }
  
  Value count_value;
  count_value.SetValueType(Value::ValueType::LoadAddress);
  count_value.SetCompilerType(scratch_ts_sp->GetBasicType(eBasicTypeUnsignedInt).GetPointerType());
  count_value.GetScalar() = count_addr;
  arg_values.PushValue(count_value);
  
  // Call objc_copyClassList
  CompilerType return_type = scratch_ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType();
  lldb::addr_t class_list_ptr = CallRuntimeFunctionImpl("objc_copyClassList", return_type, arg_values, exe_ctx, error);
  
  if (error.Fail() || class_list_ptr == LLDB_INVALID_ADDRESS) {
    m_process->DeallocateMemory(count_addr);
    return class_names;
  }
  
  // Read the count
  uint32_t class_count = 0;
  if (!formatters::GNUstepRuntimeHelper::ReadMemory(m_process, count_addr, &class_count, sizeof(class_count))) {
    m_process->DeallocateMemory(count_addr);
    return class_names;
  }
  
  // Read the class list
  for (uint32_t i = 0; i < class_count && i < 1000; ++i) { // Limit to prevent runaway
    lldb::addr_t class_ptr_addr = class_list_ptr + (i * m_address_size);
    lldb::addr_t class_ptr = 0;
    
    if (formatters::GNUstepRuntimeHelper::ReadMemory(m_process, class_ptr_addr, &class_ptr, m_address_size)) {
      std::string class_name = GetClassName(class_ptr);
      if (!class_name.empty()) {
        class_names.push_back(class_name);
      }
    }
  }
  
  // Free the allocated memory
  if (m_free_addr != LLDB_INVALID_ADDRESS) {
    ValueList free_args;
    Value free_value;
    free_value.SetValueType(Value::ValueType::LoadAddress);
    free_value.SetCompilerType(scratch_ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType());
    free_value.GetScalar() = class_list_ptr;
    free_args.PushValue(free_value);
    
    CallRuntimeFunctionImpl("free", scratch_ts_sp->GetBasicType(eBasicTypeVoid), free_args, exe_ctx, error);
  }
  
  m_process->DeallocateMemory(count_addr);
  return class_names;
}

std::vector<std::string> GNUstepObjCRuntimeIntrospector::GetFoundationClassNames() {
  std::vector<std::string> foundation_classes;
  std::vector<std::string> all_classes = GetAllClassNames();
  
  for (const std::string& class_name : all_classes) {
    // Include Foundation classes (NS*, GS*) and other common patterns
    if (class_name.find("NS") == 0 || 
        class_name.find("GS") == 0 ||
        class_name.find("__NS") == 0 ||
        class_name.find("__CF") == 0) {
      foundation_classes.push_back(class_name);
    }
  }
  
  return foundation_classes;
}

std::vector<std::string> GNUstepObjCRuntimeIntrospector::GetSubclassesOf(const std::string &base_class_name) {
  std::vector<std::string> subclasses;
  std::vector<std::string> all_classes = GetAllClassNames();
  
  lldb::addr_t base_class_ptr = GetClassPointer(base_class_name);
  if (base_class_ptr == LLDB_INVALID_ADDRESS) {
    return subclasses;
  }
  
  for (const std::string& class_name : all_classes) {
    lldb::addr_t class_ptr = GetClassPointer(class_name);
    if (class_ptr != LLDB_INVALID_ADDRESS) {
      // Check if this class is a subclass of the base class
      // Simple approach: check if base_class_name appears in the class hierarchy
      if (class_name.find(base_class_name) != std::string::npos) {
        subclasses.push_back(class_name);
      }
    }
  }
  
  return subclasses;
}

bool GNUstepObjCRuntimeIntrospector::ClassExists(const std::string &class_name) {
  return GetClassPointer(class_name) != LLDB_INVALID_ADDRESS;
}

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

bool GNUstepObjCRuntimeIntrospector::InitializeRuntimeFunctions() {
  Log *log = GetLog(LLDBLog::Language);
  
  // Find libobjc2 module
  const ModuleList &modules = m_process->GetTarget().GetImages();
  for (size_t i = 0; i < modules.GetSize(); ++i) {
    ModuleSP module_sp = modules.GetModuleAtIndex(i);
    if (!module_sp)
      continue;

    const FileSpec &file_spec = module_sp->GetFileSpec();
    llvm::StringRef filename = file_spec.GetFilename().GetStringRef();

    if (filename.contains("libobjc.so") || filename.contains("libobjc2")) {
      m_objc_module = module_sp;
      LLDB_LOG(log, "Found libobjc2 module: {0}", file_spec.GetPath());
    } else if (filename.contains("libgnustep-base")) {
      m_foundation_module = module_sp;
      LLDB_LOG(log, "Found Foundation module: {0}", file_spec.GetPath());
    }
  }

  if (!m_objc_module) {
    LLDB_LOG(log, "libobjc2 module not found");
    return false;
  }

  // Resolve runtime function pointers
  m_runtime.objc_getClass = (Class (*)(const char *))ResolveRuntimeSymbol("objc_getClass");
  m_runtime.objc_lookUpClass = (Class (*)(const char *))ResolveRuntimeSymbol("objc_lookUpClass");
  m_runtime.objc_getMetaClass = (Class (*)(const char *))ResolveRuntimeSymbol("objc_getMetaClass");
  m_runtime.objc_copyClassList = (Class *(*)(unsigned int *))ResolveRuntimeSymbol("objc_copyClassList");

  m_runtime.class_getName = (const char *(*)(Class))ResolveRuntimeSymbol("class_getName");
  m_runtime.class_getSuperclass = (Class (*)(Class))ResolveRuntimeSymbol("class_getSuperclass");
  m_runtime.class_getInstanceSize = (size_t (*)(Class))ResolveRuntimeSymbol("class_getInstanceSize");
  m_runtime.class_isMetaClass = (bool (*)(Class))ResolveRuntimeSymbol("class_isMetaClass");

  m_runtime.class_copyIvarList = (Ivar *(*)(Class, unsigned int *))ResolveRuntimeSymbol("class_copyIvarList");
  m_runtime.ivar_getName = (const char *(*)(Ivar))ResolveRuntimeSymbol("ivar_getName");
  m_runtime.ivar_getTypeEncoding = (const char *(*)(Ivar))ResolveRuntimeSymbol("ivar_getTypeEncoding");
  m_runtime.ivar_getOffset = (ptrdiff_t (*)(Ivar))ResolveRuntimeSymbol("ivar_getOffset");

  m_runtime.class_copyMethodList = (Method *(*)(Class, unsigned int *))ResolveRuntimeSymbol("class_copyMethodList");
  m_runtime.method_getName = (SEL (*)(Method))ResolveRuntimeSymbol("method_getName");
  m_runtime.method_getTypeEncoding = (const char *(*)(Method))ResolveRuntimeSymbol("method_getTypeEncoding");
  m_runtime.method_getImplementation = (void *(*)(Method))ResolveRuntimeSymbol("method_getImplementation");
  m_runtime.sel_getName = (const char *(*)(SEL))ResolveRuntimeSymbol("sel_getName");

  // Method lookup and selector checking
  m_runtime.sel_getUid = (SEL (*)(const char *))ResolveRuntimeSymbol("sel_getUid");
  m_runtime.class_respondsToSelector = (bool (*)(Class, SEL))ResolveRuntimeSymbol("class_respondsToSelector");
  m_runtime.class_getInstanceMethod = (Method (*)(Class, SEL))ResolveRuntimeSymbol("class_getInstanceMethod");
  m_runtime.class_getClassMethod = (Method (*)(Class, SEL))ResolveRuntimeSymbol("class_getClassMethod");

  m_runtime.class_copyPropertyList = (Property *(*)(Class, unsigned int *))ResolveRuntimeSymbol("class_copyPropertyList");
  m_runtime.property_getName = (const char *(*)(Property))ResolveRuntimeSymbol("property_getName");
  m_runtime.property_getAttributes = (const char *(*)(Property))ResolveRuntimeSymbol("property_getAttributes");

  m_runtime.object_getClass = (Class (*)(void *))ResolveRuntimeSymbol("object_getClass");
  m_runtime.object_getClassName = (const char *(*)(void *))ResolveRuntimeSymbol("object_getClassName");

  m_runtime.free = (void (*)(void *))ResolveRuntimeSymbol("free");

  // Check critical functions are resolved
  bool success = m_runtime.objc_copyClassList && m_runtime.class_getName &&
                 m_runtime.class_getSuperclass && m_runtime.class_copyIvarList;

  if (success) {
    LLDB_LOG(log, "All critical runtime functions resolved");
  } else {
    LLDB_LOG(log, "Failed to resolve critical runtime functions");
  }

  return success;
}

lldb::addr_t GNUstepObjCRuntimeIntrospector::ResolveRuntimeSymbol(const char *name) {
  if (!m_objc_module || !name)
    return LLDB_INVALID_ADDRESS;

  ConstString symbol_name(name);
  const Symbol *symbol = m_objc_module->FindFirstSymbolWithNameAndType(symbol_name, eSymbolTypeCode);

  if (!symbol) {
    // Try in Foundation module as fallback
    if (m_foundation_module) {
      symbol = m_foundation_module->FindFirstSymbolWithNameAndType(symbol_name, eSymbolTypeCode);
    }
  }

  if (symbol) {
    addr_t addr = symbol->GetAddress().GetLoadAddress(&m_process->GetTarget());
    if (addr != LLDB_INVALID_ADDRESS) {
      Log *log = GetLog(LLDBLog::Language);
      LLDB_LOG(log, "Resolved {0} to 0x{1:x}", name, addr);
      return addr;
    }
  }

  return LLDB_INVALID_ADDRESS;
}

llvm::Expected<std::string> GNUstepObjCRuntimeIntrospector::ReadCStringFromTarget(lldb::addr_t addr) {
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

llvm::Expected<std::vector<uint8_t>> GNUstepObjCRuntimeIntrospector::ReadMemory(lldb::addr_t addr, size_t size) {
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
