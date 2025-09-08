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

// RuntimeSymbolCache implementation
RuntimeSymbolCache::RuntimeSymbolCache(Process *process) : m_process(process) {}

lldb::addr_t RuntimeSymbolCache::GetSymbolAddress(const char *symbol_name) {
  if (!symbol_name || !m_process) {
    return LLDB_INVALID_ADDRESS;
  }

  // Check cache first
  auto it = m_symbol_cache.find(symbol_name);
  if (it != m_symbol_cache.end()) {
    return it->second;
  }

  // Resolve and cache
  lldb::addr_t addr = ResolveSymbol(symbol_name);
  m_symbol_cache[symbol_name] = addr;
  return addr;
}

void RuntimeSymbolCache::InvalidateCache() { m_symbol_cache.clear(); }

bool RuntimeSymbolCache::HasEssentialSymbols() const {
  // Check if we have the core symbols needed for basic runtime operations
  static const char *essential_symbols[] = {"objc_getClass", "objc_msgSend",
                                            "object_getClass", "class_getName"};

  for (const char *symbol : essential_symbols) {
    auto it = m_symbol_cache.find(symbol);
    if (it == m_symbol_cache.end() || it->second == LLDB_INVALID_ADDRESS) {
      return false;
    }
  }
  return true;
}

lldb::addr_t RuntimeSymbolCache::ResolveSymbol(const char *symbol_name) {
  if (!m_process || !symbol_name) {
    return LLDB_INVALID_ADDRESS;
  }

  Target &target = m_process->GetTarget();
  const ModuleList &modules = target.GetImages();

  // First try to find in GNUstep runtime modules
  for (uint32_t idx = 0; idx < modules.GetSize(); idx++) {
    ModuleSP module_sp = modules.GetModuleAtIndex(idx);
    if (!module_sp)
      continue;

    const char *module_name =
        module_sp->GetFileSpec().GetFilename().GetCString();
    if (!module_name)
      continue;

    // Check if this is a GNUstep runtime module
    if (strstr(module_name, "libobjc.so") || strstr(module_name, "libobjc2") ||
        strstr(module_name, "libgnustep-base.so") ||
        (strstr(module_name, "libobjc-") && strstr(module_name, ".dll")) ||
        (strstr(module_name, "gnustep-base") && strstr(module_name, ".dll"))) {

      const Symbol *symbol = module_sp->FindFirstSymbolWithNameAndType(
          ConstString(symbol_name), eSymbolTypeCode);

      if (symbol) {
        lldb::addr_t addr = symbol->GetAddress().GetLoadAddress(&target);
        if (addr != LLDB_INVALID_ADDRESS) {
          Log *log = GetLog(LLDBLog::Language);
          LLDB_LOG(log, "[GNUstepUtilities] Found {0} in module {1} at 0x{2:x}",
                   symbol_name, module_name, addr);
          return addr;
        }
      }
    }
  }

  // Fallback: try to find in any module
  SymbolContextList sc_list;
  target.GetImages().FindSymbolsWithNameAndType(ConstString(symbol_name),
                                                eSymbolTypeCode, sc_list);

  if (sc_list.GetSize() > 0) {
    SymbolContext sc;
    if (sc_list.GetContextAtIndex(0, sc) && sc.symbol) {
      lldb::addr_t addr = sc.symbol->GetAddress().GetLoadAddress(&target);
      if (addr != LLDB_INVALID_ADDRESS) {
        Log *log = GetLog(LLDBLog::Language);
        LLDB_LOG(log, "[GNUstepUtilities] Found {0} (fallback) at 0x{1:x}",
                 symbol_name, addr);
        return addr;
      }
    }
  }

  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(log, "[GNUstepUtilities] Failed to resolve symbol: {0}",
           symbol_name);
  return LLDB_INVALID_ADDRESS;
}

/// Logging helper implementations
lldb_private::Log *GNUStepLogger::GetLanguageLog() {
  return GetLog(LLDBLog::Language);
}

