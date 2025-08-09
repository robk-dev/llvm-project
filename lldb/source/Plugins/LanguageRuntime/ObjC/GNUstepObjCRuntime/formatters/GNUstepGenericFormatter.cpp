//===-- GNUstepGenericFormatter.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepGenericFormatter.h"
#include "GNUstepStringFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <map>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

// Correct struct definitions for GNUstep/libobjc2 runtime
struct objc_ivar {
  lldb::addr_t name;          // const char* (address to name string)
  lldb::addr_t type;          // const char* (address to type encoding)
  lldb::addr_t offset;        // int* (address to offset value)
  uint32_t size;              // Size of this ivar
  uint32_t flags;             // Flags
};

struct objc_ivar_list {
  uint32_t count;             // Number of ivars (int in runtime)
  uint32_t padding;           // Padding for alignment on 64-bit
  uint64_t size;              // Size of each ivar struct (size_t in runtime)
  // ivars follow...
};

bool GNUstepGenericFormatter::WouldWork(ValueObject& valobj) {
  // This formatter works for any Objective-C object
  return GNUstepRuntimeHelper::IsValidGNUstepObject(valobj);
}

bool GNUstepGenericFormatter::FormatObject(ValueObject &valobj, Stream &stream,
                                          const TypeSummaryOptions &options) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    WriteErrorSummary(stream, "no process");
    return false;
  }

  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    stream.PutCString("nil");
    return true;
  }

  // Get the class name first
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  if (class_name.empty()) {
    WriteErrorSummary(stream, "unknown class");
    return false;
  }

  // Check if this class has a specific formatter
  // (In practice, specific formatters are registered first and take precedence)
  
  // Collect all ivars from the entire class hierarchy
  std::vector<IvarInfo> all_ivars = CollectAllIvars(process, obj_addr);
  
  // Format the output
  stream.Printf("%s(", class_name.c_str());
  
  bool first = true;
  for (const auto& ivar : all_ivars) {
    if (!first) {
      stream.PutCString(", ");
    }
    first = false;
    
    stream.Printf("%s=", ivar.name.c_str());
    std::string formatted_value = FormatIvar(process, ivar);
    stream.PutCString(formatted_value.c_str());
  }
  
  stream.PutChar(')');
  return true;
}

lldb::addr_t GNUstepGenericFormatter::GetClassFromObject(Process *process, 
                                                         lldb::addr_t obj_addr) {
  if (!process || obj_addr == LLDB_INVALID_ADDRESS)
    return LLDB_INVALID_ADDRESS;
  
  Status error;
  lldb::addr_t isa = GNUstepRuntimeHelper::ReadPointer(process, obj_addr, error);
  if (error.Fail())
    return LLDB_INVALID_ADDRESS;
    
  return isa;
}

lldb::addr_t GNUstepGenericFormatter::GetSuperclass(Process *process, 
                                                    lldb::addr_t class_addr) {
  if (!process || class_addr == LLDB_INVALID_ADDRESS)
    return LLDB_INVALID_ADDRESS;
  
  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // Read the super_class field (second pointer in the struct)
  Status error;
  lldb::addr_t super_addr = class_addr + addr_size;
  lldb::addr_t super_class = GNUstepRuntimeHelper::ReadPointer(process, super_addr, error);
  
  if (error.Fail() || super_class == 0)
    return LLDB_INVALID_ADDRESS;
    
  return super_class;
}

std::string GNUstepGenericFormatter::GetClassName(Process *process, 
                                                  lldb::addr_t class_addr) {
  if (!process || class_addr == LLDB_INVALID_ADDRESS)
    return "";
  
  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // Read the name field (third pointer in the struct)
  Status error;
  lldb::addr_t name_addr_ptr = class_addr + (addr_size * 2);
  lldb::addr_t name_addr = GNUstepRuntimeHelper::ReadPointer(process, name_addr_ptr, error);
  
  if (error.Fail() || name_addr == 0)
    return "";
    
  return GNUstepRuntimeHelper::ReadUTF8String(process, name_addr, 256);
}

