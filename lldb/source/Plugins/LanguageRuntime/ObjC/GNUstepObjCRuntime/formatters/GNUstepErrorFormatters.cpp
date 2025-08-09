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
  oss << "Error(";
  
  if (!domain.empty()) {
    oss << "Domain=" << domain;
  } else {
    oss << "Domain=Unknown";
  }
  
  oss << ", Code=" << code;
  
  if (!description.empty()) {
    oss << ", Description=" << description;
  }
  
  oss << ")";
  
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

  // NSError layout based on memory analysis:
  // struct NSError { void *isa; int64_t _code; NSString *_domain; NSDictionary *_userInfo; }
  // _domain is at offset 16 (isa=8 + code=8)
  lldb::addr_t domain_addr = object_addr + 16;
  
  
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
  
  // NSError layout: _code comes directly after isa pointer at offset 8
  lldb::addr_t code_addr = object_addr + addr_size;
  
  Status error;
  // Based on memory analysis, _code is actually 64-bit aligned
  int64_t code = 0;
  size_t bytes_read = process->ReadMemory(code_addr, &code, sizeof(int64_t), error);
  
  if (bytes_read != sizeof(int64_t) || error.Fail()) {
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

  // UserInfo dictionary comes after domain pointer
  // Layout: [isa][_code][_domain][_userInfo] at offset 24 (isa=8 + code=8 + domain=8)
  lldb::addr_t user_info_addr = object_addr + 24;
  
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

  // GNUstep NSConstantString structure (based on working NSString formatter):
  // struct {
  //   Class isa;          // Object's class pointer (offset 0)
  //   uint32_t len;       // String length (offset 8)
  //   uint32_t padding;   // Padding (offset 12)
  //   uint64_t len2;      // Length again? (offset 16)
  //   const char *str;    // C string data pointer (offset 24)
  // };
  
  // The string pointer appears to be at offset 24 (after isa, two length fields)
  lldb::addr_t str_ptr_addr = string_addr + 24;  // Skip isa (8) + len (4) + padding (4) + len2 (8)
  
  Status error;
  lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  if (error.Fail() || str_data_addr == 0 || str_data_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read the string length from offset 8 (after ISA pointer)
  lldb::addr_t len_addr = string_addr + 8;
  uint32_t string_length = 0;
  
  if (!GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length))) {
    // Fall back to reading as null-terminated string
    string_length = 0;
  }
  
  // Limit string length for safety
  if (string_length > 1024) {
    string_length = 1024;
  }
  
  if (string_length > 0) {
    return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, string_length);
  }
  
  // Fallback: try to read a null-terminated string (up to 256 chars for domain names)
  return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 256);
}

bool lldb_private::formatters::GNUstepNSErrorFormatterFunction(ValueObject &valobj, Stream &stream,
                                    const TypeSummaryOptions &options) {
  GNUstepNSErrorSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}