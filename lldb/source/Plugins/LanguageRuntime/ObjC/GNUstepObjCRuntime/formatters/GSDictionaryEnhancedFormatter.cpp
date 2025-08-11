//===-- GSDictionaryEnhancedFormatter.cpp - Enhanced GSDictionary formatter ===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements an enhanced formatter for GSDictionary that directly
// reads the GSIMapTable structure to access key-value pairs without fallback.
//
//===----------------------------------------------------------------------===//

#include "../GNUstepObjCRuntimeIntrospector.h"
#include "GNUstepDictionaryFormatters.h"
#include "GNUstepStringFormatters.h"
#include "GNUstepIdDispatcher.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Symbol/CompilerType.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/lldb-enumerations.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace {

// GSIMapTable structure from GNUstep's GSIMap.h
struct GSIMapBucket {
  uintptr_t nodeCount;  // Number of nodes in bucket
  void *firstNode;      // The linked list of nodes
};

struct GSIMapNode {
  void *nextInBucket;   // Linked list of bucket
  // Key and value follow, but their format depends on GSIMapKey/GSIMapVal union
};

struct GSIMapTable {
  void *zone;           // NSZone pointer
  uintptr_t nodeCount;  // Number of used nodes in map
  uintptr_t bucketCount; // Number of buckets in map
  GSIMapBucket *buckets; // Array of buckets
  void *freeNodes;      // List of unused nodes
  uintptr_t chunkCount; // Number of chunks in array
  void **nodeChunks;    // Chunks of allocated memory
};

} // namespace

class GSDictionaryEnhancedSyntheticProvider : public SyntheticChildrenFrontEnd {
public:
  GSDictionaryEnhancedSyntheticProvider(ValueObject &valobj)
      : SyntheticChildrenFrontEnd(valobj), m_process(nullptr),
        m_ptr_size(8), m_node_count(0), m_bucket_count(0),
        m_buckets_ptr(LLDB_INVALID_ADDRESS) {}

  ~GSDictionaryEnhancedSyntheticProvider() override = default;

  size_t CalculateNumChildren() override {
    if (!UpdateData())
      return 0;
    return m_node_count + 1; // +1 for count child
  }

  lldb::ValueObjectSP GetChildAtIndex(size_t idx) override;
  bool Update() override;
  bool MightHaveChildren() override { return true; }
  size_t GetIndexOfChildWithName(ConstString name) override;

private:
  bool UpdateData();
  bool ReadGSIMapTable();
  bool TraverseBuckets();
  std::string GetObjectSummary(lldb::addr_t obj_addr);
  
  struct KeyValuePair {
    lldb::addr_t key_addr;
    lldb::addr_t value_addr;
  };

  Process *m_process;
  uint32_t m_ptr_size;
  size_t m_node_count;
  size_t m_bucket_count;
  lldb::addr_t m_buckets_ptr;
  std::vector<KeyValuePair> m_pairs;
};

bool GSDictionaryEnhancedSyntheticProvider::UpdateData() {
  m_pairs.clear();
  
  if (!m_process)
    return false;
    
  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (!valobj_sp)
    return false;
    
  lldb::addr_t dict_addr = valobj_sp->GetPointerValue();
  if (dict_addr == 0 || dict_addr == LLDB_INVALID_ADDRESS)
    return false;
    
  // Check if this is actually a GSDictionary
  Status error;
  lldb::addr_t isa_addr = m_process->ReadPointerFromMemory(dict_addr, error);
  if (error.Fail())
    return false;
    
  // Read GSIMapTable structure
  return ReadGSIMapTable();
}

