//===-- GNUstepSetFormatters.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepSetFormatters.h"
#include "GNUstepNumberFormatters.h"
#include "GNUstepArrayFormatters.h"
#include "GNUstepDictionaryFormatters.h"
#include "GNUstepStringFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/DataFormatters/DumpValueObjectOptions.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Symbol/CompilerType.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "lldb/Target/Language.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Utility/StreamString.h"

// GNUstep small object (tagged pointer) detection
// On 64-bit systems, the low 3 bits are used
#define GNUSTEP_SMALL_OBJECT_MASK 7

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

//===----------------------------------------------------------------------===//
// NSSet Summary Provider
//===----------------------------------------------------------------------===//

bool GNUstepNSSetSummaryProvider::FormatObject(ValueObject &valobj, 
                                               Stream &stream, 
                                               const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid object");
    return false;
  }
  
  uint32_t count = ExtractSetCount(valobj);
  
  // Format the summary with inline elements like Apple's formatters
  if (count == 0) {
    stream.Printf("{}");
    return true;
  }
  
  // Show inline element preview (like Apple's formatters)
  std::string inline_elements = GetInlineElementsPreview(valobj, count);
  if (!inline_elements.empty()) {
    stream.Printf("%s", inline_elements.c_str());
  } else {
    // Fallback to just showing count if preview fails
    stream.Printf("{%u elements}", count);
  }
  
  return true;
}

uint32_t GNUstepNSSetSummaryProvider::ExtractSetCount(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return 0;
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return 0;
  }
  
  // GNUstep GSSet structure (from GSSet.m):
  // @interface GSSet : NSSet
  // {
  // @public
  //   GSIMapTable_t map;  // offset 8 (after isa)
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
  if (class_name.empty() || class_name == "<unknown>") {
    class_name = "GSSet"; // Fallback to most common set class
  }
  
  ptrdiff_t map_offset = GNUstepRuntimeHelper::GetIvarOffset(process, class_name, "map");
  if (map_offset < 0) {
    map_offset = 8; // Fallback to hardcoded offset (after isa)
  }
  
  // The nodeCount is at offset 8 within the GSIMapTable_t structure
  // So total offset is map_offset + 8
  lldb::addr_t count_addr = obj_addr + map_offset + 8;
  uint64_t count = 0;
  
  if (!GNUstepRuntimeHelper::ReadMemory(process, count_addr, &count, sizeof(count))) {
    return 0;
  }
  
  // Sanity check - set shouldn't have millions of entries
  if (count > 10000000) {
    return 0;
  }
  
  return static_cast<uint32_t>(count);
}

std::string GNUstepNSSetSummaryProvider::GetInlineElementsPreview(ValueObject &valobj, uint32_t count) {
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
  
  // Extract elements for preview (limit to 5 elements to avoid performance issues)
  std::vector<lldb::addr_t> elements;
  if (!ExtractSetElementsForPreview(process, obj_addr, elements, MAX_COLLECTION_ELEMENTS_INLINE)) {
    return "";
  }
  
  if (elements.empty()) {
    return "";
  }
  
  // Build inline preview showing first elements
  uint32_t preview_limit = std::min(static_cast<uint32_t>(elements.size()), MAX_COLLECTION_ELEMENTS_INLINE);
  std::string result = "{";
  
  for (uint32_t i = 0; i < preview_limit; ++i) {
    if (i > 0) {
      result += ", ";
    }
    
    // Get element summary
    std::string element_summary = GetElementSummary(process, elements[i]);
    if (element_summary.empty()) {
      element_summary = "<object>";
    }
    
    result += element_summary;
  }
  
  // Add ellipsis if there are more elements
  if (count > preview_limit) {
    result += ", ...";
  }
  
  result += "}";
  return result;
}

