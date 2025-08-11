//===-- GNUstepDictionaryFormatters.cpp -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepDictionaryFormatters.h"
#include "GNUstepNumberFormatters.h"
#include "GNUstepArrayFormatters.h"
#include "GNUstepSetFormatters.h"
#include "GNUstepIdDispatcher.h"
#include "GNUstepPerformanceTimer.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/DataFormatters/DumpValueObjectOptions.h"
#include "lldb/Symbol/CompilerType.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Expression/UserExpression.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "lldb/lldb-enumerations.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"

#include <sstream>
#include <algorithm>

// GNUstep small object (tagged pointer) detection
// On 64-bit systems, the low 3 bits are used
#define GNUSTEP_SMALL_OBJECT_MASK 7

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

//===----------------------------------------------------------------------===//
// NSDictionary Summary Provider
//===----------------------------------------------------------------------===//

bool GNUstepNSDictionarySummaryProvider::FormatObject(ValueObject &valobj, 
                                                      Stream &stream, 
                                                      const TypeSummaryOptions &options) {
  GNUSTEP_PERFORMANCE_TIMER("NSDictionarySummary");
  
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid object");
    return false;
  }
  
  uint32_t count = ExtractDictionaryCount(valobj);
  
  // Format the summary with inline elements like Apple's formatters
  if (count == 0) {
    stream.Printf("{}");
    return true;
  }
  
  // Show inline key-value preview
  std::string inline_pairs = GetInlinePairsPreview(valobj, count);
  if (!inline_pairs.empty()) {
    stream.Printf("%s", inline_pairs.c_str());
  } else {
    // Fallback to just showing count if preview fails
    stream.Printf("{%u pairs}", count);
  }
  
  return true;
}

uint32_t GNUstepNSDictionarySummaryProvider::ExtractDictionaryCount(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    // No process available
    return 0;
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    // Invalid object address
    return 0;
  }

  // GNUstep GSDictionary structure (from GSDictionary.m):
  // @interface GSDictionary : NSDictionary
  // {
  // @public
  //   GSIMapTable_t map;
  // }
  // @end
  //
  // GSIMapTable_t structure (from GSIMap.h):
  // struct _GSIMapTable {
  //   NSZone    *zone;         // offset 0
  //   uintptr_t  nodeCount;    // offset 8
  //   uintptr_t  bucketCount;  // offset 16
  //   GSIMapBucket buckets;    // offset 24
  //   ...
  // }
  
  // Get dynamic offset for map field
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  // Class name retrieved
  if (class_name.empty() || class_name == "<unknown>") {
    class_name = "GSDictionary"; // Fallback to most common dictionary class
  }
  
  ptrdiff_t map_offset = GNUstepRuntimeHelper::GetIvarOffset(process, class_name, "map");
  // Map offset found
  if (map_offset < 0) {
    map_offset = 8; // Fallback to hardcoded offset (after isa)
    // Using fallback offset
  }
  
  // The nodeCount is at offset 8 within the GSIMapTable_t structure
  // So total offset is map_offset + 8
  lldb::addr_t count_addr = obj_addr + map_offset + 8;
  uint64_t count = 0;
  
  if (!GNUstepRuntimeHelper::ReadMemory(process, count_addr, &count, sizeof(count))) {
    return 0;
  }
  
  // Sanity check - dictionary shouldn't have millions of entries
  if (count > 10000000) {
    return 0;
  }
  
  return static_cast<uint32_t>(count);
}

std::string GNUstepNSDictionarySummaryProvider::GetInlinePairsPreview(ValueObject &valobj, uint32_t count) {
  if (count == 0) {
    return "";
  }
  
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read the map table info to get key-value pairs
  MapTableInfo map_info;
  if (!ReadMapTableInfo(process, obj_addr, map_info)) {
    return "";
  }
  
  // Extract key-value pairs for preview (limit to 5 pairs to avoid performance issues)
  std::vector<KeyValuePair> pairs;
  if (!ExtractKeyValuePairsForPreview(process, map_info, pairs, 5)) {
    return "";
  }
  
  if (pairs.empty()) {
    return "";
  }
  
  // Build inline preview showing first pairs  
  uint32_t preview_limit = std::min(static_cast<uint32_t>(pairs.size()), MAX_COLLECTION_ELEMENTS_INLINE);
  
  // PERFORMANCE OPTIMIZATION: Pre-allocate string with estimated capacity
  size_t estimated_capacity = 2 + (preview_limit * 20) + 10; // "@{" + key:value pairs + margin
  std::string result;
  result.reserve(estimated_capacity);
  result = "@{";
  
  // Create formatter context to prevent infinite recursion
  FormatterContext context;
  
  for (uint32_t i = 0; i < preview_limit; ++i) {
    if (i > 0) {
      result += ", ";
    }
    
    // Get key summary - pass storage address directly
    std::string key_summary = GetElementSummary(process, pairs[i].key_addr, context);
    if (key_summary.empty()) {
      key_summary = "<key>";
    }
    
    // Get value summary - pass storage address directly
    std::string value_summary = GetElementSummary(process, pairs[i].value_addr, context);
    if (value_summary.empty()) {
      value_summary = "<value>";
    }
    
    result += key_summary + ": " + value_summary;
  }
  
  // Add ellipsis if there are more pairs
  if (count > preview_limit) {
    result += ", ...";
  }
  
  result += "}";
  return result;
}

bool GNUstepNSDictionarySummaryProvider::ReadMapTableInfo(Process *process, 
                                                          lldb::addr_t obj_addr, 
                                                          MapTableInfo &map_info) {
  // Get dynamic offset for map field
  std::string class_name = "GSDictionary"; // Default to most common class
  ptrdiff_t map_offset = GNUstepRuntimeHelper::GetIvarOffset(process, class_name, "map");
  if (map_offset < 0) {
    map_offset = 8; // Fallback to hardcoded offset (after isa)
  }
  
  lldb::addr_t map_addr = obj_addr + map_offset;
  
  // Read nodeCount
  lldb::addr_t node_count_addr = map_addr + 8;
  uint64_t node_count = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, node_count_addr, 
                                        &node_count, sizeof(node_count))) {
    return false;
  }
  
  // Read bucketCount
  lldb::addr_t bucket_count_addr = map_addr + 16;
  uint64_t bucket_count = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, bucket_count_addr, 
                                        &bucket_count, sizeof(bucket_count))) {
    return false;
  }
  
  // Read buckets pointer
  lldb::addr_t buckets_ptr_addr = map_addr + 24;
  lldb::addr_t buckets_ptr = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, buckets_ptr_addr, 
                                        &buckets_ptr, sizeof(buckets_ptr))) {
    return false;
  }
  
  // Sanity checks
  if (node_count > 10000000 || bucket_count > 10000000 || buckets_ptr == 0) {
    return false;
  }
  
  map_info.buckets_ptr = buckets_ptr;
  map_info.bucket_count = static_cast<uint32_t>(bucket_count);
  map_info.node_count = static_cast<uint32_t>(node_count);
  
  return true;
}

bool GNUstepNSDictionarySummaryProvider::ExtractKeyValuePairsForPreview(Process *process, 
                                                                        const MapTableInfo &map_info,
                                                                        std::vector<KeyValuePair> &pairs, 
                                                                        uint32_t max_pairs) {
  pairs.clear();
  pairs.reserve(std::min(map_info.node_count, max_pairs));
  
  size_t pairs_extracted = 0;
  
  // Iterate through buckets
  for (uint32_t bucket_idx = 0; 
       bucket_idx < map_info.bucket_count && pairs_extracted < max_pairs; 
       ++bucket_idx) {
    
    // Read bucket
    lldb::addr_t bucket_addr = map_info.buckets_ptr + (bucket_idx * 16);
    
    // Read firstNode pointer (at offset 8 in bucket structure)
    lldb::addr_t first_node_ptr_addr = bucket_addr + 8;
    lldb::addr_t node_ptr = 0;
    if (!GNUstepRuntimeHelper::ReadMemory(process, first_node_ptr_addr, 
                                          &node_ptr, sizeof(node_ptr))) {
      continue;
    }
    
    // Walk the linked list of nodes in this bucket
    while (node_ptr != 0 && pairs_extracted < max_pairs) {
      // Store the ADDRESSES where the key and value pointers are stored,
      // not the pointer values themselves. This is consistent with the synthetic provider.
      
      // The key is stored at offset 8 in the node
      lldb::addr_t key_storage_addr = node_ptr + 8;
      
      // The value is stored at offset 16 in the node  
      lldb::addr_t value_storage_addr = node_ptr + 16;
      
      // Verify these addresses contain valid pointers before adding
      lldb::addr_t key_ptr = 0, value_ptr = 0;
      bool key_ok = GNUstepRuntimeHelper::ReadMemory(process, key_storage_addr, 
                                                     &key_ptr, sizeof(key_ptr));
      bool value_ok = GNUstepRuntimeHelper::ReadMemory(process, value_storage_addr, 
                                                       &value_ptr, sizeof(value_ptr));
      
      if (key_ok && value_ok && (key_ptr != 0 || value_ptr != 0)) {
        // Store storage addresses, not the pointer values
        pairs.push_back({key_storage_addr, value_storage_addr});
        pairs_extracted++;
      }
      
      // Read next node pointer (at offset 0 in node)
      lldb::addr_t next_ptr = 0;
      if (!GNUstepRuntimeHelper::ReadMemory(process, node_ptr, 
                                            &next_ptr, sizeof(next_ptr))) {
        break;
      }
      node_ptr = next_ptr;
    }
  }
  
  return true;
}