bool GSDictionaryEnhancedSyntheticProvider::ReadGSIMapTable() {
  ValueObjectSP valobj_sp = m_backend.GetSP();
  lldb::addr_t dict_addr = valobj_sp->GetPointerValue();
  
  // GSDictionary layout:
  // Class isa (8 bytes)
  // GSIMapTable_t map (inline structure)
  
  // The map structure starts right after the ISA pointer
  lldb::addr_t map_addr = dict_addr + m_ptr_size;
  
  Status error;
  
  // Read GSIMapTable fields
  // Skip zone pointer (8 bytes)
  lldb::addr_t field_addr = map_addr + m_ptr_size;
  
  // Read nodeCount
  m_node_count = m_process->ReadPointerFromMemory(field_addr, error);
  if (error.Fail())
    return false;
  field_addr += m_ptr_size;
  
  // Read bucketCount
  m_bucket_count = m_process->ReadPointerFromMemory(field_addr, error);
  if (error.Fail())
    return false;
  field_addr += m_ptr_size;
  
  // Read buckets pointer
  m_buckets_ptr = m_process->ReadPointerFromMemory(field_addr, error);
  if (error.Fail() || m_buckets_ptr == 0)
    return false;
    
  // Now traverse the buckets to find all key-value pairs
  return TraverseBuckets();
}

bool GSDictionaryEnhancedSyntheticProvider::TraverseBuckets() {
  if (m_bucket_count == 0 || m_buckets_ptr == 0)
    return false;
    
  Status error;
  
  // Iterate through all buckets
  for (size_t i = 0; i < m_bucket_count && m_pairs.size() < m_node_count; ++i) {
    // Each bucket is a GSIMapBucket structure
    lldb::addr_t bucket_addr = m_buckets_ptr + (i * 2 * m_ptr_size);
    
    // Read nodeCount for this bucket
    uintptr_t node_count = m_process->ReadPointerFromMemory(bucket_addr, error);
    if (error.Fail())
      continue;
      
    if (node_count == 0)
      continue;
      
    // Read firstNode pointer
    lldb::addr_t node_ptr = m_process->ReadPointerFromMemory(
        bucket_addr + m_ptr_size, error);
    if (error.Fail() || node_ptr == 0)
      continue;
      
    // Traverse the linked list of nodes in this bucket
    while (node_ptr != 0 && m_pairs.size() < m_node_count) {
      // GSIMapNode layout:
      // void *nextInBucket (8 bytes)
      // GSIMapKey key (8 bytes for object pointer)
      // GSIMapVal value (8 bytes for object pointer)
      
      // Read next pointer
      lldb::addr_t next_ptr = m_process->ReadPointerFromMemory(node_ptr, error);
      if (error.Fail())
        break;
        
      // Read key (object pointer)
      lldb::addr_t key_addr = m_process->ReadPointerFromMemory(
          node_ptr + m_ptr_size, error);
      if (error.Fail())
        break;
        
      // Read value (object pointer)
      lldb::addr_t value_addr = m_process->ReadPointerFromMemory(
          node_ptr + 2 * m_ptr_size, error);
      if (error.Fail())
        break;
        
      // Store the key-value pair
      m_pairs.push_back({key_addr, value_addr});
      
      // Move to next node
      node_ptr = next_ptr;
    }
  }
  
  return !m_pairs.empty();
}

bool GSDictionaryEnhancedSyntheticProvider::Update() {
  m_process = m_backend.GetProcessSP().get();
  if (!m_process)
    return false;
    
  m_ptr_size = m_process->GetAddressByteSize();
  return UpdateData();
}

lldb::ValueObjectSP GSDictionaryEnhancedSyntheticProvider::GetChildAtIndex(size_t idx) {
  if (!m_process)
    return nullptr;
    
  // First child is the count
  if (idx == 0) {
    StreamString name;
    name.Printf("count");
    
    DataExtractor data(&m_node_count, sizeof(m_node_count),
                      m_process->GetByteOrder(),
                      m_process->GetAddressByteSize());
    
    return CreateValueObjectFromData(name.GetString(), data,
                                    m_backend.GetExecutionContextRef(),
                                    m_backend.GetCompilerType());
  }
  
  // Adjust index for key-value pairs
  idx--;
  if (idx >= m_pairs.size())
    return nullptr;
    
  const auto &pair = m_pairs[idx];
  
  // Create a synthetic child showing key = value
  StreamString name;
  std::string key_summary = GetObjectSummary(pair.key_addr);
  
  // Use key summary as child name if available
  if (!key_summary.empty() && key_summary != "<invalid>") {
    // Clean up the key summary for use as a name
    if (key_summary.front() == '"' && key_summary.back() == '"' && 
        key_summary.length() > 1) {
      key_summary = key_summary.substr(1, key_summary.length() - 2);
    }
    name.Printf("%s", key_summary.c_str());
  } else {
    name.Printf("[%zu]", idx);
  }
  
  // Create value object for the value
  ExecutionContext exe_ctx(m_backend.GetExecutionContextRef());
  CompilerType id_type = m_process->GetTarget()
                              .GetScratchClangASTContext()
                              ->GetBasicType(eBasicTypeObjCID);
  
  return ValueObject::CreateValueObjectFromAddress(
      name.GetString(), pair.value_addr, exe_ctx, id_type);
}