bool GNUstepNSSetSummaryProvider::ExtractSetElementsForPreview(Process *process, 
                                                               lldb::addr_t obj_addr,
                                                               std::vector<lldb::addr_t> &elements, 
                                                               uint32_t max_elements) {
  elements.clear();
  
  // The map field starts at offset 8 (after isa)
  lldb::addr_t map_ptr = obj_addr + 8;
  
  // Read bucket count from map.bucketCount (offset 16 within map)
  lldb::addr_t bucket_count_addr = map_ptr + 16;
  uint64_t bucket_count = 0;
  
  if (!GNUstepRuntimeHelper::ReadMemory(process, bucket_count_addr, &bucket_count, sizeof(bucket_count))) {
    return false;
  }
  
  if (bucket_count == 0 || bucket_count > 1000000) {
    return false;
  }
  
  // Read buckets pointer
  lldb::addr_t buckets_ptr_addr = map_ptr + 24;
  lldb::addr_t buckets_ptr = 0;
  
  if (!GNUstepRuntimeHelper::ReadMemory(process, buckets_ptr_addr, &buckets_ptr, sizeof(buckets_ptr))) {
    return false;
  }
  
  if (buckets_ptr == 0 || buckets_ptr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Iterate through all buckets and collect elements
  elements.reserve(max_elements);
  
  for (uint64_t bucket_idx = 0; bucket_idx < bucket_count && elements.size() < max_elements; ++bucket_idx) {
    // Read bucket at index
    lldb::addr_t bucket_addr = buckets_ptr + (bucket_idx * 16);
    
    // Read firstNode pointer (at offset 8 in bucket structure)
    lldb::addr_t first_node_ptr_addr = bucket_addr + 8;
    lldb::addr_t node_ptr = 0;
    
    if (!GNUstepRuntimeHelper::ReadMemory(process, first_node_ptr_addr, &node_ptr, sizeof(node_ptr))) {
      continue;
    }
    
    // Walk the linked list of nodes in this bucket
    while (node_ptr != 0 && node_ptr != LLDB_INVALID_ADDRESS && elements.size() < max_elements) {
      // Read the object (key.obj at offset 8)
      lldb::addr_t obj_addr = node_ptr + 8;
      lldb::addr_t obj_ptr = 0;
      
      if (!GNUstepRuntimeHelper::ReadMemory(process, obj_addr, &obj_ptr, sizeof(obj_ptr))) {
        break;
      }
      
      // Add element if valid
      if (obj_ptr != 0 && obj_ptr != LLDB_INVALID_ADDRESS) {
        elements.push_back(obj_ptr);
      }
      
      // Read next node pointer (at offset 0)
      lldb::addr_t next_ptr = 0;
      if (!GNUstepRuntimeHelper::ReadMemory(process, node_ptr, &next_ptr, sizeof(next_ptr))) {
        break;
      }
      
      node_ptr = next_ptr;
    }
  }
  
  return true;
}

std::string GNUstepNSSetSummaryProvider::GetElementSummary(Process *process, lldb::addr_t element_addr) {
  if (!process || element_addr == 0 || element_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Always try to extract string content first (handles both tagged pointers and regular strings)
  // This includes tagged strings which are the most common case
  std::string string_content = TryExtractStringContent(process, element_addr);
  if (!string_content.empty()) {
    // Return quoted string, truncated for inline display
    const size_t MAX_STRING_PREVIEW_LENGTH = 50;
    if (string_content.length() > MAX_STRING_PREVIEW_LENGTH) {
      return "\"" + string_content.substr(0, MAX_STRING_PREVIEW_LENGTH - 3) + "...\"";
    }
    return "\"" + string_content + "\"";
  }
  
  // FALLBACK: If string content extraction failed, try using LLDB's formatter pipeline
  // by creating a temporary ValueObject and using our string formatters directly
  ExecutionContextScope *exe_scope = process->GetTarget().GetProcessSP().get();
  if (exe_scope) {
    TypeSystemClangSP scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(
        process->GetTarget());
    if (scratch_ts_sp) {
      CompilerType id_type = scratch_ts_sp->GetType(
          scratch_ts_sp->getASTContext().ObjCBuiltinIdTy);
      
      ExecutionContext exe_ctx;
      exe_scope->CalculateExecutionContext(exe_ctx);
      ValueObjectSP valobj_sp = ValueObject::CreateValueObjectFromAddress(
          "element", element_addr, exe_ctx, id_type);
      
      if (valobj_sp) {
        // Try using GNUstep string formatter directly
        std::string class_name;
        GNUstepObjCRuntimeIntrospector introspector(process);
        Status error;
        lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, element_addr, error);
        if (error.Success() && isa_addr != 0) {
          class_name = introspector.GetClassName(isa_addr);
          
          // If it's a string class, use the string formatter
          if (class_name.find("String") != std::string::npos ||
              class_name.find("NSString") != std::string::npos) {
            GNUstepNSStringSummaryProvider string_formatter;
            StreamString string_stream;
            TypeSummaryOptions string_options;
            if (string_formatter.FormatObject(*valobj_sp, string_stream, string_options)) {
              std::string result = string_stream.GetString().str();
              // Remove any extra quotes or formatting
              if (!result.empty()) {
                return result;
              }
            }
          }
        }
      }
    }
  }
  
  // Check if it's a tagged pointer that couldn't be decoded as string
  GNUstepObjCRuntimeIntrospector introspector(process);
  if (introspector.IsTaggedPointer(element_addr)) {
    // For non-string tagged pointers, try to get proper formatting
    uint64_t tag = element_addr & 7;
    
    // For tagged numbers, decode directly using the same logic as GNUstepNumberFormatters
    if (tag == 1 || tag == 2 || tag == 3 || tag == 5) {
      const int SMALL_OBJECT_SHIFT = 3;
      
      if (tag == 1) {
        // NSSmallInt - integer value is ptr >> 3
        int64_t int_value = ((int64_t)element_addr) >> SMALL_OBJECT_SHIFT;
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
        return std::string(buffer);
      }
      
      // Unknown tagged number type
      return "<NSNumber>";
    }
    
    // For other tagged pointers
    switch (tag) {
      case 4: return "<NSString>"; // Fallback if decoding failed
      default: return "<tagged>";
    }
  }
  
  // For regular objects (non-tagged), check if it's an NSNumber and handle it specially
  Status error;
  lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, element_addr, error);
  if (error.Success() && isa_addr != 0) {
    std::string class_name = introspector.GetClassName(isa_addr);
    
    // Check if this is an NSNumber class
    if (class_name.find("NSNumber") != std::string::npos ||
        class_name.find("Number") != std::string::npos) {
      
      // Create a ValueObject for the NSNumber and use the NSNumber formatter
      ExecutionContextScope *exe_scope2 = process->GetTarget().GetProcessSP().get();
      if (exe_scope2) {
        TypeSystemClangSP scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(
            process->GetTarget());
        if (scratch_ts_sp) {
          CompilerType id_type = scratch_ts_sp->GetType(
              scratch_ts_sp->getASTContext().ObjCBuiltinIdTy);
          
          // CRITICAL FIX: Create ValueObject from ADDRESS, not from data containing pointer
          // This allows LLDB to properly resolve the object and apply formatters
          ExecutionContext exe_ctx2;
          exe_scope2->CalculateExecutionContext(exe_ctx2);
          ValueObjectSP valobj_sp = ValueObject::CreateValueObjectFromAddress(
              "element", element_addr, exe_ctx2, id_type);
          
          if (valobj_sp) {
            // CRITICAL FIX: For NSNumber objects in collections, manually extract the value
            // instead of relying on the complex NSNumber formatter which may fail in nested contexts
            
            // Read the ISA pointer first to get the exact class name
            GNUstepObjCRuntimeIntrospector introspector(process);
            Status isa_error;
            lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, element_addr, isa_error);
            if (isa_error.Success() && isa_addr != 0) {
              std::string exact_class_name = introspector.GetClassName(isa_addr);
              
              // Handle different NSNumber subclass types by reading value directly from memory
              if (exact_class_name.find("IntNumber") != std::string::npos) {
                // NSIntNumber stores int32_t at offset 8
                int32_t int_value = 0;
                if (GNUstepRuntimeHelper::ReadMemory(process, element_addr + 8, &int_value, sizeof(int_value))) {
                  return std::to_string(int_value);
                }
              } else if (exact_class_name.find("LongLongNumber") != std::string::npos) {
                // NSLongLongNumber stores int64_t at offset 8  
                int64_t ll_value = 0;
                if (GNUstepRuntimeHelper::ReadMemory(process, element_addr + 8, &ll_value, sizeof(ll_value))) {
                  return std::to_string(ll_value);
                }
              } else if (exact_class_name.find("FloatNumber") != std::string::npos) {
                // NSFloatNumber stores float at offset 8
                float float_value = 0.0f;
                if (GNUstepRuntimeHelper::ReadMemory(process, element_addr + 8, &float_value, sizeof(float_value))) {
                  char buffer[32];
                  snprintf(buffer, sizeof(buffer), "%g", float_value);
                  return std::string(buffer);
                }
              } else if (exact_class_name.find("DoubleNumber") != std::string::npos) {
                // NSDoubleNumber stores double at offset 8
                double double_value = 0.0;
                if (GNUstepRuntimeHelper::ReadMemory(process, element_addr + 8, &double_value, sizeof(double_value))) {
                  char buffer[32];  
                  snprintf(buffer, sizeof(buffer), "%g", double_value);
                  return std::string(buffer);
                }
              } else if (exact_class_name.find("BoolNumber") != std::string::npos) {
                // NSBoolNumber stores BOOL at offset 8
                uint32_t bool_value = 0;
                if (GNUstepRuntimeHelper::ReadMemory(process, element_addr + 8, &bool_value, sizeof(bool_value))) {
                  return (bool_value != 0) ? "YES" : "NO";
                }
              }
            }
            
            // Fallback: Use the GNUstep NSNumber formatter
            GNUstepNSNumberSummaryProvider number_formatter;
            StreamString number_stream;
            TypeSummaryOptions number_options;
            if (number_formatter.FormatObject(*valobj_sp, number_stream, number_options)) {
              return number_stream.GetString().str();
            }
          }
        }
      }
      
      // Fallback for NSNumber if formatter fails
      return "<NSNumber>";
    }
  }
  
  // For other regular objects, use generic ValueObject formatting
  ExecutionContextScope *exe_scope3 = process->GetTarget().GetProcessSP().get();
  if (exe_scope3) {
    TypeSystemClangSP scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(
        process->GetTarget());
    if (scratch_ts_sp) {
      CompilerType id_type = scratch_ts_sp->GetType(
          scratch_ts_sp->getASTContext().ObjCBuiltinIdTy);
      
      // CRITICAL FIX: Create ValueObject from ADDRESS, not from data containing pointer
      // This allows LLDB to properly resolve nested objects and apply formatters recursively
      ExecutionContext exe_ctx3;
      exe_scope3->CalculateExecutionContext(exe_ctx3);
      ValueObjectSP valobj_sp = ValueObject::CreateValueObjectFromAddress(
          "element", element_addr, exe_ctx3, id_type);
      
      if (valobj_sp) {
        // CRITICAL FIX: Manually apply GNUstep formatters since LLDB may not
        // automatically select them for nested objects created programmatically
        
        // Try to get the class name to determine which formatter to use
        std::string class_name = introspector.GetClassName(isa_addr);
        
        if (class_name.find("Dictionary") != std::string::npos ||
            class_name.find("NSDictionary") != std::string::npos) {
          // Apply dictionary formatter
          GNUstepNSDictionarySummaryProvider dict_formatter;
          StreamString dict_stream;
          TypeSummaryOptions dict_options;
          if (dict_formatter.FormatObject(*valobj_sp, dict_stream, dict_options)) {
            return dict_stream.GetString().str();
          }
        } else if (class_name.find("Array") != std::string::npos ||
                   class_name.find("NSArray") != std::string::npos) {
          // Apply array formatter
          GNUstepNSArraySummaryProvider array_formatter;
          StreamString array_stream;
          TypeSummaryOptions array_options;
          if (array_formatter.FormatObject(*valobj_sp, array_stream, array_options)) {
            return array_stream.GetString().str();
          }
        } else if (class_name.find("Set") != std::string::npos ||
                   class_name.find("NSSet") != std::string::npos) {
          // Apply set formatter
          GNUstepNSSetSummaryProvider set_formatter;
          StreamString set_stream;
          TypeSummaryOptions set_options;
          if (set_formatter.FormatObject(*valobj_sp, set_stream, set_options)) {
            return set_stream.GetString().str();
          }
        }
        
        // Fallback to LLDB's automatic summary
        const char *summary = valobj_sp->GetSummaryAsCString();
        if (summary && strlen(summary) > 0) {
          return summary;
        }
      }
    }
  }
  
  // For non-string objects where we couldn't get a better summary
  return "";
}