lldb_private::Log *GNUStepLogger::GetProcessLog() {
  return GetLog(LLDBLog::Process | LLDBLog::Types);
}

GNUStepLogger::ScopedLogger::ScopedLogger(const char *function_name,
                                          const char *prefix)
    : m_log(GetLanguageLog()), m_prefix(prefix),
      m_function_name(function_name) {
  if (m_log) {
    LLDB_LOG(m_log, "{0} {1}: Starting", m_prefix, m_function_name);
  }
}

GNUStepLogger::ScopedLogger::~ScopedLogger() {
  if (m_log) {
    LLDB_LOG(m_log, "{0} {1}: Finished", m_prefix, m_function_name);
  }
}

/// Symbol resolution helper implementations
const Symbol *
SymbolResolver::FindSymbolWithFallback(const ConstString &symbol_name,
                                       lldb::SymbolType symbol_type,
                                       lldb::ModuleSP preferred_module) {

  lldb_private::Log *log = GetLog(LLDBLog::Language);

  // Try preferred module first
  if (preferred_module) {
    if (const Symbol *symbol = preferred_module->FindFirstSymbolWithNameAndType(
            symbol_name, symbol_type)) {
      if (log) {
        LLDB_LOG(log,
                 "[GNUstepSymbolResolver] Found {0} in preferred module {1} at "
                 "0x{2:x}",
                 symbol_name.GetCString(),
                 preferred_module->GetFileSpec().GetFilename().GetCString(),
                 symbol->GetLoadAddress(&m_target));
      }
      return symbol;
    }
  }

  // Fallback to global search
  SymbolContextList sc_list;
  m_target.GetImages().FindSymbolsWithNameAndType(symbol_name, symbol_type,
                                                  sc_list);

  if (sc_list.GetSize() > 0) {
    SymbolContext sc;
    sc_list.GetContextAtIndex(0, sc);
    if (sc.symbol) {
      if (log) {
        LLDB_LOG(
            log,
            "[GNUstepSymbolResolver] Found {0} via fallback search at 0x{1:x}",
            symbol_name.GetCString(), sc.symbol->GetLoadAddress(&m_target));
      }
      return sc.symbol;
    }
  }

  if (log) {
    LLDB_LOG(log, "[GNUstepSymbolResolver] Failed to resolve symbol: {0}",
             symbol_name.GetCString());
  }
  return nullptr;
}

void SymbolResolver::FindSymbolsAcrossModules(const ConstString &symbol_name,
                                              lldb::SymbolType symbol_type,
                                              SymbolContextList &sc_list) {

  lldb_private::Log *log = GetLog(LLDBLog::Language);
  if (log) {
    LLDB_LOG(log,
             "[GNUstepSymbolResolver] Searching for {0} across all modules",
             symbol_name.GetCString());
  }

  m_target.GetImages().FindSymbolsWithNameAndType(symbol_name, symbol_type,
                                                  sc_list);

  if (log) {
    LLDB_LOG(log, "[GNUstepSymbolResolver] Found {0} instances of {1}",
             sc_list.GetSize(), symbol_name.GetCString());
  }
}

/// RuntimeFunctionCaller implementation - consolidates 3 different implementations
RuntimeFunctionCaller::RuntimeFunctionCaller(Process *process)
    : m_process(process), m_symbol_cache(process), 
      m_formatter_cache(std::make_unique<FormatterCache>()) {}

lldb::addr_t RuntimeFunctionCaller::GetRuntimeFunctionAddress(const char *function_name) {
  return m_symbol_cache.GetSymbolAddress(function_name);
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
      if (module_name && (strstr(module_name, "libobjc.so") ||
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
                          (strstr(module_name, "gnustep-base") && strstr(module_name, ".dll")))) {
        return module_sp;
      }
    }
  }
  return lldb::ModuleSP();
}

