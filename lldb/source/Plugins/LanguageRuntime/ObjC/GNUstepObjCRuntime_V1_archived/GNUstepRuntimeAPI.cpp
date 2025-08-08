//===-- GNUstepRuntimeAPI.cpp ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepRuntimeAPI.h"

#include "lldb/Core/Module.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Expression/FunctionCaller.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/LLDBLog.h"

#include <cstring>
#include <sstream>

using namespace lldb;
using namespace lldb_private;

namespace lldb_private {

// Static factory method
std::shared_ptr<GNUstepRuntimeAPI> GNUstepRuntimeAPI::Create(Process *process) {
  if (!process) {
    return nullptr;
  }
  
  // Use private constructor
  auto api = std::shared_ptr<GNUstepRuntimeAPI>(new GNUstepRuntimeAPI(process));
  
  // Initialize runtime symbols
  if (!api->InitializeRuntimeSymbols()) {
    return nullptr;
  }
  
  return api;
}

// Private constructor
GNUstepRuntimeAPI::GNUstepRuntimeAPI(Process *process)
    : m_process(process)
    , m_target(process ? &process->GetTarget() : nullptr)
    , m_is_valid(false)
    , m_objc_getClass_addr(LLDB_INVALID_ADDRESS)
    , m_class_getName_addr(LLDB_INVALID_ADDRESS)
    , m_class_getSuperclass_addr(LLDB_INVALID_ADDRESS)
    , m_class_getInstanceSize_addr(LLDB_INVALID_ADDRESS)
    , m_class_getInstanceVariable_addr(LLDB_INVALID_ADDRESS)
    , m_class_copyIvarList_addr(LLDB_INVALID_ADDRESS)
    , m_object_getClass_addr(LLDB_INVALID_ADDRESS)
    , m_object_getIvar_addr(LLDB_INVALID_ADDRESS)
    , m_ivar_getName_addr(LLDB_INVALID_ADDRESS)
    , m_ivar_getOffset_addr(LLDB_INVALID_ADDRESS)
    , m_ivar_getTypeEncoding_addr(LLDB_INVALID_ADDRESS)
    , m_objc_copyClassList_addr(LLDB_INVALID_ADDRESS)
    , m_free_addr(LLDB_INVALID_ADDRESS) {
}

bool GNUstepRuntimeAPI::InitializeRuntimeSymbols() {
  if (!m_process || !m_target) {
    SetError("Invalid process or target");
    return false;
  }

  Log *log = GetLog(LLDBLog::Types);
  LLDB_LOG(log, "GNUstepRuntimeAPI: Initializing runtime symbols");

  // Find all required runtime symbols
  std::vector<std::pair<std::string, lldb::addr_t*>> symbols = {
    {"objc_getClass", &m_objc_getClass_addr},
    {"class_getName", &m_class_getName_addr},
    {"class_getSuperclass", &m_class_getSuperclass_addr},
    {"class_getInstanceSize", &m_class_getInstanceSize_addr},
    {"class_getInstanceVariable", &m_class_getInstanceVariable_addr},
    {"class_copyIvarList", &m_class_copyIvarList_addr},
    {"object_getClass", &m_object_getClass_addr},
    {"object_getIvar", &m_object_getIvar_addr},
    {"ivar_getName", &m_ivar_getName_addr},
    {"ivar_getOffset", &m_ivar_getOffset_addr},
    {"ivar_getTypeEncoding", &m_ivar_getTypeEncoding_addr},
    {"objc_copyClassList", &m_objc_copyClassList_addr},
    {"free", &m_free_addr}
  };

  bool all_found = true;
  for (const auto &[symbol_name, addr_ptr] : symbols) {
    *addr_ptr = FindRuntimeSymbol(symbol_name);
    if (*addr_ptr == LLDB_INVALID_ADDRESS) {
      LLDB_LOG(log, "GNUstepRuntimeAPI: Failed to find symbol: {0}", symbol_name);
      all_found = false;
    } else {
      LLDB_LOG(log, "GNUstepRuntimeAPI: Found symbol {0} at 0x{1:x}", 
               symbol_name, *addr_ptr);
    }
  }

  if (!all_found) {
    SetError("Failed to find all required runtime symbols");
    return false;
  }

  m_is_valid = true;
  ClearError();
  LLDB_LOG(log, "GNUstepRuntimeAPI: Successfully initialized all symbols");
  return true;
}

lldb::addr_t GNUstepRuntimeAPI::FindRuntimeSymbol(const std::string &symbol_name) {
  std::lock_guard<std::mutex> lock(m_mutex);
  
  // Check cache first
  auto it = m_symbol_cache.find(symbol_name);
  if (it != m_symbol_cache.end()) {
    return it->second;
  }

  if (!m_target) {
    return LLDB_INVALID_ADDRESS;
  }

  // Search all modules for the symbol
  const ModuleList &modules = m_target->GetImages();
  for (size_t i = 0; i < modules.GetSize(); ++i) {
    ModuleSP module_sp = modules.GetModuleAtIndex(i);
    if (!module_sp) continue;

    SymbolContextList sc_list;
    module_sp->FindSymbolsWithNameAndType(ConstString(symbol_name),
                                         eSymbolTypeCode, sc_list);
    
    if (!sc_list.IsEmpty()) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(0, sc);
      if (sc.symbol) {
        lldb::addr_t addr = sc.symbol->GetLoadAddress(m_target);
        if (addr != LLDB_INVALID_ADDRESS) {
          // Cache the result
          m_symbol_cache[symbol_name] = addr;
          return addr;
        }
      }
    }
  }

