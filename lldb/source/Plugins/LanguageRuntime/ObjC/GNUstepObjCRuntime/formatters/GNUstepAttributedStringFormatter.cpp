//===-- GNUstepAttributedStringFormatter.cpp -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepAttributedStringFormatter.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Utility/Status.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSAttributedStringSummaryProvider::FormatObject(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;

  // Get the attributed string object pointer
  addr_t attrstring_ptr = valobj.GetPointerValue();
  if (attrstring_ptr == 0 || attrstring_ptr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }
  
  // NSAttributedString typically has these ivars:
  // - _string (NSString*) - the underlying string
  // - _attributes (id) - dictionary or array of attributes
  
  // Try to find the string ivar
  ValueObjectSP string_sp;
  ValueObjectSP attributes_sp;
  
  // First try to get child members directly
  auto num_children_or_error = valobj.GetNumChildren();
  size_t num_children = num_children_or_error ? *num_children_or_error : 0;
  for (size_t i = 0; i < num_children; i++) {
    ValueObjectSP child_sp = valobj.GetChildAtIndex(i);
    if (!child_sp)
      continue;
      
    const char *child_name = child_sp->GetName().GetCString();
    if (child_name) {
      if (strcmp(child_name, "_string") == 0 || strcmp(child_name, "string") == 0) {
        string_sp = child_sp;
      } else if (strcmp(child_name, "_attributes") == 0 || strcmp(child_name, "attributes") == 0) {
        attributes_sp = child_sp;
      }
    }
  }
  
  // If we couldn't find the ivars directly, try via memory layout
  // NSAttributedString layout typically has string at offset 8
  if (!string_sp) {
    uint32_t addr_size = process_sp->GetAddressByteSize();
    Status error;
    
    // Read string pointer (typically at offset 8)
    addr_t string_addr = attrstring_ptr + addr_size; // Skip isa
    addr_t string_ptr = GNUstepRuntimeHelper::ReadPointer(process_sp.get(), string_addr, error);
    
    // Try to read the string if we got a valid pointer
    std::string string_content;
    
    if (string_ptr && string_ptr != LLDB_INVALID_ADDRESS) {
      // For now, just indicate we have a string
      string_content = "<string>";
    }
    
    // Try to estimate attribute count (this is simplified)
    size_t attr_count = 0;
    if (attributes_sp) {
      auto attr_count_or_error = attributes_sp->GetNumChildren();
      attr_count = attr_count_or_error ? *attr_count_or_error : 0;
    }
    
    // Format the output
    if (!string_content.empty()) {
      if (attr_count > 0) {
        stream.Printf("\"%s\" (%zu attributes)", string_content.c_str(), attr_count);
      } else {
        stream.Printf("\"%s\" (no attributes)", string_content.c_str());
      }
    } else {
      stream.Printf("NSAttributedString");
    }
    
    return true;
  }
  
  // If we found the string ivar, try to get its summary
  std::string string_summary;
  
  if (string_sp) {
    const char *summary = string_sp->GetSummaryAsCString();
    if (summary) {
      string_summary = summary;
      // Remove quotes if present (they'll be added back)
      if (string_summary.length() >= 2 && 
          string_summary.front() == '"' && string_summary.back() == '"') {
        string_summary = string_summary.substr(1, string_summary.length() - 2);
      }
    }
  }
  
  // Try to count attributes
  size_t attr_count = 0;
  if (attributes_sp) {
    // Attributes could be a dictionary or array
    auto attr_count_or_error = attributes_sp->GetNumChildren();
    attr_count = attr_count_or_error ? *attr_count_or_error : 0;
  }
  
  // Format the output
  if (!string_summary.empty()) {
    if (attr_count > 0) {
      stream.Printf("\"%s\" (%zu attributes)", string_summary.c_str(), attr_count);
    } else {
      stream.Printf("\"%s\" (no attributes)", string_summary.c_str());
    }
  } else {
    stream.Printf("NSAttributedString");
  }
  
  return true;
}

bool lldb_private::formatters::GNUstepNSAttributedStringFormatterFunction(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  GNUstepNSAttributedStringSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}