std::string GNUstepNSDictionarySummaryProvider::GetElementSummary(Process *process, lldb::addr_t storage_addr, FormatterContext &context) {
  if (!process || storage_addr == 0 || storage_addr == LLDB_INVALID_ADDRESS) {
    return "nil";
  }
  
  // Read the actual element pointer from the storage address
  Status error;
  lldb::addr_t element_addr = GNUstepRuntimeHelper::ReadPointer(process, storage_addr, error);
  if (error.Fail() || element_addr == 0 || element_addr == LLDB_INVALID_ADDRESS) {
    return "nil";
  }
  
  
  // Check for GSTinyString first (tagged pointer with tag 4)
  if ((element_addr & 0x7) == 4) {
    // This is a GSTinyString - decode it directly
    // Length is in bits 3-7 (5 bits)
    int length = (element_addr >> 3) & 0x1F;
    
    if (length > 0 && length <= 9) {
      std::string result;
      // Characters are extracted using the TINY_STRING_CHAR macro:
      // ((s & (0xFE00000000000000 >> (x*7))) >> (57-(x*7)))
      for (int i = 0; i < length; i++) {
        uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
        char c = (element_addr & mask) >> (57 - (i * 7));
        if (c >= 0x20 && c <= 0x7e) {
          result += c;
        } else if (c != 0) {
          // Non-printable character
          result += '?';
        }
      }
      
      if (!result.empty()) {
        // Return with quotes for string display
        return "\"" + result + "\"";
      }
    }
  }
  
  // Check for recursion depth limit and cycle detection
  if (context.ShouldStopRecursion(element_addr)) {
    return "<...>"; // Indicate recursion was stopped
  }
  
  // Enter this object in our recursion tracking
  context.EnterObject(element_addr);
  
  // First attempt: Try to directly extract string content for NSConstantString
  // This is more reliable than going through the ID dispatcher for simple strings
  GNUstepObjCRuntimeIntrospector introspector(process);
  Status isa_error;
  lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, element_addr, isa_error);
  std::string class_name;
  if (isa_error.Success() && isa_addr != 0) {
    class_name = introspector.GetClassName(isa_addr);
  }
  
  // CRITICAL FIX: Handle arrays and numbers directly without creating ValueObjects
  // This avoids issues with GetSummaryAsCString() on nested objects
  
  // Handle arrays directly  
  if (class_name.find("Array") != std::string::npos || class_name.find("GSInlineArray") != std::string::npos) {
    // Read array count directly using the same logic as GNUstepNSArraySummaryProvider
    ptrdiff_t count_offset = GNUstepRuntimeHelper::GetIvarOffset(process, class_name, "_count");
    if (count_offset < 0) {
      count_offset = 16; // Fallback to hardcoded offset
    }
    
    lldb::addr_t count_addr = element_addr + count_offset;
    uint32_t array_count = 0;
    if (GNUstepRuntimeHelper::ReadMemory(process, count_addr, &array_count, sizeof(array_count))) {
      if (array_count < 10000000) { // Sanity check
        context.ExitObject(element_addr);
        if (array_count == 0) {
          return "@[]";
        } else if (array_count == 1) {
          return "@[1 object]";
        } else {
          return "@[" + std::to_string(array_count) + " objects]";
        }
      }
    }
    // If we failed to read count, fall through to try other methods
  }
  
  // Handle NSNumber directly for tagged pointers
  if (class_name.find("Number") != std::string::npos) {
    // Check if it's a tagged number
    uint8_t tag = element_addr & 0x7;
    if (tag == 1) {
      // NSSmallInt - integer value is ptr >> 3
      int64_t int_value = ((int64_t)element_addr) >> 3;
      context.ExitObject(element_addr);
      return std::to_string(int_value);
    }
    // For non-tagged numbers, try to extract value
    // This is complex, so we'll fall through to the ValueObject approach
  }
  
  // Handle NSConstantString specially as it's very common in dictionaries
  // Also check for partial matches in case the class name has a prefix
  if (class_name == "NSConstantString" || class_name == "__NSConstantString" ||
      class_name.find("NSConstantString") != std::string::npos || 
      class_name.empty()) { // Try for empty class names too, in case introspection failed
    std::string string_content = TryExtractStringContent(process, element_addr);
    if (!string_content.empty()) {
      context.ExitObject(element_addr);
      // Format the string with quotes
      const size_t MAX_STRING_PREVIEW_LENGTH = 30;
      if (string_content.length() > MAX_STRING_PREVIEW_LENGTH) {
        return "\"" + string_content.substr(0, MAX_STRING_PREVIEW_LENGTH - 3) + "...\"";
      }
      return "\"" + string_content + "\"";
    }
  }
  
  // Second attempt: Try the ID dispatcher for more complex objects
  ExecutionContextScope *exe_scope = process->GetTarget().GetProcessSP().get();
  if (exe_scope) {
    TypeSystemClangSP scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(
        process->GetTarget());
    if (scratch_ts_sp) {
      CompilerType id_type = scratch_ts_sp->GetType(
          scratch_ts_sp->getASTContext().ObjCBuiltinIdTy);
      
      ExecutionContext exe_ctx;
      exe_scope->CalculateExecutionContext(exe_ctx);
      
      // For tagged pointers vs regular objects
      bool is_tagged_pointer = (element_addr & 0x7) != 0;
      ValueObjectSP element_valobj_sp;
      
      if (is_tagged_pointer) {
        // For tagged pointers, create ValueObject from data containing the tagged value
        DataBufferSP data_buffer_sp(new DataBufferHeap(&element_addr, sizeof(element_addr)));
        DataExtractor data(data_buffer_sp, process->GetByteOrder(), 
                          process->GetAddressByteSize());
        element_valobj_sp = ValueObject::CreateValueObjectFromData("element", data, exe_ctx, id_type);
      } else {
        // For regular objects, create from storage address (not the object address!)
        // CRITICAL FIX: Use storage_addr which contains the pointer, not element_addr which IS the pointer
        // This matches how arrays and dictionaries create their synthetic children
        element_valobj_sp = ValueObject::CreateValueObjectFromAddress("element", 
                                                                      storage_addr,  // Use storage address!
                                                                      exe_ctx, id_type);
      }
      
      if (element_valobj_sp) {
        // Get dynamic value for regular objects
        if (!is_tagged_pointer) {
          ValueObjectSP dynamic_valobj_sp = element_valobj_sp->GetDynamicValue(eDynamicCanRunTarget);
          if (dynamic_valobj_sp) {
            element_valobj_sp = dynamic_valobj_sp;
          }
        }
        
        // Try ID dispatcher
        StreamString dispatch_stream;
        TypeSummaryOptions dispatch_options;
        
        if (GNUstepIdDispatcherFunction(*element_valobj_sp, dispatch_stream, dispatch_options)) {
          context.ExitObject(element_addr);
          std::string result = dispatch_stream.GetString().str();
          
          // ENHANCED FIX: The ID dispatcher should handle all formatting,
          // but we need to ensure proper display for dictionary values.
          // For collections in dictionaries, we want concise representations.
          
          // Check result type patterns to determine formatting
          bool is_already_quoted = (result.find("@\"") == 0 || (result.length() > 0 && result[0] == '"'));
          bool is_numeric = false;
          bool is_boolean = (result == "YES" || result == "NO" || result == "true" || result == "false");
          bool is_collection = (result.find("@[") == 0 || result.find("@{") == 0 || result.find("{") == 0);
          bool is_object_ref = (result.find("<") == 0);
          bool is_nil = (result == "nil" || result == "(null)");
          
          // Check if it's a pure number (handles integers, floats, scientific notation)
          if (!is_boolean && !is_collection && !is_object_ref && !is_nil && !is_already_quoted && !result.empty()) {
            // Try parsing as number - if entire string parses, it's numeric
            char *endptr;
            strtod(result.c_str(), &endptr);
            is_numeric = (*endptr == '\0' && result[0] != '\0');
            
            // Additional check for negative numbers and scientific notation
            if (!is_numeric && (result[0] == '-' || result.find('e') != std::string::npos || result.find('E') != std::string::npos)) {
              is_numeric = (*endptr == '\0' && result[0] != '\0');
            }
          }
          
          // ENHANCED FIX: Truncate very long collections for dictionary values
          if (is_collection && result.length() > 100) {
            // For arrays: @[item1, item2, ...]
            if (result.find("@[") == 0) {
              size_t comma_pos = result.find(",");
              if (comma_pos != std::string::npos) {
                size_t second_comma = result.find(",", comma_pos + 1);
                if (second_comma != std::string::npos) {
                  result = result.substr(0, second_comma) + ", ...]";
                }
              }
            }
            // For dictionaries: @{key1: value1, ...}
            else if (result.find("@{") == 0) {
              size_t comma_pos = result.find(",");
              if (comma_pos != std::string::npos) {
                result = result.substr(0, comma_pos) + ", ...}";
              }
            }
          }
          
          // Apply quoting logic:
          // - Numbers, booleans, collections, object refs, nil: return as-is
          // - Already quoted strings: return as-is  
          // - Plain strings: add quotes
          if (is_numeric || is_boolean || is_collection || is_object_ref || is_nil || is_already_quoted) {
            return result;
          } else {
            // This is a plain string that needs quoting
            const size_t MAX_STRING_PREVIEW_LENGTH = 30; // Increased for dictionary values
            if (result.length() > MAX_STRING_PREVIEW_LENGTH) {
              result = result.substr(0, MAX_STRING_PREVIEW_LENGTH - 3) + "...";
            }
            return "\"" + result + "\"";
          }
        }
      }
    }
  }
  
  // FALLBACK: Additional string extraction attempts for other string types
  // Try custom string extraction for non-NSConstantString types
  if (class_name.find("String") != std::string::npos) {
    std::string string_content = TryExtractStringContent(process, element_addr);
    if (!string_content.empty()) {
      context.ExitObject(element_addr);
      // Return quoted string, truncated for inline display
      const size_t MAX_STRING_PREVIEW_LENGTH = 30;
      if (string_content.length() > MAX_STRING_PREVIEW_LENGTH) {
        return "\"" + string_content.substr(0, MAX_STRING_PREVIEW_LENGTH - 3) + "...\"";
      }
      return "\"" + string_content + "\"";
    }
  }
  
  // Check if it's a tagged pointer that couldn't be decoded as string
  if (introspector.IsTaggedPointer(element_addr)) {
    // For non-string tagged pointers, try to get proper formatting
    uint64_t tag = element_addr & 7;
    
    // For tagged numbers, decode directly using the same logic as GNUstepNumberFormatters
    if (tag == 1 || tag == 2 || tag == 3 || tag == 5) {
      const int SMALL_OBJECT_SHIFT = 3;
      
      if (tag == 1) {
        // NSSmallInt - integer value is ptr >> 3
        int64_t int_value = ((int64_t)element_addr) >> SMALL_OBJECT_SHIFT;
        context.ExitObject(element_addr);
        return std::to_string(int_value);
      } else if (tag == 5) {
        // NSSmallFloat
        union {
          uint64_t bits;
          double d;
        } converter;
        converter.bits = element_addr & ~0x7ULL;  // Clear tag bits
        float float_value = (float)converter.d;
        
        // Format float with minimal precision for inline display
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.6g", float_value);
        context.ExitObject(element_addr);
        return std::string(buffer);
      } else if (tag == 2) {
        // NSSmallExtendedDouble
        uint64_t mask = element_addr & 8;
        union {
          uint64_t bits;
          double d;
        } converter;
        converter.bits = (element_addr & ~7) | (mask >> 1) | (mask >> 2) | (mask >> 3);
        
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.6g", converter.d);
        context.ExitObject(element_addr);
        return std::string(buffer);
      } else if (tag == 3) {
        // NSSmallRepeatingDouble
        uint64_t mask = element_addr & 56;
        union {
          uint64_t bits;
          double d;
        } converter;
        converter.bits = (element_addr & ~7) | (mask >> 3);
        
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.6g", converter.d);
        context.ExitObject(element_addr);
        return std::string(buffer);
      }
      
      // Unknown tagged number type
      context.ExitObject(element_addr);
      return "<NSNumber>";
    }
    
    context.ExitObject(element_addr);
    // For other tagged pointers
    switch (tag) {
      case 4: return "<NSString>"; // Fallback if decoding failed
      default: return "<tagged>";
    }
  }
  
  // For regular objects (non-tagged), check if it's an NSNumber and handle it specially
  if (!class_name.empty()) {
    // We already have the class name from above
    
    // Check if this is an NSNumber class
    if (class_name.find("NSNumber") != std::string::npos ||
        class_name.find("Number") != std::string::npos) {
      
      // Create a ValueObject for the NSNumber and use the NSNumber formatter
      ExecutionContextScope *exe_scope = process->GetTarget().GetProcessSP().get();
      if (exe_scope) {
        auto scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(
            process->GetTarget());
        if (scratch_ts_sp) {
          CompilerType id_type = scratch_ts_sp->GetType(
              scratch_ts_sp->getASTContext().ObjCBuiltinIdTy);
          
          // CRITICAL FIX: Create ValueObject from ADDRESS, not from data containing pointer
          // This allows LLDB to properly resolve the object and apply formatters
          ExecutionContext exe_ctx;
          exe_scope->CalculateExecutionContext(exe_ctx);
          // Use same logic as above for tagged vs regular objects
          bool is_tagged = (element_addr & 0x7) != 0;
          ValueObjectSP valobj_sp;
          
          if (is_tagged) {
            // For tagged pointers, create ValueObject from data containing the tagged value
            DataBufferSP data_buffer_sp(new DataBufferHeap(&element_addr, sizeof(element_addr)));
            DataExtractor data(data_buffer_sp, process->GetByteOrder(), 
                              process->GetAddressByteSize());
            valobj_sp = ValueObject::CreateValueObjectFromData("element", data, exe_ctx, id_type);
          } else {
            // For regular objects, create from address
            valobj_sp = ValueObject::CreateValueObjectFromAddress("element", element_addr, exe_ctx, id_type);
          }
          
          if (valobj_sp) {
            // Use the GNUstep NSNumber formatter directly
            GNUstepNSNumberSummaryProvider number_formatter;
            StreamString number_stream;
            TypeSummaryOptions number_options;
            if (number_formatter.FormatObject(*valobj_sp, number_stream, number_options)) {
              context.ExitObject(element_addr);
              return number_stream.GetString().str();
            }
          }
        }
      }
      
      // Fallback for NSNumber if formatter fails
      context.ExitObject(element_addr);
      return "<NSNumber>";
    }
  }
  
  // For other regular objects, create a ValueObject
  // and use LLDB's formatting system to get the proper summary
  ExecutionContextScope *exe_scope2 = process->GetTarget().GetProcessSP().get();
  if (exe_scope2) {
    // Get the ObjC id type from the scratch TypeSystem
    TypeSystemClangSP scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(
        process->GetTarget());
    if (scratch_ts_sp) {
      CompilerType id_type = scratch_ts_sp->GetType(
          scratch_ts_sp->getASTContext().ObjCBuiltinIdTy);
      
      // CRITICAL FIX: Create ValueObject from ADDRESS, not from data containing pointer
      // This allows LLDB to properly resolve nested objects and apply formatters recursively
      ExecutionContext exe_ctx;
      exe_scope->CalculateExecutionContext(exe_ctx);
      // Use same logic as above for tagged vs regular objects
      bool is_tagged = (element_addr & 0x7) != 0;
      ValueObjectSP valobj_sp;
      
      if (is_tagged) {
        // For tagged pointers, create ValueObject from data containing the tagged value
        DataBufferSP data_buffer_sp(new DataBufferHeap(&element_addr, sizeof(element_addr)));
        DataExtractor data(data_buffer_sp, process->GetByteOrder(), 
                          process->GetAddressByteSize());
        valobj_sp = ValueObject::CreateValueObjectFromData("element", data, exe_ctx, id_type);
      } else {
        // For regular objects, create from address
        valobj_sp = ValueObject::CreateValueObjectFromAddress("element", element_addr, exe_ctx, id_type);
      }
      
      if (valobj_sp) {
        // CRITICAL FIX: Manually apply GNUstep formatters since LLDB may not
        // automatically select them for nested objects created programmatically
        
        // Try to get the class name to determine which formatter to use
        std::string class_name = introspector.GetClassName(isa_addr);
        
        // CRITICAL FIX: For nested collections, prefer the ID dispatcher over direct formatter calls
        // The ID dispatcher will properly route to the correct formatter and handle all edge cases
        // This provides consistent behavior and proper nested formatting
        
        // STEP 1: Try ID dispatcher for nested collections - this is the most robust approach
        StreamString nested_dispatch_stream;
        TypeSummaryOptions nested_dispatch_options;
        if (GNUstepIdDispatcherFunction(*valobj_sp, nested_dispatch_stream, nested_dispatch_options)) {
          context.ExitObject(element_addr);
          std::string nested_result = nested_dispatch_stream.GetString().str();
          
          // For nested collections, we want to show a reasonable amount of detail but avoid clutter
          // Limit very long collection displays
          if (nested_result.length() > 50 && 
              (nested_result.find("@[") == 0 || nested_result.find("@{") == 0)) {
            // For long collections, show truncated version
            size_t comma_count = 0;
            size_t pos = 0;
            const size_t MAX_NESTED_ITEMS = 3;
            
            // Count items shown (comma separated)
            while ((pos = nested_result.find(",", pos)) != std::string::npos && comma_count < MAX_NESTED_ITEMS) {
              comma_count++;
              pos++;
            }
            
            if (comma_count >= MAX_NESTED_ITEMS) {
              // Truncate after MAX_NESTED_ITEMS
              pos = 0;
              for (size_t i = 0; i < MAX_NESTED_ITEMS && pos != std::string::npos; i++) {
                pos = nested_result.find(",", pos + 1);
              }
              if (pos != std::string::npos) {
                nested_result = nested_result.substr(0, pos) + ", ..." + 
                               nested_result.substr(nested_result.length() - 1); // Keep closing brace
              }
            }
          }
          
          return nested_result;
        }
        
        // STEP 2: Fallback to direct formatter calls if ID dispatcher fails
        if (class_name.find("Dictionary") != std::string::npos ||
            class_name.find("NSDictionary") != std::string::npos) {
          // Dictionary fallback - show count with pairs
          lldb::addr_t count_addr = element_addr + 16;
          uint64_t count64 = 0;
          if (GNUstepRuntimeHelper::ReadMemory(process, count_addr, &count64, sizeof(count64))) {
            uint32_t nested_count = static_cast<uint32_t>(count64);
            if (nested_count > 0 && nested_count < 1000000) {
              context.ExitObject(element_addr);
              return nested_count == 1 ? "@{1 pair}" : "@{" + std::to_string(nested_count) + " pairs}";
            }
          }
          context.ExitObject(element_addr);
          return "@{...}";
        } else if (class_name.find("Array") != std::string::npos ||
                   class_name.find("NSArray") != std::string::npos) {
          // Array fallback - show count with objects
          lldb::addr_t count_addr = element_addr + 16;
          uint32_t nested_count = 0;
          if (GNUstepRuntimeHelper::ReadMemory(process, count_addr, &nested_count, sizeof(nested_count))) {
            if (nested_count > 0 && nested_count < 1000000) {
              context.ExitObject(element_addr);
              return nested_count == 1 ? "@[1 object]" : "@[" + std::to_string(nested_count) + " objects]";
            }
          }
          context.ExitObject(element_addr);
          return "@[...]";
        } else if (class_name.find("Set") != std::string::npos ||
                   class_name.find("NSSet") != std::string::npos) {
          // Set fallback - show generic set indicator
          context.ExitObject(element_addr);
          return "{set}";
        }
        
        // CRITICAL FIX: Try to get dynamic value to ensure proper type resolution
        ValueObjectSP dynamic_valobj_sp = valobj_sp->GetDynamicValue(eDynamicCanRunTarget);
        if (dynamic_valobj_sp) {
          valobj_sp = dynamic_valobj_sp;
        }
        
        // Fallback to LLDB's automatic summary
        const char *summary = valobj_sp->GetSummaryAsCString();
        if (summary && strlen(summary) > 0) {
          context.ExitObject(element_addr);
          return summary;
        }
        
        // ENHANCED FIX: If LLDB's automatic summary failed, try our ID dispatcher directly
        // This handles cases where LLDB's type system doesn't properly invoke our formatters
        StreamString dispatch_stream;
        TypeSummaryOptions dispatch_options;
        if (GNUstepIdDispatcherFunction(*valobj_sp, dispatch_stream, dispatch_options)) {
          context.ExitObject(element_addr);
          std::string result = dispatch_stream.GetString().str();
          
          // CRITICAL FIX: Apply the same improved logic as above for value type detection
          bool is_already_quoted = (result.find("@\"") == 0 || (result.length() > 0 && result[0] == '"'));
          bool is_numeric = false;
          bool is_boolean = (result == "YES" || result == "NO" || result == "true" || result == "false");
          bool is_collection = (result.find("@[") == 0 || result.find("@{") == 0 || result.find("{") == 0);
          bool is_object_ref = (result.find("<") == 0);
          bool is_nil = (result == "nil" || result == "(null)");
          
          if (!is_boolean && !is_collection && !is_object_ref && !is_nil && !is_already_quoted && !result.empty()) {
            char *endptr;
            strtod(result.c_str(), &endptr);
            is_numeric = (*endptr == '\0' && result[0] != '\0');
            
            if (!is_numeric && (result[0] == '-' || result.find('e') != std::string::npos || result.find('E') != std::string::npos)) {
              is_numeric = (*endptr == '\0' && result[0] != '\0');
            }
          }
          
          if (is_numeric || is_boolean || is_collection || is_object_ref || is_nil || is_already_quoted) {
            return result;
          } else {
            return "\"" + result + "\"";
          }
        }
      }
    }
  }
  
  // Exit object tracking
  context.ExitObject(element_addr);
  
  // For non-string objects where we couldn't get a better summary
  return "";
}

