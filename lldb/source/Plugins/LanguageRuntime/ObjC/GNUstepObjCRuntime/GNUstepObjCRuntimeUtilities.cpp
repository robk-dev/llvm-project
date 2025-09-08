//===-- GNUstepObjCRuntimeUtilities.cpp ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCRuntimeUtilities.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/ModuleList.h"
#include "lldb/Core/Value.h"
#include "lldb/Expression/DiagnosticManager.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Target/ABI.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/ThreadList.h"
#include "lldb/Target/ThreadPlanCallFunction.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include "lldb/ValueObject/ValueObject.h"
#include <chrono>

using namespace lldb;
using namespace lldb_private;

namespace lldb_private {
namespace gnustep_objc_runtime_utilities {

EvaluateExpressionOptions
MakeSafeExpressionOptions(bool for_utility_expression) {
  EvaluateExpressionOptions opts;
  opts.SetUnwindOnError(true);
  opts.SetIgnoreBreakpoints(true);
  opts.SetTryAllThreads(false);
  opts.SetTimeout(std::chrono::microseconds(2500000)); // 2.5 seconds
  opts.SetTrapExceptions(false);                       // Critical on Windows

  if (for_utility_expression) {
    opts.SetOneThreadTimeout(std::chrono::milliseconds(250));
    opts.SetStopOthers(true);
    opts.SetIsForUtilityExpr(true);
  }

  return opts;
}

// TargetStringAllocator implementation
TargetStringAllocator::TargetStringAllocator(Process *process,
                                             const std::string &str)
    : m_process(process), m_address(LLDB_INVALID_ADDRESS) {

  if (!m_process || str.empty()) {
    m_error = Status::FromErrorString("Invalid process or empty string");
    return;
  }

  // Allocate memory for the string plus null terminator
  m_address = m_process->AllocateMemory(str.length() + 1,
                                        lldb::ePermissionsReadable, m_error);

  if (m_address == LLDB_INVALID_ADDRESS || m_error.Fail()) {
    return;
  }

  // Write the string to target memory
  size_t bytes_written =
      m_process->WriteMemory(m_address, str.c_str(), str.length() + 1, m_error);

  if (bytes_written != str.length() + 1 || m_error.Fail()) {
    // Clean up on write failure
    m_process->DeallocateMemory(m_address);
    m_address = LLDB_INVALID_ADDRESS;
    if (m_error.Success()) {
      m_error = Status::FromErrorString(
          "Failed to write complete string to target memory");
    }
  }
}

TargetStringAllocator::~TargetStringAllocator() { Cleanup(); }

TargetStringAllocator::TargetStringAllocator(
    TargetStringAllocator &&other) noexcept
    : m_process(other.m_process), m_address(other.m_address),
      m_error(std::move(other.m_error)) {
  other.m_process = nullptr;
  other.m_address = LLDB_INVALID_ADDRESS;
}

TargetStringAllocator &
TargetStringAllocator::operator=(TargetStringAllocator &&other) noexcept {
  if (this != &other) {
    Cleanup();

    m_process = other.m_process;
    m_address = other.m_address;
    m_error = std::move(other.m_error);

    other.m_process = nullptr;
    other.m_address = LLDB_INVALID_ADDRESS;
  }
  return *this;
}

void TargetStringAllocator::Cleanup() {
  if (m_process && m_address != LLDB_INVALID_ADDRESS) {
    m_process->DeallocateMemory(m_address);
    m_address = LLDB_INVALID_ADDRESS;
  }
}

// Runtime execution context setup
bool SetupRuntimeExecutionContext(Process *process, ExecutionContext &exe_ctx) {
  if (!process) {
    return false;
  }

  // Get a thread suitable for expression execution
  ThreadSP thread_sp = process->GetThreadList().GetExpressionExecutionThread();
  if (!thread_sp) {
    // Fallback to selected thread
    thread_sp = process->GetThreadList().GetSelectedThread();
  }

  if (!thread_sp) {
    return false;
  }

  // Ensure thread is stopped and safe for function calls
  if (!thread_sp->SafeToCallFunctions()) {
    return false;
  }

  // Build execution context
  thread_sp->CalculateExecutionContext(exe_ctx);

  // Ensure we have a frame
  if (!exe_ctx.GetFramePtr()) {
    StackFrameSP frame_sp =
        thread_sp->GetSelectedFrame(DoNoSelectMostRelevantFrame);
    if (!frame_sp) {
      frame_sp = thread_sp->GetStackFrameAtIndex(0);
    }
    exe_ctx.SetFrameSP(frame_sp);
  }

  return exe_ctx.HasThreadScope() && exe_ctx.HasProcessScope();
}

/// RuntimeFunctionCaller implementation - consolidates 3 different
/// implementations
RuntimeFunctionCaller::RuntimeFunctionCaller(Process *process)
    : m_process(process) {}

lldb::addr_t
RuntimeFunctionCaller::GetRuntimeFunctionAddress(const char *function_name) {
  if (!function_name || !m_process) {
    return LLDB_INVALID_ADDRESS;
  }

  // Check cache first
  auto it = m_symbol_cache.find(function_name);
  if (it != m_symbol_cache.end()) {
    return it->second;
  }

  // Resolve the symbol
  Target &target = m_process->GetTarget();
  const ModuleList &modules = target.GetImages();
  SymbolContextList sc_list;
  modules.FindSymbolsWithNameAndType(ConstString(function_name),
                                     eSymbolTypeCode, sc_list);

  if (sc_list.GetSize() > 0) {
    SymbolContext sc;
    if (sc_list.GetContextAtIndex(0, sc) && sc.symbol) {
      lldb::addr_t addr = sc.symbol->GetAddress().GetLoadAddress(&target);
      if (addr != LLDB_INVALID_ADDRESS) {
        m_symbol_cache[function_name] = addr;
        return addr;
      }
    }
  }

  return LLDB_INVALID_ADDRESS;
}

lldb::ModuleSP RuntimeFunctionCaller::FindObjCModule() const {
  if (!m_process) {
    return lldb::ModuleSP();
  }

  Target &target = m_process->GetTarget();
  const ModuleList &modules = target.GetImages();

  for (uint32_t idx = 0; idx < modules.GetSize(); idx++) {
    ModuleSP module_sp = modules.GetModuleAtIndex(idx);
    if (module_sp) {
      const char *module_name =
          module_sp->GetFileSpec().GetFilename().GetCString();
      if (module_name &&
          (strstr(module_name, "libobjc.so") ||
           strstr(module_name, "libobjc2") ||
           (strstr(module_name, "libobjc-") && strstr(module_name, ".dll")) ||
           (strstr(module_name, "libobjc2") && strstr(module_name, ".dll")))) {
        return module_sp;
      }
    }
  }
  return lldb::ModuleSP();
}

lldb::ModuleSP RuntimeFunctionCaller::FindFoundationModule() const {
  if (!m_process) {
    return lldb::ModuleSP();
  }

  Target &target = m_process->GetTarget();
  const ModuleList &modules = target.GetImages();

  for (uint32_t idx = 0; idx < modules.GetSize(); idx++) {
    ModuleSP module_sp = modules.GetModuleAtIndex(idx);
    if (module_sp) {
      const char *module_name =
          module_sp->GetFileSpec().GetFilename().GetCString();
      if (module_name && (strstr(module_name, "libgnustep-base.so") ||
                          (strstr(module_name, "gnustep-base") &&
                           strstr(module_name, ".dll")))) {
        return module_sp;
      }
    }
  }
  return lldb::ModuleSP();
}

lldb::addr_t
RuntimeFunctionCaller::CallRuntimeFunction(const char *function_name,
                                           const char *string_arg) {
  // Use mutex for both thread safety and reentrancy protection
  std::unique_lock<std::mutex> lock(m_runtime_mutex, std::try_to_lock);
  if (!lock.owns_lock()) {
    return LLDB_INVALID_ADDRESS; // Already in a function call or locked by
                                 // another thread
  }

  if (!m_process || !function_name || !string_arg) {
    return LLDB_INVALID_ADDRESS;
  }

  // Check process state before making runtime call
  if (m_process->GetState() != lldb::eStateStopped) {
    return LLDB_INVALID_ADDRESS;
  }

  // Setup execution context
  ExecutionContext exe_ctx;
  if (!SetupRuntimeExecutionContext(m_process, exe_ctx)) {
    return LLDB_INVALID_ADDRESS;
  }

  // Get type system for building argument types
  TypeSystemClangSP scratch_ts_sp =
      ScratchTypeSystemClang::GetForTarget(exe_ctx.GetTargetRef());
  if (!scratch_ts_sp) {
    return LLDB_INVALID_ADDRESS;
  }

  // Allocate string in target memory
  TargetStringAllocator string_alloc(m_process, string_arg);
  if (!string_alloc.IsValid()) {
    return LLDB_INVALID_ADDRESS;
  }

  // Build argument list
  ValueList arg_values;
  Value string_value;
  string_value.SetValueType(Value::ValueType::LoadAddress);
  string_value.SetCompilerType(scratch_ts_sp->GetCStringType(true));
  string_value.GetScalar() = string_alloc.GetAddress();
  arg_values.PushValue(string_value);

  // Return type is a void pointer (Class)
  CompilerType return_type =
      scratch_ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType();

  // Call the implementation
  Status error;
  lldb::addr_t result = CallRuntimeFunctionImpl(function_name, return_type,
                                                arg_values, exe_ctx, error);

  if (error.Fail()) {
    return LLDB_INVALID_ADDRESS;
  }

  return result;
}

lldb::addr_t RuntimeFunctionCaller::CallRuntimeFunction(
    const std::string &function_name, const std::vector<lldb::addr_t> &args) {
  // Use mutex for both thread safety and reentrancy protection
  std::unique_lock<std::mutex> lock(m_runtime_mutex, std::try_to_lock);
  if (!lock.owns_lock()) {
    return LLDB_INVALID_ADDRESS; // Already in a function call or locked by
                                 // another thread
  }

  if (!m_process || function_name.empty()) {
    return LLDB_INVALID_ADDRESS;
  }

  // Check process state before making runtime call
  if (m_process->GetState() != lldb::eStateStopped) {
    return LLDB_INVALID_ADDRESS;
  }

  // Setup execution context
  ExecutionContext exe_ctx;
  if (!SetupRuntimeExecutionContext(m_process, exe_ctx)) {
    return LLDB_INVALID_ADDRESS;
  }

  // Get type system for building argument types
  TypeSystemClangSP scratch_ts_sp =
      ScratchTypeSystemClang::GetForTarget(exe_ctx.GetTargetRef());
  if (!scratch_ts_sp) {
    return LLDB_INVALID_ADDRESS;
  }

  // Build argument list
  ValueList arg_values;
  for (lldb::addr_t arg : args) {
    Value arg_value;
    CompilerType type;

    // Choose appropriate type based on function
    if (function_name == "objc_lookup_class" ||
        function_name == "objc_getClass" ||
        function_name == "objc_getMetaClass") {
      type = scratch_ts_sp->GetCStringType(true);
    } else {
      type = scratch_ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType();
    }

    arg_value.SetCompilerType(type);
    arg_value.SetValueType(Value::ValueType::Scalar);
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
    return LLDB_INVALID_ADDRESS;
  }

  return result;
}

lldb::addr_t RuntimeFunctionCaller::CallRuntimeFunctionImpl(
    const char *function_name, const CompilerType &return_type, ValueList &args,
    ExecutionContext &exe_ctx, Status &error) {
  // Mutex is already held by caller - no need to lock again

  // Double-check process state
  if (m_process->GetState() != lldb::eStateStopped) {
    error =
        Status::FromErrorString("Process state changed during runtime call");
    return LLDB_INVALID_ADDRESS;
  }

  // Resolve function address
  lldb::addr_t func_addr = GetRuntimeFunctionAddress(function_name);
  if (func_addr == LLDB_INVALID_ADDRESS) {
    error = Status::FromErrorStringWithFormat("Could not resolve function '%s'",
                                              function_name);
    return LLDB_INVALID_ADDRESS;
  }

  // Set up function address
  Address function_address;
  function_address.SetLoadAddress(func_addr, &m_process->GetTarget());

  // Get ABI for function calling conventions
  ABISP abi_sp = m_process->GetABI();
  if (!abi_sp) {
    error = Status::FromErrorString("No ABI available");
    return LLDB_INVALID_ADDRESS;
  }

  // Convert ValueList to ArrayRef<addr_t>
  std::vector<addr_t> arg_addrs;
  for (size_t i = 0; i < args.GetSize(); ++i) {
    Value *val = args.GetValueAtIndex(i);
    if (val) {
      arg_addrs.push_back(val->GetScalar().ULongLong());
    }
  }

  Thread *thread = exe_ctx.GetThreadPtr();
  if (!thread) {
    error = Status::FromErrorString("No thread available for execution");
    return LLDB_INVALID_ADDRESS;
  }

  // Create the call plan
  EvaluateExpressionOptions options = MakeSafeExpressionOptions(true);
  ThreadPlanSP call_plan_sp(
      new ThreadPlanCallFunction(*thread, function_address, return_type,
                                 llvm::ArrayRef<addr_t>(arg_addrs), options));

  if (!call_plan_sp || !call_plan_sp->ValidatePlan(nullptr)) {
    error = Status::FromErrorString("Failed to create valid call plan");
    return LLDB_INVALID_ADDRESS;
  }

  // Execute the function call
  DiagnosticManager diagnostics;
  ExpressionResults result =
      m_process->RunThreadPlan(exe_ctx, call_plan_sp, options, diagnostics);

  if (result != eExpressionCompleted) {
    error = Status::FromErrorStringWithFormat("Function execution failed: %s",
                                              diagnostics.GetString().c_str());
    return LLDB_INVALID_ADDRESS;
  }

  // Get the return value
  ValueObjectSP return_value_sp = call_plan_sp->GetReturnValueObject();
  if (!return_value_sp) {
    error = Status::FromErrorString("No return value available");
    return LLDB_INVALID_ADDRESS;
  }

  return return_value_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
}

lldb::addr_t
RuntimeFunctionCaller::GetSelectorForName(const char *selector_name) {
  if (!selector_name || !m_process)
    return LLDB_INVALID_ADDRESS;

  // Just call the runtime function directly
  return CallRuntimeFunction("sel_getUid", selector_name);
}

lldb::addr_t RuntimeFunctionCaller::CallObjCMethod(lldb::addr_t object_addr,
                                                   const char *selector_name) {
  if (!selector_name || !m_process || object_addr == LLDB_INVALID_ADDRESS)
    return LLDB_INVALID_ADDRESS;

  // Get selector
  lldb::addr_t selector_addr = GetSelectorForName(selector_name);
  if (selector_addr == LLDB_INVALID_ADDRESS) {
    return LLDB_INVALID_ADDRESS;
  }

  // Call the method
  std::vector<lldb::addr_t> args = {object_addr, selector_addr};
  return CallRuntimeFunction("objc_msgSend", args);
}

} // namespace gnustep_objc_runtime_utilities
} // namespace lldb_private