bool GNUstepNSSetSummaryProvider::IsGNUstepTaggedPointer(lldb::addr_t addr) {
  return (addr & 7) != 0;
}

std::string GNUstepNSSetSummaryProvider::GetTaggedPointerSummary(lldb::addr_t addr) {
  // This method should not be used anymore - tagged pointer decoding
  // is handled properly in GetElementSummary() using the introspector
  return "<tagged>";
}

std::string GNUstepNSSetSummaryProvider::TryExtractStringContent(Process *process, lldb::addr_t obj_addr) {
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
    
    lldb::addr_t cstring_ptr_addr = obj_addr + 24;  // Corrected offset
    lldb::addr_t cstring_ptr = GNUstepRuntimeHelper::ReadPointer(process, cstring_ptr_addr, error);
    if (error.Success() && cstring_ptr != 0 && cstring_ptr != LLDB_INVALID_ADDRESS) {
      // Read the length at correct offset 12
      lldb::addr_t len_addr = obj_addr + 12;
      uint32_t string_length = 0;
      GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length));
      
      // Use length if available, otherwise fallback to null-terminated read
      size_t max_len = (string_length > 0 && string_length < 1024) ? string_length : 1023;
      
      char buffer[1024] = {0};
      size_t bytes_read = process->ReadMemory(cstring_ptr, buffer, max_len, error);
      if (error.Success() && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        // Find actual string length (might be shorter than buffer)
        size_t len = strnlen(buffer, bytes_read);
        if (len > 0) {
          return std::string(buffer, len);
        }
      }
    }
  } else if (class_name.find("GSString") != std::string::npos ||
             class_name.find("NSString") != std::string::npos) {
    // Regular GSString/NSString layout:
    // struct {
    //   Class isa;          // offset 0
    //   NSUInteger count;   // offset 8
    //   NSUInteger size;    // offset 16 (sometimes)
    //   char *bytes;        // offset 16 or 24
    // }
    
    // Try reading count first
    lldb::addr_t count_addr = obj_addr + 8;
    uint64_t count = 0;
    if (GNUstepRuntimeHelper::ReadMemory(process, count_addr, &count, sizeof(count))) {
      if (count > 0 && count < 10000) {  // Sanity check
        // Try offset 16 first (most common)
        lldb::addr_t bytes_ptr_addr = obj_addr + 16;
        lldb::addr_t bytes_ptr = GNUstepRuntimeHelper::ReadPointer(process, bytes_ptr_addr, error);
        if (error.Success() && bytes_ptr != 0 && bytes_ptr != LLDB_INVALID_ADDRESS) {
          std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, bytes_ptr, count);
          if (!result.empty()) {
            return result;
          }
        }
        
        // Try offset 24 as fallback
        bytes_ptr_addr = obj_addr + 24;
        bytes_ptr = GNUstepRuntimeHelper::ReadPointer(process, bytes_ptr_addr, error);
        if (error.Success() && bytes_ptr != 0 && bytes_ptr != LLDB_INVALID_ADDRESS) {
          return GNUstepRuntimeHelper::ReadUTF8String(process, bytes_ptr, count);
        }
      }
    }
  }
  
  return "";
}