bool GNUstepNSDictionarySummaryProvider::IsGNUstepTaggedPointer(lldb::addr_t addr) {
  return (addr & 7) != 0;
}

std::string GNUstepNSDictionarySummaryProvider::GetTaggedPointerSummary(lldb::addr_t addr) {
  // This method should not be used anymore - tagged pointer decoding
  // is handled properly in GetElementSummary() using the introspector
  return "<tagged>";
}

std::string GNUstepNSDictionarySummaryProvider::TryExtractStringContent(Process *process, lldb::addr_t obj_addr) {
  if (!process || obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // First check if this is a tagged pointer
  GNUstepObjCRuntimeIntrospector introspector(process);
  if (introspector.IsTaggedPointer(obj_addr)) {
    // For GNUstep tagged strings, we need special handling
    // These are compile-time constant strings that are encoded specially
    
    uint64_t tag = obj_addr & 0x7;
    if (tag == 4) {
      // This is a tagged string - decode it properly
      std::string decoded = introspector.DecodeTaggedString(obj_addr);
      if (!decoded.empty()) {
        return decoded;
      }
      // Fallback if decoding fails
      return "<tagged_string>";
    }
    
    // Check if it's a tagged number - return empty to let proper number handling take over
    if (tag == 2 || tag == 3 || tag == 5 || tag == 1) {
      // Don't handle tagged numbers here - let the proper number formatter handle them
      // This function is specifically for string content extraction
      return "";
    }
    
    // Not a string tagged pointer
    return "";
  }
  
  // For regular NSString objects, we need to properly extract the content
  // by using the same logic as the standalone string formatter
  
  // First, read the ISA pointer to determine the exact string type
  Status error;
  lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, obj_addr, error);
  if (error.Fail() || isa_addr == 0) {
    return "";
  }
  
  // Get the class name from the ISA
  std::string class_name = introspector.GetClassName(isa_addr);
  
  // Handle different string types based on their class
  if (class_name.find("NSConstantString") != std::string::npos || 
      class_name.find("__NSConstantString") != std::string::npos) {
    // NSConstantString layout (compile-time constant strings that aren't tagged):
    // NEW_ABI (GNUstep 2.1+):
    // struct {
    //   Class isa;          // offset 0
    //   uint32_t flags;     // offset 8
    //   uint32_t length;    // offset 12 <-- String length
    //   uint32_t size;      // offset 16
    //   uint32_t hash;      // offset 20
    //   const char *str;    // offset 24 <-- String pointer
    // };
    
    // Read the string pointer at correct offset 24
    lldb::addr_t str_ptr_addr = obj_addr + 24;
    lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
    if (error.Fail() || str_data_addr == 0) {
      return "";
    }
    
    // Read the length at correct offset 12
    lldb::addr_t len_addr = obj_addr + 12;
    uint32_t string_length = 0;
    if (!GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length))) {
      string_length = 0;
    }
    
    // Limit length for inline display
    const size_t MAX_STRING_PREVIEW_LENGTH = 20;
    if (string_length > MAX_STRING_PREVIEW_LENGTH) {
      string_length = MAX_STRING_PREVIEW_LENGTH;
    }
    
    if (string_length > 0) {
      return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, string_length);
    }
    
    // Fallback
    return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, MAX_STRING_PREVIEW_LENGTH);
  }
  
  // For other string types (GSPlaceholderString, GSCString, GSString, etc.)
  // Try multiple common layouts used by GNUstep string classes
  
  lldb::addr_t str_data_addr = 0;
  uint32_t string_length = 0;
  
  // First try reading length at offset 8 (common for most string types)
  lldb::addr_t len_addr = obj_addr + 8;
  if (!GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length))) {
    // Some string types might not have length at offset 8
    string_length = 0;
  }
  
  // GSString and similar: try multiple possible offsets for the string data
  // Different string implementations use different layouts
  const size_t possible_offsets[] = {24, 16, 20, 32, 12}; // Common offsets
  
  for (size_t offset : possible_offsets) {
    lldb::addr_t str_ptr_addr = obj_addr + offset;
    str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
    
    if (!error.Fail() && str_data_addr != 0 && str_data_addr != LLDB_INVALID_ADDRESS) {
      // Verify this looks like a valid string pointer
      char test_char;
      if (GNUstepRuntimeHelper::ReadMemory(process, str_data_addr, &test_char, 1)) {
        // Check if it's a printable character or null terminator
        if ((test_char >= 0x20 && test_char <= 0x7e) || test_char == 0) {
          break; // Found valid string data
        }
      }
      str_data_addr = 0; // Reset if not valid
    }
  }
  
  if (str_data_addr == 0) {
    // Last resort: check if the string data is embedded directly after the object header
    // Some string types store short strings inline
    lldb::addr_t inline_addr = obj_addr + 16;
    char test_char;
    if (GNUstepRuntimeHelper::ReadMemory(process, inline_addr, &test_char, 1)) {
      if ((test_char >= 0x20 && test_char <= 0x7e) || test_char == 0) {
        str_data_addr = inline_addr;
      }
    }
  }
  
  if (str_data_addr == 0) {
    return "";
  }
  
  // Limit length for inline display
  const size_t MAX_STRING_PREVIEW_LENGTH = 20;
  if (string_length > MAX_STRING_PREVIEW_LENGTH) {
    string_length = MAX_STRING_PREVIEW_LENGTH;
  }
  
  if (string_length > 0) {
    return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, string_length);
  }
  
  // Fallback
  return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, MAX_STRING_PREVIEW_LENGTH);
}

