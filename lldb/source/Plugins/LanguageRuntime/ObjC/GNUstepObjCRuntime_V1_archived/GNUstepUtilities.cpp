//===-- GNUstepUtilities.cpp ----------------------------*- C++ -*-===//
//
// Utility classes and functions for GNUstep Objective-C runtime support
//
//===----------------------------------------------------------------------===//

#include "GNUstepUtilities.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Core/Module.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolFile.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"

#include <map>

using namespace lldb;
using namespace lldb_private;

//===----------------------------------------------------------------------===//
// GNUstepAddressResolver implementation
//===----------------------------------------------------------------------===//

addr_t GNUstepAddressResolver::ResolveObjectAddress(ValueObject &valobj) {
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  // For pointer types, GetValueAsUnsigned gives us the address the pointer points to
  addr_t obj_ptr = valobj.GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
  
  // GetAddressOf gives us the address where the pointer variable is stored (stack/heap)
  addr_t var_addr = valobj.GetAddressOf();
  
  LLDB_LOG(log, "GNUstepAddressResolver: obj_ptr=0x{0:x}, var_addr=0x{1:x}", obj_ptr, var_addr);
  
  // For BankAccount pointers, we want the address the pointer points to (obj_ptr)
  // not the address where the pointer is stored (var_addr)
  if (obj_ptr != LLDB_INVALID_ADDRESS && obj_ptr != 0) {
    LLDB_LOG(log, "GNUstepAddressResolver: Using object address 0x{0:x}", obj_ptr);
    return obj_ptr;
  }
  
  // Fallback to variable address if object pointer is invalid
  if (var_addr != LLDB_INVALID_ADDRESS && var_addr != 0) {
    LLDB_LOG(log, "GNUstepAddressResolver: Fallback to variable address 0x{0:x}", var_addr);
    return var_addr;
  }
  
  LLDB_LOG(log, "GNUstepAddressResolver: No valid address found");
  return LLDB_INVALID_ADDRESS;
}

bool GNUstepAddressResolver::IsLikelyISAPointer(addr_t addr) {
  // ISA pointers are typically in high memory ranges (> 0x7000000000000000)
  // This is a heuristic check for debugging purposes
  return addr > 0x7000000000000000ULL;
}

const char *GNUstepAddressResolver::GetFoundationTypeName(const char *gnustep_name) {
  // Comprehensive mapping of GNUstep internal classes to Foundation public classes
  static std::map<std::string, const char *> foundation_map = {
    // String classes
    {"GSString", "NSString"},
    {"GSMutableString", "NSMutableString"},
    {"GSConstantString", "NSString"},
    {"GSTinyString", "NSString"},
    {"NSConstantString", "NSString"},
    
    // Array classes
    {"GSInlineArray", "NSArray"},
    {"GSMutableArray", "NSMutableArray"},
    {"GSArrayEnumerator", "NSEnumerator"},
    
    // Dictionary classes
    {"GSDictionary", "NSDictionary"},
    {"GSMutableDictionary", "NSMutableDictionary"},
    {"GSDictionaryEnumerator", "NSEnumerator"},
    
    // Set classes
    {"GSSet", "NSSet"},
    {"GSMutableSet", "NSMutableSet"},
    {"GSCountedSet", "NSCountedSet"},
    {"GSSetEnumerator", "NSEnumerator"},
    
    // Number classes
    {"GSNumber", "NSNumber"},
    {"GSDecimalNumber", "NSDecimalNumber"},
    
    // Date classes
    {"GSDate", "NSDate"},
    {"GSCalendarDate", "NSCalendarDate"},
    {"GSTimeZone", "NSTimeZone"},
    
    // Data classes
    {"GSData", "NSData"},
    {"GSMutableData", "NSMutableData"},
    
    // URL classes
    {"GSURL", "NSURL"},
    
    // Value classes
    {"GSValue", "NSValue"},
    
    // Misc classes
    {"GSNull", "NSNull"},
    {"GSError", "NSError"},
    {"GSException", "NSException"},
    {"GSLocale", "NSLocale"},
    
    // Collection utility classes
    {"GSEnumerator", "NSEnumerator"},
    {"GSIndexSet", "NSIndexSet"},
    {"GSMutableIndexSet", "NSMutableIndexSet"}
  };
  
  if (!gnustep_name)
    return nullptr;
    
  auto it = foundation_map.find(gnustep_name);
  if (it != foundation_map.end())
    return it->second;
    
  // If no mapping found, return original name
  return gnustep_name;
}

