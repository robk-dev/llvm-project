//===-- GNUstepGenericFormatter.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepGenericFormatter.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include <sstream>
#include <iomanip>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

// GNUstep runtime structures based on libobjc2
struct objc_class {
  lldb::addr_t isa;           // Metaclass pointer
  lldb::addr_t super_class;   // Superclass pointer
  lldb::addr_t name;          // Class name (char*)
  long version;
  unsigned long info;
  long instance_size;
  lldb::addr_t ivars;         // struct objc_ivar_list*
  lldb::addr_t methods;       // struct objc_method_list*
  lldb::addr_t dtable;        // Dispatch table
  lldb::addr_t subclass_list; // Subclasses
  lldb::addr_t sibling_list;  // Sibling classes
  lldb::addr_t protocols;     // Protocol list
  lldb::addr_t gc_layout;     // GC layout
  lldb::addr_t ext;           // Extended info
};

struct objc_ivar {
  lldb::addr_t name;          // const char*
  lldb::addr_t type;          // const char*
  lldb::addr_t offset;        // int* (pointer to offset value)
  uint32_t size;
  uint32_t flags;
};

struct objc_ivar_list {
  uint32_t count;
  uint32_t size;              // Size of each ivar struct
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
  
  if (!process || class_addr == LLDB_INVALID_ADDRESS)
    return ivars;
  
  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  Status error;
  
  // Read the ivars pointer (7th field in objc_class)
  lldb::addr_t ivars_addr_ptr = class_addr + (addr_size * 6);
  lldb::addr_t ivars_addr = GNUstepRuntimeHelper::ReadPointer(process, ivars_addr_ptr, error);
  
  if (error.Fail() || ivars_addr == 0)
    return ivars;
  
  // Read the ivar list header
  objc_ivar_list ivar_list;
  if (!GNUstepRuntimeHelper::ReadMemory(process, ivars_addr, &ivar_list, 8))
    return ivars;
  
  // Sanity check
  if (ivar_list.count > MAX_LOOP_ITERATIONS || ivar_list.size == 0)
    return ivars;
  
  // Read each ivar
  lldb::addr_t ivar_ptr = ivars_addr + 8; // Skip header
  
  for (uint32_t i = 0; i < ivar_list.count && i < MAX_LOOP_ITERATIONS; ++i) {
    objc_ivar ivar_data;
    
    // Read the ivar structure
    if (!GNUstepRuntimeHelper::ReadMemory(process, ivar_ptr, &ivar_data, 
                                          sizeof(lldb::addr_t) * 3 + 8)) {
      break;
    }
    
    IvarInfo info;
    
    // Read ivar name
    if (ivar_data.name != 0) {
      info.name = GNUstepRuntimeHelper::ReadUTF8String(process, ivar_data.name, 256);
    }
    
    // Read ivar type encoding
    if (ivar_data.type != 0) {
      info.type_encoding = GNUstepRuntimeHelper::ReadUTF8String(process, ivar_data.type, 256);
    }
    
    // Read the offset value (it's a pointer to the offset)
    if (ivar_data.offset != 0) {
      int32_t offset_value = 0;
      if (GNUstepRuntimeHelper::ReadMemory(process, ivar_data.offset, 
                                           &offset_value, sizeof(int32_t))) {
        info.offset = offset_value;
      }
    }
    
    info.size = ivar_data.size;
    info.value_addr = obj_addr + info.offset;
    
    ivars.push_back(info);
    
    // Move to next ivar
    ivar_ptr += ivar_list.size;
  }
  
  return ivars;
}