std::string GNUstepNSSetSummaryProvider::TryExtractCollectionSummary(Process *process, lldb::addr_t obj_addr) {
  // This would try to detect if the object is an NSArray, NSDictionary, NSSet, etc.
  // For now, return empty to fall back to generic object display
  return "";
}

//===----------------------------------------------------------------------===//
// NSSet Synthetic Children Provider
//===----------------------------------------------------------------------===//

GNUstepNSSetSyntheticProvider::GNUstepNSSetSyntheticProvider(
    lldb::ValueObjectSP valobj_sp)
    : GNUstepSyntheticProvider(valobj_sp),
      m_map_ptr(LLDB_INVALID_ADDRESS),
      m_count(0),
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

bool GNUstepNSSetSyntheticProvider::UpdateImpl() {
  // Update execution context reference
  m_exe_ctx_ref = m_backend.GetExecutionContextRef();
  
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(m_backend);
  if (!process) {
    return false;
  }
  
  // CRITICAL: Store the process reference for use in GetChildAtIndex
  m_process = process;
  
  lldb::addr_t obj_addr = m_backend.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Check if this is a mutable set
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(m_backend);
  m_is_mutable = (class_name.find("Mutable") != std::string::npos);
  
  // The map field starts at offset 8 (after isa)
  m_map_ptr = obj_addr + 8;
  
  // Read the count from map.nodeCount (offset 8 within map)
  lldb::addr_t count_addr = m_map_ptr + 8;
  uint64_t count = 0;
  
  if (!GNUstepRuntimeHelper::ReadMemory(process, count_addr, &count, sizeof(count))) {
    return false;
  }
  
  // Sanity check
  if (count > 10000000) {
    return false;
  }
  
  m_count = static_cast<uint32_t>(count);
  
  // Read the set elements
  return ReadSetElements();
}

