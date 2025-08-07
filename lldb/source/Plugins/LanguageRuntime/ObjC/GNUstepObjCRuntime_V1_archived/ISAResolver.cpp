//===-- ISAResolver.cpp ------------------------------------------------===//
//
// Enhanced ISA Pointer Resolution for GNUstep Runtime
// Addresses VS Code debugging issues with nested object expansion
//
//===----------------------------------------------------------------------===//

#include "ISAResolver.h"
#include "GNUstepObjCRuntime.h"

#include "lldb/Core/Module.h"
#include "lldb/Expression/DiagnosticManager.h"
#include "lldb/Expression/UserExpression.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/ArchSpec.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"

#include "llvm/Support/FormatVariadic.h"

using namespace lldb;
using namespace lldb_private;

ISAResolver::ISAResolver(Process *process) 
    : m_process(process), m_object_getClassName_addr(LLDB_INVALID_ADDRESS),
      m_class_getName_addr(LLDB_INVALID_ADDRESS) {
  LoadRuntimeSymbols();
}

void ISAResolver::LoadRuntimeSymbols() {
  if (!m_process)
    return;
    
  Target &target = m_process->GetTarget();
  const ModuleList &modules = target.GetImages();
  std::lock_guard<std::recursive_mutex> guard(modules.GetMutex());
  
  for (size_t i = 0; i < modules.GetSize(); ++i) {
    lldb::ModuleSP module_sp = modules.GetModuleAtIndexUnlocked(i);
    if (!module_sp)
      continue;
      
    // Look for object_getClassName
    SymbolContextList sc_list;
    module_sp->FindSymbolsWithNameAndType(ConstString("object_getClassName"),
                                         lldb::eSymbolTypeCode, sc_list);
    if (!sc_list.IsEmpty()) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(0, sc);
      if (sc.symbol) {
        m_object_getClassName_addr = sc.symbol->GetLoadAddress(&target);
      }
    }
    
    // Look for class_getName
    sc_list.Clear();
    module_sp->FindSymbolsWithNameAndType(ConstString("class_getName"),
                                         lldb::eSymbolTypeCode, sc_list);
    if (!sc_list.IsEmpty()) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(0, sc);
      if (sc.symbol) {
        m_class_getName_addr = sc.symbol->GetLoadAddress(&target);
      }
    }
  }
}

std::string ISAResolver::GetClassNameFromISA(lldb::addr_t isa_ptr) {
  if (!m_process || isa_ptr == LLDB_INVALID_ADDRESS)
    return "";
    
  Log *log = GetLog(lldb_private::LLDBLog::Process | lldb_private::LLDBLog::Types);
  
  // Strategy 1: Use object_getClassName if available
  if (m_object_getClassName_addr != LLDB_INVALID_ADDRESS) {
    std::string result = GetClassNameViaRuntime(isa_ptr);
    if (!result.empty()) {
      LLDB_LOG(log, "ISAResolver: Runtime method resolved isa 0x{0:x} -> {1}", 
               isa_ptr, result.c_str());
      return result;
    }
  }
  
  // Strategy 2: Direct memory reading from class structure  
  std::string result = GetClassNameViaMemory(isa_ptr);
  if (!result.empty()) {
    LLDB_LOG(log, "ISAResolver: Memory method resolved isa 0x{0:x} -> {1}", 
             isa_ptr, result.c_str());
    return result;
  }
  
  // Strategy 3: Symbol-based lookup
  result = GetClassNameViaSymbols(isa_ptr);
  if (!result.empty()) {
    LLDB_LOG(log, "ISAResolver: Symbol method resolved isa 0x{0:x} -> {1}", 
             isa_ptr, result.c_str());
    return result;
  }
  
  LLDB_LOG(log, "ISAResolver: Failed to resolve isa 0x{0:x}", isa_ptr);
  return "";
}

std::string ISAResolver::GetClassNameFromObject(lldb::addr_t obj_addr) {
  if (!m_process || obj_addr == LLDB_INVALID_ADDRESS)
    return "";
  
  // Check for GSTinyString tagged pointer first
  uint8_t tag = (obj_addr >> 61) & 0x7;
  if (tag == 3) {
    // Return NSString for VS Code compatibility
    return "NSString";
  }
    
  // Read ISA pointer from object
  Status error;
  addr_t isa_ptr = m_process->ReadPointerFromMemory(obj_addr, error);
  if (error.Fail() || isa_ptr == LLDB_INVALID_ADDRESS)
    return "";
    
  return GetClassNameFromISA(isa_ptr);
}

bool ISAResolver::IsValidISAPointer(lldb::addr_t isa_ptr) {
  if (isa_ptr == LLDB_INVALID_ADDRESS || isa_ptr == 0)
    return false;
    
  // Basic sanity checks
  if (isa_ptr < 0x1000)  // Null page
    return false;
    
  if (isa_ptr > 0x7FFFFFFFFFFF)  // Invalid user space on x64
    return false;
    
  // Try to read memory at ISA location
  Status error;
  m_process->ReadUnsignedIntegerFromMemory(isa_ptr, 8, 0, error);
  return error.Success();
}