std::vector<IvarInfo> GNUstepGenericFormatter::CollectAllIvars(Process *process,
                                                               lldb::addr_t obj_addr) {
  std::vector<IvarInfo> all_ivars;
  
  if (!process || obj_addr == LLDB_INVALID_ADDRESS)
    return all_ivars;
  
  // Get the class of this object
  lldb::addr_t class_addr = GetClassFromObject(process, obj_addr);
  if (class_addr == LLDB_INVALID_ADDRESS)
    return all_ivars;
  
  // Walk up the class hierarchy
  std::vector<lldb::addr_t> class_hierarchy;
  lldb::addr_t current_class = class_addr;
  
  // Collect classes from most derived to base
  while (current_class != LLDB_INVALID_ADDRESS && current_class != 0) {
    class_hierarchy.push_back(current_class);
    current_class = GetSuperclass(process, current_class);
    
    // Prevent infinite loops
    if (class_hierarchy.size() > MAX_FORMATTER_DEPTH)
      break;
  }
  
  // Now collect ivars from base to most derived (reverse order)
  for (auto it = class_hierarchy.rbegin(); it != class_hierarchy.rend(); ++it) {
    std::vector<IvarInfo> class_ivars = ExtractIvarsFromClass(process, *it, obj_addr);
    
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
  
  return all_ivars;
}

std::string GNUstepGenericFormatter::FormatIvar(Process *process, const IvarInfo &ivar) {
  if (!process || ivar.value_addr == LLDB_INVALID_ADDRESS)
    return "<invalid>";
  
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
  if (!process)
    return "nil";
  
  Status error;
  lldb::addr_t obj_ptr = GNUstepRuntimeHelper::ReadPointer(process, obj_addr, error);
  
  if (error.Fail() || obj_ptr == 0)
    return "nil";
  
  // Get the class of this object
  lldb::addr_t isa = GetClassFromObject(process, obj_ptr);
  if (isa == LLDB_INVALID_ADDRESS)
    return "<invalid object>";
  
  std::string class_name = GetClassName(process, isa);
  if (class_name.empty())
    return "<unknown object>";
  
  // For known Foundation classes, provide better summaries
  if (class_name.find("NSString") != std::string::npos ||
      class_name.find("GSString") != std::string::npos ||
      class_name.find("GSCString") != std::string::npos) {
    // Try to extract string content
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
  
  if (!m_process)
    return false;
    
  m_obj_addr = m_backend.GetPointerValue();
  if (m_obj_addr == 0 || m_obj_addr == LLDB_INVALID_ADDRESS)
    return false;
  
  // Create a temporary formatter to reuse its ivar collection logic
  GNUstepGenericFormatter formatter;
  std::vector<IvarInfo> all_ivars = formatter.CollectAllIvars(m_process, m_obj_addr);
  
  // Debug: Print number of ivars collected
  printf("GNUstepGenericObjectSyntheticProvider: Collected %zu ivars for object at 0x%llx\n", 
         all_ivars.size(), (unsigned long long)m_obj_addr);
  
  // Filter out the isa pointer and any other runtime-internal ivars
  for (const auto& ivar : all_ivars) {
    // Debug: Print each ivar
    printf("  Ivar: name='%s', type='%s', offset=%d\n", 
           ivar.name.c_str(), ivar.type_encoding.c_str(), ivar.offset);
    
    // Skip isa pointer - it's always the first ivar at offset 0 with type "@"
    if (ivar.name == "isa")
      continue;
      
    // Optionally skip other runtime internals (uncomment if needed)
    // if (ivar.name.starts_with("_"))
    //   continue;
      
    m_ivars.push_back(ivar);
  }
  
  printf("GNUstepGenericObjectSyntheticProvider: After filtering, %zu ivars remain\n", m_ivars.size());
  
  return true;
}

llvm::Expected<uint32_t> GNUstepGenericObjectSyntheticProvider::CalculateNumChildren() {
  if (!m_update_called)
    return 0;
  return static_cast<uint32_t>(m_ivars.size());
}

lldb::ValueObjectSP GNUstepGenericObjectSyntheticProvider::GetChildAtIndex(uint32_t idx) {
  if (!m_update_called || idx >= m_ivars.size())
    return nullptr;
    
  const IvarInfo& ivar = m_ivars[idx];
  
  // Determine the type for the ivar
  CompilerType ivar_type;
  ExecutionContext exe_ctx(m_backend.GetExecutionContextRef());
  
  // For object types, use 'id'
  if (ivar.type_encoding[0] == '@') {
    auto type_system = m_backend.GetCompilerType().GetTypeSystem();
    if (type_system) {
      ivar_type = type_system->GetBasicTypeFromAST(eBasicTypeObjCID);
    }
  } else {
    // For primitive types, try to get the appropriate type
    // This is simplified - a full implementation would parse the type encoding
    auto type_system = m_backend.GetCompilerType().GetTypeSystem();
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
  
  // Create a value object for this ivar
  DataExtractor data;
  if (ivar.type_encoding[0] == '@' || ivar.type_encoding[0] == '^' || ivar.type_encoding[0] == '*') {
    // For pointer types, read the pointer value
    Status error;
    lldb::addr_t ptr_value = GNUstepRuntimeHelper::ReadPointer(m_process, ivar.value_addr, error);
    if (error.Success()) {
      return CreateValueObjectFromAddress(ivar.name, ptr_value, ivar_type);
    }
  } else {
    // For primitive types, read the value directly
    std::vector<uint8_t> buffer(ivar.size);
    if (GNUstepRuntimeHelper::ReadMemory(m_process, ivar.value_addr, buffer.data(), ivar.size)) {
      DataBufferSP data_buffer = std::make_shared<DataBufferHeap>(buffer.data(), buffer.size());
      data.SetData(data_buffer, m_process->GetByteOrder(), m_process->GetAddressByteSize());
      return CreateValueObjectFromData(ivar.name, data, ivar_type);
    }
  }
  
  return nullptr;
}

SyntheticChildrenFrontEnd *
lldb_private::formatters::GNUstepGenericObjectSyntheticFrontEndCreator(
    CXXSyntheticChildren *synth, lldb::ValueObjectSP valobj_sp) {
  return new GNUstepGenericObjectSyntheticProvider(valobj_sp);
}