std::vector<IvarInfo> GNUstepGenericFormatter::ExtractIvarsFromClass(
    Process *process, lldb::addr_t class_addr, lldb::addr_t obj_addr) {
  
  std::vector<IvarInfo> ivars;
  
  // ExtractIvarsFromClass: class_addr, obj_addr
  
  if (!process || class_addr == LLDB_INVALID_ADDRESS || obj_addr == LLDB_INVALID_ADDRESS) {
  // printf("ExtractIvarsFromClass: Invalid process or class address\n");
    return ivars;
  }
  
  // Validate process state
  if (!process->IsValid()) {
  // printf("ExtractIvarsFromClass: Process is not in valid state\n");
    return ivars;
  }
  
  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  Status error;
  
  // Read the ivars pointer (7th field in objc_class)
  lldb::addr_t ivars_addr_ptr = class_addr + (addr_size * 6);
  lldb::addr_t ivars_addr = GNUstepRuntimeHelper::ReadPointer(process, ivars_addr_ptr, error);
  
  // printf("ExtractIvarsFromClass: ivars pointer at 0x%llx = 0x%llx\n");
  
  if (error.Fail() || ivars_addr == 0) {
  // printf("ExtractIvarsFromClass: No ivars pointer or read failed\n");
    return ivars;
  }
  
  // Read the ivar list header with proper validation
  objc_ivar_list ivar_list;
  memset(&ivar_list, 0, sizeof(ivar_list)); // Initialize to prevent garbage values
  
  // Validate the address before reading
  if (ivars_addr == 0 || ivars_addr == LLDB_INVALID_ADDRESS) {
  // printf("ExtractIvarsFromClass: Invalid ivars address\n");
    return ivars;
  }
  
  const size_t ivar_list_header_size = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t); // count + padding + size
  if (!GNUstepRuntimeHelper::ReadMemory(process, ivars_addr, &ivar_list, ivar_list_header_size)) {
  // printf("ExtractIvarsFromClass: Failed to read ivar list header\n");
    return ivars;
  }
  
  // printf("ExtractIvarsFromClass: ivar_list count=%d, size=%d\n");
  
  // Comprehensive sanity checks
  if (ivar_list.count == 0) {
  // printf("ExtractIvarsFromClass: No ivars in this class\n");
    return ivars; // Not an error, just no ivars
  }
  
  if (ivar_list.count > MAX_LOOP_ITERATIONS) {
  // printf("ExtractIvarsFromClass: Too many ivars (%u), limiting to %u\n", ivar_list.count, MAX_LOOP_ITERATIONS);
    // Don't return empty - process what we can safely
  }
  
  if (ivar_list.size == 0 || ivar_list.size > 1024) { // Reasonable upper bound for ivar struct size
  // printf("ExtractIvarsFromClass: Invalid ivar struct size: %llu\n", (unsigned long long)ivar_list.size);
    return ivars;
  }
  
  // Read each ivar with proper bounds checking
  lldb::addr_t ivar_ptr = ivars_addr + ivar_list_header_size; // Skip header
  
  // Additional safety check for the starting address
  if (ivar_ptr == LLDB_INVALID_ADDRESS) {
  // printf("ExtractIvarsFromClass: Invalid ivar array start address\n");
    return ivars;
  }
  
  // printf("ExtractIvarsFromClass: Starting to read %d ivars from 0x%llx\n");
  
  const uint32_t safe_count = (ivar_list.count > MAX_LOOP_ITERATIONS) ? MAX_LOOP_ITERATIONS : ivar_list.count;
  
  for (uint32_t i = 0; i < safe_count; ++i) {
  // printf("ExtractIvarsFromClass: Reading ivar %d at 0x%llx\n", i, (unsigned long long)ivar_ptr);
    
    // Validate the ivar pointer before reading
    if (ivar_ptr == 0 || ivar_ptr == LLDB_INVALID_ADDRESS) {
  // printf("ExtractIvarsFromClass: Invalid ivar pointer at index %u\n", i);
      break;
    }
    
    // Read the ivar structure with proper size validation
    objc_ivar ivar_data;
    memset(&ivar_data, 0, sizeof(ivar_data)); // Initialize to prevent garbage
    
    const size_t ivar_struct_size = sizeof(lldb::addr_t) * 3 + sizeof(uint32_t) * 2; // name, type, offset, size, flags
    if (!GNUstepRuntimeHelper::ReadMemory(process, ivar_ptr, &ivar_data, ivar_struct_size)) {
  // printf("ExtractIvarsFromClass: Failed to read ivar at 0x%llx\n", (unsigned long long)ivar_ptr);
      break; // Don't continue if we can't read this ivar
    }
    
  // printf("ExtractIvarsFromClass: ivar %d - name=0x%llx, type=0x%llx, offset=0x%llx, size=%u\n");
    
    IvarInfo info;
    
    // Read ivar name with proper validation
    if (ivar_data.name != 0 && ivar_data.name != LLDB_INVALID_ADDRESS) {
      info.name = GNUstepRuntimeHelper::ReadUTF8String(process, ivar_data.name, 256);
      // Validate the name was read successfully
      if (info.name.empty()) {
  // printf("ExtractIvarsFromClass: Failed to read ivar name at 0x%llx\n", (unsigned long long)ivar_data.name);
        info.name = "<unknown>";
      }
  // printf("  Name: '%s'\n", info.name.c_str());
    } else {
      info.name = "<unnamed>";
    }
    
    // Read ivar type encoding with proper validation
    if (ivar_data.type != 0 && ivar_data.type != LLDB_INVALID_ADDRESS) {
      info.type_encoding = GNUstepRuntimeHelper::ReadUTF8String(process, ivar_data.type, 256);
      // Validate the type encoding was read successfully
      if (info.type_encoding.empty()) {
  // printf("ExtractIvarsFromClass: Failed to read ivar type at 0x%llx\n", (unsigned long long)ivar_data.type);
        info.type_encoding = "?";
      }
  // printf("  Type: '%s'\n", info.type_encoding.c_str());
    } else {
      info.type_encoding = "?";
    }
    
    // Read the offset value with proper validation (it's a pointer to the offset)
    if (ivar_data.offset != 0 && ivar_data.offset != LLDB_INVALID_ADDRESS) {
      int32_t offset_value = 0;
      if (GNUstepRuntimeHelper::ReadMemory(process, ivar_data.offset, 
                                           &offset_value, sizeof(int32_t))) {
        info.offset = offset_value;
  // printf("  Ivar '%s': Offset: %d, Size: %u, Type: %s\n", 
  //        info.name.c_str(), offset_value, ivar_data.size, info.type_encoding.c_str());
      } else {
  // printf("ExtractIvarsFromClass: Failed to read ivar offset at 0x%llx\n", (unsigned long long)ivar_data.offset);
        info.offset = -1; // Invalid offset
      }
    } else {
      info.offset = -1; // Invalid offset
    }
    
    info.size = ivar_data.size;
    
    // Only calculate value_addr if we have a valid offset
    if (info.offset >= 0) {
      info.value_addr = obj_addr + info.offset;
  // printf("    -> value_addr = 0x%llx (obj_addr 0x%llx + offset %d)\n", 
  //        (unsigned long long)info.value_addr, (unsigned long long)obj_addr, info.offset);
      
      // Basic sanity check for the calculated address
      if (info.value_addr != LLDB_INVALID_ADDRESS && info.value_addr != 0) {
        ivars.push_back(info);
      } else {
  // printf("ExtractIvarsFromClass: Invalid calculated value address for ivar '%s'\n", info.name.c_str());
      }
    } else {
  // printf("ExtractIvarsFromClass: Skipping ivar '%s' due to invalid offset\n", info.name.c_str());
    }
    
    // Move to next ivar with overflow protection
    if (ivar_ptr > LLDB_INVALID_ADDRESS - ivar_list.size) {
  // printf("ExtractIvarsFromClass: Address overflow protection triggered\n");
      break;
    }
    ivar_ptr += ivar_list.size;
  }
  
  return ivars;
}