lldb::addr_t RuntimeFunctionCaller::CallRuntimeFunction(const char *function_name, const char *string_arg) {
  GNUStepLogger::ScopedLogger logger("CallRuntimeFunction", "[GNUstepUtilities]");
  logger.LogMessage("Calling {0}(\"{1}\")", function_name, string_arg);

  // Apply reentrancy guard
  ReentrancyGuard guard(m_in_function_call);
  if (!guard.IsAcquired()) {
    logger.LogMessage("Reentrancy detected, blocking call");
    return LLDB_INVALID_ADDRESS;
  }

  if (!m_process || !function_name || !string_arg) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Check process state before making runtime call
  ProcessStateGuard state_guard(m_process);
  if (!state_guard.IsValid()) {
    logger.LogMessage("Process not in valid state for runtime call");
    return LLDB_INVALID_ADDRESS;
  }

  // Setup execution context
  ExecutionContext exe_ctx;
  if (!SetupRuntimeExecutionContext(m_process, exe_ctx)) {
    logger.LogMessage("Failed to setup execution context");
    return LLDB_INVALID_ADDRESS;
  }

  // Get type system for building argument types
  TypeSystemClangSP scratch_ts_sp =
      ScratchTypeSystemClang::GetForTarget(exe_ctx.GetTargetRef());
  if (!scratch_ts_sp) {
    logger.LogMessage("Failed to get type system");
    return LLDB_INVALID_ADDRESS;
  }

  // Allocate string in target memory
  TargetStringAllocator string_alloc(m_process, string_arg);
  if (!string_alloc.IsValid()) {
    logger.LogMessage("Failed to allocate string: {0}", string_alloc.GetError().AsCString());
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
  CompilerType return_type = scratch_ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType();

  // Call the implementation
  Status error;
  lldb::addr_t result = CallRuntimeFunctionImpl(function_name, return_type, arg_values, exe_ctx, error);

  if (error.Fail()) {
    logger.LogMessage("Call failed: {0}", error.AsCString());
    return LLDB_INVALID_ADDRESS;
  }

  logger.LogMessage("Call succeeded, returned 0x{0:x}", result);
  return result;
}

lldb::addr_t RuntimeFunctionCaller::CallRuntimeFunction(const std::string &function_name, 
                                                        const std::vector<lldb::addr_t> &args) {
  GNUStepLogger::ScopedLogger logger("CallRuntimeFunction", "[GNUstepUtilities]");
  logger.LogMessage("Calling {0} with {1} args", function_name, args.size());

  // Apply reentrancy guard
  ReentrancyGuard guard(m_in_function_call);
  if (!guard.IsAcquired()) {
    logger.LogMessage("Reentrancy detected, blocking call");
    return LLDB_INVALID_ADDRESS;
  }

  if (!m_process || function_name.empty()) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Check process state before making runtime call
  ProcessStateGuard state_guard(m_process);
  if (!state_guard.IsValid()) {
    logger.LogMessage("Process not in valid state for runtime call");
    return LLDB_INVALID_ADDRESS;
  }

  // Setup execution context
  ExecutionContext exe_ctx;
  if (!SetupRuntimeExecutionContext(m_process, exe_ctx)) {
    logger.LogMessage("Failed to setup execution context");
    return LLDB_INVALID_ADDRESS;
  }

  // Get type system for building argument types
  TypeSystemClangSP scratch_ts_sp =
      ScratchTypeSystemClang::GetForTarget(exe_ctx.GetTargetRef());
  if (!scratch_ts_sp) {
    logger.LogMessage("Failed to get type system");
    return LLDB_INVALID_ADDRESS;
  }

  // Build argument list
  ValueList arg_values;
  for (lldb::addr_t arg : args) {
    Value arg_value;
    CompilerType type;
    
    // Choose appropriate type based on function
    if (function_name == "objc_lookup_class" || function_name == "objc_getClass" ||
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
  CompilerType return_type = scratch_ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType();

  // Call the implementation
  Status error;
  lldb::addr_t result = CallRuntimeFunctionImpl(function_name.c_str(), return_type, arg_values, exe_ctx, error);

  if (error.Fail()) {
    logger.LogMessage("Call failed: {0}", error.AsCString());
    return LLDB_INVALID_ADDRESS;
  }

  logger.LogMessage("Call succeeded, returned 0x{0:x}", result);
  return result;
}

lldb::addr_t RuntimeFunctionCaller::CallRuntimeFunctionImpl(const char *function_name,
                                                            const CompilerType &return_type,
                                                            ValueList &args,
                                                            ExecutionContext &exe_ctx,
                                                            Status &error) {
  // Lock mutex for thread-safe runtime call
  std::lock_guard<std::mutex> lock(m_runtime_mutex);
  
  // Double-check process state inside the lock
  ProcessStateGuard state_guard(m_process);
  if (!state_guard.IsValid()) {
    error = Status::FromErrorString("Process state changed during runtime call");
    return LLDB_INVALID_ADDRESS;
  }
  
  // Resolve function address
  lldb::addr_t func_addr = GetRuntimeFunctionAddress(function_name);
  if (func_addr == LLDB_INVALID_ADDRESS) {
    error = Status::FromErrorStringWithFormat("Could not resolve function '%s'", function_name);
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
  ThreadPlanSP call_plan_sp(new ThreadPlanCallFunction(
      *thread, function_address, return_type, llvm::ArrayRef<addr_t>(arg_addrs), options));

  if (!call_plan_sp || !call_plan_sp->ValidatePlan(nullptr)) {
    error = Status::FromErrorString("Failed to create valid call plan");
    return LLDB_INVALID_ADDRESS;
  }

  // Execute the function call
  DiagnosticManager diagnostics;
  ExpressionResults result = m_process->RunThreadPlan(exe_ctx, call_plan_sp, options, diagnostics);

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

lldb::addr_t RuntimeFunctionCaller::GetSelectorForName(const char *selector_name) {
  if (!selector_name || !m_process)
    return LLDB_INVALID_ADDRESS;
    
  // Check cache first
  std::string sel_str(selector_name);
  auto it = m_formatter_cache->selector_cache.find(sel_str);
  if (it != m_formatter_cache->selector_cache.end()) {
    // Cache hit - return cached selector
    return it->second;
  }
  
  // Cache miss - call runtime and cache result
  lldb::addr_t selector = CallRuntimeFunction("sel_getUid", selector_name);
  if (selector != LLDB_INVALID_ADDRESS) {
    m_formatter_cache->selector_cache[sel_str] = selector;
  }
  
  return selector;
}

lldb::addr_t RuntimeFunctionCaller::CallObjCMethod(lldb::addr_t object_addr, const char *selector_name) {
  if (!selector_name || !m_process || object_addr == LLDB_INVALID_ADDRESS)
    return LLDB_INVALID_ADDRESS;
    
  // Invalidate cache if needed
  m_formatter_cache->InvalidateIfNeeded(m_process);
  
  // Check if this is a cacheable method (allKeys, allObjects, count)
  std::string sel_str(selector_name);
  bool is_cacheable = (sel_str == "allKeys" || sel_str == "allObjects" || 
                       sel_str == "count" || sel_str == "allValues");
  
  if (is_cacheable) {
    // Check method result cache
    auto cache_key = std::make_pair(object_addr, sel_str);
    auto it = m_formatter_cache->method_cache.find(cache_key);
    if (it != m_formatter_cache->method_cache.end()) {
      // Cache hit - return cached result
      return it->second;
    }
  }
  
  // Get selector (will use cache if available)
  lldb::addr_t selector_addr = GetSelectorForName(selector_name);
  if (selector_addr == LLDB_INVALID_ADDRESS) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Call the method
  std::vector<lldb::addr_t> args = {object_addr, selector_addr};
  lldb::addr_t result = CallRuntimeFunction("objc_msgSend", args);
  
  // Cache result if method is cacheable and call succeeded
  // Don't cache: LLDB_INVALID_ADDRESS, null, or suspiciously low addresses
  if (is_cacheable && result != LLDB_INVALID_ADDRESS && result != 0) {
    // Additional validation: don't cache if the address looks invalid
    // (e.g., too low to be a heap address)
    if (result > 0x1000) {  // Basic sanity check for valid heap address
      auto cache_key = std::make_pair(object_addr, sel_str);
      m_formatter_cache->method_cache[cache_key] = result;
    }
  }
  
  return result;
}

std::vector<lldb::addr_t> RuntimeFunctionCaller::GetArrayElements(
    lldb::addr_t array_addr, uint32_t start_idx, uint32_t count) {
  
  if (!m_process || array_addr == LLDB_INVALID_ADDRESS)
    return {};
    
  // Invalidate cache if needed
  m_formatter_cache->InvalidateIfNeeded(m_process);
  
  // Check if we have this array cached
  auto& cache_entry = m_formatter_cache->collection_cache.array_cache[array_addr];
  
  // Determine batch boundaries
  const uint32_t BATCH_SIZE = 20;
  uint32_t batch_start = (start_idx / BATCH_SIZE) * BATCH_SIZE;
  uint32_t batch_size = std::min(BATCH_SIZE, std::max(count, BATCH_SIZE));
  
  // Check if we already have this batch
  if (cache_entry.HasElement(start_idx)) {
    // Build result from cache
    std::vector<lldb::addr_t> result;
    for (uint32_t i = start_idx; i < start_idx + count; i++) {
      lldb::addr_t elem = cache_entry.GetElement(i);
      if (elem != LLDB_INVALID_ADDRESS) {
        result.push_back(elem);
        m_formatter_cache->collection_cache.stats.hits++;
      } else {
        break;  // No more cached elements
      }
    }
    if (result.size() == count)
      return result;  // Got everything from cache
  }
  
  m_formatter_cache->collection_cache.stats.misses++;
  m_formatter_cache->collection_cache.stats.batch_fetches++;
  
  // Need to fetch - try direct memory access first (GNUstep GSArray structure)
  // This would require knowing the internal structure of GSArray
  // For now, fall back to runtime calls
  
  // Fetch a batch of elements using objectAtIndex:
  lldb::addr_t selector_addr = GetSelectorForName("objectAtIndex:");
  if (selector_addr == LLDB_INVALID_ADDRESS)
    return {};
    
  std::vector<lldb::addr_t> batch_elements;
  batch_elements.reserve(batch_size);
  
  for (uint32_t i = batch_start; i < batch_start + batch_size; i++) {
    std::vector<lldb::addr_t> args = {array_addr, selector_addr, i};
    lldb::addr_t element = CallRuntimeFunction("objc_msgSend", args);
    
    if (element == LLDB_INVALID_ADDRESS)
      break;  // Reached end of array or error
      
    batch_elements.push_back(element);
  }
  
  // Cache the batch
  if (!batch_elements.empty()) {
    cache_entry.AddBatch(batch_start, std::move(batch_elements), m_process->GetStopID());
    cache_entry.last_accessed_stop_id = m_process->GetStopID();
  }
  
  // Return requested subset
  std::vector<lldb::addr_t> result;
  for (uint32_t i = start_idx; i < start_idx + count; i++) {
    lldb::addr_t elem = cache_entry.GetElement(i);
    if (elem != LLDB_INVALID_ADDRESS)
      result.push_back(elem);
    else
      break;
  }
  
  return result;
}

std::pair<std::vector<lldb::addr_t>, std::vector<lldb::addr_t>>
RuntimeFunctionCaller::GetDictionaryKeysAndValues(lldb::addr_t dict_addr) {
  
  if (!m_process || dict_addr == LLDB_INVALID_ADDRESS)
    return {{}, {}};
    
  // Invalidate cache if needed
  m_formatter_cache->InvalidateIfNeeded(m_process);
  
  // Check cache
  auto it = m_formatter_cache->collection_cache.dict_cache.find(dict_addr);
  if (it != m_formatter_cache->collection_cache.dict_cache.end()) {
    uint32_t current_stop = m_process->GetStopID();
    // Allow cache to persist for a few stops
    if (current_stop <= it->second.cached_at_stop_id + 5) {
      m_formatter_cache->collection_cache.stats.hits++;
      return {it->second.keys, it->second.values};
    }
  }
  
  m_formatter_cache->collection_cache.stats.misses++;
  m_formatter_cache->collection_cache.stats.batch_fetches++;
  
  // Fetch all keys
  lldb::addr_t all_keys = CallObjCMethod(dict_addr, "allKeys");
  if (all_keys == LLDB_INVALID_ADDRESS)
    return {{}, {}};
    
  // Get count of keys
  lldb::addr_t count_result = CallObjCMethod(all_keys, "count");
  if (count_result == LLDB_INVALID_ADDRESS || count_result > 10000)
    return {{}, {}};
    
  uint32_t count = (uint32_t)count_result;
  
  std::vector<lldb::addr_t> keys;
  std::vector<lldb::addr_t> values;
  keys.reserve(count);
  values.reserve(count);
  
  // Batch fetch keys
  auto key_elements = GetArrayElements(all_keys, 0, count);
  keys = std::move(key_elements);
  
  // Batch fetch values for each key
  lldb::addr_t selector_addr = GetSelectorForName("objectForKey:");
  if (selector_addr != LLDB_INVALID_ADDRESS) {
    for (lldb::addr_t key : keys) {
      std::vector<lldb::addr_t> args = {dict_addr, selector_addr, key};
      lldb::addr_t value = CallRuntimeFunction("objc_msgSend", args);
      values.push_back(value);
    }
  }
  
  // Cache the results
  FormatterCache::CollectionCache::DictCacheEntry entry;
  entry.keys = keys;
  entry.values = values;
  entry.cached_at_stop_id = m_process->GetStopID();
  m_formatter_cache->collection_cache.dict_cache[dict_addr] = std::move(entry);
  
  return {keys, values};
}

std::vector<lldb::addr_t> RuntimeFunctionCaller::GetSetElements(lldb::addr_t set_addr) {
  
  if (!m_process || set_addr == LLDB_INVALID_ADDRESS)
    return {};
    
  // Invalidate cache if needed
  m_formatter_cache->InvalidateIfNeeded(m_process);
  
  // Check cache
  auto it = m_formatter_cache->collection_cache.set_cache.find(set_addr);
  if (it != m_formatter_cache->collection_cache.set_cache.end()) {
    uint32_t current_stop = m_process->GetStopID();
    // Allow cache to persist for a few stops
    if (current_stop <= it->second.cached_at_stop_id + 5) {
      m_formatter_cache->collection_cache.stats.hits++;
      return it->second.elements;
    }
  }
  
  m_formatter_cache->collection_cache.stats.misses++;
  m_formatter_cache->collection_cache.stats.batch_fetches++;
  
  // Get all objects from set
  lldb::addr_t all_objects = CallObjCMethod(set_addr, "allObjects");
  if (all_objects == LLDB_INVALID_ADDRESS)
    return {};
    
  // Get count
  lldb::addr_t count_result = CallObjCMethod(all_objects, "count");
  if (count_result == LLDB_INVALID_ADDRESS || count_result > 10000)
    return {};
    
  uint32_t count = (uint32_t)count_result;
  
  // Fetch all elements from the array
  auto elements = GetArrayElements(all_objects, 0, count);
  
  // Cache the results
  FormatterCache::CollectionCache::SetCacheEntry entry;
  entry.elements = elements;
  entry.cached_at_stop_id = m_process->GetStopID();
  m_formatter_cache->collection_cache.set_cache[set_addr] = std::move(entry);
  
  return elements;
}

} // namespace gnustep_objc_runtime_utilities
} // namespace lldb_private
