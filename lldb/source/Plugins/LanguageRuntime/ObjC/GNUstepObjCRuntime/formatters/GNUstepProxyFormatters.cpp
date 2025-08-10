//===-- GNUstepProxyFormatters.cpp ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepProxyFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include <sstream>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSProxySummaryProvider::FormatObject(ValueObject &valobj, Stream &stream,
                                                 const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid NSProxy object");
    return false;
  }

  // Get the actual class name for type-specific formatting
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    WriteErrorSummary(stream, "could not get process");
    return false;
  }
  
  GNUstepObjCRuntimeIntrospector introspector(process);
  std::string class_name = introspector.GetClassNameFromObject(valobj);
  if (class_name.empty()) {
    WriteErrorSummary(stream, "could not determine proxy type");
    return false;
  }
  
  // Validate proxy state before attempting detailed extraction
  if (!ValidateProxyState(valobj)) {
    WriteErrorSummary(stream, "corrupted proxy object");
    return false;
  }

  return FormatProxyByType(valobj, stream, class_name);
}

bool GNUstepNSProxySummaryProvider::FormatProxyByType(ValueObject &valobj, Stream &stream, 
                                                     const std::string &class_name) {
  // Handle different proxy types with specific formatting
  
  // NSDistantObject - proxy for distributed objects
  if (class_name.find("DistantObject") != std::string::npos ||
      class_name.find("DistantProxy") != std::string::npos) {
    return FormatDistantObject(valobj, stream);
  }
  
  // NSProtocolChecker - protocol-restricting proxy
  if (class_name.find("ProtocolChecker") != std::string::npos ||
      class_name.find("ProtocolProxy") != std::string::npos) {
    return FormatProtocolChecker(valobj, stream);
  }
  
  // Abstract NSProxy base class
  if (class_name == "NSProxy" || class_name == "GSProxy") {
    WriteQuotedString(stream, "AbstractProxy(NSProxy base class - should not be instantiated)");
    return true;
  }
  
  // Custom proxy classes and generic handling
  return FormatGenericProxy(valobj, stream, class_name);
}

bool GNUstepNSProxySummaryProvider::FormatDistantObject(ValueObject &valobj, Stream &stream) {
  std::string target_info = ExtractTargetInfo(valobj);
  std::string connection_info = ExtractConnectionInfo(valobj);
  
  std::ostringstream oss;
  oss << "DistantProxy(";
  
  if (!connection_info.empty()) {
    oss << "connection=" << connection_info;
  } else {
    oss << "connection=Unknown";
  }
  
  if (!target_info.empty()) {
    oss << ", target=" << target_info;
  } else {
    oss << ", target=Remote";
  }
  
  oss << ")";
  
  WriteQuotedString(stream, oss.str());
  return true;
}

bool GNUstepNSProxySummaryProvider::FormatProtocolChecker(ValueObject &valobj, Stream &stream) {
  std::string target_info = ExtractTargetInfo(valobj);
  std::string protocol_info = ExtractProtocolInfo(valobj);
  
  std::ostringstream oss;
  oss << "ProtocolProxy(";
  
  if (!protocol_info.empty()) {
    oss << "protocol=" << protocol_info;
  } else {
    oss << "protocol=Unknown";
  }
  
  if (!target_info.empty()) {
    oss << ", target=" << target_info;
  } else {
    oss << ", target=Unknown";
  }
  
  oss << ")";
  
  WriteQuotedString(stream, oss.str());
  return true;
}

bool GNUstepNSProxySummaryProvider::FormatGenericProxy(ValueObject &valobj, Stream &stream, 
                                                      const std::string &class_name) {
  std::string target_info = ExtractTargetInfo(valobj);
  
  std::ostringstream oss;
  
  // Clean up class name for display (remove GS prefix if present)
  std::string display_class = class_name;
  if (display_class.find("GS") == 0 && display_class.length() > 2) {
    display_class = "NS" + display_class.substr(2);
  }
  
  oss << display_class << "(";
  
  if (!target_info.empty()) {
    oss << "target=" << target_info;
  } else {
    oss << "target=Unknown";
  }
  
  oss << ")";
  
  WriteQuotedString(stream, oss.str());
  return true;
}

