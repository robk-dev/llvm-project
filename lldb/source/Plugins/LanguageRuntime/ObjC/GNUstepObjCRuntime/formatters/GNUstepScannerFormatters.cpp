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
  WriteQuotedString(stream, info.string.empty() ? "<null>" : info.string);
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
  
  // For regular NSString objects, apply the same logic as the working NSString formatter
  // Based on GNUstep NSConstantString structure analysis:
  // struct {
  //   Class isa;          // Object's class pointer (offset 0)
  //   uint32_t len;       // String length (offset 8)
  //   uint32_t padding;   // Padding (offset 12)
  //   uint64_t len2;      // Length again? (offset 16)
  //   const char *str;    // C string data pointer (offset 24)
  // };
  
  // Read the string data pointer from offset 24 (same as working NSString formatter)
  lldb::addr_t str_ptr_addr = string_obj_addr + 24;
  lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  if (!error.Fail() && str_data_addr != 0 && str_data_addr != LLDB_INVALID_ADDRESS) {
    // Read the string length from offset 8 (after ISA pointer)
    lldb::addr_t len_addr = string_obj_addr + 8;
    uint32_t string_length = 0;
    
    if (GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length))) {
      // Limit string length for safety
      if (string_length > 1024) {
        string_length = 1024;
      }
      
      if (string_length > 0) {
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