std::vector<IvarInfo> GNUstepGenericFormatter::CollectAllIvars(Process *process,
                                                               lldb::addr_t obj_addr) {
  std::vector<IvarInfo> all_ivars;
  
  // printf("CollectAllIvars: Starting for object at 0x%llx\n", (unsigned long long)obj_addr);
  
  if (!process || obj_addr == LLDB_INVALID_ADDRESS) {
  // printf("CollectAllIvars: Invalid process or object address\n");
    return all_ivars;
  }
  
  // Get the class of this object
  lldb::addr_t class_addr = GetClassFromObject(process, obj_addr);
  // printf("CollectAllIvars: Class address is 0x%llx\n", (unsigned long long)class_addr);
  if (class_addr == LLDB_INVALID_ADDRESS) {
  // printf("CollectAllIvars: Failed to get class address\n");
    return all_ivars;
  }
  
  // Walk up the class hierarchy
  std::vector<lldb::addr_t> class_hierarchy;
  lldb::addr_t current_class = class_addr;
  
  // Collect classes from most derived to base
  while (current_class != LLDB_INVALID_ADDRESS && current_class != 0) {
    std::string class_name = GetClassName(process, current_class);
  // printf("CollectAllIvars: Found class '%s' at 0x%llx\n");
    class_hierarchy.push_back(current_class);
    current_class = GetSuperclass(process, current_class);
    
    // Prevent infinite loops
    if (class_hierarchy.size() > MAX_FORMATTER_DEPTH)
      break;
  }
  
  // printf("CollectAllIvars: Class hierarchy has %zu classes\n", class_hierarchy.size());
  
  // Now collect ivars from base to most derived (reverse order)
  for (auto it = class_hierarchy.rbegin(); it != class_hierarchy.rend(); ++it) {
    std::string class_name = GetClassName(process, *it);
    std::vector<IvarInfo> class_ivars = ExtractIvarsFromClass(process, *it, obj_addr);
  // printf("CollectAllIvars: Found %zu ivars in class '%s'\n", class_ivars.size(), class_name.c_str());
    
    // Add class name prefix for inherited ivars (optional, for clarity)
    // Currently we don't prefix inherited ivars, but could be enabled in the future
    // if (it != class_hierarchy.rbegin()) {
    //   std::string class_name = GetClassName(process, *it);
    //   if (!class_name.empty() && class_name != "NSObject") {
    //     for (auto& ivar : class_ivars) {
    //       // Optionally prefix inherited ivars
    //       // ivar.name = class_name + "." + ivar.name;
    //     }
    //   }
    // }
    
    all_ivars.insert(all_ivars.end(), class_ivars.begin(), class_ivars.end());
  }
  
  // printf("CollectAllIvars: Total ivars collected: %zu\n", all_ivars.size());
  
  return all_ivars;
}