std::string ISAResolver::GetClassNameViaRuntime(lldb::addr_t isa_ptr) {
  if (!m_process || m_object_getClassName_addr == LLDB_INVALID_ADDRESS)
    return "";
    
  ExecutionContext exe_ctx;
  m_process->CalculateExecutionContext(exe_ctx);
  
  if (!exe_ctx.HasThreadScope())
    return "";
    
  // Call object_getClassName((id)isa_ptr)
  DiagnosticManager diagnostics;
  EvaluateExpressionOptions options;
  options.SetUnwindOnError(true);
  options.SetIgnoreBreakpoints(true);
  options.SetTimeout(std::chrono::seconds(5));
  
  ValueObjectSP result_sp;
  std::string expr = llvm::formatv("(const char*)object_getClassName((id)0x{0:x})", 
                                   isa_ptr).str();
  
  ExpressionResults expr_result = UserExpression::Evaluate(exe_ctx, options, 
                                                          expr.c_str(), "", 
                                                          result_sp, nullptr);
  
  if (expr_result != eExpressionCompleted || !result_sp)
    return "";
    
  addr_t name_addr = result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
  if (name_addr == LLDB_INVALID_ADDRESS)
    return "";
    
  return ReadCString(name_addr, 128);
}

std::string ISAResolver::GetClassNameViaMemory(lldb::addr_t isa_ptr) {
  if (!m_process || !IsValidISAPointer(isa_ptr))
    return "";
    
  Log *log = GetLog(lldb_private::LLDBLog::Process | lldb_private::LLDBLog::Types);
  
  // GNUstep class structure layout (simplified):
  // struct objc_class {
  //   struct objc_class *isa;      // +0
  //   struct objc_class *super;    // +8  
  //   const char *name;            // +16
  //   ...
  // }
  
  Status error;
  
  // First verify we can read the ISA pointer itself
  // Verify we can read from this address
  (void)m_process->ReadPointerFromMemory(isa_ptr, error);
  if (error.Fail()) {
    LLDB_LOG(log, "ISAResolver: Cannot read ISA pointer at 0x{0:x}", isa_ptr);
    return "";
  }
  
  // Read name pointer from class structure at offset +16
  addr_t name_ptr = m_process->ReadPointerFromMemory(isa_ptr + 16, error);
  if (error.Fail() || name_ptr == LLDB_INVALID_ADDRESS) {
    LLDB_LOG(log, "ISAResolver: Cannot read name pointer from class at 0x{0:x}", isa_ptr);
    return "";
  }
  
  // Read the class name string
  std::string class_name = ReadCString(name_ptr, 128);
  if (!class_name.empty()) {
    LLDB_LOG(log, "ISAResolver: Successfully read class name '{0}' from isa 0x{1:x}", 
             class_name.c_str(), isa_ptr);
  }
  
  return class_name;
}

std::string ISAResolver::GetClassNameViaSymbols(lldb::addr_t isa_ptr) {
  if (!m_process)
    return "";
    
  Target &target = m_process->GetTarget();
  
  // Look up what symbol this address corresponds to
  Address isa_address;
  if (!target.ResolveLoadAddress(isa_ptr, isa_address))
    return "";
    
  SymbolContext sc;
  if (!isa_address.CalculateSymbolContext(&sc))
    return "";
    
  if (!sc.symbol)
    return "";
    
  // Extract class name from symbol name
  std::string symbol_name = sc.symbol->GetName().GetCString();
  
  // Look for patterns like "OBJC_CLASS_$_ClassName" or "_OBJC_CLASS_$_ClassName"
  size_t class_prefix = symbol_name.find("OBJC_CLASS_$_");
  if (class_prefix != std::string::npos) {
    return symbol_name.substr(class_prefix + 13);  // Skip "OBJC_CLASS_$_"
  }
  
  // Look for patterns like "__objc_class_name_ClassName"
  size_t name_prefix = symbol_name.find("__objc_class_name_");
  if (name_prefix != std::string::npos) {
    return symbol_name.substr(name_prefix + 18);  // Skip "__objc_class_name_"
  }
  
  return "";
}

std::string ISAResolver::ReadCString(lldb::addr_t addr, size_t max_len) {
  if (!m_process || addr == LLDB_INVALID_ADDRESS)
    return "";
    
  Status error;
  char buffer[256];
  size_t buffer_size = std::min(max_len, sizeof(buffer) - 1);
  
  size_t bytes_read = m_process->ReadCStringFromMemory(addr, buffer, 
                                                       buffer_size, error);
  if (error.Fail() || bytes_read == 0) {
    Log *log = GetLog(lldb_private::LLDBLog::Process | lldb_private::LLDBLog::Types);
    LLDB_LOG(log, "ISAResolver: Failed to read C string from 0x{0:x}: {1}", 
             addr, error.AsCString());
    return "";
  }
    
  buffer[bytes_read] = '\0';  // Ensure null termination
  return std::string(buffer);
}

void ISAResolver::CacheClassNameForISA(lldb::addr_t isa_ptr, const std::string &class_name) {
  std::lock_guard<std::mutex> lock(m_cache_mutex);
  m_isa_to_class_cache[isa_ptr] = class_name;
}

std::string ISAResolver::GetCachedClassNameForISA(lldb::addr_t isa_ptr) {
  std::lock_guard<std::mutex> lock(m_cache_mutex);
  auto it = m_isa_to_class_cache.find(isa_ptr);
  return (it != m_isa_to_class_cache.end()) ? it->second : "";
}

void ISAResolver::ClearCache() {
  std::lock_guard<std::mutex> lock(m_cache_mutex);
  m_isa_to_class_cache.clear();
}