std::string GNUstepNSDictionarySummaryProvider::TryExtractCollectionSummary(Process *process, lldb::addr_t obj_addr) {
  // This would try to detect if the object is an NSArray, NSDictionary, NSSet, etc.
  // and return a brief summary like "[3 objects]" or "{2 pairs}"
  
  // For now, we'll implement a simple version that checks common patterns
  // A full implementation would need proper class introspection
  
  // Try to read ISA and check for known collection classes
  lldb::addr_t isa_addr = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, obj_addr, &isa_addr, sizeof(isa_addr))) {
    return "";
  }
  
  // This is a simplified approach - in practice we'd need better class detection
  // For now, return empty to fall back to generic object display
  return "";
}

//===----------------------------------------------------------------------===//
// NSDictionary Synthetic Children Provider
//===----------------------------------------------------------------------===//

GNUstepNSDictionarySyntheticProvider::GNUstepNSDictionarySyntheticProvider(
    lldb::ValueObjectSP valobj_sp)
    : GNUstepSyntheticProvider(valobj_sp),
      m_map_info{LLDB_INVALID_ADDRESS, 0, 0},
      m_is_mutable(false),
      m_exe_ctx_ref(),
      m_id_type() {
  // Initialize the ObjC id type for child elements
  if (valobj_sp) {
    TypeSystemClangSP scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(
        *valobj_sp->GetExecutionContextRef().GetTargetSP());
    if (scratch_ts_sp) {
      m_id_type = scratch_ts_sp->GetType(
          scratch_ts_sp->getASTContext().ObjCBuiltinIdTy);
    }
  }
}

