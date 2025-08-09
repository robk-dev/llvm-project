//===-- GNUstepExceptionFormatter.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepExceptionFormatter.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "GNUstepFormattersBase.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Utility/Status.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSExceptionSummaryProvider::FormatObject(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;

  // Get the exception object pointer
  addr_t exception_ptr = valobj.GetPointerValue();
  if (exception_ptr == 0 || exception_ptr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }
  
  // NSException typically has these ivars:
  // - _name (NSString*)
  // - _reason (NSString*)
  // - _userInfo (NSDictionary*)
  // - _callStackReturnAddresses (NSArray*)
  // - _callStackSymbols (NSArray*)
  
  // Try to find the name and reason ivars
  ValueObjectSP name_sp;
  ValueObjectSP reason_sp;
  
  // First try to get child members directly
  auto num_children_or_error = valobj.GetNumChildren();
  size_t num_children = num_children_or_error ? *num_children_or_error : 0;
  for (size_t i = 0; i < num_children; i++) {
    ValueObjectSP child_sp = valobj.GetChildAtIndex(i);
    if (!child_sp)
      continue;
      
    const char *child_name = child_sp->GetName().GetCString();
    if (child_name) {
      if (strcmp(child_name, "_name") == 0 || strcmp(child_name, "name") == 0) {
        name_sp = child_sp;
      } else if (strcmp(child_name, "_reason") == 0 || strcmp(child_name, "reason") == 0) {
        reason_sp = child_sp;
      }
    }
  }
  
  // If we couldn't find the ivars directly, try via memory layout
  // NSException layout typically has name at offset 8 and reason at offset 16
  if (!name_sp || !reason_sp) {
    uint32_t addr_size = process_sp->GetAddressByteSize();
    Status error;
    
    // Read name pointer (typically at offset 8)
    addr_t name_addr = exception_ptr + addr_size; // Skip isa
    addr_t name_ptr = GNUstepRuntimeHelper::ReadPointer(process_sp.get(), name_addr, error);
    
    // Read reason pointer (typically at offset 16)
    addr_t reason_addr = exception_ptr + (2 * addr_size); // Skip isa and name
    addr_t reason_ptr = GNUstepRuntimeHelper::ReadPointer(process_sp.get(), reason_addr, error);
    
    // Try to read the strings if we got valid pointers
    std::string name_str;
    std::string reason_str;
    
    if (name_ptr && name_ptr != LLDB_INVALID_ADDRESS) {
      // Read the NSString content
      GNUstepObjCRuntimeIntrospector introspector(process_sp.get());
      
      // Check if it's a tagged pointer
      if (introspector.IsTaggedPointer(name_ptr)) {
        name_str = introspector.DecodeTaggedString(name_ptr);
      } else {
        // Try to read as NSConstantString
        // NSConstantString has the C string pointer at offset 24
        addr_t str_ptr_addr = name_ptr + 24;
        addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process_sp.get(), str_ptr_addr, error);
        if (error.Success() && str_data_addr && str_data_addr != LLDB_INVALID_ADDRESS) {
          name_str = GNUstepRuntimeHelper::ReadUTF8String(process_sp.get(), str_data_addr, 256);
        }
      }
      
      if (name_str.empty()) {
        name_str = "<name>";
      }
    }
    
    if (reason_ptr && reason_ptr != LLDB_INVALID_ADDRESS) {
      // Read the NSString content
      GNUstepObjCRuntimeIntrospector introspector(process_sp.get());
      
      // Check if it's a tagged pointer
      if (introspector.IsTaggedPointer(reason_ptr)) {
        reason_str = introspector.DecodeTaggedString(reason_ptr);
      } else {
        // Try to read as NSConstantString
        // NSConstantString has the C string pointer at offset 24
        addr_t str_ptr_addr = reason_ptr + 24;
        addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process_sp.get(), str_ptr_addr, error);
        if (error.Success() && str_data_addr && str_data_addr != LLDB_INVALID_ADDRESS) {
          reason_str = GNUstepRuntimeHelper::ReadUTF8String(process_sp.get(), str_data_addr, 256);
        }
      }
      
      if (reason_str.empty()) {
        reason_str = "<reason>";
      }
    }
    
    // Format the output
    if (!name_str.empty() && !reason_str.empty()) {
      stream.Printf("NSException: %s - %s", name_str.c_str(), reason_str.c_str());
    } else if (!name_str.empty()) {
      stream.Printf("NSException: %s", name_str.c_str());
    } else {
      stream.Printf("NSException");
    }
    
    return true;
  }
  
  // If we found the ivars, try to get their summaries
  std::string name_summary;
  std::string reason_summary;
  
  if (name_sp) {
    const char *summary = name_sp->GetSummaryAsCString();
    if (summary)
      name_summary = summary;
  }
  
  if (reason_sp) {
    const char *summary = reason_sp->GetSummaryAsCString();
    if (summary)
      reason_summary = summary;
  }
  
  // Format the output
  if (!name_summary.empty() && !reason_summary.empty()) {
    stream.Printf("NSException: %s - %s", name_summary.c_str(), reason_summary.c_str());
  } else if (!name_summary.empty()) {
    stream.Printf("NSException: %s", name_summary.c_str());
  } else if (!reason_summary.empty()) {
    stream.Printf("NSException: %s", reason_summary.c_str());
  } else {
    stream.Printf("NSException");
  }
  
  return true;
}

bool lldb_private::formatters::GNUstepNSExceptionFormatterFunction(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  GNUstepNSExceptionSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}