bool GNUstepNSSetSyntheticProvider::ReadSetElements() {
  m_elements.clear();
  
  if (m_count == 0) {
    return true;
  }
  
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(m_backend);
  if (!process) {
    return false;
  }
  
  // GSIMapTable structure:
  // struct _GSIMapTable {
  //   NSZone    *zone;         // offset 0
  //   uintptr_t  nodeCount;    // offset 8
  //   uintptr_t  bucketCount;  // offset 16
  //   GSIMapBucket buckets;    // offset 24
  //   ...
  // }
  //
  // GSIMapBucket is a pointer to GSIMapNode
  // 
  // GSIMapNode structure:
  // struct _GSIMapNode {
  //   GSIMapNode next;  // offset 0
  //   union {
  //     id obj;         // For sets, the key is the object
  //     ...
  //   } key;            // offset 8
  //   union {
  //     id obj;         // Sets don't use the value field
  //     ...
  //   } value;          // offset 16 (not used for sets)
  // }
  
  // Read bucket count
  lldb::addr_t bucket_count_addr = m_map_ptr + 16;
  uint64_t bucket_count = 0;
  
  if (!GNUstepRuntimeHelper::ReadMemory(process, bucket_count_addr, &bucket_count, sizeof(bucket_count))) {
    return false;
  }
  
  if (bucket_count == 0 || bucket_count > 1000000) {
    return false;
  }
  
  // Read buckets pointer
  lldb::addr_t buckets_ptr_addr = m_map_ptr + 24;
  lldb::addr_t buckets_ptr = 0;
  
  if (!GNUstepRuntimeHelper::ReadMemory(process, buckets_ptr_addr, &buckets_ptr, sizeof(buckets_ptr))) {
    return false;
  }
  
  if (buckets_ptr == 0 || buckets_ptr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Iterate through all buckets and collect elements
  m_elements.reserve(m_count);
  
  for (uint64_t bucket_idx = 0; bucket_idx < bucket_count && m_elements.size() < m_count; ++bucket_idx) {
    // Read bucket at index (GSIMapBucket structure is {nodeCount, firstNode})
    lldb::addr_t bucket_addr = buckets_ptr + (bucket_idx * 16); // sizeof(GSIMapBucket_t)
    
    // Read firstNode pointer (at offset 8 in bucket structure)
    lldb::addr_t first_node_ptr_addr = bucket_addr + 8;
    lldb::addr_t node_ptr = 0;
    
    if (!GNUstepRuntimeHelper::ReadMemory(process, first_node_ptr_addr, &node_ptr, sizeof(node_ptr))) {
      continue;
    }
    
    // Walk the linked list of nodes in this bucket
    while (node_ptr != 0 && node_ptr != LLDB_INVALID_ADDRESS && m_elements.size() < m_count) {
      // Read the object (key.obj at offset 8)
      lldb::addr_t obj_addr = node_ptr + 8;
      lldb::addr_t obj_ptr = 0;
      
      if (!GNUstepRuntimeHelper::ReadMemory(process, obj_addr, &obj_ptr, sizeof(obj_ptr))) {
        break;
      }
      
      // Add element even if it's nil (for proper indexing)
      if (obj_ptr != LLDB_INVALID_ADDRESS) {
        m_elements.push_back(obj_ptr);
      }
      
      // Read next node pointer (at offset 0)
      lldb::addr_t next_ptr = 0;
      if (!GNUstepRuntimeHelper::ReadMemory(process, node_ptr, &next_ptr, sizeof(next_ptr))) {
        break;
      }
      
      node_ptr = next_ptr;
    }
  }
  
  return true;
}

CompilerType GNUstepNSSetSyntheticProvider::GetConcreteTypeForObject(lldb::addr_t obj_addr) {
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

llvm::Expected<uint32_t> GNUstepNSSetSyntheticProvider::CalculateNumChildren() {
  return m_count;
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

lldb::ValueObjectSP GNUstepNSSetSyntheticProvider::GetChildAtIndex(uint32_t idx) {
  if (idx >= m_count || idx >= m_elements.size()) {
    return nullptr;
  }
  
  lldb::addr_t element_value = m_elements[idx];
  if (element_value == 0 || element_value == LLDB_INVALID_ADDRESS) {
    return nullptr;
  }
  
  StreamString idx_name;
  idx_name.Printf("[%u]", idx);
  ExecutionContext exe_ctx(m_exe_ctx_ref);
  
  if (!m_process) {
    return nullptr;
  }
  
  // Check if this is a tagged pointer (low 3 bits set)
  bool is_tagged_pointer = (element_value & 0x7) != 0;
  
  if (is_tagged_pointer) {
    // NEW APPROACH: Instead of trying to create synthetic pointer ValueObjects,
    // create ValueObjects with the actual decoded primitive values
    
    uint8_t tag = element_value & 0x7;
    
    // Get type system for creating primitive types
    auto type_system_or_err = exe_ctx.GetTargetRef().GetScratchTypeSystemForLanguage(lldb::eLanguageTypeC);
    if (!type_system_or_err) {
      return nullptr;
    }
    auto type_system = type_system_or_err.get();
    
    if (tag == 4) {
      // GSTinyString - create a ValueObject with the tagged pointer as NSString* type
      // This allows LLDB to properly display it as an NSString instead of generic id
      
      // Try to get NSString* type from the runtime
      CompilerType nsstring_type = m_id_type; // Default to id if we can't get NSString*
      
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
      DataBufferSP ptr_buffer = std::make_shared<DataBufferHeap>(&element_value, sizeof(element_value));
      DataExtractor ptr_data(ptr_buffer, exe_ctx.GetByteOrder(), exe_ctx.GetAddressByteSize());
      
      return ValueObjectConstResult::Create(exe_ctx.GetBestExecutionContextScope(),
                                            nsstring_type,  // Use NSString* type instead of id
                                            ConstString(idx_name.GetString()),
                                            ptr_data);
      
    } else if (tag == 1 || tag == 3) {
      // GSTaggedNumber - create an integer ValueObject  
      int64_t decoded_number = (int64_t)(element_value >> 3);  // Remove tag bits
      
      // Create integer type
      CompilerType int_type = type_system->GetBasicTypeFromAST(eBasicTypeLongLong);
      
      // Create number data buffer
      DataBufferSP number_buffer = std::make_shared<DataBufferHeap>(&decoded_number, sizeof(decoded_number));
      DataExtractor number_data(number_buffer, exe_ctx.GetByteOrder(), exe_ctx.GetAddressByteSize());
      
      return ValueObjectConstResult::Create(exe_ctx.GetBestExecutionContextScope(),
                                            int_type,
                                            ConstString(idx_name.GetString()),
                                            number_data);
    } else {
      // Unknown tagged pointer - show as hex value
      CompilerType ptr_type = type_system->GetBasicTypeFromAST(eBasicTypeVoid).GetPointerType();
      
      DataBufferSP ptr_buffer = std::make_shared<DataBufferHeap>(&element_value, sizeof(element_value));
      DataExtractor ptr_data(ptr_buffer, exe_ctx.GetByteOrder(), exe_ctx.GetAddressByteSize());
      
      return ValueObjectConstResult::Create(exe_ctx.GetBestExecutionContextScope(),
                                            ptr_type,
                                            ConstString(idx_name.GetString()),
                                            ptr_data);
    }
  } else {
    // For regular object pointers, use the standard approach with synthetic storage
    CompilerType element_type = GetConcreteTypeForObject(element_value);
    if (!element_type.IsValid()) {
      element_type = m_id_type;
    }
    
    if (!m_synthetic_storage.get()) {
      // Create synthetic storage for element pointers on first use
      size_t ptr_size = exe_ctx.GetAddressByteSize(); 
      size_t storage_size = m_count * ptr_size;
      m_synthetic_storage = std::make_unique<DataBufferHeap>(storage_size, 0);
      
      // Populate the synthetic storage with element pointers
      uint8_t *storage_bytes = (uint8_t*)m_synthetic_storage->GetBytes();
      for (size_t i = 0; i < m_elements.size() && i < m_count; ++i) {
        if (ptr_size == 8) {
          uint64_t element_ptr = m_elements[i];
          memcpy(storage_bytes + (i * ptr_size), &element_ptr, sizeof(uint64_t));
        } else {
          uint32_t element_ptr = static_cast<uint32_t>(m_elements[i]);
          memcpy(storage_bytes + (i * ptr_size), &element_ptr, sizeof(uint32_t));
        }
      }
    }
    
    // Create DataExtractor for this element's storage
    size_t ptr_size = exe_ctx.GetAddressByteSize();
    uint8_t *element_storage_ptr = (uint8_t*)m_synthetic_storage->GetBytes() + (idx * ptr_size);
    
    DataExtractor element_data(element_storage_ptr, ptr_size, 
                             exe_ctx.GetByteOrder(), ptr_size);
    
    // Create ValueObject from synthetic storage
    return ValueObject::CreateValueObjectFromData(idx_name.GetString(), 
                                                  element_data, exe_ctx, element_type);
  }
}

lldb::addr_t GNUstepNSSetSyntheticProvider::GetElementAtIndex(uint32_t idx) {
  if (idx >= m_elements.size()) {
    return LLDB_INVALID_ADDRESS;
  }
  return m_elements[idx];
}

//===----------------------------------------------------------------------===//
// Function wrappers for LLDB registration
//===----------------------------------------------------------------------===//

bool lldb_private::formatters::GNUstepNSSetFormatterFunction(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  GNUstepNSSetSummaryProvider provider;
  return provider.FormatObject(valobj, stream, options);
}

SyntheticChildrenFrontEnd *
lldb_private::formatters::GNUstepNSSetSyntheticFrontEndCreator(
    CXXSyntheticChildren *synth, lldb::ValueObjectSP valobj_sp) {
  if (!valobj_sp) {
    return nullptr;
  }
  return new GNUstepNSSetSyntheticProvider(valobj_sp);
}