bool GNUstepNSDictionarySyntheticProvider::UpdateImpl() {
  
  // Update execution context reference
  m_exe_ctx_ref = m_backend.GetExecutionContextRef();
  
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(m_backend);
  if (!process) {
    // Process validation failed
    return false;
  }
  
  // CRITICAL: Store the process reference for use in GetChildAtIndex
  m_process = process;
  
  lldb::addr_t obj_addr = m_backend.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Check if this is a mutable dictionary
  m_is_mutable = false;
  ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process);
  if (runtime) {
    ObjCLanguageRuntime::ClassDescriptorSP class_descriptor_sp =
        runtime->GetClassDescriptor(m_backend);
    if (class_descriptor_sp) {
      const char *class_name = class_descriptor_sp->GetClassName().AsCString("");
      m_is_mutable = (class_name && strstr(class_name, "Mutable"));
    }
  }
  
  // Read the map table info
  if (!ReadMapTableInfo(process, obj_addr)) {
    return false;
  }
  
  // Extract key-value pairs
  if (!ExtractKeyValuePairs(process)) {
    return false;
  }
  
  return true;
}

bool GNUstepNSDictionarySyntheticProvider::ReadMapTableInfo(Process *process, 
                                                            lldb::addr_t obj_addr) {
  // Get dynamic offset for map field like we do in the summary provider
  // Get class name from the object itself
  std::string class_name;
  GNUstepObjCRuntimeIntrospector introspector(process);
  Status isa_error;
  lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, obj_addr, isa_error);
  if (isa_error.Success() && isa_addr != 0) {
    class_name = introspector.GetClassName(isa_addr);
  }
  
  if (class_name.empty() || class_name == "<unknown>") {
    class_name = "GSDictionary"; // Fallback to most common dictionary class
  }
  
  ptrdiff_t map_offset = GNUstepRuntimeHelper::GetIvarOffset(process, class_name, "map");
  if (map_offset < 0) {
    map_offset = 8; // Fallback to hardcoded offset (after isa)
  }
  
  lldb::addr_t map_addr = obj_addr + map_offset;
  
  // Read GSIMapTable fields:
  // struct _GSIMapTable {
  //   NSZone    *zone;         // offset 0
  //   uintptr_t  nodeCount;    // offset 8
  //   uintptr_t  bucketCount;  // offset 16
  //   GSIMapBucket buckets;    // offset 24 (pointer to bucket array)
  //   ...
  // }
  
  // Read nodeCount
  lldb::addr_t node_count_addr = map_addr + 8;
  uint64_t node_count = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, node_count_addr, 
                                        &node_count, sizeof(node_count))) {
    return false;
  }
  
  // Read bucketCount
  lldb::addr_t bucket_count_addr = map_addr + 16;
  uint64_t bucket_count = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, bucket_count_addr, 
                                        &bucket_count, sizeof(bucket_count))) {
    return false;
  }
  
  // Read buckets pointer (GSIMapBucket is already a pointer type)
  lldb::addr_t buckets_ptr_addr = map_addr + 24;
  lldb::addr_t buckets_ptr = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, buckets_ptr_addr, 
                                        &buckets_ptr, sizeof(buckets_ptr))) {
    return false;
  }
  
  // Sanity checks
  if (node_count > 10000000 || bucket_count > 10000000 || buckets_ptr == 0) {
    return false;
  }
  
  m_map_info.buckets_ptr = buckets_ptr;
  m_map_info.bucket_count = static_cast<uint32_t>(bucket_count);
  m_map_info.node_count = static_cast<uint32_t>(node_count);
  
  return true;
}

