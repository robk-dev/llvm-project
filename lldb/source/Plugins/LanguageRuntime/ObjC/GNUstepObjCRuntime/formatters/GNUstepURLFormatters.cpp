//===-- GNUstepURLFormatters.cpp -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepURLFormatters.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSURLSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream,
                                              const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid NSURL object");
    return false;
  }

  std::string url_string = ExtractURLString(valobj);
  if (url_string.empty()) {
    WriteErrorSummary(stream, "could not extract URL string");
    return false;
  }
  
  WriteQuotedString(stream, url_string);
  return true;
}

std::string GNUstepNSURLSummaryProvider::ExtractURLString(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return "";

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return "";

  // Try to extract from _urlString ivar first
  std::string url_string = ExtractURLStringIvar(valobj);
  if (!url_string.empty()) {
    return url_string;
  }

  // If that fails, try to call -absoluteString method
  // For now, we'll return a placeholder since dynamic method calls are complex
  return "";
}

std::string GNUstepNSURLSummaryProvider::ExtractURLStringIvar(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return "";

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return "";

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // NSURL layout (simplified for GNUstep):
  // [isa][_urlString NSString*][_baseURL NSURL*][other ivars...]
  lldb::addr_t url_string_addr = object_addr + addr_size;
  
  Status error;
  lldb::addr_t string_obj_addr = GNUstepRuntimeHelper::ReadPointer(process, url_string_addr, error);
  
  if (string_obj_addr == 0 || string_obj_addr == LLDB_INVALID_ADDRESS || error.Fail()) {
    return "";
  }

  // Create a ValueObject for the NSString and extract its content
  // This is a simplified approach - in practice we'd need proper type resolution
  
  // For now, try to read the string directly using the string reading utility
  // This assumes the NSString follows standard GNUstep layout
  
  // Skip the isa pointer and length field to get to the actual string data
  lldb::addr_t string_data_addr = string_obj_addr + (addr_size * 2);
  
  // Read up to 1024 characters (reasonable limit for URLs)
  std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, string_data_addr, 1024);
  
  return result;
}

bool lldb_private::formatters::GNUstepNSURLFormatterFunction(ValueObject &valobj, Stream &stream,
                                  const TypeSummaryOptions &options) {
  GNUstepNSURLSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}