std::string GNUstepNSProxySummaryProvider::ExtractTargetInfo(ValueObject &valobj) {
  lldb::addr_t target_addr = FindTargetObject(valobj);
  
  if (target_addr == 0 || target_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  // Try to get a description of the target object
  std::string target_desc = GetTargetDescription(process, target_addr);
  if (!target_desc.empty()) {
    return target_desc;
  }
  
  // Fall back to showing the address
  std::ostringstream oss;
  oss << "0x" << std::hex << target_addr;
  return oss.str();
}

lldb::addr_t GNUstepNSProxySummaryProvider::FindTargetObject(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return LLDB_INVALID_ADDRESS;
  }
  
  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS) {
    return LLDB_INVALID_ADDRESS;
  }
  
  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  Status error;
  
  // NSProxy has minimal structure: [isa] + subclass ivars
  // Try different offsets where target might be stored
  lldb::addr_t potential_offsets[] = {
    addr_size,      // Right after isa (most common)
    addr_size * 2,  // Second ivar
    addr_size * 3,  // Third ivar
    0
  };
  
  for (size_t offset_idx = 0; potential_offsets[offset_idx] != 0; ++offset_idx) {
    lldb::addr_t target_ptr_addr = object_addr + potential_offsets[offset_idx];
    lldb::addr_t target_addr = GNUstepRuntimeHelper::ReadPointer(process, target_ptr_addr, error);
    
    if (!error.Fail() && target_addr != 0 && target_addr != LLDB_INVALID_ADDRESS) {
      // Validate that this looks like a real object by checking if it has an isa pointer
      lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, target_addr, error);
      if (!error.Fail() && isa_addr != 0 && isa_addr != LLDB_INVALID_ADDRESS) {
        return target_addr;
      }
    }
  }
  
  return LLDB_INVALID_ADDRESS;
}

std::string GNUstepNSProxySummaryProvider::ExtractConnectionInfo(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // For NSDistantObject, connection info is typically after target
  // Layout might be: [isa][target][connection][...] 
  lldb::addr_t connection_addr = object_addr + (addr_size * 2);
  
  Status error;
  lldb::addr_t connection_ptr = GNUstepRuntimeHelper::ReadPointer(process, connection_addr, error);
  
  if (error.Fail() || connection_ptr == 0 || connection_ptr == LLDB_INVALID_ADDRESS) {
    return "Disconnected";
  }
  
  // For now, just indicate that there's a connection
  // Full connection details would require understanding NSConnection structure
  return "Active";
}

std::string GNUstepNSProxySummaryProvider::ExtractProtocolInfo(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // For NSProtocolChecker, protocol is typically stored after target
  // Layout might be: [isa][target][protocol][...]
  lldb::addr_t protocol_addr = object_addr + (addr_size * 2);
  
  Status error;
  lldb::addr_t protocol_ptr = GNUstepRuntimeHelper::ReadPointer(process, protocol_addr, error);
  
  if (error.Fail() || protocol_ptr == 0 || protocol_ptr == LLDB_INVALID_ADDRESS) {
    return "Unknown";
  }
  
  // Protocol names are stored differently in runtime, this would need more complex extraction
  // For now, indicate that there's a protocol restriction
  return "Restricted";
}

bool GNUstepNSProxySummaryProvider::ValidateProxyState(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return false;
  }
  
  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == 0 || object_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Basic validation: check that isa pointer looks valid
  Status error;
  lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, object_addr, error);
  
  if (error.Fail() || isa_addr == 0 || isa_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Additional validation: isa should point to a class object
  // Class objects have their isa pointing to metaclass
  lldb::addr_t metaclass_addr = GNUstepRuntimeHelper::ReadPointer(process, isa_addr, error);
  
  if (error.Fail() || metaclass_addr == 0 || metaclass_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  return true;
}

std::string GNUstepNSProxySummaryProvider::ExtractStringFromAddress(Process *process, lldb::addr_t string_addr) {
  if (!process || string_addr == 0 || string_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Use the same NSString extraction logic as other formatters
  // GNUstep NSConstantString structure:
  // struct { Class isa; uint32_t len; uint32_t padding; uint64_t len2; const char *str; };
  
  lldb::addr_t str_ptr_addr = string_addr + 24;  // Skip to string pointer
  
  Status error;
  lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  if (error.Fail() || str_data_addr == 0 || str_data_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read string length from offset 8
  lldb::addr_t len_addr = string_addr + 8;
  uint32_t string_length = 0;
  
  if (!GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length))) {
    string_length = 0;
  }
  
  // Limit string length for safety
  if (string_length > 512) {
    string_length = 512;
  }
  
  if (string_length > 0) {
    return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, string_length);
  }
  
  // Fallback: null-terminated string
  return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 256);
}

std::string GNUstepNSProxySummaryProvider::GetTargetDescription(Process *process, lldb::addr_t target_addr) {
  if (!process || target_addr == 0 || target_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Try to get class name of target object
  Status error;
  lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, target_addr, error);
  
  if (error.Fail() || isa_addr == 0 || isa_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // This would need full class name resolution - complex operation
  // For now, just indicate we found a target
  std::ostringstream oss;
  oss << "Object@0x" << std::hex << target_addr;
  return oss.str();
}

bool lldb_private::formatters::GNUstepNSProxyFormatterFunction(ValueObject &valobj, Stream &stream,
                                    const TypeSummaryOptions &options) {
  GNUstepNSProxySummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}