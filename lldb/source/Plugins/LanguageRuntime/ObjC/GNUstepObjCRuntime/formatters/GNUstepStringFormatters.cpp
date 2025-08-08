//===-- GNUstepStringFormatters.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepStringFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/DataFormatters/StringPrinter.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSStringSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream, 
                                                  const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid object");
    return false;
  }
  
  std::string content = ExtractStringContent(valobj);
  if (content.empty()) {
    WriteErrorSummary(stream, "could not extract string content");
    return false;
  }
  
  WriteQuotedString(stream, content);
  return true;
}

std::string GNUstepNSStringSummaryProvider::ExtractStringContent(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Use runtime introspector to check if this is a tagged pointer
  GNUstepObjCRuntimeIntrospector introspector(process);
  if (introspector.IsTaggedPointer(obj_addr)) {
    return introspector.DecodeTaggedString(obj_addr);
  }
  
  // Get the class name to determine how to extract the string
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  // For now, try different approaches based on common GNUstep string types
  if (class_name.find("NSConstantString") != std::string::npos || 
      class_name.find("__NSConstantString") != std::string::npos) {
    return ExtractConstantString(valobj);
  } else if (class_name.find("NSMutableString") != std::string::npos) {
    return ExtractMutableString(valobj);
  } else if (class_name == "NSString") {
    // Try both constant and mutable string formats
    std::string content = ExtractConstantString(valobj);
    if (!content.empty()) {
      return content;
    }
    return ExtractMutableString(valobj);
  } else {
    // Default NSString handling - try constant string format first
    return ExtractConstantString(valobj);
  }
}

std::string GNUstepNSStringSummaryProvider::ExtractConstantString(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // GNUstep NSConstantString structure (based on memory analysis):
  // struct {
  //   Class isa;          // Object's class pointer (offset 0)
  //   uint32_t len;       // String length (offset 8)
  //   uint32_t padding;   // Padding (offset 12)
  //   uint64_t len2;      // Length again? (offset 16)
  //   const char *str;    // C string data pointer (offset 24)
  // };
  
  // The string pointer appears to be at offset 24 (after isa, two length fields)
  lldb::addr_t str_ptr_addr = obj_addr + 24;  // Skip isa (8) + len (4) + padding (4) + len2 (8)
  
  Status error;
  lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  if (error.Fail() || str_data_addr == 0 || str_data_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read the string length from offset 8 (after ISA pointer)
  lldb::addr_t len_addr = obj_addr + 8;
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
  
  // Fallback: try to read a null-terminated string
  return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 256);
}

std::string GNUstepNSStringSummaryProvider::ExtractMutableString(ValueObject &valobj) {
  // For now, use the same approach as constant strings
  // TODO: Implement proper NSMutableString extraction once we understand the layout better
  return ExtractConstantString(valobj);
}

// Function wrapper for LLDB registration
bool lldb_private::formatters::GNUstepNSStringFormatterFunction(ValueObject &valobj, Stream &stream, 
                                     const TypeSummaryOptions &options) {
  GNUstepNSStringSummaryProvider provider_instance;
  return provider_instance.FormatObject(valobj, stream, options);
}

// Smart id formatter that checks runtime type and delegates to appropriate formatter
bool lldb_private::formatters::GNUstepIdFormatterFunction(ValueObject &valobj, Stream &stream, 
                                                          const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    return false; // Let LLDB handle it with default formatting
  }
  
  // Get the actual runtime class name
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  // Delegate to the appropriate formatter based on runtime type
  if (class_name == "NSString" || class_name.find("String") != std::string::npos) {
    // Use string formatter
    GNUstepNSStringSummaryProvider string_provider;
    return string_provider.FormatObject(valobj, stream, options);
  }
  
  // For other types, return false to let LLDB handle with default formatting
  return false;
}