std::string GNUstepGenericFormatter::FormatIvar(Process *process, const IvarInfo &ivar) {
  if (!process || ivar.value_addr == LLDB_INVALID_ADDRESS)
    return "<invalid>";
  
  // Special handling for ISA field (which is a Class pointer, not an object)
  if (ivar.name == "isa") {
    Status error;
    lldb::addr_t class_addr = GNUstepRuntimeHelper::ReadPointer(process, ivar.value_addr, error);
    if (!error.Fail() && class_addr != 0 && class_addr != LLDB_INVALID_ADDRESS) {
      std::string class_name = GetClassName(process, class_addr);
      if (!class_name.empty()) {
        return class_name;
      }
    }
    return "<Class>";
  }
  
  BasicType type = GetBasicType(ivar.type_encoding);
  
  switch (type) {
    case BasicType::Object:
      return FormatObjectIvar(process, ivar.value_addr);
      
    case BasicType::Integer:
    case BasicType::Float:
    case BasicType::Boolean:
      return FormatPrimitiveIvar(process, ivar);
      
    case BasicType::CString: {
      Status error;
      lldb::addr_t str_addr = GNUstepRuntimeHelper::ReadPointer(process, 
                                                                ivar.value_addr, error);
      if (!error.Fail() && str_addr != 0)
        return FormatCStringIvar(process, str_addr);
      return "NULL";
    }
    
    case BasicType::Pointer: {
      Status error;
      lldb::addr_t ptr_value = GNUstepRuntimeHelper::ReadPointer(process, 
                                                                 ivar.value_addr, error);
      if (!error.Fail()) {
        if (ptr_value == 0)
          return "NULL";
        std::stringstream ss;
        ss << "0x" << std::hex << ptr_value;
        return ss.str();
      }
      return "<error>";
    }
    
    case BasicType::Struct:
      return FormatStructIvar(process, ivar);
      
    case BasicType::Array:
      return "<array>";
      
    default:
      return "<unknown type>";
  }
}

