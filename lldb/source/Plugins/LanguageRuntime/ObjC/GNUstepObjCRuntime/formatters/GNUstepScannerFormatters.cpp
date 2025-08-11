//===-- GNUstepScannerFormatters.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepScannerFormatters.h"
#include "GNUstepStringFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/Utility/StreamString.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSScannerSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream, 
                                                   const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid object");
    return false;
  }
  
  ScannerInfo info = ExtractScannerInfo(valobj);
  if (!info.valid) {
    WriteErrorSummary(stream, "could not extract scanner info");
    return false;
  }
  
  // Format: NSScanner(string='Hello, World!', position=7, remaining='World!')
  stream.Printf("NSScanner(string=");
  // Check if we extracted a string vs. if extraction failed
  if (info.valid && info.string.empty()) {
    // Valid scanner with empty string
    stream.Printf("\"\"");
  } else if (!info.valid) {
    // Invalid scanner or extraction failed
    stream.Printf("<null>");
  } else {
    // Valid scanner with non-empty string
    WriteQuotedString(stream, info.string);
  }
  stream.Printf(", position=%llu, remaining=", (unsigned long long)info.scanLocation);
  WriteQuotedString(stream, info.remaining);
  stream.Printf(")");
  
  return true;
}

GNUstepNSScannerSummaryProvider::ScannerInfo 
GNUstepNSScannerSummaryProvider::ExtractScannerInfo(ValueObject &valobj) {
  ScannerInfo info;
  
  // Extract scan string
  info.string = ExtractScanString(valobj);
  
  // Extract scan location
  info.scanLocation = ExtractScanLocation(valobj);
  
  // Extract case sensitivity setting
  info.caseSensitive = ExtractCaseSensitive(valobj);
  
  // Calculate remaining text
  info.remaining = CalculateRemainingText(info.string, info.scanLocation);
  
  // Scanner is valid if we have a string or valid object pointer
  info.valid = !info.string.empty() || valobj.GetPointerValue() != 0;
  
  return info;
}

std::string GNUstepNSScannerSummaryProvider::ExtractScanString(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Based on runtime memory analysis:
  // GNUstep NSScanner memory layout (64-bit):
  // 0x00: isa pointer (8 bytes)
  // 0x08: _string pointer (8 bytes) <- String object to scan
  
  // Read the _string instance variable at offset 8
  lldb::addr_t string_ptr_addr = obj_addr + 8;
  
  Status error;
  lldb::addr_t string_obj_addr = GNUstepRuntimeHelper::ReadPointer(process, string_ptr_addr, error);
  if (error.Fail() || string_obj_addr == 0 || string_obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Use runtime introspector to check if this is a tagged pointer
  GNUstepObjCRuntimeIntrospector introspector(process);
  if (introspector.IsTaggedPointer(string_obj_addr)) {
    return introspector.DecodeTaggedString(string_obj_addr);
  }
  
  // Read the ISA pointer to determine the string type
  lldb::addr_t isa_addr = string_obj_addr;
  lldb::addr_t isa_ptr = GNUstepRuntimeHelper::ReadPointer(process, isa_addr, error);
  if (error.Fail()) {
    return "";
  }
  
  // Get class name from ISA
  ConstString class_name_const = introspector.GetClassNameFromISA(isa_ptr);
  std::string class_name = class_name_const.GetCString() ? class_name_const.GetCString() : "";
  
  // Handle GSUInlineString / GSCInlineString (common for literal strings)
  if (class_name.find("InlineString") != std::string::npos) {
    // GSUInlineString structure:
    // offset 0: isa (8 bytes)
    // offset 8: _contents pointer (8 bytes) - points to inline data
    // offset 16: _count (4 bytes) - string length
    // offset 20: _flags (4 bytes) - bit 0 = wide (UTF-16)
    // offset 24: start of inline character data
    
    // Read the string length
    lldb::addr_t count_addr = string_obj_addr + 16;
    uint32_t string_length = 0;
    if (!GNUstepRuntimeHelper::ReadMemory(process, count_addr, &string_length, sizeof(string_length))) {
      return "";
    }
    
    // Sanity check - allow zero-length strings
    if (string_length > 1024) {
      return "";
    }
    
    // Handle empty string case
    if (string_length == 0) {
      return "";  // Return empty string which is valid
    }
    
    // Read flags to check if UTF-16
    lldb::addr_t flags_addr = string_obj_addr + 20;
    uint32_t flags = 0;
    if (!GNUstepRuntimeHelper::ReadMemory(process, flags_addr, &flags, sizeof(flags))) {
      return "";
    }
    
    bool is_wide = (flags & 0x1) != 0;
    
    // Inline data starts at offset 24
    lldb::addr_t data_addr = string_obj_addr + 24;
    
    if (is_wide || class_name == "GSUInlineString") {
      // UTF-16 characters
      size_t byte_size = string_length * sizeof(uint16_t);
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
      // 8-bit ASCII/UTF-8
      return GNUstepRuntimeHelper::ReadUTF8String(process, data_addr, string_length);
    }
  }
  
  // Handle NSConstantString and other string types
  // NSConstantString structure:
  // offset 0: isa (8 bytes)
  // offset 8: length (4 bytes)
  // offset 12: padding (4 bytes)
  // offset 16: length2 (8 bytes)
  // offset 24: char* pointer to C string
  
  // Try reading as NSConstantString
  lldb::addr_t str_ptr_addr = string_obj_addr + 24;
  lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  if (!error.Fail() && str_data_addr != 0 && str_data_addr != LLDB_INVALID_ADDRESS) {
    // Read the string length from offset 8
    lldb::addr_t len_addr = string_obj_addr + 8;
    uint32_t string_length = 0;
    
    if (GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length))) {
      if (string_length > 0 && string_length <= 1024) {
        std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, string_length);
        if (!result.empty()) {
          return result;
        }
      }
    }
    
    // Fallback: try to read as null-terminated string
    std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 256);
    if (!result.empty()) {
      return result;
    }
  }
  
  return "";
}

