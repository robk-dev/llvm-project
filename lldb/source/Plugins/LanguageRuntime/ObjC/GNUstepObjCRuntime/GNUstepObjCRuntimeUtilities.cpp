//===-- GNUstepObjCRuntimeUtilities.cpp ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCRuntimeUtilities.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/ThreadList.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Target.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/ModuleList.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include <chrono>

using namespace lldb;
using namespace lldb_private;

namespace lldb_private {
namespace gnustep_objc_runtime_utilities {

EvaluateExpressionOptions MakeSafeExpressionOptions(bool for_utility_expression) {
  EvaluateExpressionOptions opts;
  opts.SetUnwindOnError(true);
  opts.SetIgnoreBreakpoints(true);
  opts.SetTryAllThreads(false);
  opts.SetTimeout(std::chrono::microseconds(2500000));  // 2.5 seconds
  opts.SetTrapExceptions(false);       // Critical on Windows
  
  if (for_utility_expression) {
    opts.SetOneThreadTimeout(std::chrono::milliseconds(250));
    opts.SetStopOthers(true);
    opts.SetIsForUtilityExpr(true);
  }
  
  return opts;
}

// TargetStringAllocator implementation
TargetStringAllocator::TargetStringAllocator(Process *process, const std::string &str)
    : m_process(process), m_address(LLDB_INVALID_ADDRESS) {
  
  if (!m_process || str.empty()) {
    m_error = Status::FromErrorString("Invalid process or empty string");
    return;
  }
  
  // Allocate memory for the string plus null terminator
  m_address = m_process->AllocateMemory(
      str.length() + 1, 
      lldb::ePermissionsReadable, 
      m_error);
  
  if (m_address == LLDB_INVALID_ADDRESS || m_error.Fail()) {
    return;
  }
  
  // Write the string to target memory
  size_t bytes_written = m_process->WriteMemory(
      m_address, str.c_str(), str.length() + 1, m_error);
  
  if (bytes_written != str.length() + 1 || m_error.Fail()) {
    // Clean up on write failure
    m_process->DeallocateMemory(m_address);
    m_address = LLDB_INVALID_ADDRESS;
    if (m_error.Success()) {
      m_error = Status::FromErrorString("Failed to write complete string to target memory");
    }
  }
}

TargetStringAllocator::~TargetStringAllocator() {
  Cleanup();
}

TargetStringAllocator::TargetStringAllocator(TargetStringAllocator &&other) noexcept
    : m_process(other.m_process), m_address(other.m_address), m_error(std::move(other.m_error)) {
  other.m_process = nullptr;
  other.m_address = LLDB_INVALID_ADDRESS;
}

TargetStringAllocator &TargetStringAllocator::operator=(TargetStringAllocator &&other) noexcept {
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
    StackFrameSP frame_sp = thread_sp->GetSelectedFrame(DoNoSelectMostRelevantFrame);
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

void RuntimeSymbolCache::InvalidateCache() {
  m_symbol_cache.clear();
}

bool RuntimeSymbolCache::HasEssentialSymbols() const {
  // Check if we have the core symbols needed for basic runtime operations
  static const char* essential_symbols[] = {
    "objc_getClass",
    "objc_msgSend", 
    "object_getClass",
    "class_getName"
  };
  
  for (const char* symbol : essential_symbols) {
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
    if (!module_sp) continue;

    const char *module_name = module_sp->GetFileSpec().GetFilename().GetCString();
    if (!module_name) continue;

    // Check if this is a GNUstep runtime module
    if (strstr(module_name, "libobjc.so") || 
        strstr(module_name, "libobjc2") ||
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
  target.GetImages().FindSymbolsWithNameAndType(
      ConstString(symbol_name), eSymbolTypeCode, sc_list);

  if (sc_list.GetSize() > 0) {
    SymbolContext sc;
    if (sc_list.GetContextAtIndex(0, sc) && sc.symbol) {
      lldb::addr_t addr = sc.symbol->GetAddress().GetLoadAddress(&target);
      if (addr != LLDB_INVALID_ADDRESS) {
        Log *log = GetLog(LLDBLog::Language);
        LLDB_LOG(log, "[GNUstepUtilities] Found {0} (fallback) at 0x{1:x}", symbol_name, addr);
        return addr;
      }
    }
  }

  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(log, "[GNUstepUtilities] Failed to resolve symbol: {0}", symbol_name);
  return LLDB_INVALID_ADDRESS;
}

} // namespace gnustep_objc_runtime_utilities
} // namespace lldb_private
