//===-- GNUstepErrorFormatters.cpp ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepErrorFormatters.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include <sstream>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSErrorSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream,
                                                 const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid NSError object");
    return false;
  }

  std::string domain = ExtractDomain(valobj);
  int64_t code = ExtractCode(valobj);
  std::string description = ExtractDescription(valobj);
  
  if (domain.empty() && code == 0) {
    WriteErrorSummary(stream, "could not extract error info");
    return false;
  }
  
  std::ostringstream oss;
  if (!domain.empty()) {
    oss << domain;
  } else {
    oss << "(unknown domain)";
  }
  
  oss << "(" << code << ")";
  
  if (!description.empty()) {
    oss << ": " << description;
  }
  
  WriteQuotedString(stream, oss.str());
  return true;
}

std::string GNUstepNSErrorSummaryProvider::ExtractDomain(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return "";

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return "";

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // NSError layout (simplified for GNUstep):
  // [isa][_domain NSString*][_code NSInteger][_userInfo NSDictionary*]
  lldb::addr_t domain_addr = object_addr + addr_size;
  
  Status error;
  lldb::addr_t domain_string_addr = GNUstepRuntimeHelper::ReadPointer(process, domain_addr, error);
  
  if (domain_string_addr == 0 || domain_string_addr == LLDB_INVALID_ADDRESS || error.Fail()) {
    return "";
  }

  return ExtractStringFromAddress(process, domain_string_addr);
}

int64_t GNUstepNSErrorSummaryProvider::ExtractCode(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return 0;

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return 0;

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // Code comes after domain pointer
  lldb::addr_t code_addr = object_addr + (addr_size * 2);
  
  Status error;
  int64_t code = 0;
  size_t bytes_read = process->ReadMemory(code_addr, &code, sizeof(int64_t), error);
  
  if (bytes_read != sizeof(int64_t) || error.Fail()) {
    // Try reading as 32-bit integer if 64-bit fails
    int32_t code32 = 0;
    bytes_read = process->ReadMemory(code_addr, &code32, sizeof(int32_t), error);
    if (bytes_read == sizeof(int32_t) && !error.Fail()) {
      return static_cast<int64_t>(code32);
    }
    return 0;
  }

  return code;
}

std::string GNUstepNSErrorSummaryProvider::ExtractDescription(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return "";

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return "";

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // UserInfo dictionary comes after code
  // Layout: [isa][domain][code][userInfo]
  lldb::addr_t user_info_addr = object_addr + (addr_size * 2) + sizeof(int64_t);
  
  Status error;
  lldb::addr_t user_info_dict_addr = GNUstepRuntimeHelper::ReadPointer(process, user_info_addr, error);
  
  if (user_info_dict_addr == 0 || user_info_dict_addr == LLDB_INVALID_ADDRESS || error.Fail()) {
    return "";
  }

  // For now, we don't extract from the userInfo dictionary as that would require
  // complex dictionary traversal. In a full implementation, we would look for
  // NSLocalizedDescriptionKey in the userInfo dictionary.
  return "";
}

std::string GNUstepNSErrorSummaryProvider::ExtractStringFromAddress(Process *process, lldb::addr_t string_addr) {
  if (!process || string_addr == 0 || string_addr == LLDB_INVALID_ADDRESS)
    return "";

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // Skip the isa pointer and length field to get to the actual string data
  lldb::addr_t string_data_addr = string_addr + (addr_size * 2);
  
  // Read up to 256 characters for domain names (reasonable limit)
  return GNUstepRuntimeHelper::ReadUTF8String(process, string_data_addr, 256);
}

bool GNUstepNSErrorFormatterFunction(ValueObject &valobj, Stream &stream,
                                    const TypeSummaryOptions &options) {
  GNUstepNSErrorSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}