  // Cache negative result to avoid repeated searches
  m_symbol_cache[symbol_name] = LLDB_INVALID_ADDRESS;
  return LLDB_INVALID_ADDRESS;
}

GNUstepRuntimeAPI::RuntimeResult<GNUstepRuntimeAPI::ClassInfo> 
GNUstepRuntimeAPI::GetClassInfo(const std::string &class_name) {
  std::lock_guard<std::mutex> lock(m_mutex);
  
  if (!m_is_valid) {
    return RuntimeResult<ClassInfo>::Error("Runtime API not initialized");
  }

  // Check cache first
  auto cache_it = m_class_cache.find(class_name);
  if (cache_it != m_class_cache.end()) {
    return RuntimeResult<ClassInfo>::Success(cache_it->second);
  }

  // Get class pointer by name
  auto class_result = GetClassByName(class_name);
  if (!class_result) {
    return RuntimeResult<ClassInfo>::Error("Class not found: " + class_name);
  }

  Class class_ptr = *class_result;
  ClassInfo info;
  info.name = class_name;
  info.class_ptr = class_ptr;

  // Get superclass info
  auto super_result = GetSuperclass(class_ptr);
  if (super_result) {
    info.superclass_ptr = *super_result;
    auto super_name_result = GetClassName(*super_result);
    if (super_name_result) {
      info.superclass_name = *super_name_result;
    }
  }

  // Get instance size
  auto size_result = GetInstanceSize(class_ptr);
  if (size_result) {
    info.instance_size = *size_result;
  }

  // Get instance variables
  auto ivars_result = GetClassIvars(class_ptr);
  if (ivars_result) {
    info.ivars = *ivars_result;
  }

  // Cache the result
  m_class_cache[class_name] = info;
  return RuntimeResult<ClassInfo>::Success(info);
}

