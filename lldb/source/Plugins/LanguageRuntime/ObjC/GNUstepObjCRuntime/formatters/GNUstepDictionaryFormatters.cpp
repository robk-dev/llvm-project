//===-- GNUstepDictionaryFormatters.cpp -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepDictionaryFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Symbol/CompilerType.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"

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
// NSDictionary Summary Provider
//===----------------------------------------------------------------------===//

bool GNUstepNSDictionarySummaryProvider::FormatObject(ValueObject &valobj, 
                                                      Stream &stream, 
                                                      const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid object");
    return false;
  }
  
  uint32_t count = ExtractDictionaryCount(valobj);
  
  // Format the summary with inline elements like Apple's formatters
  if (count == 0) {
    stream.Printf("0 key/value pairs");
    return true;
  }
  
  // Show count and first few key-value pairs inline
  if (count == 1) {
    stream.Printf("1 key/value pair");
  } else {
    stream.Printf("%u key/value pairs", count);
  }
  
  // Add inline key-value preview for better UX
  std::string inline_pairs = GetInlinePairsPreview(valobj, count);
  if (!inline_pairs.empty()) {
    stream.Printf(" %s", inline_pairs.c_str());
  }
  
  return true;
}

uint32_t GNUstepNSDictionarySummaryProvider::ExtractDictionaryCount(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return 0;
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return 0;
  }
  
  // GNUstep GSDictionary structure (from GSDictionary.m):
  // @interface GSDictionary : NSDictionary
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
  
  // Extract key-value pairs for preview (limit to 3 pairs to avoid performance issues)
  std::vector<KeyValuePair> pairs;
  if (!ExtractKeyValuePairsForPreview(process, map_info, pairs, 3)) {
    return "";
  }
  
  if (pairs.empty()) {
    return "";
  }
  
  // Build inline preview showing first pairs
  uint32_t preview_limit = std::min(static_cast<uint32_t>(pairs.size()), 20u);
  std::string result = "@{";
  
  for (uint32_t i = 0; i < preview_limit; ++i) {
    if (i > 0) {
      result += ", ";
    }
    
    // Get key summary
    std::string key_summary = GetElementSummary(process, pairs[i].key_addr);
    if (key_summary.empty()) {
      key_summary = "<key>";
    }
    
    // Get value summary  
    std::string value_summary = GetElementSummary(process, pairs[i].value_addr);
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
  // GSDictionary has GSIMapTable_t at offset 8 (after isa)
  lldb::addr_t map_addr = obj_addr + 8;
  
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
      // Read key (at offset 8 in node)
      lldb::addr_t key_addr = node_ptr + 8;
      lldb::addr_t key_ptr = 0;
      if (!GNUstepRuntimeHelper::ReadMemory(process, key_addr, 
                                            &key_ptr, sizeof(key_ptr))) {
        break;
      }
      
      // Read value (at offset 16 in node)
      lldb::addr_t value_addr = node_ptr + 16;
      lldb::addr_t value_ptr = 0;
      if (!GNUstepRuntimeHelper::ReadMemory(process, value_addr, 
                                            &value_ptr, sizeof(value_ptr))) {
        break;
      }
      
      // Add key-value pair
      if (key_ptr != 0 || value_ptr != 0) {
        pairs.push_back({key_ptr, value_ptr});
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

std::string GNUstepNSDictionarySummaryProvider::GetElementSummary(Process *process, lldb::addr_t element_addr) {
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

bool GNUstepNSDictionarySummaryProvider::IsGNUstepTaggedPointer(lldb::addr_t addr) {
  return (addr & 7) != 0;
}

std::string GNUstepNSDictionarySummaryProvider::GetTaggedPointerSummary(lldb::addr_t addr) {
  // This method should not be used anymore - tagged pointer decoding
  // is handled properly in GetElementSummary() using the introspector
  return "<tagged>";
}

std::string GNUstepNSDictionarySummaryProvider::TryExtractStringContent(Process *process, lldb::addr_t obj_addr) {
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
      m_is_mutable(false) {}

bool GNUstepNSDictionarySyntheticProvider::UpdateImpl() {
  printf("[GNUstepDict] UpdateImpl called\n");
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(m_backend);
  if (!process) {
    printf("[GNUstepDict] No process\n");
    return false;
  }
  
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
  // GSDictionary has GSIMapTable_t at offset 8 (after isa)
  lldb::addr_t map_addr = obj_addr + 8;
  
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
      // Read key (at offset 8 in node)
      lldb::addr_t key_addr = node_ptr + 8;
      lldb::addr_t key_ptr = 0;
      if (!GNUstepRuntimeHelper::ReadMemory(process, key_addr, 
                                            &key_ptr, sizeof(key_ptr))) {
        break;
      }
      
      // Read value (at offset 16 in node)
      lldb::addr_t value_addr = node_ptr + 16;
      lldb::addr_t value_ptr = 0;
      if (!GNUstepRuntimeHelper::ReadMemory(process, value_addr, 
                                            &value_ptr, sizeof(value_ptr))) {
        break;
      }
      
      // Add key-value pair (keys can be nil in some cases, but add them anyway)
      if (key_ptr != 0 || value_ptr != 0) {
        m_pairs.push_back({key_ptr, value_ptr});
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

llvm::Expected<uint32_t> GNUstepNSDictionarySyntheticProvider::CalculateNumChildren() {
  // Return twice the number of pairs (one child for key, one for value)
  // Plus optionally show count as first child
  return static_cast<uint32_t>(m_pairs.size() * 2 + 1); // +1 for count
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
  
  // Adjust index for key-value pairs
  idx--;
  
  size_t pair_idx = idx / 2;
  bool is_key = (idx % 2) == 0;
  
  if (pair_idx >= m_pairs.size()) {
    return nullptr;
  }
  
  const KeyValuePair &pair = m_pairs[pair_idx];
  
  // Get the ObjC runtime to get type information
  ObjCLanguageRuntime *objc_runtime = ObjCLanguageRuntime::Get(*m_process);
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
  
  // Create name and get the appropriate address
  char name[32];
  lldb::addr_t addr;
  
  if (is_key) {
    snprintf(name, sizeof(name), "[%zu].key", pair_idx);
    addr = pair.key_addr;
  } else {
    snprintf(name, sizeof(name), "[%zu].value", pair_idx);
    addr = pair.value_addr;
  }
  
  // Create the value object from the address
  // For tagged pointers, we need to handle them specially
  printf("[GNUstepDict] Creating child %s with addr=0x%llx, is_tagged=%d\n",
         name, (unsigned long long)addr, IsGNUstepTaggedPointer(addr));
  if (IsGNUstepTaggedPointer(addr)) {
    // Tagged pointer - for now, create as a hex integer to avoid dereferencing
    auto type_system = m_backend.GetCompilerType().GetTypeSystem();
    if (type_system) {
      // Create as uint64_t to show the tagged pointer value
      CompilerType uint_type = type_system->GetBasicTypeFromAST(eBasicTypeUnsignedLongLong);
      if (uint_type.IsValid()) {
        DataBufferSP data_buffer_sp(new DataBufferHeap(&addr, sizeof(addr)));
        DataExtractor data(data_buffer_sp, m_process->GetByteOrder(), 
                           m_process->GetAddressByteSize());
        // Create with a descriptive name
        std::string tagged_name = name;
        tagged_name += " (tagged)";
        return CreateValueObjectFromData(tagged_name, data, uint_type);
      }
    }
    // Fallback if we can't create uint type
    return CreateValueObjectFromAddress(name, addr, id_type);
  } else {
    // Regular pointer - use the normal address-based creation
    return CreateValueObjectFromAddress(name, addr, id_type);
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

SyntheticChildrenFrontEnd *
lldb_private::formatters::GNUstepNSDictionarySyntheticFrontEndCreator(
    CXXSyntheticChildren *synth, lldb::ValueObjectSP valobj_sp) {
  return new GNUstepNSDictionarySyntheticProvider(valobj_sp);
}