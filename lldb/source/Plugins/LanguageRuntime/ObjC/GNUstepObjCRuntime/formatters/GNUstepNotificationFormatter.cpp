//===-- GNUstepNotificationFormatter.cpp ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepNotificationFormatter.h"
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

bool GNUstepNSNotificationSummaryProvider::FormatObject(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;

  // Get the notification object pointer
  addr_t notification_ptr = valobj.GetPointerValue();
  if (notification_ptr == 0 || notification_ptr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }
  
  // NSNotification typically has these ivars:
  // - _name (NSString*) - notification name
  // - _object (id) - object posting the notification
  // - _userInfo (NSDictionary*) - additional information
  
  // Try to find the name and object ivars
  ValueObjectSP name_sp;
  ValueObjectSP object_sp;
  ValueObjectSP userinfo_sp;
  
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
      } else if (strcmp(child_name, "_object") == 0 || strcmp(child_name, "object") == 0) {
        object_sp = child_sp;
      } else if (strcmp(child_name, "_userInfo") == 0 || strcmp(child_name, "userInfo") == 0) {
        userinfo_sp = child_sp;
      }
    }
  }
  
  // If we couldn't find the ivars directly, try via memory layout
  // NSNotification layout typically has name at offset 8, object at offset 16
  if (!name_sp || !object_sp) {
    uint32_t addr_size = process_sp->GetAddressByteSize();
    Status error;
    
    // Read name pointer (typically at offset 8)
    addr_t name_addr = notification_ptr + addr_size; // Skip isa
    addr_t name_ptr = GNUstepRuntimeHelper::ReadPointer(process_sp.get(), name_addr, error);
    
    // Read object pointer (typically at offset 16)
    addr_t object_addr = notification_ptr + (2 * addr_size); // Skip isa and name
    addr_t object_ptr = GNUstepRuntimeHelper::ReadPointer(process_sp.get(), object_addr, error);
    
    // Try to read the name string if we got a valid pointer
    std::string name_str;
    std::string object_str;
    
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
    
    if (object_ptr && object_ptr != LLDB_INVALID_ADDRESS) {
      // Check if object is nil
      if (object_ptr == 0) {
        object_str = "nil";
      } else {
        object_str = "<object>";
      }
    }
    
    // Format the output
    if (!name_str.empty()) {
      if (!object_str.empty() && object_str != "nil") {
        stream.Printf("NSNotification: %s from %s", name_str.c_str(), object_str.c_str());
      } else {
        stream.Printf("NSNotification: %s", name_str.c_str());
      }
    } else {
      stream.Printf("NSNotification");
    }
    
    return true;
  }
  
  // If we found the ivars, try to get their summaries
  std::string name_summary;
  std::string object_summary;
  
  if (name_sp) {
    const char *summary = name_sp->GetSummaryAsCString();
    if (summary) {
      name_summary = summary;
      // Remove quotes if present
      if (name_summary.length() >= 2 && 
          name_summary.front() == '"' && name_summary.back() == '"') {
        name_summary = name_summary.substr(1, name_summary.length() - 2);
      }
    }
  }
  
  if (object_sp) {
    // Check if object is nil
    addr_t obj_ptr = object_sp->GetValueAsUnsigned(0);
    if (obj_ptr == 0) {
      object_summary = "nil";
    } else {
      const char *summary = object_sp->GetSummaryAsCString();
      if (summary) {
        object_summary = summary;
      } else {
        // Try to get the class name at least
        object_summary = object_sp->GetTypeName().GetCString();
        if (object_summary.empty()) {
          object_summary = "<object>";
        }
      }
    }
  }
  
  // Format the output
  if (!name_summary.empty()) {
    if (!object_summary.empty() && object_summary != "nil") {
      stream.Printf("NSNotification: %s from %s", name_summary.c_str(), object_summary.c_str());
    } else {
      stream.Printf("NSNotification: %s", name_summary.c_str());
    }
  } else {
    stream.Printf("NSNotification");
  }
  
  return true;
}

bool lldb_private::formatters::GNUstepNSNotificationFormatterFunction(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  GNUstepNSNotificationSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}