//===-- GNUstepIndexPathFormatter.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepIndexPathFormatter.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Utility/Status.h"
#include <sstream>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSIndexPathSummaryProvider::FormatObject(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  // Use the same validation pattern as other GNUstep formatters
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    stream.Printf("invalid object");
    return false;
  }

  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;

  // Get the index path object pointer
  addr_t indexpath_ptr = valobj.GetPointerValue();
  if (indexpath_ptr == 0 || indexpath_ptr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }
  
  // Extract and format the indexes
  std::string formatted_path = FormatIndexPath(valobj);
  if (!formatted_path.empty()) {
    stream.Printf("%s", formatted_path.c_str());
  } else {
    stream.Printf("NSIndexPath");
  }
  
  return true;
}

std::string GNUstepNSIndexPathSummaryProvider::FormatIndexPath(ValueObject &valobj) {
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return "";

  addr_t indexpath_ptr = valobj.GetPointerValue();
  if (indexpath_ptr == 0 || indexpath_ptr == LLDB_INVALID_ADDRESS)
    return "";
  
  // NSIndexPath typically has these ivars:
  // - _indexes (NSUInteger*) - pointer to array of indexes
  // - _length (NSUInteger) - number of indexes
  
  // Try to find the indexes and length ivars first
  ValueObjectSP indexes_sp;
  ValueObjectSP length_sp;
  
  auto num_children_or_error = valobj.GetNumChildren();
  size_t num_children = num_children_or_error ? *num_children_or_error : 0;
  for (size_t i = 0; i < num_children; i++) {
    ValueObjectSP child_sp = valobj.GetChildAtIndex(i);
    if (!child_sp)
      continue;
      
    const char *child_name = child_sp->GetName().GetCString();
    if (child_name) {
      if (strcmp(child_name, "_indexes") == 0 || strcmp(child_name, "indexes") == 0) {
        indexes_sp = child_sp;
      } else if (strcmp(child_name, "_length") == 0 || strcmp(child_name, "length") == 0) {
        length_sp = child_sp;
      }
    }
  }
  
  // If we found both ivars via child access, use them
  if (indexes_sp && length_sp) {
    uint64_t length = length_sp->GetValueAsUnsigned(0);
    if (length == 0) {
      return "(empty)";
    }
    
    addr_t indexes_ptr = indexes_sp->GetValueAsUnsigned(0);
    if (indexes_ptr && indexes_ptr != LLDB_INVALID_ADDRESS) {
      return ReadIndexesFromMemory(indexes_ptr, length, process_sp.get());
    }
  }
  
  // Fallback: try to read via memory layout
  // NSIndexPath layout typically has indexes at offset 8 and length at offset 16
  uint32_t addr_size = process_sp->GetAddressByteSize();
  Status error;
  
  // Read indexes pointer (typically at offset 8)
  addr_t indexes_addr = indexpath_ptr + addr_size; // Skip isa
  addr_t indexes_ptr = GNUstepRuntimeHelper::ReadPointer(process_sp.get(), indexes_addr, error);
  
  // Read length (typically at offset 16)
  addr_t length_addr = indexpath_ptr + (2 * addr_size); // Skip isa and indexes pointer
  uint64_t length = 0;
  process_sp->ReadMemory(length_addr, &length, addr_size, error);
  
  // Read the indexes if we got valid values
  if (error.Success() && indexes_ptr && indexes_ptr != LLDB_INVALID_ADDRESS && length > 0 && length < 100) {
    return ReadIndexesFromMemory(indexes_ptr, length, process_sp.get());
  }
  
  return "";
}

std::string GNUstepNSIndexPathSummaryProvider::ReadIndexesFromMemory(addr_t indexes_ptr, uint64_t length, Process *process) {
  if (!process || length == 0 || length > 100)  // Sanity check
    return "";
    
  std::stringstream path_stream;
  uint32_t addr_size = process->GetAddressByteSize();
  Status error;
  
  for (uint64_t i = 0; i < length && i < 10; i++) { // Limit to 10 for safety
    if (i > 0)
      path_stream << ".";
      
    uint64_t index_value = 0;
    addr_t index_addr = indexes_ptr + (i * addr_size);
    process->ReadMemory(index_addr, &index_value, addr_size, error);
    
    if (error.Success()) {
      path_stream << index_value;
    } else {
      path_stream << "?";
    }
  }
  
  if (length > 10) {
    path_stream << "...";
  }
  
  return path_stream.str();
}

bool lldb_private::formatters::GNUstepNSIndexPathFormatterFunction(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  GNUstepNSIndexPathSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}