std::string GNUstepGenericFormatter::FormatObjectIvar(Process *process, 
                                                      lldb::addr_t obj_addr) {
  if (!process || !process->IsValid())
    return "nil";
  
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS)
    return "nil";
  
  // CRITICAL FIX: For object ivars, obj_addr is the ADDRESS where the object pointer is stored,
  // not the object itself. We need to read the pointer first.
  Status error;
  lldb::addr_t obj_ptr = GNUstepRuntimeHelper::ReadPointer(process, obj_addr, error);
  
  if (error.Fail() || obj_ptr == 0 || obj_ptr == LLDB_INVALID_ADDRESS) {
    return "nil";
  }
  
  // Check if this is a tagged string
  if ((obj_ptr & 0x8000000000000000ULL) != 0) {
    // This is a tagged string
    GNUstepObjCRuntimeIntrospector introspector(process);
    std::string decoded = introspector.DecodeTaggedString(obj_ptr);
    if (!decoded.empty()) {
      return "\"" + decoded + "\"";
    }
    return "<tagged_string>";
  }
  
  // Verify it's a valid object by checking for ISA at offset 0
  lldb::addr_t isa = GNUstepRuntimeHelper::ReadPointer(process, obj_ptr, error);
  
  if (error.Fail() || isa == 0 || isa == LLDB_INVALID_ADDRESS) {
    return "nil";
  }
  
  // Now obj_ptr definitely points to a valid object
  // Use the ISA we already read
  if (isa == LLDB_INVALID_ADDRESS)
    return "<invalid object>";
  
  std::string class_name = GetClassName(process, isa);
  if (class_name.empty())
    return "<unknown object>";
  
  // For known Foundation classes, provide better summaries
  if (class_name.find("NSConstantString") != std::string::npos || 
      class_name.find("__NSConstantString") != std::string::npos) {
    // NSConstantString layout: { Class isa; char *cString; uint32_t length; }
    lldb::addr_t str_ptr_addr = obj_ptr + 8;  // char* at offset 8
    lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
    
    if (!error.Fail() && str_data_addr != 0) {
      // Read the string directly
      std::string str_content = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 256);
      if (!str_content.empty()) {
        // Truncate for display
        if (str_content.length() > 64) {
          str_content = str_content.substr(0, 61) + "...";
        }
        return "\"" + str_content + "\"";
      }
    }
  } else if (class_name.find("NSString") != std::string::npos ||
      class_name.find("GSString") != std::string::npos ||
      class_name.find("GSCString") != std::string::npos) {
    // Try to extract string content for other string types
    uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
    lldb::addr_t chars_addr = obj_ptr + addr_size + 4 + 4; // Skip isa, count, len
    std::string str_content = GNUstepRuntimeHelper::ReadUTF8String(process, 
                                                                   chars_addr, 64);
    if (!str_content.empty()) {
      return "\"" + str_content + "\"";
    }
  } else if (class_name.find("NSNumber") != std::string::npos ||
             class_name.find("GSNumber") != std::string::npos) {
    // Could extract number value
    return "<" + class_name + ">";
  } else if (class_name.find("NSArray") != std::string::npos ||
             class_name.find("GSArray") != std::string::npos) {
    // Try to get count
    uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
    lldb::addr_t count_addr = obj_ptr + addr_size; // Count is usually after isa
    uint32_t count = 0;
    GNUstepRuntimeHelper::ReadMemory(process, count_addr, &count, 4);
    std::stringstream ss;
    ss << "<" << count << " items>";
    return ss.str();
  } else if (class_name.find("NSDictionary") != std::string::npos ||
             class_name.find("GSDictionary") != std::string::npos) {
    return "<" + class_name + ">";
  }
  
  // For other objects, just show the class name
  std::stringstream ss;
  ss << "<" << class_name << " 0x" << std::hex << obj_ptr << ">";
  return ss.str();
}

std::string GNUstepGenericFormatter::FormatPrimitiveIvar(Process *process, 
                                                         const IvarInfo &ivar) {
  if (!process || ivar.value_addr == LLDB_INVALID_ADDRESS)
    return "<invalid>";
  
  BasicType type = GetBasicType(ivar.type_encoding);
  std::stringstream ss;
  
  if (type == BasicType::Integer) {
    // Check if signed or unsigned
    bool is_unsigned = (ivar.type_encoding.find_first_of("ILQ") != std::string::npos);
    
    if (is_unsigned) {
      uint64_t value = ReadUnsignedInteger(process, ivar.value_addr, ivar.size);
      ss << value;
    } else {
      int64_t value = ReadSignedInteger(process, ivar.value_addr, ivar.size);
      ss << value;
    }
  } else if (type == BasicType::Float) {
    double value = ReadFloatingPoint(process, ivar.value_addr, ivar.size);
    ss << std::fixed << std::setprecision(2) << value;
  } else if (type == BasicType::Boolean) {
    uint8_t value = 0;
    GNUstepRuntimeHelper::ReadMemory(process, ivar.value_addr, &value, 1);
    ss << (value ? "YES" : "NO");
  }
  
  return ss.str();
}

std::string GNUstepGenericFormatter::FormatCStringIvar(Process *process, 
                                                       lldb::addr_t str_addr) {
  if (!process || str_addr == 0)
    return "NULL";
  
  std::string str = GNUstepRuntimeHelper::ReadUTF8String(process, str_addr, 256);
  if (str.empty())
    return "\"\"";
  
  return "\"" + str + "\"";
}

std::string GNUstepGenericFormatter::FormatStructIvar(Process *process, 
                                                      const IvarInfo &ivar) {
  // Basic struct formatting - could be enhanced to parse struct fields
  std::stringstream ss;
  ss << "{" << ivar.type_encoding << "}";
  return ss.str();
}