//===----------------------------------------------------------------------===//
// GNUstepMemoryReader implementation
//===----------------------------------------------------------------------===//

uint64_t GNUstepMemoryReader::ReadUnsigned(addr_t addr, size_t size, uint64_t default_val) {
  if (!IsValidAddress(addr) || !m_process) {
    return default_val;
  }
  
  Status error;
  uint64_t value = m_process->ReadUnsignedIntegerFromMemory(addr, size, default_val, error);
  if (error.Fail()) {
    return default_val;
  }
  
  return value;
}

addr_t GNUstepMemoryReader::ReadPointer(addr_t addr) {
  if (!IsValidAddress(addr) || !m_process) {
    return LLDB_INVALID_ADDRESS;
  }
  
  Status error;
  addr_t value = m_process->ReadPointerFromMemory(addr, error);
  if (error.Fail()) {
    return LLDB_INVALID_ADDRESS;
  }
  
  return value;
}

std::string GNUstepMemoryReader::ReadCString(addr_t addr, size_t max_len) {
  if (!IsValidAddress(addr) || !m_process) {
    return "";
  }
  
  Status error;
  char buffer[256];
  size_t actual_max = std::min(max_len, sizeof(buffer) - 1);
  
  size_t bytes_read = m_process->ReadCStringFromMemory(addr, buffer, actual_max, error);
  if (error.Fail() || bytes_read == 0) {
    return "";
  }
  
  buffer[bytes_read] = '\0';
  return std::string(buffer);
}

std::vector<uint8_t> GNUstepMemoryReader::ReadBytes(addr_t addr, size_t size) {
  std::vector<uint8_t> result;
  
  if (!IsValidAddress(addr) || !m_process || size == 0) {
    return result;
  }
  
  Status error;
  std::vector<uint8_t> buffer(size);
  size_t bytes_read = m_process->ReadMemory(addr, buffer.data(), size, error);
  
  if (error.Success() && bytes_read > 0) {
    result.resize(bytes_read);
    std::copy(buffer.begin(), buffer.begin() + bytes_read, result.begin());
  }
  
  return result;
}