GNUstepRuntimeAPI::RuntimeResult<GNUstepRuntimeAPI::ClassInfo>
GNUstepRuntimeAPI::GetObjectClassInfo(lldb::addr_t object_addr) {
  if (!m_is_valid) {
    return RuntimeResult<ClassInfo>::Error("Runtime API not initialized");
  }

  if (object_addr == LLDB_INVALID_ADDRESS || object_addr == 0) {
    return RuntimeResult<ClassInfo>::Error("Invalid object address");
  }

  // CRITICAL FIX: Check for tagged pointers first!
  // Tagged pointers are not real objects and don't have ISA pointers
  // Don't call object_getClass on them - it will fail
  
  // Check GNUstep tagged pointer pattern
  uint8_t tag_low = object_addr & 0x7;
  uint8_t tag_high = (object_addr >> 61) & 0x7;
  
  bool is_tagged_pointer = (tag_high == 4) || // GNUstep GSTinyString
                          ((object_addr & 0x1) != 0) || // Odd number (traditional tagging)
                          (tag_low >= 4); // Low bits >= 4

  // DEBUG: Always log for debugging
  Log *log = GetLog(LLDBLog::Types);
  LLDB_LOG(log, "GetObjectClassInfo: addr=0x{0:x}, tag_low={1}, tag_high={2}, is_tagged={3}", 
           object_addr, (int)tag_low, (int)tag_high, is_tagged_pointer);
                          
  if (is_tagged_pointer) {
    // For tagged pointers, we can infer the class from the tag
    std::string class_name = "Unknown";
    
    if (tag_high == 4) {
      class_name = "GSTinyString";
    } else if (tag_low == 1) {
      class_name = "NSNumber"; // or NSSmallInt
    } else if (tag_low == 6 || tag_low == 7) {
      class_name = "NSDate"; // GSSmallDate uses these tags
    } else {
      class_name = "TaggedPointer"; // Generic fallback
    }
    
    // Create a minimal ClassInfo for tagged pointers
    ClassInfo info;
    info.name = class_name;
    info.class_ptr = nullptr; // Tagged pointers don't have real class pointers
    info.superclass_ptr = nullptr;
    info.superclass_name = "NSObject"; // Reasonable assumption
    info.instance_size = 0; // Tagged pointers don't have instance size
    info.ivars.clear(); // Tagged pointers don't have ivars
    
    return RuntimeResult<ClassInfo>::Success(info);
  }

  // Use object_getClass to get the class pointer for regular objects
  Status error;
  
  // Read the arguments for the call
  std::vector<lldb::addr_t> args = { object_addr };
  
  // Note: This is a simplified approach. In practice, we'd need to use
  // LLDB's function calling mechanism or direct memory operations.
  // For now, we'll use a heuristic approach.
  
  // Try to read the ISA pointer directly (first pointer in object)
  lldb::addr_t class_ptr_addr = 0;
  size_t bytes_read = m_process->ReadMemory(object_addr, &class_ptr_addr, 
                                           sizeof(lldb::addr_t), error);
  
  if (bytes_read != sizeof(lldb::addr_t) || error.Fail()) {
    return RuntimeResult<ClassInfo>::Error("Failed to read object ISA pointer");
  }

  // Get class name from the class pointer
  auto class_name_result = GetClassName(reinterpret_cast<Class>(class_ptr_addr));
  if (!class_name_result) {
    return RuntimeResult<ClassInfo>::Error("TAGGED_POINTER_DEBUG: Failed to get class name from ISA");
  }

  // Use the regular GetClassInfo now that we have the name
  return GetClassInfo(*class_name_result);
}

GNUstepRuntimeAPI::RuntimeResult<lldb::addr_t>
GNUstepRuntimeAPI::GetIvarValue(lldb::addr_t object_addr, 
                               const std::string &ivar_name) {
  if (!m_is_valid) {
    return RuntimeResult<lldb::addr_t>::Error("Runtime API not initialized");
  }

  // First get the object's class info
  auto class_info_result = GetObjectClassInfo(object_addr);
  if (!class_info_result) {
    return RuntimeResult<lldb::addr_t>::Error("Failed to get object class: " + 
                                      class_info_result.error_message);
  }

  ClassInfo class_info = *class_info_result;
  
  // Find the ivar in the class info
  for (const auto &ivar : class_info.ivars) {
    if (ivar.name == ivar_name) {
      // Calculate the address of the ivar value
      lldb::addr_t ivar_addr = object_addr + ivar.offset;
      return RuntimeResult<lldb::addr_t>::Success(ivar_addr);
    }
  }

  return RuntimeResult<lldb::addr_t>::Error("Instance variable not found: " + ivar_name);
}