GNUstepGenericFormatter::BasicType GNUstepGenericFormatter::GetBasicType(
    const std::string &type_encoding) {
  
  if (type_encoding.empty())
    return BasicType::Unknown;
  
  char first_char = type_encoding[0];
  
  switch (first_char) {
    case '@':
      return BasicType::Object;
    case 'i': case 'I': case 's': case 'S':
    case 'l': case 'L': case 'q': case 'Q':
      return BasicType::Integer;
    case 'f': case 'd':
      return BasicType::Float;
    case 'c': case 'C':
      // Check if it's actually a BOOL (size 1)
      if (type_encoding.length() == 1)
        return BasicType::Boolean;
      return BasicType::Integer;
    case '*':
      return BasicType::CString;
    case '^':
      return BasicType::Pointer;
    case '{':
      return BasicType::Struct;
    case '[':
      return BasicType::Array;
    default:
      return BasicType::Unknown;
  }
}

int64_t GNUstepGenericFormatter::ReadSignedInteger(Process *process, 
                                                   lldb::addr_t addr, 
                                                   uint32_t size) {
  if (!process || addr == LLDB_INVALID_ADDRESS)
    return 0;
  
  int64_t value = 0;
  
  switch (size) {
    case 1: {
      int8_t val8 = 0;
      GNUstepRuntimeHelper::ReadMemory(process, addr, &val8, 1);
      value = val8;
      break;
    }
    case 2: {
      int16_t val16 = 0;
      GNUstepRuntimeHelper::ReadMemory(process, addr, &val16, 2);
      value = val16;
      break;
    }
    case 4: {
      int32_t val32 = 0;
      GNUstepRuntimeHelper::ReadMemory(process, addr, &val32, 4);
      value = val32;
      break;
    }
    case 8: {
      GNUstepRuntimeHelper::ReadMemory(process, addr, &value, 8);
      break;
    }
  }
  
  return value;
}

uint64_t GNUstepGenericFormatter::ReadUnsignedInteger(Process *process, 
                                                      lldb::addr_t addr, 
                                                      uint32_t size) {
  if (!process || addr == LLDB_INVALID_ADDRESS)
    return 0;
  
  uint64_t value = 0;
  
  switch (size) {
    case 1: {
      uint8_t val8 = 0;
      GNUstepRuntimeHelper::ReadMemory(process, addr, &val8, 1);
      value = val8;
      break;
    }
    case 2: {
      uint16_t val16 = 0;
      GNUstepRuntimeHelper::ReadMemory(process, addr, &val16, 2);
      value = val16;
      break;
    }
    case 4: {
      uint32_t val32 = 0;
      GNUstepRuntimeHelper::ReadMemory(process, addr, &val32, 4);
      value = val32;
      break;
    }
    case 8: {
      GNUstepRuntimeHelper::ReadMemory(process, addr, &value, 8);
      break;
    }
  }
  
  return value;
}

double GNUstepGenericFormatter::ReadFloatingPoint(Process *process, 
                                                  lldb::addr_t addr, 
                                                  uint32_t size) {
  if (!process || addr == LLDB_INVALID_ADDRESS)
    return 0.0;
  
  if (size == 4) {
    float value = 0.0f;
    GNUstepRuntimeHelper::ReadMemory(process, addr, &value, 4);
    return static_cast<double>(value);
  } else if (size == 8) {
    double value = 0.0;
    GNUstepRuntimeHelper::ReadMemory(process, addr, &value, 8);
    return value;
  }
  
  return 0.0;
}

bool lldb_private::formatters::GNUstepGenericFormatterFunction(ValueObject &valobj, Stream &stream,
                                     const TypeSummaryOptions &options) {
  GNUstepGenericFormatter formatter;
  return formatter.FormatObject(valobj, stream, options);
}

// ===== GNUstepGenericObjectSyntheticProvider Implementation =====

GNUstepGenericObjectSyntheticProvider::GNUstepGenericObjectSyntheticProvider(
    lldb::ValueObjectSP valobj_sp)
    : GNUstepSyntheticProvider(valobj_sp), m_obj_addr(LLDB_INVALID_ADDRESS) {
}

