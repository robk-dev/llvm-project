//===-- GNUstepSetFormatters.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepSetFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Symbol/CompilerType.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Utility/StreamString.h"

// GNUstep small object (tagged pointer) detection
// On 64-bit systems, the low 3 bits are used
#define GNUSTEP_SMALL_OBJECT_MASK 7

static bool IsGNUstepTaggedPointer(lldb::addr_t addr) {
  return (addr & GNUSTEP_SMALL_OBJECT_MASK) != 0;
}

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
    stream.Printf("0 objects");
    return true;
  }
  
  // Show count and first few elements inline
  if (count == 1) {
    stream.Printf("1 object");
  } else {
    stream.Printf("%u objects", count);
  }
  
  // Add inline element preview for better UX (like Apple's formatters)
  std::string inline_elements = GetInlineElementsPreview(valobj, count);
  if (!inline_elements.empty()) {
    stream.Printf(" %s", inline_elements.c_str());
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
  
  // The map field starts at offset 8 (after isa)
  // nodeCount is at offset 8 within the map structure
  // So total offset is 8 + 8 = 16
  lldb::addr_t count_addr = obj_addr + 16;
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
  
  // Extract elements for preview (limit to 3 elements to avoid performance issues)
  std::vector<lldb::addr_t> elements;
  if (!ExtractSetElementsForPreview(process, obj_addr, elements, 3)) {
    return "";
  }
  
  if (elements.empty()) {
    return "";
  }
  
  // Build inline preview showing first elements
  uint32_t preview_limit = std::min(static_cast<uint32_t>(elements.size()), 20u);
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
  
  // Try to extract string content first (handles both tagged pointers and regular strings)
  std::string string_content = TryExtractStringContent(process, element_addr);
  if (!string_content.empty()) {
    // Return quoted string, truncated for inline display
    if (string_content.length() > 15) {
      return "\"" + string_content.substr(0, 12) + "...\"";
    }
    return "\"" + string_content + "\"";
  }
  
  // Check if it's a tagged pointer that couldn't be decoded as string
  GNUstepObjCRuntimeIntrospector introspector(process);
  if (introspector.IsTaggedPointer(element_addr)) {
    // For non-string tagged pointers, show the tag type
    uint64_t tag = element_addr & 7;
    switch (tag) {
      case 1: return "<NSNumber:tagged>";
      case 2: return "<NSDate:tagged>";
      case 4: return "<NSString:tagged>"; // Fallback if decoding failed
      default: return "<tagged>";
    }
  }
  
  // Try to detect other collection types for recursive display
  std::string collection_summary = TryExtractCollectionSummary(process, element_addr);
  if (!collection_summary.empty()) {
    return collection_summary;
  }
  
  // For other objects, try to get a basic summary
  return "<object>";
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
  // Simplified string extraction for inline display
  lldb::addr_t str_ptr_addr = obj_addr + 24;
  
  Status error;
  lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  if (error.Fail() || str_data_addr == 0 || str_data_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read the string length (limit for inline display)
  lldb::addr_t len_addr = obj_addr + 8;
  uint32_t string_length = 0;
  
  if (!GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length))) {
    string_length = 0;
  }
  
  // Limit length for inline display
  if (string_length > 15) {
    string_length = 15;
  }
  
  if (string_length > 0) {
    std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, string_length);
    if (string_length == 15) {
      result += "...";
    }
    return result;
  }
  
  // Fallback
  std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 15);
  if (result.length() >= 15) {
    result = result.substr(0, 12) + "...";
  }
  return result;
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
      m_is_mutable(false) {}

bool GNUstepNSSetSyntheticProvider::UpdateImpl() {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(m_backend);
  if (!process) {
    return false;
  }
  
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

llvm::Expected<uint32_t> GNUstepNSSetSyntheticProvider::CalculateNumChildren() {
  return m_count;
}

lldb::ValueObjectSP GNUstepNSSetSyntheticProvider::GetChildAtIndex(uint32_t idx) {
  if (idx >= m_count || idx >= m_elements.size()) {
    return nullptr;
  }
  
  lldb::addr_t element_addr = m_elements[idx];
  if (element_addr == 0 || element_addr == LLDB_INVALID_ADDRESS) {
    return nullptr;
  }
  
  // Create a synthetic child for the element
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(m_backend);
  if (!process) {
    return nullptr;
  }
  
  // Get the ObjC runtime to get type information
  ObjCLanguageRuntime *objc_runtime = ObjCLanguageRuntime::Get(*process);
  CompilerType id_type;
  
  if (objc_runtime) {
    // Try to get the id type from runtime
    auto type_system = m_backend.GetCompilerType().GetTypeSystem();
    if (type_system) {
      id_type = type_system->GetBasicTypeFromAST(eBasicTypeObjCID);
    }
  }
  
  if (!id_type.IsValid()) {
    // Fallback to void*
    auto type_system = m_backend.GetCompilerType().GetTypeSystem();
    if (type_system) {
      id_type = type_system->GetBasicTypeFromAST(eBasicTypeVoid).GetPointerType();
    }
  }
  
  if (!id_type.IsValid()) {
    return nullptr;
  }
  
  // Create child name
  StreamString name;
  name.Printf("[%u]", idx);
  
  // Create the value object from the address
  // For tagged pointers, we need to handle them specially
  if (IsGNUstepTaggedPointer(element_addr)) {
    // Tagged pointer - for now, create as a hex integer to avoid dereferencing
    Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(m_backend);
    if (!process) {
      return nullptr;
    }
    auto type_system = m_backend.GetCompilerType().GetTypeSystem();
    if (type_system) {
      // Create as uint64_t to show the tagged pointer value
      CompilerType uint_type = type_system->GetBasicTypeFromAST(eBasicTypeUnsignedLongLong);
      if (uint_type.IsValid()) {
        DataBufferSP data_buffer_sp(new DataBufferHeap(&element_addr, sizeof(element_addr)));
        DataExtractor data(data_buffer_sp, process->GetByteOrder(), 
                           process->GetAddressByteSize());
        // Create with a descriptive name
        std::string tagged_name = name.GetData();
        tagged_name += " (tagged)";
        return CreateValueObjectFromData(tagged_name, data, uint_type);
      }
    }
    // Fallback if we can't create uint type
    return CreateValueObjectFromAddress(name.GetData(), element_addr, id_type);
  } else {
    // Regular pointer - use the normal address-based creation
    return CreateValueObjectFromAddress(name.GetData(), element_addr, id_type);
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