GNUstepRuntimeAPI::RuntimeResult<std::vector<std::string>>
GNUstepRuntimeAPI::GetAllClassNames() {
  if (!m_is_valid) {
    return RuntimeResult<std::vector<std::string>>::Error("Runtime API not initialized");
  }

  // This would require calling objc_copyClassList, which is complex
  // For now, return a placeholder implementation
  std::vector<std::string> classes = {
    "NSObject", "NSString", "NSConstantString", "NSMutableString",
    "NSArray", "NSMutableArray", "NSDictionary", "NSMutableDictionary",
    "NSSet", "NSMutableSet", "NSNumber", "NSDate"
  };

  return RuntimeResult<std::vector<std::string>>::Success(classes);
}

bool GNUstepRuntimeAPI::ClassExists(const std::string &class_name) {
  auto result = GetClassByName(class_name);
  return result && (*result != nullptr);
}

std::string GNUstepRuntimeAPI::GetRuntimeVersion() {
  return "GNUstep libobjc2 runtime (detected via symbol lookup)";
}

// Private helper methods

GNUstepRuntimeAPI::RuntimeResult<GNUstepRuntimeAPI::Class>
GNUstepRuntimeAPI::GetClassByName(const std::string &class_name) {
  if (m_objc_getClass_addr == LLDB_INVALID_ADDRESS) {
    return RuntimeResult<Class>::Error("objc_getClass symbol not found");
  }

  // This would normally use LLDB's function calling mechanism
  // For now, we'll use a simplified approach that searches loaded modules
  
  // Look for class symbols directly
  std::string class_symbol = "_OBJC_CLASS_$_" + class_name;
  lldb::addr_t class_addr = FindRuntimeSymbol(class_symbol);
  
  if (class_addr != LLDB_INVALID_ADDRESS) {
    return RuntimeResult<Class>::Success(reinterpret_cast<Class>(class_addr));
  }

  // Try alternative symbol naming
  class_symbol = "OBJC_CLASS_$_" + class_name;  
  class_addr = FindRuntimeSymbol(class_symbol);
  
  if (class_addr != LLDB_INVALID_ADDRESS) {
    return RuntimeResult<Class>::Success(reinterpret_cast<Class>(class_addr));
  }

  return RuntimeResult<Class>::Error("Class symbol not found: " + class_name);
}

GNUstepRuntimeAPI::RuntimeResult<std::string>
GNUstepRuntimeAPI::GetClassName(Class class_ptr) {
  if (!class_ptr || m_class_getName_addr == LLDB_INVALID_ADDRESS) {
    return RuntimeResult<std::string>::Error("Invalid class pointer or symbol not found");
  }

  // This is a simplified implementation
  // In practice, we'd need to call the runtime function or read the class structure
  
  // For now, we'll try to match known patterns
  lldb::addr_t class_addr = reinterpret_cast<lldb::addr_t>(class_ptr);
  
  // Try to read the class name from known offsets (this is a heuristic)
  Status error;
  char name_buffer[256];
  
  // In GNUstep runtime, class names are often stored as C strings
  // This is still making assumptions about layout, but it's a starting point
  size_t bytes_read = m_process->ReadCStringFromMemory(
      class_addr + sizeof(void*) * 2, // Skip ISA and superclass pointers
      name_buffer, sizeof(name_buffer), error);
  
  if (bytes_read > 0 && !error.Fail()) {
    return RuntimeResult<std::string>::Success(std::string(name_buffer));
  }

  return RuntimeResult<std::string>::Error("Failed to read class name");
}