bool GNUstepNSDictionarySyntheticProvider::ExtractKeyValuePairs(Process *process) {
  m_pairs.clear();
  m_pairs.reserve(std::min(m_map_info.node_count, 100u)); // Limit to first 100 for performance
  
  // GSIMapBucket structure:
  // struct _GSIMapBucket {
  //   uintptr_t  nodeCount;    // offset 0
  //   GSIMapNode firstNode;    // offset 8 (pointer to first node)
  // }
  
  // GSIMapNode structure:
  // struct _GSIMapNode {
  //   GSIMapNode nextInBucket; // offset 0 (pointer to next node)
  //   GSIMapKey  key;          // offset 8 (union containing id)
  //   GSIMapVal  value;        // offset 16 (union containing id)
  // }
  
  size_t pairs_extracted = 0;
  const size_t max_pairs = std::min(static_cast<size_t>(m_map_info.node_count), size_t(100));
  
  // Iterate through buckets
  for (uint32_t bucket_idx = 0; 
       bucket_idx < m_map_info.bucket_count && pairs_extracted < max_pairs; 
       ++bucket_idx) {
    
    // Read bucket at index (GSIMapBucket structure is {nodeCount, firstNode})
    lldb::addr_t bucket_addr = m_map_info.buckets_ptr + (bucket_idx * 16); // sizeof(GSIMapBucket_t)
    
    // Read firstNode pointer (at offset 8 in bucket structure)
    lldb::addr_t first_node_ptr_addr = bucket_addr + 8;
    lldb::addr_t node_ptr = 0;
    if (!GNUstepRuntimeHelper::ReadMemory(process, first_node_ptr_addr, 
                                          &node_ptr, sizeof(node_ptr))) {
      continue;
    }
    
    // Walk the linked list of nodes in this bucket
    while (node_ptr != 0 && pairs_extracted < max_pairs) {
      // CRITICAL: Store the ADDRESSES where the key and value pointers are stored,
      // not the pointer values themselves. This is what allows CreateValueObjectFromAddress
      // to work correctly - it will read the pointer from these addresses.
      
      // The key is stored at offset 8 in the node
      lldb::addr_t key_storage_addr = node_ptr + 8;
      
      // The value is stored at offset 16 in the node
      lldb::addr_t value_storage_addr = node_ptr + 16;
      
      // We store the addresses where the pointers are located in memory
      // CreateValueObjectFromAddress will read the actual pointers from these locations
      m_pairs.push_back({key_storage_addr, value_storage_addr});
      pairs_extracted++;
      
      // Read next node pointer (at offset 0 in node)
      lldb::addr_t next_ptr = 0;
      if (!GNUstepRuntimeHelper::ReadMemory(process, node_ptr, 
                                            &next_ptr, sizeof(next_ptr))) {
        break;
      }
      node_ptr = next_ptr;
    }
  }
  
  return true;
}

CompilerType GNUstepNSDictionarySyntheticProvider::GetConcreteTypeForObject(lldb::addr_t obj_addr) {
  // Always return the generic 'id' type for synthetic children.
  //
  // This follows Apple's LLDB formatter pattern where synthetic children providers
  // return 'id' types and let LLDB's dynamic type resolution pipeline handle
  // the conversion to concrete types automatically:
  //
  // 1. Synthetic children return 'id' types
  // 2. LLDB calls GetDynamicTypeAndAddress() on the runtime
  // 3. Runtime returns concrete class name (NSString, NSNumber, etc.)
  // 4. LLDB applies appropriate formatters automatically
  //
  // This approach is more robust and consistent with LLDB's architecture.
  return m_id_type;
}

llvm::Expected<uint32_t> GNUstepNSDictionarySyntheticProvider::CalculateNumChildren() {
  // Return two children per key-value pair (key and value shown separately)
  // Plus count as first child
  return static_cast<uint32_t>(m_pairs.size() * 2 + 1); // *2 for key+value, +1 for count
}

std::string GNUstepNSDictionarySyntheticProvider::GetElementSummary(Process *process, lldb::addr_t storage_addr, FormatterContext &context) {
  // Performance optimized - removed debug file I/O
  
  if (!process || storage_addr == 0 || storage_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read the actual element pointer from the storage address
  Status read_error;
  lldb::addr_t element_addr = GNUstepRuntimeHelper::ReadPointer(process, storage_addr, read_error);
  if (read_error.Fail() || element_addr == 0 || element_addr == LLDB_INVALID_ADDRESS) {
    return "<invalid>";
  }
  
  // Element address read successfully
  
  // For GSTinyString (tagged pointer with tag 4), decode directly
  // GSTinyString uses tag 4 in the low 3 bits
  if ((element_addr & 0x7) == 4) {
    // GSTinyString tagged pointer detected
    // This is a GSTinyString - decode it directly using the correct encoding
    // Length is in bits 3-7 (5 bits)
    int length = (element_addr >> 3) & 0x1F;
    
    // Decoding GSTinyString with optimized bit operations
    
    if (length > 0 && length <= 9) {
      std::string result;
      // Characters are extracted using the TINY_STRING_CHAR macro:
      // ((s & (0xFE00000000000000 >> (x*7))) >> (57-(x*7)))
      for (int i = 0; i < length; i++) {
        uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
        char c = (element_addr & mask) >> (57 - (i * 7));
        if (c >= 0x20 && c <= 0x7e) {
          result += c;
        } else if (c != 0) {
          // Non-printable character
          result += '?';
        }
      }
      
      if (!result.empty()) {
        return result;
      }
    }
    
    // GSTinyString decoding failed, continuing with fallback
  }
  
  // Try expression evaluation as a fallback
  // This is more reliable but slower
  ExecutionContext exe_ctx(process);
  Target *target = exe_ctx.GetTargetPtr();
  if (target) {
    // Call -UTF8String on the object to get its string representation
    std::string expr = llvm::formatv("(const char*)[(id)0x{0:x} UTF8String]", element_addr).str();
    
    ValueObjectSP result_sp;
    EvaluateExpressionOptions options;
    options.SetUnwindOnError(true);
    options.SetTryAllThreads(false);
    options.SetTimeout(std::chrono::milliseconds(500));
    
    target->EvaluateExpression(expr.c_str(), exe_ctx.GetFramePtr(), result_sp, options);
    
    if (result_sp && result_sp->GetError().Success()) {
      // Get the actual C string value
      lldb::addr_t str_addr = result_sp->GetValueAsUnsigned(0);
      if (str_addr && str_addr != LLDB_INVALID_ADDRESS) {
        std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, str_addr, 100);
        if (!result.empty()) {
          return result;
        }
      }
    }
  }
  
  // Fallback to the original implementation
  
  // Check if this is a tagged pointer first
  bool is_tagged = (element_addr & 0x7) != 0;
  
  if (is_tagged) {
    // Handle tagged pointers directly
    uint64_t tag = element_addr & 0x7;
    
    // Tagged strings (tag 4)
    if (tag == 4) {
      // GNUstep tiny string - extract characters from the pointer bits
      int length = (element_addr >> 3) & 0x1F;
      if (length > 0 && length <= 8) {
        std::string result;
        for (int i = 0; i < length; i++) {
          uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
          char c = (element_addr & mask) >> (57 - (i * 7));
          if (c >= 0x20 && c <= 0x7e) {
            result += c;
          } else {
            break;
          }
        }
        if (!result.empty()) {
          return result; // Return without quotes for use as key name
        }
      }
    }
    // Tagged numbers (tags 1-3)
    else if (tag >= 1 && tag <= 3) {
      // Simple integer extraction for tagged numbers
      int64_t value = static_cast<int64_t>(element_addr) >> 4;
      return std::to_string(value);
    }
    
    return "<tagged>";
  }
  
  // For non-tagged pointers, try to extract string content directly
  // This is faster than going through the full summary provider
  GNUstepObjCRuntimeIntrospector introspector(process);
  Status error;
  lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, element_addr, error);
  // ISA address read
  if (error.Success() && isa_addr != 0) {
    std::string class_name = introspector.GetClassName(isa_addr);
    // Class name retrieved
    // Class name introspection completed
    
    // Handle different string types
    if (class_name.find("String") != std::string::npos) {
      // Check if it's GSCInlineString
      if (class_name == "GSCInlineString" || class_name == "GSUInlineString") {
        // GSCInlineString stores data inline after the object structure
        // Structure: isa(8) + _contents(8) + _count(4) + _flags(4) + inline_data
        
        // Read string length
        uint32_t string_length = 0;
        lldb::addr_t count_addr = element_addr + 16;
        size_t bytes_read = process->ReadMemory(count_addr, &string_length, sizeof(string_length), error);
        
        if (error.Success() && string_length > 0 && string_length < 1000) {
          // Read flags to check if wide characters
          uint32_t flags = 0;
          lldb::addr_t flags_addr = element_addr + 20;
          bytes_read = process->ReadMemory(flags_addr, &flags, sizeof(flags), error);
          
          if (error.Success()) {
            bool is_wide = (flags & 0x1) != 0;
            
            // Data starts at offset 24
            lldb::addr_t data_addr = element_addr + 24;
            
            if (!is_wide) {
              // 8-bit characters
              std::vector<char> buffer(string_length + 1, 0);
              bytes_read = process->ReadMemory(data_addr, buffer.data(), string_length, error);
              if (error.Success() && bytes_read > 0) {
                return std::string(buffer.data(), bytes_read);
              }
            } else {
              // 16-bit Unicode characters - convert to UTF-8
              std::vector<uint16_t> wide_buffer(string_length);
              bytes_read = process->ReadMemory(data_addr, wide_buffer.data(), 
                                         string_length * sizeof(uint16_t), error);
              if (error.Success() && bytes_read > 0) {
                std::string result;
                for (size_t i = 0; i < string_length; i++) {
                  if (wide_buffer[i] < 128) {
                    result += static_cast<char>(wide_buffer[i]);
                  } else {
                    // Simple UTF-8 encoding for non-ASCII
                    result += '?'; // Placeholder for complex Unicode
                  }
                }
                return result;
              }
            }
          }
        }
      }
      // Handle NSConstantString
      else if (class_name.find("ConstantString") != std::string::npos) {
        // NSConstantString layout in GNUstep: {isa(8), flags/hash(8), char*(8), length(8)}
        // The string pointer is at offset 16, not 8
        lldb::addr_t str_ptr_addr = element_addr + 16;
        lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
        if (error.Success() && str_data_addr != 0) {
          std::string content = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 50);
          // String content read
          if (!content.empty()) {
            return content; // Return without quotes for use as key name
          }
        }
      }
      // Generic string handling fallback
      else {
        // Try NSConstantString layout first (string pointer at offset 16)
        lldb::addr_t str_ptr_addr = element_addr + 16;
        lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
        if (error.Success() && str_data_addr != 0) {
          std::string content = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 50);
          if (!content.empty()) {
            return content;
          }
        }
        // If that fails, try offset 8 (older layout)
        str_ptr_addr = element_addr + 8;
        str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
        if (error.Success() && str_data_addr != 0) {
          std::string content = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 50);
          if (!content.empty()) {
            return content;
          }
        }
      }
    }
    
    // For numbers
    if (class_name.find("Number") != std::string::npos) {
      return "<NSNumber>";
    }
  }
  
  // Returning fallback key representation
  return "<key>";
}

