//===-- GNUstepUtilities.h --------------------------------------*- C++ -*-===//
//
// Utility classes and functions for GNUstep Objective-C runtime support
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPUTILS_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPUTILS_H

#include "lldb/lldb-private.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/Status.h"
#include "lldb/Core/Address.h"

#include <string>
#include <vector>

namespace lldb_private {

// Forward declarations
class ValueObject;

// Address resolution utilities for LLDB dynamic value handling
class GNUstepAddressResolver {
public:
  // Resolve object address handling ISA pointer vs structure address
  // This fixes the issue where target.prefer-dynamic-value causes
  // GetValueAsUnsigned() to return ISA/class pointers instead of object addresses
  static lldb::addr_t ResolveObjectAddress(ValueObject &valobj);
  
  // Check if an address looks like an ISA pointer (high memory address)
  static bool IsLikelyISAPointer(lldb::addr_t addr);
  
  // Map GNUstep internal class names to Foundation public class names
  static const char *GetFoundationTypeName(const char *gnustep_name);
};

// Utility class for memory operations
class GNUstepMemoryReader {
public:
  explicit GNUstepMemoryReader(Process *process) : m_process(process) {}
  
  // Read unsigned integer of specified size
  uint64_t ReadUnsigned(lldb::addr_t addr, size_t size, uint64_t default_val = 0);
  
  // Read pointer value
  lldb::addr_t ReadPointer(lldb::addr_t addr);
  
  // Read C string
  std::string ReadCString(lldb::addr_t addr, size_t max_len = 256);
  
  // Read raw bytes
  std::vector<uint8_t> ReadBytes(lldb::addr_t addr, size_t size);
  
  // Check if address is valid for reading
  bool IsValidAddress(lldb::addr_t addr) const;
  
private:
  Process *m_process;
};

// Type encoding decoder for Objective-C types
class GNUstepTypeDecoder {
public:
  struct DecodedType {
    std::string base_type;     // "object", "int", "float", etc.
    std::string class_name;    // For object types: "NSString", etc.
    size_t size;              // Size in bytes
    bool is_pointer;          // Is this a pointer type?
    bool is_object;           // Is this an object reference (@)?
  };
  
  static DecodedType DecodeTypeEncoding(const std::string &encoding);
  
  // Map runtime class names to Foundation equivalents
  static std::string MapToFoundationType(const std::string &runtime_class);
  
private:
  static const std::map<char, std::pair<std::string, size_t>> s_basic_types;
  static const std::map<std::string, std::string> s_foundation_map;
};

// Collection type detector and handler
class GNUstepCollectionHandler {
public:
  enum CollectionType {
    kNotCollection,
    kArray,
    kDictionary,
    kString
  };
  
  static CollectionType DetectCollectionType(const std::string &class_name);
  
  // Array handling
  struct ArrayInfo {
    size_t count;
    lldb::addr_t elements_ptr;
    bool is_inline;  // GSInlineArray vs GSMutableArray
  };
  
  static ArrayInfo GetArrayInfo(lldb::addr_t obj_addr, const std::string &class_name, 
                               GNUstepMemoryReader &reader);
  
  // Dictionary handling
  struct DictionaryInfo {
    size_t count;
    std::vector<std::pair<lldb::addr_t, lldb::addr_t>> key_value_pairs;
  };
  
  static DictionaryInfo GetDictionaryInfo(lldb::addr_t obj_addr, const std::string &class_name,
                                         GNUstepMemoryReader &reader);
  
  // String handling
  struct StringInfo {
    std::string content;
    size_t length;
    lldb::addr_t string_ptr;
  };
  
  static StringInfo GetStringInfo(lldb::addr_t obj_addr, const std::string &class_name,
                                 GNUstepMemoryReader &reader);
};

// Runtime symbol resolver
class GNUstepSymbolResolver {
public:
  explicit GNUstepSymbolResolver(Process *process) : m_process(process) {}
  
  // Find runtime function addresses
  lldb::addr_t FindSymbol(const std::string &symbol_name);
  
  // Get commonly used runtime function addresses
  struct RuntimeAddresses {
    lldb::addr_t objc_copyClassList = LLDB_INVALID_ADDRESS;
    lldb::addr_t class_getName = LLDB_INVALID_ADDRESS;
    lldb::addr_t class_copyIvarList = LLDB_INVALID_ADDRESS;
    lldb::addr_t ivar_getName = LLDB_INVALID_ADDRESS;
    lldb::addr_t ivar_getOffset = LLDB_INVALID_ADDRESS;
    lldb::addr_t ivar_getTypeEncoding = LLDB_INVALID_ADDRESS;
    lldb::addr_t free_func = LLDB_INVALID_ADDRESS;
  };
  
  RuntimeAddresses LoadRuntimeAddresses();
  
private:
  Process *m_process;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPUTILS_H