uint64_t GNUstepNSScannerSummaryProvider::ExtractScanLocation(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return 0;
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return 0;
  }
  
  // Based on runtime memory analysis, _scanLocation is at offset 40 (0x28) from scanner base
  // GNUstep NSScanner memory layout (64-bit):
  // 0x00: isa pointer (8 bytes)
  // 0x08: _string pointer (8 bytes) 
  // 0x10: unknown field (8 bytes)
  // 0x18: unknown field (8 bytes)
  // 0x20: unknown field (8 bytes)
  // 0x28: _scanLocation (8 bytes) <- This is offset 40
  lldb::addr_t location_addr = obj_addr + 40;  // Correct offset from memory analysis
  
  Status error;
  
  // Read as 64-bit value first (NSUInteger is 64-bit on 64-bit systems)
  uint64_t location_64 = 0;
  if (process->ReadMemory(location_addr, &location_64, sizeof(uint64_t), error) == sizeof(uint64_t) && !error.Fail()) {
    return location_64;
  }
  
  // Fall back to 32-bit if needed
  uint32_t location_32 = 0;
  if (process->ReadMemory(location_addr, &location_32, sizeof(uint32_t), error) == sizeof(uint32_t) && !error.Fail()) {
    return location_32;
  }
  
  return 0;
}

bool GNUstepNSScannerSummaryProvider::ExtractCaseSensitive(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return true;  // Default to case sensitive
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return true;
  }
  
  // Try to read the _caseSensitive instance variable at offset 32
  // This is after isa (8), _string (8), _scanLocation (8), _charactersToBeSkipped (8)
  lldb::addr_t case_sensitive_addr = obj_addr + 32;
  
  Status error;
  uint8_t case_sensitive_byte = 1;  // Default to case sensitive
  if (process->ReadMemory(case_sensitive_addr, &case_sensitive_byte, sizeof(case_sensitive_byte), error) != sizeof(case_sensitive_byte) || error.Fail()) {
    return true;  // Default to case sensitive on failure
  }
  
  return case_sensitive_byte != 0;
}

std::string GNUstepNSScannerSummaryProvider::CalculateRemainingText(const std::string &fullString, uint64_t location) {
  if (fullString.empty() || location >= fullString.length()) {
    return "";  // No remaining text
  }
  
  // Return substring from current position to end
  return fullString.substr(location);
}

// Function wrapper for LLDB registration
bool lldb_private::formatters::GNUstepNSScannerFormatterFunction(ValueObject &valobj, Stream &stream, 
                                                                 const TypeSummaryOptions &options) {
  GNUstepNSScannerSummaryProvider provider;
  return provider.FormatObject(valobj, stream, options);
}