// Helper function to get summary for tagged pointers without creating ValueObjects
static std::string GetTaggedPointerSummary(Process *process, lldb::addr_t tagged_ptr) {
  if (!process || tagged_ptr == 0) {
    return "<nil>";
  }
  
  // Check the tag in the low 3 bits
  uint8_t tag = tagged_ptr & 0x7;
  
  // GSTinyString (tag 4)
  if (tag == 4) {
    // Length is in bits 3-7 (5 bits)
    int length = (tagged_ptr >> 3) & 0x1F;
    
    if (length > 0 && length <= 9) {
      std::string result = "\"";
      // Characters are extracted using the TINY_STRING_CHAR macro
      for (int i = 0; i < length; i++) {
        uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
        char c = (tagged_ptr & mask) >> (57 - (i * 7));
        if (c >= 0x20 && c <= 0x7e) {
          result += c;
        } else if (c != 0) {
          result += '?';
        }
      }
      result += "\"";
      return result;
    }
  }
  
  // GSTaggedNumber (tag 1, 2, 3, or 5) 
  if (tag == 1 || tag == 2 || tag == 3 || tag == 5) {
    const int SMALL_OBJECT_SHIFT = 3;
    
    if (tag == 1) {
      // NSSmallInt - integer value is ptr >> 3
      int64_t int_value = ((int64_t)tagged_ptr) >> SMALL_OBJECT_SHIFT;
      return std::to_string(int_value);
    } else if (tag == 5) {
      // NSSmallFloat
      union {
        uint64_t bits;
        double d;
      } converter;
      converter.bits = tagged_ptr & ~0x7ULL;  // Clear tag bits
      float float_value = (float)converter.d;
      
      // Format float with minimal precision for inline display
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%.6g", float_value);
      return std::string(buffer);
    } else if (tag == 2) {
      // NSSmallExtendedDouble
      uint64_t mask = tagged_ptr & 8;
      union {
        uint64_t bits;
        double d;
      } converter;
      converter.bits = (tagged_ptr & ~7ULL) | (mask >> 1) | (mask >> 2) | (mask >> 3);
      
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%.6g", converter.d);
      return std::string(buffer);
    } else if (tag == 3) {
      // NSSmallRepeatingDouble
      uint64_t mask = tagged_ptr & 56;
      union {
        uint64_t bits;
        double d;
      } converter;
      converter.bits = (tagged_ptr & ~7ULL) | (mask >> 3);
      
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%.6g", converter.d);
      return std::string(buffer);
    }
  }
  
  // Other tagged types - show raw value for debugging
  return llvm::formatv("<tagged:0x{0:x}>", tagged_ptr);
}

lldb::ValueObjectSP GNUstepNSDictionarySyntheticProvider::GetChildAtIndex(uint32_t idx) {
  if (idx == 0) {
    // First child is the count - create it as a synthetic value
    auto type_system = m_backend.GetCompilerType().GetTypeSystem();
    if (!type_system) {
      return nullptr;
    }
    
    CompilerType uint_type = type_system->GetBasicTypeFromAST(eBasicTypeUnsignedInt);
    if (!uint_type.IsValid()) {
      return nullptr;
    }
    
    // Create a data buffer with the count value
    uint32_t count = m_map_info.node_count;
    DataBufferSP data_buffer_sp(new DataBufferHeap(&count, sizeof(count)));
    DataExtractor data(data_buffer_sp, m_process->GetByteOrder(), 
                       m_process->GetAddressByteSize());
    
    return CreateValueObjectFromData("count", data, uint_type);
  }
  
  // Adjust index for key-value pairs (skip count)
  idx--;
  
  // Determine which pair and whether it's key or value
  size_t pair_idx = idx / 2;
  bool is_key = (idx % 2) == 0;
  
  if (pair_idx >= m_pairs.size()) {
    return nullptr;
  }
  
  const KeyValuePair &pair = m_pairs[pair_idx];
  
  // Get the id type from the scratch TypeSystem
  auto scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(m_process->GetTarget());
  if (!scratch_ts_sp) {
    return nullptr;
  }
  
  CompilerType id_type = scratch_ts_sp->GetType(
      scratch_ts_sp->getASTContext().ObjCBuiltinIdTy);
  
  if (!id_type.IsValid()) {
    return nullptr;
  }
  
  // Read the actual object pointer from memory
  Status error;
  lldb::addr_t object_ptr;
  lldb::addr_t storage_addr;
  
  if (is_key) {
    storage_addr = pair.key_addr;
  } else {
    storage_addr = pair.value_addr;
  }
  
  object_ptr = GNUstepRuntimeHelper::ReadPointer(m_process, storage_addr, error);
  if (error.Fail() || object_ptr == 0 || object_ptr == LLDB_INVALID_ADDRESS) {
    return nullptr;
  }
  
  // Create name for the child
  StreamString name_stream;
  if (is_key) {
    name_stream.Printf("[%zu].key", pair_idx);
  } else {
    name_stream.Printf("[%zu].value", pair_idx);
  }
  
  // Create execution context
  ExecutionContext exe_ctx(m_exe_ctx_ref);
  
  // Check if it's a tagged pointer
  bool is_tagged = (object_ptr & 0x7) != 0;
  
  // Handle the value based on whether it's a tagged pointer
  CompilerType element_type = GetConcreteTypeForObject(object_ptr);
  if (!element_type.IsValid()) {
    element_type = id_type;
  }
  
  if (is_tagged) {
    // Handle tagged pointers specially - they ARE the data, not pointers to data
    uint8_t tag = object_ptr & 0x7;
    
    if (tag == 4) {
      // GSTinyString - create a ValueObject with the tagged pointer as NSString* type
      // This allows LLDB to properly display it as an NSString instead of generic id
      
      // Try to get NSString* type from the runtime
      CompilerType nsstring_type = id_type; // Default to id if we can't get NSString*
      
      // Attempt to get the NSString class type
      ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*m_process);
      if (runtime) {
        // Look for NSString class in the runtime
        ConstString nsstring_name("NSString");
        
        // Try to get the class descriptor for NSString
        ObjCLanguageRuntime::ClassDescriptorSP nsstring_class = 
            runtime->GetClassDescriptorFromClassName(nsstring_name);
        
        if (nsstring_class) {
          // Get the CompilerType for NSString*
          TypeSP nsstring_type_sp = nsstring_class->GetType();
          if (nsstring_type_sp) {
            CompilerType nsstring_base_type = nsstring_type_sp->GetForwardCompilerType();
            if (nsstring_base_type.IsValid()) {
              // Make it a pointer type
              nsstring_type = nsstring_base_type.GetPointerType();
            }
          }
        }
      }
      
      // Create an NSString* type ValueObject with the tagged pointer value
      DataBufferSP ptr_buffer = std::make_shared<DataBufferHeap>(&object_ptr, sizeof(object_ptr));
      DataExtractor ptr_data(ptr_buffer, exe_ctx.GetByteOrder(), exe_ctx.GetAddressByteSize());
      
      return ValueObjectConstResult::Create(exe_ctx.GetBestExecutionContextScope(),
                                            nsstring_type,  // Use NSString* type instead of id
                                            ConstString(name_stream.GetString()),
                                            ptr_data);
    } else {
      // Other tagged pointers (numbers, etc) - create with appropriate type
      DataBufferSP ptr_buffer = std::make_shared<DataBufferHeap>(&object_ptr, sizeof(object_ptr));
      DataExtractor ptr_data(ptr_buffer, exe_ctx.GetByteOrder(), exe_ctx.GetAddressByteSize());
      
      return ValueObjectConstResult::Create(exe_ctx.GetBestExecutionContextScope(),
                                            element_type,
                                            ConstString(name_stream.GetString()),
                                            ptr_data);
    }
  } else {
    // For regular object pointers, create ValueObject from the storage address
    // CRITICAL FIX: Use storage_addr (storage address) not object_ptr (actual object)
    // This follows the same pattern as GNUstepNSArraySyntheticProvider - pass the address
    // where the pointer is stored, not the pointer value itself. LLDB will read from this
    // address and properly handle the object pointer (including dynamic type resolution).
    return SyntheticChildrenFrontEnd::CreateValueObjectFromAddress(
        name_stream.GetString(), 
        storage_addr,  // Storage address where the pointer is stored
        exe_ctx, element_type);
  }
}