bool GNUstepGenericObjectSyntheticProvider::UpdateImpl() {
  m_ivars.clear();
  m_children_cache.clear(); // Clear child cache on update
  
  if (!m_process)
    return false;
    
  m_obj_addr = m_backend.GetPointerValue();
  if (m_obj_addr == 0 || m_obj_addr == LLDB_INVALID_ADDRESS)
    return false;
  
  // CRITICAL: Check class name and skip problematic types that should not have synthetic children
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(m_backend);
  if (class_name == "NSException" || class_name == "NSIndexPath" || 
      class_name == "NSNotification" || class_name == "GSException" ||
      class_name == "GSIndexPath" || class_name == "GSNotification") {
    // These types should use their specific formatters with noop synthetic providers
    // Returning false prevents synthetic children from being created
    return false;
  }
  
  // Create a temporary formatter to reuse its ivar collection logic
  GNUstepGenericFormatter formatter;
  std::vector<IvarInfo> all_ivars = formatter.CollectAllIvars(m_process, m_obj_addr);
  
  // Debug: Print number of ivars collected
  // printf("GNUstepGenericObjectSyntheticProvider: Collected %zu ivars for object at 0x%llx\n");
  
  // Filter out the isa pointer and any other runtime-internal ivars
  for (const auto& ivar : all_ivars) {
    // Debug: Print each ivar
  // printf("  Ivar: name='%s', type='%s', offset=%d\n");
    
    // Skip isa pointer - it's always the first ivar at offset 0 with type "@"
    if (ivar.name == "isa")
      continue;
      
    // Optionally skip other runtime internals (uncomment if needed)
    // if (ivar.name.starts_with("_"))
    //   continue;
      
    m_ivars.push_back(ivar);
  }
  
  // printf("GNUstepGenericObjectSyntheticProvider: After filtering, %zu ivars remain\n", m_ivars.size());
  
  return true;
}

llvm::Expected<uint32_t> GNUstepGenericObjectSyntheticProvider::CalculateNumChildren() {
  if (!m_update_called)
    return 0;
  return static_cast<uint32_t>(m_ivars.size());
}

