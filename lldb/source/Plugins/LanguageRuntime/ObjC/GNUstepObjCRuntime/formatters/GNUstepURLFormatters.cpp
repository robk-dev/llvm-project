//===-- GNUstepURLFormatters.cpp -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepURLFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/StreamString.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/lldb-enumerations.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSURLSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream,
                                              const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    stream.Printf("nil");
    return true;
  }

  std::string url_string = ExtractURLString(valobj);
  
  // Debug: Show what we extracted
  if (url_string.empty()) {
    // Show a placeholder with address for debugging
    lldb::addr_t addr = valobj.GetPointerValue();
    stream.Printf("<NSURL: 0x%" PRIx64 ">", addr);
  } else {
    // Check if we got garbage characters
    bool has_printable = false;
    for (char c : url_string) {
      if (isprint(c) && c != ' ') {
        has_printable = true;
        break;
      }
    }
    
    if (!has_printable || url_string.length() < 3) {
      // Got garbage, show placeholder
      lldb::addr_t addr = valobj.GetPointerValue();
      stream.Printf("<NSURL: 0x%" PRIx64 " bad_string>", addr);
    } else {
      // Got valid string, show it with quotes for clarity
      stream.Printf("\"%s\"", url_string.c_str());
    }
  }
  
  return true;
}

std::string GNUstepNSURLSummaryProvider::ExtractURLString(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return "";

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return "";

  // Direct memory reading approach based on GNUstep NSURL layout
  // After analysis, found that the URL string NSString* is at offset 56 (7*8 bytes)
  // This appears consistent across GNUstep NSURL objects
  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // The NSString pointer is at offset 7*addr_size from NSURL object
  lldb::addr_t string_ptr_offset = object_addr + (7 * addr_size);
  
  Status error;
  lldb::addr_t nsstring_addr = GNUstepRuntimeHelper::ReadPointer(process, string_ptr_offset, error);
  
  if (error.Fail() || nsstring_addr == 0 || nsstring_addr == LLDB_INVALID_ADDRESS) {
    // Fallback to trying the _urlString ivar approach
    return ExtractURLStringIvar(valobj);
  }
  
  // Now we have an NSString object, extract its string content
  // The NSString has a pointer to the actual C string at offset 8 (1*addr_size)
  lldb::addr_t cstring_ptr_offset = nsstring_addr + addr_size;
  lldb::addr_t cstring_addr = GNUstepRuntimeHelper::ReadPointer(process, cstring_ptr_offset, error);
  
  if (error.Fail() || cstring_addr == 0 || cstring_addr == LLDB_INVALID_ADDRESS) {
    // Try alternative extraction
    return ExtractURLStringIvar(valobj);
  }
  
  // Read the C string
  std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, cstring_addr, 1024);
  
  if (!result.empty()) {
    return result;
  }

  // Final fallback to ivar extraction
  return ExtractURLStringIvar(valobj);
}

std::string GNUstepNSURLSummaryProvider::ExtractURLStringIvar(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return "";

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return "";

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // NSURL layout from NSURL.h:
  // @interface NSURL: NSObject <NSCoding, NSCopying, NSURLHandleClient>
  // {
  //   NSString *_urlString;
  //   NSURL    *_baseURL;
  //   void     *_clients;
  //   void     *_data;
  // }
  
  // _urlString is at offset 8 (after isa)
  lldb::addr_t url_string_addr = object_addr + addr_size;
  
  Status error;
  lldb::addr_t string_obj_addr = GNUstepRuntimeHelper::ReadPointer(process, url_string_addr, error);
  
  if (string_obj_addr == 0 || string_obj_addr == LLDB_INVALID_ADDRESS || error.Fail()) {
    return "";
  }
  

  // Use the runtime introspector to properly extract the NSString content
  GNUstepObjCRuntimeIntrospector introspector(process);
  
  // Check if it's a tagged pointer
  if (introspector.IsTaggedPointer(string_obj_addr)) {
    return introspector.DecodeTaggedString(string_obj_addr);
  }
  
  // If not a direct string, try to get the class of the string object to determine its layout
  lldb::addr_t isa_addr = 0;
  isa_addr = process->ReadPointerFromMemory(string_obj_addr, error);
  if (error.Fail() || isa_addr == 0 || isa_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Wait, we found that the URL string pointer was at 0x00005555555592bb
  // Let me try the Apple offset which is also 24 bytes but from the NSURL, not the NSString!
  // Actually no, the issue is we're reading the wrong layout
  
  // GNUstep NSConstantString memory layout inspection showed:
  // offset 0: ISA (0x00007ffff7d9f6f8)
  // offset 8: length as uint64 (0x0000002800000000 = 40 in little endian)
  // offset 16: hash (0x0000000000000028)
  // offset 24: C string pointer (0x00005555555592bb)
  
  // Let's read this as a simpler struct - we saw the string was at offset 24 in our dump
  // But wait, the issue is we're reading the length field, not the string!
  // The problem is the actual layout doesn't match what we expected.
  
  // Let me try reading directly at offset 8 as it might be a compressed format
  lldb::addr_t str_ptr_addr = string_obj_addr + (addr_size * 3);  // Skip ISA, length, hash
  lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  
  if (error.Success() && str_data_addr && str_data_addr != LLDB_INVALID_ADDRESS) {
    std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 1024);
    if (!result.empty()) {
      return result;
    }
  }
  
  // Fallback: Try reading as a regular NSString
  // GNUstep NSString may have different layouts, try common ones
  // For _NSConstantString, the string is inline after the header
  lldb::addr_t string_data_addr = string_obj_addr + (addr_size * 3);  // Skip isa, char*, length
  
  // Read up to 1024 characters (reasonable limit for URLs)
  std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, string_data_addr, 1024);
  
  return result;
}

bool lldb_private::formatters::GNUstepNSURLFormatterFunction(ValueObject &valobj, Stream &stream,
                                  const TypeSummaryOptions &options) {
  GNUstepNSURLSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}