std::string GSDictionaryEnhancedSyntheticProvider::GetObjectSummary(lldb::addr_t obj_addr) {
  if (!m_process || obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS)
    return "<nil>";
    
  // Handle tagged pointers first
  if ((obj_addr & 0x7) == 4) { // GSTinyString
    // Decode GSTinyString
    int length = (obj_addr >> 3) & 0x1F;
    if (length > 0 && length <= 8) {
      std::string result;
      for (int i = 0; i < length; i++) {
        uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
        char c = (obj_addr & mask) >> (57 - (i * 7));
        if (c >= 0x20 && c <= 0x7e) {
          result += c;
        } else {
          break;
        }
      }
      if (!result.empty()) {
        return "\"" + result + "\"";
      }
    }
  }
  
  // For regular objects, try to get the string representation
  Status error;
  lldb::addr_t isa_addr = m_process->ReadPointerFromMemory(obj_addr, error);
  if (error.Fail())
    return "<invalid>";
    
  // Try to use the ID dispatcher to get a proper summary
  ExecutionContext exe_ctx(m_backend.GetExecutionContextRef());
  CompilerType id_type = m_process->GetTarget()
                              .GetScratchClangASTContext()
                              ->GetBasicType(eBasicTypeObjCID);
  
  ValueObjectSP valobj_sp = ValueObject::CreateValueObjectFromAddress(
      "temp", obj_addr, exe_ctx, id_type);
      
  if (valobj_sp) {
    StreamString stream;
    TypeSummaryOptions options;
    
    // Try to get summary using GNUstep formatters
    if (GNUstepIdDispatcherSummaryProvider(*valobj_sp, stream, options)) {
      std::string summary = stream.GetString();
      if (!summary.empty())
        return summary;
    }
    
    // Fallback to basic string extraction for GSCInlineString
    const char *class_name = valobj_sp->GetObjectDescription();
    if (class_name && std::string(class_name).find("String") != std::string::npos) {
      // Try to extract string content directly
      GNUstepNSStringSummaryProvider string_provider;
      if (string_provider.GetSummaryStatic(*valobj_sp, stream, options)) {
        std::string summary = stream.GetString();
        if (!summary.empty())
          return summary;
      }
    }
  }
  
  return "<object>";
}

size_t GSDictionaryEnhancedSyntheticProvider::GetIndexOfChildWithName(ConstString name) {
  if (name == ConstString("count"))
    return 0;
    
  // Search for matching key
  for (size_t i = 0; i < m_pairs.size(); ++i) {
    std::string key_summary = GetObjectSummary(m_pairs[i].key_addr);
    // Clean up the key summary
    if (key_summary.front() == '"' && key_summary.back() == '"' && 
        key_summary.length() > 1) {
      key_summary = key_summary.substr(1, key_summary.length() - 2);
    }
    if (key_summary == name.GetStringRef())
      return i + 1; // +1 for count
  }
  
  return UINT32_MAX;
}

// Factory function
SyntheticChildrenFrontEnd *
GSDictionaryEnhancedSyntheticFrontEndCreator(CXXSyntheticChildren *,
                                             lldb::ValueObjectSP valobj_sp) {
  if (!valobj_sp)
    return nullptr;
  return new GSDictionaryEnhancedSyntheticProvider(*valobj_sp);
}