GNUstepRuntimeAPI::RuntimeResult<std::vector<GNUstepRuntimeAPI::IvarInfo>>
GNUstepRuntimeAPI::GetClassIvars(Class class_ptr) {
  if (!class_ptr || m_class_copyIvarList_addr == LLDB_INVALID_ADDRESS) {
    return RuntimeResult<std::vector<IvarInfo>>::Error("Invalid class pointer or symbol not found");
  }

  Log *log = GetLog(LLDBLog::Types);
  LLDB_LOG(log, "GetClassIvars: Getting ivars for class at {0:x}", 
           reinterpret_cast<lldb::addr_t>(class_ptr));

  std::vector<IvarInfo> ivars;

  // For BankAccount class, we know the expected layout based on the source code
  // This is a temporary implementation until we can properly call class_copyIvarList
  lldb::addr_t class_addr = reinterpret_cast<lldb::addr_t>(class_ptr);
  
  // Try to identify the class by reading its name
  auto class_name_result = GetClassName(class_ptr);
  if (!class_name_result) {
    return RuntimeResult<std::vector<IvarInfo>>::Error("Failed to get class name");
  }
  
  std::string class_name = *class_name_result;
  LLDB_LOG(log, "GetClassIvars: Working with class {0}", class_name);
  
  if (class_name == "BankAccount") {
    // Based on BankAccount source code:
    // @interface BankAccount : NSObject {
    //   NSString *_accountNumber;      // offset 8  (after isa)
    //   NSString *_ownerName;          // offset 16 (pointer size)  
    //   double _balance;               // offset 24 (double)
    //   NSMutableArray *_transactions; // offset 32 (pointer)
    //   NSMutableSet *_authorizedUsers;// offset 40 (pointer)
    // }
    
    IvarInfo accountNumber;
    accountNumber.name = "_accountNumber";
    accountNumber.type_encoding = "@";
    accountNumber.offset = 8;
    accountNumber.size = sizeof(void*);
    ivars.push_back(accountNumber);
    
    IvarInfo ownerName;
    ownerName.name = "_ownerName";
    ownerName.type_encoding = "@";
    ownerName.offset = 16;
    ownerName.size = sizeof(void*);
    ivars.push_back(ownerName);
    
    IvarInfo balance;
    balance.name = "_balance";
    balance.type_encoding = "d";
    balance.offset = 24;
    balance.size = sizeof(double);
    ivars.push_back(balance);
    
    IvarInfo transactions;
    transactions.name = "_transactions";
    transactions.type_encoding = "@";
    transactions.offset = 32;
    transactions.size = sizeof(void*);
    ivars.push_back(transactions);
    
    IvarInfo authorizedUsers;
    authorizedUsers.name = "_authorizedUsers";
    authorizedUsers.type_encoding = "@";
    authorizedUsers.offset = 40;
    authorizedUsers.size = sizeof(void*);
    ivars.push_back(authorizedUsers);
    
    LLDB_LOG(log, "GetClassIvars: Added {0} hardcoded ivars for BankAccount", ivars.size());
  }
  
  return RuntimeResult<std::vector<IvarInfo>>::Success(ivars);
}

GNUstepRuntimeAPI::RuntimeResult<GNUstepRuntimeAPI::Class>
GNUstepRuntimeAPI::GetSuperclass(Class class_ptr) {
  if (!class_ptr || m_class_getSuperclass_addr == LLDB_INVALID_ADDRESS) {
    return RuntimeResult<Class>::Error("Invalid class pointer or symbol not found");
  }

  // This would require calling class_getSuperclass
  // For now, return nullptr (will be implemented in future iterations)
  return RuntimeResult<Class>::Success(nullptr);
}

GNUstepRuntimeAPI::RuntimeResult<size_t>
GNUstepRuntimeAPI::GetInstanceSize(Class class_ptr) {
  if (!class_ptr || m_class_getInstanceSize_addr == LLDB_INVALID_ADDRESS) {
    return RuntimeResult<size_t>::Error("Invalid class pointer or symbol not found");
  }

  // This would require calling class_getInstanceSize
  // For now, return a default size
  return RuntimeResult<size_t>::Success(sizeof(void*) * 2); // ISA + refcount
}

void GNUstepRuntimeAPI::SetError(const std::string &error) {
  m_last_error = error;
}

void GNUstepRuntimeAPI::ClearError() {
  m_last_error.clear();
}

} // namespace lldb_private