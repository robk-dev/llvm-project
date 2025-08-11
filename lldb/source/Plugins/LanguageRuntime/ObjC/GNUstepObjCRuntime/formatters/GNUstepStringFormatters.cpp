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
  
  // Handle different GNUstep string types based on their specific memory layouts
  if (class_name == "GSCInlineString" || class_name == "GSUInlineString") {
    // Inline strings store data immediately after the object
    return ExtractInlineString(valobj);
  } else if (class_name.find("NSConstantString") != std::string::npos || 
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
    // Default NSString handling - try inline first (common in GNUstep),
    // then constant string format
    std::string content = ExtractInlineString(valobj);
    if (!content.empty()) {
      return content;
    }
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

std::string GNUstepNSStringSummaryProvider::ExtractInlineString(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // GSCInlineString/GSUInlineString structure (from GSPrivate.h):
  // struct {
  //   Class isa;              // offset 0 (8 bytes)
  //   union {                 // offset 8 (8 bytes) - _contents
  //     unsigned char *c;
  //     unichar *u;
  //   } _contents;
  //   unsigned int _count;    // offset 16 (4 bytes)
  //   struct {                 // offset 20 (4 bytes) - _flags
  //     unsigned int wide: 1;  // 0 = 8-bit chars, 1 = 16-bit chars
  //     unsigned int owned: 1;
  //     unsigned int unused: 2;
  //     unsigned int hash: 28;
  //   } _flags;
  // };
  // 
  // For inline strings, the actual character data is stored immediately
  // after the object structure in memory. The _contents pointer points
  // to this inline data.
  
  Status error;
  
  // Read the string length (_count at offset 16)
  lldb::addr_t count_addr = obj_addr + 16;
  uint32_t string_length = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, count_addr, &string_length, sizeof(string_length))) {
    return "";
  }
  
  // Sanity check the length
  if (string_length == 0 || string_length > 10000) {
    return "";
  }
  
  // Read the flags to check if it's wide (16-bit) characters
  lldb::addr_t flags_addr = obj_addr + 20;
  uint32_t flags = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, flags_addr, &flags, sizeof(flags))) {
    return "";
  }
  
  bool is_wide = (flags & 0x1) != 0;
  
  // Get the class name to determine the actual inline string class
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  // For inline strings, data starts right after the object structure
  // The size of the base structure depends on the class
  size_t object_size = 24; // Base GSString size: isa(8) + _contents(8) + _count(4) + _flags(4)
  
  // The inline data starts immediately after the object
  lldb::addr_t data_addr = obj_addr + object_size;
  
  if (is_wide || class_name == "GSUInlineString") {
    // 16-bit Unicode characters
    size_t byte_size = string_length * sizeof(uint16_t);
    if (byte_size > 10000 * sizeof(uint16_t)) {
      byte_size = 10000 * sizeof(uint16_t);
    }
    
    std::vector<uint16_t> buffer(string_length);
    if (!GNUstepRuntimeHelper::ReadMemory(process, data_addr, buffer.data(), byte_size)) {
      return "";
    }
    
    // Convert UTF-16 to UTF-8
    std::string result;
    result.reserve(string_length * 2);
    for (uint32_t i = 0; i < string_length; ++i) {
      uint16_t ch = buffer[i];
      if (ch < 0x80) {
        result.push_back(static_cast<char>(ch));
      } else if (ch < 0x800) {
        result.push_back(static_cast<char>(0xC0 | (ch >> 6)));
        result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
      } else {
        result.push_back(static_cast<char>(0xE0 | (ch >> 12)));
        result.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
      }
    }
    return result;
  } else {
    // 8-bit characters (UTF-8 or ASCII)
    return GNUstepRuntimeHelper::ReadUTF8String(process, data_addr, string_length);
  }
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
