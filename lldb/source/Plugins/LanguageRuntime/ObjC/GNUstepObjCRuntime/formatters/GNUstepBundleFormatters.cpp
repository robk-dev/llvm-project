//===-- GNUstepBundleFormatters.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepBundleFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSBundleSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream, 
                                                  const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid object");
    return false;
  }
  
  BundleInfo info = ExtractBundleInfo(valobj);
  if (!info.valid) {
    WriteErrorSummary(stream, "could not extract bundle info");
    return false;
  }
  
  // Format: NSBundle(path=/usr/lib/GNUstep, version=1.29.0, loaded=true)
  stream.Printf("NSBundle(path=%s, version=%s, loaded=%s)",
                info.path.empty() ? "<null>" : info.path.c_str(),
                info.version.empty() ? "<unknown>" : info.version.c_str(),
                info.loaded ? "true" : "false");
  
  return true;
}

GNUstepNSBundleSummaryProvider::BundleInfo 
GNUstepNSBundleSummaryProvider::ExtractBundleInfo(ValueObject &valobj) {
  BundleInfo info;
  
  // Extract bundle path
  info.path = ExtractBundlePath(valobj);
  
  // Extract version information
  info.version = ExtractBundleVersion(valobj);
  
  // Extract loaded status
  info.loaded = ExtractBundleLoadedStatus(valobj);
  
  // Bundle is valid if we can extract at least the path
  info.valid = !info.path.empty() || valobj.GetPointerValue() != 0;
  
  return info;
}

std::string GNUstepNSBundleSummaryProvider::ExtractBundlePath(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // GNUstep NSBundle structure analysis needed here
  // Based on typical GNUstep object layout:
  // struct NSBundle {
  //   Class isa;           // Object's class pointer (offset 0)
  //   NSString *_path;     // Bundle path (offset 8 on 64-bit)
  //   NSString *_frameworkVersion; // Version info (offset 16)
  //   BOOL _codeLoaded;    // Load status (offset 24)
  //   // ... other instance variables
  // };
  
  // Try to read the _path instance variable at offset 8
  lldb::addr_t path_ptr_addr = obj_addr + 8;  // Skip isa pointer
  
  Status error;
  lldb::addr_t path_obj_addr = GNUstepRuntimeHelper::ReadPointer(process, path_ptr_addr, error);
  if (error.Fail() || path_obj_addr == 0 || path_obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Use runtime introspector to check if this is a tagged pointer
  GNUstepObjCRuntimeIntrospector introspector(process);
  if (introspector.IsTaggedPointer(path_obj_addr)) {
    return introspector.DecodeTaggedString(path_obj_addr);
  }
  
  // For regular NSString objects, try to extract using constant string layout
  // NSConstantString structure:
  // struct {
  //   Class isa;          // Object's class pointer (offset 0)
  //   uint32_t len;       // String length (offset 8)
  //   uint32_t padding;   // Padding (offset 12)
  //   uint64_t len2;      // Length again? (offset 16)
  //   const char *str;    // C string data pointer (offset 24)
  // };
  
  lldb::addr_t str_ptr_addr = path_obj_addr + 24;  // Skip to string pointer
  lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  if (error.Fail() || str_data_addr == 0 || str_data_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read the actual string content
  return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 512);
}

std::string GNUstepNSBundleSummaryProvider::ExtractBundleVersion(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Try to read the _frameworkVersion instance variable at offset 16
  lldb::addr_t version_ptr_addr = obj_addr + 16;  // Skip isa and _path
  
  Status error;
  lldb::addr_t version_obj_addr = GNUstepRuntimeHelper::ReadPointer(process, version_ptr_addr, error);
  if (error.Fail() || version_obj_addr == 0 || version_obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Use runtime introspector to check if this is a tagged pointer
  GNUstepObjCRuntimeIntrospector introspector(process);
  if (introspector.IsTaggedPointer(version_obj_addr)) {
    return introspector.DecodeTaggedString(version_obj_addr);
  }
  
  // For regular NSString objects, extract using constant string layout
  lldb::addr_t str_ptr_addr = version_obj_addr + 24;  // Skip to string pointer
  lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  if (error.Fail() || str_data_addr == 0 || str_data_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read the actual string content
  return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 256);
}

bool GNUstepNSBundleSummaryProvider::ExtractBundleLoadedStatus(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return false;
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Try to read the _codeLoaded instance variable at offset 24
  lldb::addr_t loaded_addr = obj_addr + 24;  // Skip isa, _path, and _frameworkVersion
  
  Status error;
  uint8_t loaded_byte = 0;
  if (process->ReadMemory(loaded_addr, &loaded_byte, sizeof(loaded_byte), error) != sizeof(loaded_byte) || error.Fail()) {
    return false;
  }
  
  return loaded_byte != 0;
}

// Function wrapper for LLDB registration
bool lldb_private::formatters::GNUstepNSBundleFormatterFunction(ValueObject &valobj, Stream &stream, 
                                                               const TypeSummaryOptions &options) {
  GNUstepNSBundleSummaryProvider provider;
  return provider.FormatObject(valobj, stream, options);
}