lldb::ValueObjectSP GNUstepGenericObjectSyntheticProvider::GetChildAtIndex(uint32_t idx) {
  // printf("GNUstepGenericObjectSyntheticProvider::GetChildAtIndex(%u) called, m_update_called=%d, m_ivars.size()=%zu\n", idx, m_update_called, m_ivars.size());
  
  // CRITICAL FIX: Add safety checks to prevent crashes when expanding problematic objects
  if (!m_update_called || idx >= m_ivars.size())
    return nullptr;
  
  // CRITICAL FIX: Check child cache first to avoid recreation and address reuse
  auto cache_iter = m_children_cache.find(idx);
  if (cache_iter != m_children_cache.end() && cache_iter->second) {
    // printf("  Returning cached child at index %u: %p\n", idx, cache_iter->second.get());
    return cache_iter->second;
  }
  
  // CRITICAL FIX: Check if backend is valid to prevent crash when accessing execution context
  // ValueObject doesn't have IsValid() method, check pointer validity instead
  if (!&m_backend) {
    return nullptr;
  }
    
  const IvarInfo& ivar = m_ivars[idx];
  
  // CRITICAL FIX: Add bounds checking for ivar access to prevent buffer overrun
  if (ivar.name.empty() || ivar.type_encoding.empty()) {
    return nullptr;
  }
  
  // Determine the type for the ivar
  CompilerType ivar_type;
  
  // CRITICAL FIX: Wrap execution context creation in safety checks
  ExecutionContextRef exe_ctx_ref = m_backend.GetExecutionContextRef();
  // Skip complex validation - if the ref is invalid, ExecutionContext will handle it safely
  
  ExecutionContext exe_ctx(exe_ctx_ref);
  
  // CRITICAL FIX: Add safety checks around type system access to prevent crashes
  CompilerType backend_type = m_backend.GetCompilerType();
  if (!backend_type.IsValid()) {
    return nullptr;
  }
  
  // For object types, use 'id'
  if (ivar.type_encoding[0] == '@') {
    auto type_system = backend_type.GetTypeSystem();
    if (type_system) {
      ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeObjCID);
    }
  } else {
    // For primitive types, try to get the appropriate type
    // This is simplified - a full implementation would parse the type encoding
    auto type_system = backend_type.GetTypeSystem();
    if (type_system) {
      switch (ivar.type_encoding[0]) {
        case 'i':
          ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeInt);
          break;
        case 'l':
          ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeLong);
          break;
        case 'q':
          ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeLongLong);
          break;
        case 'I':
          ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeUnsignedInt);
          break;
        case 'L':
          ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeUnsignedLong);
          break;
        case 'Q':
          ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeUnsignedLongLong);
          break;
        case 'f':
          ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeFloat);
          break;
        case 'd':
          ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeDouble);
          break;
        case 'c':
          ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeChar);
          break;
        case 'C':
          ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeUnsignedChar);
          break;
        case '*':
          ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeChar).GetPointerType();
          break;
        default:
          // For unknown types, use void*
          ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeVoid).GetPointerType();
          break;
      }
    }
  }
  
  if (!ivar_type.IsValid()) {
    return nullptr;
  }
  
  // CRITICAL FIX: Validate ivar value address before creating ValueObject
  if (ivar.value_addr == 0 || ivar.value_addr == LLDB_INVALID_ADDRESS) {
    return nullptr;
  }
  
  // Create a value object for this ivar
  DataExtractor data;
  if (ivar.type_encoding[0] == '@') {
    // CRITICAL FIX: For object types, read the pointer value first
    Status error;
    lldb::addr_t obj_ptr = GNUstepRuntimeHelper::ReadPointer(m_process, ivar.value_addr, error);
    if (!error.Success() || obj_ptr == 0) {
      // For nil objects, create a synthetic nil value
      DataBufferSP data_buffer_sp(new DataBufferHeap(sizeof(lldb::addr_t), 0));
      DataExtractor data(data_buffer_sp, m_process->GetByteOrder(), m_process->GetAddressByteSize());
      lldb::ValueObjectSP result = ValueObject::CreateValueObjectFromData(ivar.name, data, exe_ctx, ivar_type);
      if (result) {
        m_children_cache[idx] = result;
        // printf("  Created nil object child at index %u: %p\n", idx, result.get());
      }
      return result;
    }
    
    // For non-nil objects, create from the actual object address
    // Using CreateValueObjectFromData to ensure unique instances
    DataBufferSP data_buffer_sp(new DataBufferHeap(&obj_ptr, sizeof(obj_ptr)));
    DataExtractor data(data_buffer_sp, m_process->GetByteOrder(), m_process->GetAddressByteSize());
    lldb::ValueObjectSP result = ValueObject::CreateValueObjectFromData(ivar.name, data, exe_ctx, ivar_type);
    
    // CRITICAL FIX: Cache the result to avoid recreation
    if (result) {
      m_children_cache[idx] = result;
      // printf("  Created and cached object child at index %u: %p (obj_ptr=0x%llx)\n", idx, result.get(), (unsigned long long)obj_ptr);
    }
    
    return result;
  } else if (ivar.type_encoding[0] == '^' || ivar.type_encoding[0] == '*') {
    // For non-object pointer types, read the pointer value
    Status error;
    lldb::addr_t ptr_value = GNUstepRuntimeHelper::ReadPointer(m_process, ivar.value_addr, error);
    if (error.Success()) {
      // Create value object from data for non-object pointers
      DataBufferSP data_buffer_sp(new DataBufferHeap(&ptr_value, sizeof(ptr_value)));
      DataExtractor data(data_buffer_sp, m_process->GetByteOrder(), m_process->GetAddressByteSize());
      lldb::ValueObjectSP result = ValueObject::CreateValueObjectFromData(ivar.name, data, exe_ctx, ivar_type);
      
      // CRITICAL FIX: Cache the result
      if (result) {
        m_children_cache[idx] = result;
        // printf("  Created and cached pointer child at index %u: %p\n", idx, result.get());
      }
      
      return result;
    }
  } else {
    // CRITICAL FIX: For primitive types, read the value and create from data
    // Determine the size of the primitive type
    uint32_t value_size = ivar.size;
    if (value_size == 0) {
      // Fallback to type size
      value_size = ivar_type.GetByteSize(nullptr).value_or(0);
    }
    if (value_size == 0 || value_size > 128) { // Sanity check
      return nullptr;
    }
    
    // Read the primitive value
    std::vector<uint8_t> buffer(value_size, 0);
    Status error;
    size_t bytes_read = m_process->ReadMemory(ivar.value_addr, buffer.data(), value_size, error);
    if (!error.Success() || bytes_read != value_size) {
      return nullptr;
    }
    
    // Create DataBuffer from the read data
    DataBufferSP data_buffer_sp(new DataBufferHeap(buffer.data(), buffer.size()));
    DataExtractor data(data_buffer_sp, m_process->GetByteOrder(), m_process->GetAddressByteSize());
    lldb::ValueObjectSP result = ValueObject::CreateValueObjectFromData(ivar.name, data, exe_ctx, ivar_type);
    
    // CRITICAL FIX: Cache the result
    if (result) {
      m_children_cache[idx] = result;
      // printf("  Created and cached primitive child at index %u: %p\n", idx, result.get());
    }
    
    return result;
  }
  
  return nullptr;
}

SyntheticChildrenFrontEnd *
lldb_private::formatters::GNUstepGenericObjectSyntheticFrontEndCreator(
    CXXSyntheticChildren *synth, lldb::ValueObjectSP valobj_sp) {
  return new GNUstepGenericObjectSyntheticProvider(valobj_sp);
}