bool GNUstepNSDictionarySyntheticProvider::MightHaveChildren() {
  return m_map_info.node_count > 0;
}

size_t GNUstepNSDictionarySyntheticProvider::GetIndexOfChildWithName(ConstString name) {
  if (name == ConstString("count")) {
    return 0;
  }
  
  // Parse names like "[0].key" or "[0].value"
  const char *name_str = name.GetCString();
  if (name_str && name_str[0] == '[') {
    size_t idx = 0;
    char type[10] = {0};
    if (sscanf(name_str, "[%zu].%9s", &idx, type) == 2) {
      if (strcmp(type, "key") == 0) {
        return 1 + idx * 2;
      } else if (strcmp(type, "value") == 0) {
        return 1 + idx * 2 + 1;
      }
    }
  }
  
  return UINT32_MAX;
}

//===----------------------------------------------------------------------===//
// Function wrappers for LLDB registration
//===----------------------------------------------------------------------===//

bool lldb_private::formatters::GNUstepNSDictionaryFormatterFunction(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  GNUstepNSDictionarySummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}

std::string GNUstepNSDictionarySyntheticProvider::ExtractStringFromObject(lldb::addr_t obj_addr) {
  if (!m_process || obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }

  // Check for tagged string first (GNUstep tiny strings)
  if ((obj_addr & 0x7) == 4) {
    // Tagged tiny string - extract directly
    std::string result;
    
    // Extract length from bits 3-7
    int length = (obj_addr >> 3) & 0x1F;
    if (length == 0 || length > 8) {
      return "";
    }
    
    // Extract characters from the upper bits
    for (int i = 0; i < length; i++) {
      uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
      char c = (obj_addr & mask) >> (57 - (i * 7));
      
      if (c >= 0x20 && c <= 0x7e) {
        result += c;
      } else if (c == 0) {
        break;
      } else {
        // Non-printable character
        return "";
      }
    }
    
    return result;
  }
  
  // For regular NSString objects, try to read the string content
  Status error;
  
  // Get class name to determine string type
  GNUstepObjCRuntimeIntrospector introspector(m_process);
  lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(m_process, obj_addr, error);
  if (error.Fail() || isa_addr == 0) {
    return "";
  }
  
  std::string class_name = introspector.GetClassName(isa_addr);
  
  // Early exit for NSConstantString - always handle it first
  if (class_name == "NSConstantString" || class_name.find("ConstantString") != std::string::npos) {
    // NSConstantString layout: { Class isa; char *cString; unsigned int length; }
    lldb::addr_t string_ptr = GNUstepRuntimeHelper::ReadPointer(m_process, obj_addr + 8, error);
    if (!error.Fail() && string_ptr != 0 && string_ptr != LLDB_INVALID_ADDRESS) {
      std::string result = GNUstepRuntimeHelper::ReadUTF8String(m_process, string_ptr, 256);
      if (!result.empty()) {
        return result;
      }
    }
    // If we failed to read NSConstantString, don't try other approaches
    return "";
  }
  
  // Try multiple approaches based on class type
  
  // Approach 1: NSConstantString (most common for literal strings)
  if (class_name.find("NSConstantString") != std::string::npos ||
      class_name.find("NXConstantString") != std::string::npos ||
      class_name.find("__NSConstantString") != std::string::npos) {
    // Layout: { Class isa; char *cString; unsigned int length; }
    lldb::addr_t string_ptr = GNUstepRuntimeHelper::ReadPointer(m_process, obj_addr + 8, error);
    if (!error.Fail() && string_ptr != 0 && string_ptr != LLDB_INVALID_ADDRESS) {
      return GNUstepRuntimeHelper::ReadUTF8String(m_process, string_ptr, 256);
    }
  }
  
  // Approach 2: GSCInlineString (common for dynamically created strings)
  if (class_name.find("GSCInlineString") != std::string::npos ||
      class_name.find("GSString") != std::string::npos) {
    // Try reading inline string content
    // GSCInlineString stores characters inline after the object header
    // Layout differs but usually starts at offset 16 or 24
    char buffer[257] = {0};
    size_t bytes_read = m_process->ReadMemory(obj_addr + 16, buffer, 256, error);
    if (!error.Fail() && bytes_read > 0) {
      // Ensure null termination at actual read position
      buffer[bytes_read] = '\0';
      
      // Find first null or use bytes_read
      size_t actual_len = strnlen(buffer, bytes_read);
      
      // Clean any non-printable characters
      for (size_t i = 0; i < actual_len; i++) {
        if (buffer[i] < 0x20 || buffer[i] > 0x7E) {
          if (buffer[i] != '\0') {
            actual_len = i; // Truncate at first non-printable
            break;
          }
        }
      }
      
      if (actual_len > 0) {
        return std::string(buffer, actual_len);
      }
    }
  }
  
  // Approach 2: Try GSString variants
  // Layout: { Class isa; uint32_t length; uint32_t hash; char *data; }
  uint32_t length = 0;
  if (GNUstepRuntimeHelper::ReadMemory(m_process, obj_addr + 8, &length, sizeof(length))) {
    if (length > 0 && length < 1024) { // Sanity check
      // Try different offsets for data pointer
      for (size_t offset : {16, 24}) {
        lldb::addr_t data_ptr = GNUstepRuntimeHelper::ReadPointer(m_process, obj_addr + offset, error);
        if (!error.Fail() && data_ptr != 0 && data_ptr != LLDB_INVALID_ADDRESS) {
          char buffer[257] = {0};
          size_t to_read = std::min(length, (uint32_t)256);
          size_t bytes_read = m_process->ReadMemory(data_ptr, buffer, to_read, error);
          if (!error.Fail() && bytes_read > 0) {
            // Ensure null termination at actual read position
            buffer[bytes_read] = '\0';
            
            // Find actual string length
            size_t actual_len = strnlen(buffer, bytes_read);
            
            // Clean any non-printable characters
            for (size_t i = 0; i < actual_len; i++) {
              if (buffer[i] < 0x20 || buffer[i] > 0x7E) {
                if (buffer[i] != '\0') {
                  actual_len = i; // Truncate at first non-printable
                  break;
                }
              }
            }
            
            if (actual_len > 0) {
              return std::string(buffer, actual_len);
            }
          }
        }
      }
    }
  }
  
  return "";
}

SyntheticChildrenFrontEnd *
lldb_private::formatters::GNUstepNSDictionarySyntheticFrontEndCreator(
    CXXSyntheticChildren *synth, lldb::ValueObjectSP valobj_sp) {
  return new GNUstepNSDictionarySyntheticProvider(valobj_sp);
}