bool GNUstepMemoryReader::IsValidAddress(addr_t addr) const {
  if (addr == 0 || addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Basic range check for typical heap/stack addresses
  if (addr < 0x1000) {
    return false; // Too low
  }
  
  if (addr >= 0x8000000000000000ULL) {
    return false; // Kernel space
  }
  
  return true;
}

//===----------------------------------------------------------------------===//
// GNUstepTypeDecoder implementation
//===----------------------------------------------------------------------===//

const std::map<char, std::pair<std::string, size_t>> GNUstepTypeDecoder::s_basic_types = {
  {'c', {"char", 1}},
  {'i', {"int", 4}},
  {'s', {"short", 2}},
  {'l', {"long", 8}},
  {'q', {"long long", 8}},
  {'C', {"unsigned char", 1}},
  {'I', {"unsigned int", 4}},
  {'S', {"unsigned short", 2}},
  {'L', {"unsigned long", 8}},
  {'Q', {"unsigned long long", 8}},
  {'f', {"float", 4}},
  {'d', {"double", 8}},
  {'B', {"bool", 1}},
  {'v', {"void", 0}},
  {'*', {"char*", 8}},
  {'@', {"object", 8}},
  {'#', {"Class", 8}},
  {':', {"SEL", 8}}
};

const std::map<std::string, std::string> GNUstepTypeDecoder::s_foundation_map = {
  {"GSMutableDictionary", "NSMutableDictionary"},
  {"GSDictionary", "NSDictionary"},
  {"GSConstantDictionary", "NSDictionary"},
  {"GSMutableArray", "NSMutableArray"},
  {"GSInlineArray", "NSArray"},
  {"GSArray", "NSArray"},
  // Don't map NSConstantString - it needs special handling
  // {"NSConstantString", "NSString"},
  {"GSMutableString", "NSMutableString"},
  {"GSString", "NSString"},
  {"NSIntNumber", "NSNumber"},
  {"NSDoubleNumber", "NSNumber"},
  {"NSFloatNumber", "NSNumber"}
};

GNUstepTypeDecoder::DecodedType 
GNUstepTypeDecoder::DecodeTypeEncoding(const std::string &encoding) {
  DecodedType result = {"unknown", "", 0, false, false};
  
  if (encoding.empty()) {
    return result;
  }
  
  char first_char = encoding[0];
  
  // Handle object types first
  if (first_char == '@') {
    result.base_type = "object";
    result.size = 8;
    result.is_object = true;
    
    // Extract class name from @"ClassName" format
    if (encoding.length() > 2 && encoding[1] == '"') {
      size_t end_quote = encoding.find('"', 2);
      if (end_quote != std::string::npos) {
        result.class_name = encoding.substr(2, end_quote - 2);
      }
    }
    return result;
  }
  
  // Handle pointer types
  if (first_char == '^') {
    result.is_pointer = true;
    result.size = 8;
    
    if (encoding.length() > 1) {
      // Recursively decode the pointed-to type
      DecodedType pointed_type = DecodeTypeEncoding(encoding.substr(1));
      result.base_type = pointed_type.base_type + "*";
      result.class_name = pointed_type.class_name;
    } else {
      result.base_type = "void*";
    }
    return result;
  }
  
  // Handle basic types
  auto it = s_basic_types.find(first_char);
  if (it != s_basic_types.end()) {
    result.base_type = it->second.first;
    result.size = it->second.second;
    
    if (first_char == '*') {
      result.is_pointer = true;
    }
    
    return result;
  }
  
  // Handle struct types {name=...}
  if (first_char == '{') {
    size_t equals_pos = encoding.find('=');
    if (equals_pos != std::string::npos && equals_pos > 1) {
      result.base_type = "struct";
      result.class_name = encoding.substr(1, equals_pos - 1);
      // For now, we don't calculate struct sizes
      result.size = 0;
    }
    return result;
  }
  
  return result;
}

std::string GNUstepTypeDecoder::MapToFoundationType(const std::string &runtime_class) {
  auto it = s_foundation_map.find(runtime_class);
  if (it != s_foundation_map.end()) {
    return it->second;
  }
  
  // If no direct mapping, try pattern matching
  if (runtime_class.find("Dictionary") != std::string::npos) {
    return runtime_class.find("Mutable") != std::string::npos ? 
           "NSMutableDictionary" : "NSDictionary";
  }
  
  if (runtime_class.find("Array") != std::string::npos) {
    return runtime_class.find("Mutable") != std::string::npos ? 
           "NSMutableArray" : "NSArray";
  }
  
  if (runtime_class.find("String") != std::string::npos) {
    return runtime_class.find("Mutable") != std::string::npos ? 
           "NSMutableString" : "NSString";
  }
  
  if (runtime_class.find("Number") != std::string::npos) {
    return "NSNumber";
  }
  
  // Return the original if no mapping found
  return runtime_class;
}

//===----------------------------------------------------------------------===//
// GNUstepCollectionHandler implementation
//===----------------------------------------------------------------------===//

GNUstepCollectionHandler::CollectionType 
GNUstepCollectionHandler::DetectCollectionType(const std::string &class_name) {
  if (class_name.find("String") != std::string::npos) {
    return kString;
  }
  
  if (class_name.find("Array") != std::string::npos) {
    return kArray;
  }
  
  if (class_name.find("Dictionary") != std::string::npos || 
      class_name.find("Dict") != std::string::npos) {
    return kDictionary;
  }
  
  return kNotCollection;
}

GNUstepCollectionHandler::ArrayInfo 
GNUstepCollectionHandler::GetArrayInfo(addr_t obj_addr, const std::string &class_name,
                                      GNUstepMemoryReader &reader) {
  ArrayInfo info = {0, LLDB_INVALID_ADDRESS, false};
  
  if (obj_addr == LLDB_INVALID_ADDRESS) {
    return info;
  }
  
  if (class_name.find("GSInlineArray") != std::string::npos) {
    // GSInlineArray: count at offset 16, elements inline at offset 24
    info.count = static_cast<size_t>(reader.ReadUnsigned(obj_addr + 16, 4));
    info.elements_ptr = obj_addr + 24;
    info.is_inline = true;
  } else if (class_name.find("GSMutableArray") != std::string::npos ||
             class_name.find("GSArray") != std::string::npos) {
    // GSMutableArray: _contents pointer at offset 8, count at offset 16
    addr_t contents_ptr = reader.ReadPointer(obj_addr + 8);
    if (contents_ptr != LLDB_INVALID_ADDRESS) {
      info.count = static_cast<size_t>(reader.ReadUnsigned(obj_addr + 16, 4));
      info.elements_ptr = contents_ptr;
      info.is_inline = false;
    }
  }
  
  return info;
}

GNUstepCollectionHandler::DictionaryInfo 
GNUstepCollectionHandler::GetDictionaryInfo(addr_t obj_addr, const std::string &class_name,
                                           GNUstepMemoryReader &reader) {
  DictionaryInfo info;
  
  if (obj_addr == LLDB_INVALID_ADDRESS) {
    return info;
  }
  
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  // For GNUstep dictionaries, we need to traverse the GSIMapTable
  // This is a simplified implementation - the full traversal would be more complex
  
  // Try to read the count from the map structure
  // Dictionary structure: isa(8) + map(8) -> GSIMapTable
  addr_t map_ptr = reader.ReadPointer(obj_addr + 8);
  if (map_ptr != LLDB_INVALID_ADDRESS) {
    // GSIMapTable structure has nodeCount at offset 8
    uint64_t count = reader.ReadUnsigned(map_ptr + 8, 8);
    if (count > 0 && count < 10000) { // Sanity check
      info.count = static_cast<size_t>(count);
      
      LLDB_LOG(log, "[GNUstep] Dictionary has {0} entries", count);
      
      // For now, we don't traverse the actual key-value pairs
      // This would require understanding the GSIMapTable bucket structure
      // TODO: Implement full GSIMapTable traversal
    }
  }
  
  return info;
}

GNUstepCollectionHandler::StringInfo 
GNUstepCollectionHandler::GetStringInfo(addr_t obj_addr, const std::string &class_name,
                                       GNUstepMemoryReader &reader) {
  StringInfo info = {"", 0, LLDB_INVALID_ADDRESS};
  
  if (obj_addr == LLDB_INVALID_ADDRESS) {
    return info;
  }
  
  // NSConstantString layout (verified from previous debugging):
  // isa@0, flags@8, nxcslen@12, size@16, hash@20, nxcsptr@24
  if (class_name == "NSConstantString") {
    info.length = static_cast<size_t>(reader.ReadUnsigned(obj_addr + 12, 4));
    info.string_ptr = reader.ReadPointer(obj_addr + 24);
    
    if (info.string_ptr != LLDB_INVALID_ADDRESS && info.length > 0 && info.length < 1024) {
      info.content = reader.ReadCString(info.string_ptr, info.length);
    }
  }
  
  return info;
}

//===----------------------------------------------------------------------===//
// GNUstepSymbolResolver implementation
//===----------------------------------------------------------------------===//

addr_t GNUstepSymbolResolver::FindSymbol(const std::string &symbol_name) {
  if (!m_process) {
    return LLDB_INVALID_ADDRESS;
  }
  
  Target &target = m_process->GetTarget();
  SymbolContext sc;
  
  // Search in all modules
  const ModuleList &modules = target.GetImages();
  std::lock_guard<std::recursive_mutex> guard(modules.GetMutex());
  
  for (size_t i = 0; i < modules.GetSize(); ++i) {
    ModuleSP module_sp = modules.GetModuleAtIndex(i);
    if (!module_sp) {
      continue;
    }
    
    SymbolFile *symbol_file = module_sp->GetSymbolFile();
    if (!symbol_file) {
      continue;
    }
    
    // Look for the symbol
    ConstString symbol_const(symbol_name.c_str());
    const Symbol *symbol = module_sp->FindFirstSymbolWithNameAndType(symbol_const, eSymbolTypeCode);
    
    if (symbol) {
      addr_t load_addr = symbol->GetLoadAddress(&target);
      if (load_addr != LLDB_INVALID_ADDRESS) {
        return load_addr;
      }
    }
  }
  
  return LLDB_INVALID_ADDRESS;
}

GNUstepSymbolResolver::RuntimeAddresses 
GNUstepSymbolResolver::LoadRuntimeAddresses() {
  RuntimeAddresses addrs;
  
  addrs.objc_copyClassList = FindSymbol("objc_copyClassList");
  addrs.class_getName = FindSymbol("class_getName");
  addrs.class_copyIvarList = FindSymbol("class_copyIvarList");
  addrs.ivar_getName = FindSymbol("ivar_getName");
  addrs.ivar_getOffset = FindSymbol("ivar_getOffset");
  addrs.ivar_getTypeEncoding = FindSymbol("ivar_getTypeEncoding");
  addrs.free_func = FindSymbol("free");
  
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "[GNUstep] Runtime addresses loaded:");
  LLDB_LOG(log, "  objc_copyClassList: 0x{0:x}", addrs.objc_copyClassList);
  LLDB_LOG(log, "  class_getName: 0x{0:x}", addrs.class_getName);
  LLDB_LOG(log, "  class_copyIvarList: 0x{0:x}", addrs.class_copyIvarList);
  
  return addrs;
}
