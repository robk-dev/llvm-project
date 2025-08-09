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
  
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;

  // Get the index path object pointer
  addr_t indexpath_ptr = valobj.GetPointerValue();
  if (indexpath_ptr == 0 || indexpath_ptr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }
  
  // NSIndexPath typically has these ivars:
  // - _indexes (NSUInteger*) - pointer to array of indexes
  // - _length (NSUInteger) - number of indexes
  
  // Try to find the indexes and length ivars
  ValueObjectSP indexes_sp;
  ValueObjectSP length_sp;
  
  // First try to get child members directly
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
  
  // If we couldn't find the ivars directly, try via memory layout
  // NSIndexPath layout typically has indexes at offset 8 and length at offset 16
  if (!indexes_sp || !length_sp) {
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
    if (indexes_ptr && indexes_ptr != LLDB_INVALID_ADDRESS && length > 0 && length < 100) {
      std::stringstream path_stream;
      
      for (uint64_t i = 0; i < length && i < 10; i++) { // Limit to 10 for safety
        if (i > 0)
          path_stream << ".";
          
        uint64_t index_value = 0;
        addr_t index_addr = indexes_ptr + (i * addr_size);
        process_sp->ReadMemory(index_addr, &index_value, addr_size, error);
        
        if (error.Success()) {
          path_stream << index_value;
        } else {
          path_stream << "?";
        }
      }
      
      if (length > 10) {
        path_stream << "...";
      }
      
      stream.Printf("%s", path_stream.str().c_str());
      return true;
    }
    
    // If we couldn't read the data, just show basic info
    stream.Printf("NSIndexPath");
    return true;
  }
  
  // If we found the ivars, try to extract values
  uint64_t length = 0;
  if (length_sp) {
    length = length_sp->GetValueAsUnsigned(0);
  }
  
  if (length == 0) {
    stream.Printf("(empty)");
    return true;
  }
  
  // Try to read the indexes
  if (indexes_sp && length > 0 && length < 100) {
    addr_t indexes_ptr = indexes_sp->GetValueAsUnsigned(0);
    if (indexes_ptr && indexes_ptr != LLDB_INVALID_ADDRESS) {
      std::stringstream path_stream;
      uint32_t addr_size = process_sp->GetAddressByteSize();
      Status error;
      
      for (uint64_t i = 0; i < length && i < 10; i++) { // Limit to 10 for safety
        if (i > 0)
          path_stream << ".";
          
        uint64_t index_value = 0;
        addr_t index_addr = indexes_ptr + (i * addr_size);
        process_sp->ReadMemory(index_addr, &index_value, addr_size, error);
        
        if (error.Success()) {
          path_stream << index_value;
        } else {
          path_stream << "?";
        }
      }
      
      if (length > 10) {
        path_stream << "...";
      }
      
      stream.Printf("%s", path_stream.str().c_str());
      return true;
    }
  }
  
  // Fallback
  stream.Printf("NSIndexPath[%llu]", (unsigned long long)length);
  return true;
}

bool lldb_private::formatters::GNUstepNSIndexPathFormatterFunction(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  GNUstepNSIndexPathSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}