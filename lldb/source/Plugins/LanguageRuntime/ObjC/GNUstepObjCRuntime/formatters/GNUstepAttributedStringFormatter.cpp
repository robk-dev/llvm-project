//===-- GNUstepAttributedStringFormatter.cpp -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepAttributedStringFormatter.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "GNUstepFormattersBase.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Utility/Status.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSAttributedStringSummaryProvider::FormatObject(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  // Use the same validation pattern as NSString formatter
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    stream.Printf("invalid object");
    return false;
  }

  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;

  // Get the attributed string object pointer
  addr_t attrstring_ptr = valobj.GetPointerValue();
  if (attrstring_ptr == 0 || attrstring_ptr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }
  
  // Extract the underlying string content and attribute count
  std::string string_content = ExtractStringContent(valobj);
  size_t attr_count = EstimateAttributeCount(valobj);
  
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

std::string GNUstepNSAttributedStringSummaryProvider::ExtractStringContent(ValueObject &valobj) {
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return "";

  addr_t attrstring_ptr = valobj.GetPointerValue();
  if (attrstring_ptr == 0 || attrstring_ptr == LLDB_INVALID_ADDRESS)
    return "";
  
  // NSAttributedString typically has these ivars:
  // - _string (NSString*) - the underlying string
  // - _attributes (id) - dictionary or array of attributes
  
  // Try to find the string ivar first via direct child access
  ValueObjectSP string_sp;
  
  auto num_children_or_error = valobj.GetNumChildren();
  size_t num_children = num_children_or_error ? *num_children_or_error : 0;
  for (size_t i = 0; i < num_children; i++) {
    ValueObjectSP child_sp = valobj.GetChildAtIndex(i);
    if (!child_sp)
      continue;
      
    const char *child_name = child_sp->GetName().GetCString();
    if (child_name && 
        (strcmp(child_name, "_string") == 0 || strcmp(child_name, "string") == 0)) {
      string_sp = child_sp;
      break;
    }
  }
  
  // If we found the string ivar, try to get its content
  if (string_sp) {
    // Try to get the string summary using the NSString formatter
    const char *summary = string_sp->GetSummaryAsCString();
    if (summary) {
      std::string content = summary;
      // Remove quotes if present (they'll be added back)
      if (content.length() >= 2 && 
          content.front() == '"' && content.back() == '"') {
        content = content.substr(1, content.length() - 2);
      }
      return content;
    }
    
    // Fallback: try to extract the string directly using NSString extraction logic
    addr_t string_ptr = string_sp->GetPointerValue();
    if (string_ptr && string_ptr != LLDB_INVALID_ADDRESS) {
      return ExtractStringFromPointer(string_ptr, process_sp.get());
    }
  }
  
  // Fallback: try to read the string pointer directly from memory
  // NSAttributedString layout typically has string at offset 8 (after isa)
  uint32_t addr_size = process_sp->GetAddressByteSize();
  Status error;
  
  // Read string pointer (typically at offset 8)
  addr_t string_addr = attrstring_ptr + addr_size; // Skip isa
  addr_t string_ptr = GNUstepRuntimeHelper::ReadPointer(process_sp.get(), string_addr, error);
  
  if (error.Success() && string_ptr && string_ptr != LLDB_INVALID_ADDRESS) {
    return ExtractStringFromPointer(string_ptr, process_sp.get());
  }
  
  return "";
}

std::string GNUstepNSAttributedStringSummaryProvider::ExtractStringFromPointer(addr_t string_ptr, Process *process) {
  if (!process)
    return "";
    
  // Use the same logic as NSString formatter
  GNUstepObjCRuntimeIntrospector introspector(process);
  
  // Check if it's a tagged string
  if (introspector.IsTaggedPointer(string_ptr)) {
    return introspector.DecodeTaggedString(string_ptr);
  }
  
  // For regular NSString objects, use the same memory layout as NSString formatter
  // GNUstep NSConstantString structure:
  // struct {
  //   Class isa;          // Object's class pointer (offset 0)
  //   uint32_t len;       // String length (offset 8)
  //   uint32_t padding;   // Padding (offset 12)
  //   uint64_t len2;      // Length again? (offset 16)
  //   const char *str;    // C string data pointer (offset 24)
  // };
  
  // The string pointer appears to be at offset 24 (same as NSString)
  addr_t str_ptr_addr = string_ptr + 24;
  
  Status error;
  addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  if (error.Fail() || str_data_addr == 0 || str_data_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read the actual string data
  return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 256);
}

size_t GNUstepNSAttributedStringSummaryProvider::EstimateAttributeCount(ValueObject &valobj) {
  // Try to find the attributes ivar
  auto num_children_or_error = valobj.GetNumChildren();
  size_t num_children = num_children_or_error ? *num_children_or_error : 0;
  
  for (size_t i = 0; i < num_children; i++) {
    ValueObjectSP child_sp = valobj.GetChildAtIndex(i);
    if (!child_sp)
      continue;
      
    const char *child_name = child_sp->GetName().GetCString();
    if (child_name && 
        (strcmp(child_name, "_attributes") == 0 || strcmp(child_name, "attributes") == 0)) {
      // Attributes could be a dictionary or array
      auto attr_count_or_error = child_sp->GetNumChildren();
      return attr_count_or_error ? *attr_count_or_error : 0;
    }
  }
  
  return 0; // No attributes found
}

bool lldb_private::formatters::GNUstepNSAttributedStringFormatterFunction(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  GNUstepNSAttributedStringSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}