//===-- GNUstepNSDictionary.cpp ----------------------------------------===//
//
// GNUstep NSDictionary formatters for LLDB
// 
// This implementation uses a hybrid approach:
// 1. Try expression evaluation first (most reliable, uses runtime)
// 2. Fall back to direct memory reading if expressions fail
//
//===----------------------------------------------------------------------===//

#include "GNUstepNSDictionary.h"
#include "GNUstepObjCRuntime.h"
#include "GNUstepCollectionUtilities.h"
#include "GNUstepUtilities.h"
#include "GNUstepRuntimeAPI.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

#include "clang/AST/DeclCXX.h"

#include "lldb/ValueObject/ValueObject.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/DataFormatters/StringPrinter.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/Expression/UserExpression.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

#include <algorithm>
#include <vector>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::gnustep_collection_utils;


namespace lldb_private {
namespace formatters {


// Helper function to extract key-value pair summaries for preview
bool ExtractKeyValueSummariesForPreview(addr_t dict_ptr, ProcessSP process_sp, 
                                       size_t max_pairs, 
                                       std::vector<std::pair<std::string, std::string>>& key_value_summaries,
                                       Log* log) {
  if (!process_sp || !dict_ptr) return false;
  
  Status error;
  size_t ptr_size = process_sp->GetAddressByteSize();
  uint64_t bucket_count = 0;
  addr_t buckets_ptr = 0;
  bool success = false;
  
  // Create temporary runtime API instance for offset discovery
  auto runtime_api = GNUstepRuntimeAPI::Create(process_sp.get());
  
  if (runtime_api) {
    // Use runtime API to discover offsets dynamically
    LLDB_LOG(log, "ExtractKeyValueSummariesForPreview: Using runtime API for offset discovery");
    
    auto class_info = runtime_api->GetObjectClassInfo(dict_ptr);
    if (class_info) {
      // Find the _map ivar in the dictionary class
      ptrdiff_t map_offset = -1;
      for (const auto& ivar : class_info.value.ivars) {
        if (ivar.name == "_map" || ivar.name == "map") {
          map_offset = ivar.offset;
          LLDB_LOG(log, "ExtractKeyValueSummariesForPreview: Found _map ivar at offset {0}", map_offset);
          break;
        }
      }
      
      if (map_offset >= 0) {
        addr_t map_addr = dict_ptr + map_offset;
        
        // GSIMapTable has well-known layout: nodeCount, bucketCount, buckets
        bucket_count = process_sp->ReadUnsignedIntegerFromMemory(
            map_addr + (2 * ptr_size), ptr_size, 0, error);
        if (!error.Fail() && bucket_count <= 10000) {
          buckets_ptr = process_sp->ReadPointerFromMemory(
              map_addr + (3 * ptr_size), error);
          if (!error.Fail() && buckets_ptr) {
            LLDB_LOG(log, "ExtractKeyValueSummariesForPreview: Runtime discovery successful");
            success = true;
          }
        }
      }
    }
    
    if (!success) {
      LLDB_LOG(log, "ExtractKeyValueSummariesForPreview: Runtime discovery failed, falling back");
    }
  }
  
  // Fallback to hardcoded offsets if runtime discovery failed
  if (!success) {
    LLDB_LOG(log, "ExtractKeyValueSummariesForPreview: Using hardcoded offsets");
    addr_t map_addr = dict_ptr + ptr_size;
    
    bucket_count = process_sp->ReadUnsignedIntegerFromMemory(
        map_addr + (2 * ptr_size), ptr_size, 0, error);
    if (error.Fail() || bucket_count > 10000) {
      return false;
    }
      
    buckets_ptr = process_sp->ReadPointerFromMemory(
        map_addr + (3 * ptr_size), error);
    if (error.Fail() || !buckets_ptr) {
      return false;
    }
  }
  
  // Traverse buckets to extract key-value pair summaries
  for (uint64_t bucket_idx = 0; 
       bucket_idx < bucket_count && key_value_summaries.size() < max_pairs;
       bucket_idx++) {
    
    addr_t bucket_addr = buckets_ptr + (bucket_idx * 2 * ptr_size);
    
    uint64_t bucket_node_count = process_sp->ReadUnsignedIntegerFromMemory(
        bucket_addr, ptr_size, 0, error);
    if (error.Fail() || bucket_node_count == 0)
      continue;
      
    addr_t node_ptr = process_sp->ReadPointerFromMemory(
        bucket_addr + ptr_size, error);
    if (error.Fail() || !node_ptr)
      continue;
      
    // Traverse linked list of nodes
    uint64_t nodes_read = 0;
    while (node_ptr && nodes_read < bucket_node_count && 
           key_value_summaries.size() < max_pairs) {
      
      addr_t next_ptr = process_sp->ReadPointerFromMemory(node_ptr, error);
      if (error.Fail())
        break;
        
      // GSIMapNode WITH VALUES layout:
      // - nextInBucket at offset 0
      // - key at offset ptr_size
      // - value at offset 2*ptr_size
      addr_t key_addr = process_sp->ReadPointerFromMemory(
          node_ptr + ptr_size, error);
      if (error.Fail() || !key_addr) {
        node_ptr = next_ptr;
        nodes_read++;
        continue;
      }
        
      addr_t value_addr = process_sp->ReadPointerFromMemory(
          node_ptr + (2 * ptr_size), error);
      if (error.Fail() || !value_addr) {
        node_ptr = next_ptr;
        nodes_read++;
        continue;
      }
      
      // Get summaries for both key and value using shared utilities
      TargetSP target_sp = process_sp->GetTarget().shared_from_this();
      std::string key_summary = GetObjectSummary(key_addr, process_sp, target_sp, log);
      std::string value_summary = GetObjectSummary(value_addr, process_sp, target_sp, log);
      
      if (!key_summary.empty() && !value_summary.empty()) {
        key_value_summaries.push_back(std::make_pair(key_summary, value_summary));
      }
      
      node_ptr = next_ptr;
      nodes_read++;
      
      if (nodes_read > 1000) break; // Safety
    }
  }
  
  return !key_value_summaries.empty();
}

// Summary provider implementation
bool GNUstepNSDictionarySummaryProvider(ValueObject &valobj, Stream &stream,
                                        const TypeSummaryOptions &options) {
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;
    
  // Get object address directly
  addr_t valobj_addr = valobj.GetValueAsUnsigned(0);
  if (!valobj_addr || valobj_addr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }

  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "GNUstepNSDictionarySummaryProvider: Reading memory for address 0x{0:x}", valobj_addr);
  
  size_t ptr_size = process_sp->GetAddressByteSize();
  Status error;
  
  // Try to use runtime API for dynamic offset discovery
  auto runtime_api = GNUstepRuntimeAPI::Create(process_sp.get());
  addr_t map_addr;
  ptrdiff_t map_offset = ptr_size; // Default fallback
  
  if (runtime_api) {
    auto class_info = runtime_api->GetObjectClassInfo(valobj_addr);
    if (class_info) {
      // Find the _map ivar dynamically
      for (const auto& ivar : class_info.value.ivars) {
        if (ivar.name == "_map" || ivar.name == "map") {
          map_offset = ivar.offset;
          LLDB_LOG(log, "GNUstepNSDictionarySummaryProvider: Found _map ivar at offset {0}", map_offset);
          break;
        }
      }
    } else {
      LLDB_LOG(log, "GNUstepNSDictionarySummaryProvider: Could not get class info, using default offset");
    }
  } else {
    LLDB_LOG(log, "GNUstepNSDictionarySummaryProvider: No runtime API available, using default offset");
  }
  
  map_addr = valobj_addr + map_offset;
  addr_t node_count_addr = map_addr + ptr_size;
  
  uint64_t count = process_sp->ReadUnsignedIntegerFromMemory(
      node_count_addr, ptr_size, 0, error);
      
  if (error.Fail()) {
    return false;
  }
  
  // Get preview of first 3 key-value pairs
  std::vector<std::pair<std::string, std::string>> key_value_summaries;
  bool success = ExtractKeyValueSummariesForPreview(valobj_addr, process_sp, 3, key_value_summaries, log);
  
  if (success && !key_value_summaries.empty()) {
    stream.Printf("{");
    for (size_t i = 0; i < key_value_summaries.size(); i++) {
      if (i > 0) stream.Printf(", ");
      stream.Printf("%s: %s", 
                   key_value_summaries[i].first.c_str(),
                   key_value_summaries[i].second.c_str());
    }
    if (count > 3) {
      stream.Printf(", ...");
    }
    stream.Printf("} ");
  }
  
  stream.Printf("(%" PRIu64 " pair%s)", count, count == 1 ? "" : "s");
  return true;
}

// Main synthetic provider implementation
GNUstepNSDictionarySyntheticProvider::GNUstepNSDictionarySyntheticProvider(
    lldb::ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp) {
  // Initialize ObjCBuiltinIdTy following the working array pattern
  if (valobj_sp) {
    TargetSP target_sp = valobj_sp->GetExecutionContextRef().GetTargetSP();
    if (target_sp) {
      TypeSystemClangSP scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(*target_sp);
      if (scratch_ts_sp) {
        m_objc_id_type = CompilerType(
            scratch_ts_sp->weak_from_this(),
            scratch_ts_sp->getASTContext().ObjCBuiltinIdTy.getAsOpaquePtr());
      }
    }
    
    // Initialize runtime API for dynamic offset discovery
    ProcessSP process_sp = valobj_sp->GetProcessSP();
    if (process_sp) {
      m_runtime_api = GNUstepRuntimeAPI::Create(process_sp.get());
    }
    
    Update();
  }
}

llvm::Expected<uint32_t> 
GNUstepNSDictionarySyntheticProvider::CalculateNumChildren() {
  // Return number of key-value pairs * 2 (separate children for keys and values)
  // This matches the working array pattern: direct access to individual elements
  return m_key_value_pairs.size() * 2;
}

lldb::ValueObjectSP 
GNUstepNSDictionarySyntheticProvider::GetChildAtIndex(uint32_t idx) {
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "GNUstepNSDictionarySyntheticProvider::GetChildAtIndex idx={0}, total_pairs={1}", idx, m_key_value_pairs.size());
  
  // Calculate pair index and whether this is key (even) or value (odd)
  uint32_t pair_idx = idx / 2;
  bool is_key = (idx % 2) == 0;
  
  if (pair_idx >= m_key_value_pairs.size()) {
    LLDB_LOG(log, "Invalid pair index {0} >= {1}", pair_idx, m_key_value_pairs.size());
    return lldb::ValueObjectSP();
  }
  
  addr_t target_addr = is_key ? m_key_value_pairs[pair_idx].first : m_key_value_pairs[pair_idx].second;
  
  LLDB_LOG(log, "Getting dictionary {0} at pair {1}, addr=0x{2:x}", 
           is_key ? "key" : "value", pair_idx, target_addr);
  
  // Validate address using the same checks as the working array implementation
  if (!target_addr || target_addr == LLDB_INVALID_ADDRESS) {
    LLDB_LOG(log, "Invalid {0} address at pair {1}", is_key ? "key" : "value", pair_idx);
    return lldb::ValueObjectSP();
  }
  
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  TargetSP target_sp = m_exe_ctx_ref.GetTargetSP();
  if (!process_sp || !target_sp)
    return lldb::ValueObjectSP();
  
  // CRITICAL FIX: Use the working array pattern - create ValueObject directly from address
  // This ensures proper summary provider application, just like arrays do
  
  if (!m_objc_id_type.IsValid()) {
    LLDB_LOG(log, "GetChildAtIndex: m_objc_id_type is NOT VALID - this is the problem!");
    return lldb::ValueObjectSP();
  }
  
  // Create child name in dictionary format: [0] key, [0] value, [1] key, [1] value, etc.
  StreamString child_name;
  child_name.Printf("[%" PRIu32 "] %s", pair_idx, is_key ? "key" : "value");
  
  // Create temporary data buffer containing the object pointer (following array pattern)
  size_t ptr_size = process_sp->GetAddressByteSize();
  DataBufferSP data_buffer_sp(new DataBufferHeap(ptr_size, 0));
  if (!data_buffer_sp) {
    LLDB_LOG(log, "Failed to create DataBufferHeap");
    return lldb::ValueObjectSP();
  }
  
  uint8_t *data_ptr = const_cast<uint8_t*>(data_buffer_sp->GetBytes());
  if (!data_ptr) {
    LLDB_LOG(log, "Failed to get data buffer bytes");
    return lldb::ValueObjectSP();
  }
  
  // Store the object pointer in our temporary buffer (following array pattern)
  DataExtractor data_extractor(data_buffer_sp, process_sp->GetByteOrder(), ptr_size);
  if (ptr_size == 8) {
    *reinterpret_cast<uint64_t*>(data_ptr) = target_addr;
    LLDB_LOG(log, "Stored 64-bit address 0x{0:x} in buffer for {1}", target_addr, is_key ? "key" : "value");
  } else {
    *reinterpret_cast<uint32_t*>(data_ptr) = static_cast<uint32_t>(target_addr);
    LLDB_LOG(log, "Stored 32-bit address 0x{0:x} in buffer for {1}", static_cast<uint32_t>(target_addr), is_key ? "key" : "value");
  }
  
  // Create ValueObject from our data buffer using objc id type (following array pattern)
  ValueObjectSP child = ValueObject::CreateValueObjectFromData(
      child_name.GetString(),
      data_extractor,
      m_exe_ctx_ref,
      m_objc_id_type
  );
  
  if (child && child.get()) {
    LLDB_LOG(log, "SUCCESS - Created ValueObject for {0} at pair {1}", is_key ? "key" : "value", pair_idx);
    
    // Force summary provider application (following array pattern)
    const char* summary = child->GetSummaryAsCString();
    if (summary) {
      LLDB_LOG(log, "Dictionary {0} summary: '{1}'", is_key ? "key" : "value", summary);
    } else {
      LLDB_LOG(log, "Dictionary {0} has no summary", is_key ? "key" : "value");
    }
    
    child->SetFormat(lldb::eFormatDefault);
    return child;
  } else {
    LLDB_LOG(log, "FAILED to create ValueObject for {0} at pair {1}", is_key ? "key" : "value", pair_idx);
  }
  
  return lldb::ValueObjectSP();
}

lldb::ChildCacheState
GNUstepNSDictionarySyntheticProvider::Update() {
  m_key_value_pairs.clear();
  m_has_valid_data = false;
  
  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (!valobj_sp)
    return lldb::ChildCacheState::eRefetch;
    
  m_exe_ctx_ref = valobj_sp->GetExecutionContextRef();
  ProcessSP process_sp = valobj_sp->GetProcessSP();
  if (!process_sp)
    return lldb::ChildCacheState::eRefetch;
    
  m_ptr_size = process_sp->GetAddressByteSize();
  m_dict_ptr = valobj_sp->GetValueAsUnsigned(0);
  
  if (!m_dict_ptr)
    return lldb::ChildCacheState::eRefetch;
    
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "GNUstepNSDictionarySyntheticProvider: Updating for address 0x{0:x}", m_dict_ptr);
  
  // Try to discover offsets using runtime API first
  if (!DiscoverOffsets()) {
    Log *log = GetLog(LLDBLog::DataFormatters);
    LLDB_LOG(log, "GNUstepNSDictionarySyntheticProvider::Update: Failed to discover offsets, using fallback");
  }
  
  // Skip expression evaluation - go directly to memory reading to avoid selector issues
  bool success = ExtractPairsFromMemory();
  
  m_has_valid_data = success;
  return success ? lldb::ChildCacheState::eReuse : lldb::ChildCacheState::eRefetch;
}

bool
GNUstepNSDictionarySyntheticProvider::ExtractPairsUsingExpression(StackFrameSP frame_sp) {
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "Trying to extract dictionary pairs using expression evaluation");
  
  ExecutionContext exe_ctx(frame_sp);
  
  // First get the count
  StreamString count_expr;
  count_expr.Printf("(NSUInteger)[(id)0x%" PRIx64 " count]", m_dict_ptr);
  
  ValueObjectSP count_result_sp;
  EvaluateExpressionOptions eval_options;
  eval_options.SetIgnoreBreakpoints(true);
  eval_options.SetUnwindOnError(true);
  eval_options.SetTryAllThreads(false);
  eval_options.SetTimeout(std::chrono::milliseconds(500));
  
  ExpressionResults expr_result = UserExpression::Evaluate(
      exe_ctx, eval_options, count_expr.GetString(), "", count_result_sp, nullptr);
  
  if (expr_result != eExpressionCompleted || !count_result_sp) {
    return false;
  }
  
  uint64_t count = count_result_sp->GetValueAsUnsigned(0);
  if (count == 0 || count > 100000) // Sanity check
    return true; // Empty dictionary is valid
    
  LLDB_LOG(log, "Dictionary has {0} pairs", count);
  
  // Get all keys using allKeys
  StreamString all_keys_expr;
  all_keys_expr.Printf("(id)[(id)0x%" PRIx64 " allKeys]", m_dict_ptr);
  
  ValueObjectSP keys_array_result_sp;
  expr_result = UserExpression::Evaluate(
      exe_ctx, eval_options, all_keys_expr.GetString(), "", keys_array_result_sp, nullptr);
  
  if (expr_result != eExpressionCompleted || !keys_array_result_sp) {
    return false;
  }
  
  addr_t keys_array_addr = keys_array_result_sp->GetValueAsUnsigned(0);
  if (!keys_array_addr)
    return false;
    
  // Now extract key-value pairs
  for (uint64_t i = 0; i < count && i < 1000; i++) { // Safety limit
    // Get key
    StreamString key_expr;
    key_expr.Printf("(id)[(id)0x%" PRIx64 " objectAtIndex:%" PRIu64 "]", 
                   keys_array_addr, i);
    
    ValueObjectSP key_result_sp;
    expr_result = UserExpression::Evaluate(
        exe_ctx, eval_options, key_expr.GetString(), "", key_result_sp, nullptr);
    
    if (expr_result != eExpressionCompleted || !key_result_sp) {
      continue;
    }
    
    addr_t key_addr = key_result_sp->GetValueAsUnsigned(0);
    if (!key_addr) continue;
    
    // Get value for this key
    StreamString value_expr;
    value_expr.Printf("(id)[(id)0x%" PRIx64 " objectForKey:(id)0x%" PRIx64 "]", 
                     m_dict_ptr, key_addr);
    
    ValueObjectSP value_result_sp;
    expr_result = UserExpression::Evaluate(
        exe_ctx, eval_options, value_expr.GetString(), "", value_result_sp, nullptr);
    
    if (expr_result == eExpressionCompleted && value_result_sp) {
      addr_t value_addr = value_result_sp->GetValueAsUnsigned(0);
      if (value_addr) {
        m_key_value_pairs.push_back(std::make_pair(key_addr, value_addr));
      }
    }
  }
  
  LLDB_LOG(log, "Extracted {0} pairs using expressions", m_key_value_pairs.size());
  return !m_key_value_pairs.empty();
}

bool
GNUstepNSDictionarySyntheticProvider::DiscoverOffsets() {
  if (m_offsets_cached) {
    return true;
  }
  
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "DiscoverOffsets: Starting offset discovery for dictionary at 0x{0:x}", m_dict_ptr);
  
  if (!m_runtime_api || !m_dict_ptr) {
    LLDB_LOG(log, "DiscoverOffsets: No runtime API or invalid dictionary address");
    return false;
  }
  
  // Get class information for this dictionary object
  auto class_info = m_runtime_api->GetObjectClassInfo(m_dict_ptr);
  if (!class_info) {
    LLDB_LOG(log, "DiscoverOffsets: Failed to get class info: {0}", class_info.error_message);
    return false;
  }
  
  LLDB_LOG(log, "DiscoverOffsets: Dictionary class is '{0}' with {1} ivars", 
           class_info.value.name, class_info.value.ivars.size());
  
  // Find the _map ivar in the dictionary class
  for (const auto& ivar : class_info.value.ivars) {
    LLDB_LOG(log, "DiscoverOffsets: Examining ivar '{0}' at offset {1}", ivar.name, ivar.offset);
    
    if (ivar.name == "_map" || ivar.name == "map") {
      m_map_offset = ivar.offset;
      LLDB_LOG(log, "DiscoverOffsets: Found _map ivar at offset {0}", m_map_offset);
      break;
    }
  }
  
  if (m_map_offset < 0) {
    LLDB_LOG(log, "DiscoverOffsets: Could not find _map ivar, using default offset");
    m_map_offset = m_ptr_size; // Default fallback
  }
  
  // GSIMapTable structure offsets are well-defined in the GNUstep source:
  // struct GSIMapTable {
  //   NSZone *zone;           // offset 0
  //   uintptr_t nodeCount;    // offset ptr_size
  //   uintptr_t bucketCount;  // offset 2*ptr_size  
  //   GSIMapBucket *buckets;  // offset 3*ptr_size
  //   // ... other fields
  // }
  // These are not dynamic - they're part of the GSIMapTable struct definition
  m_nodeCount_offset = m_ptr_size;
  m_bucketCount_offset = 2 * m_ptr_size;
  m_buckets_offset = 3 * m_ptr_size;
  
  m_offsets_cached = true;
  
  LLDB_LOG(log, "DiscoverOffsets: Cached offsets - map: {0}, nodeCount: {1}, bucketCount: {2}, buckets: {3}",
           m_map_offset, m_nodeCount_offset, m_bucketCount_offset, m_buckets_offset);
  
  return true;
}

bool
GNUstepNSDictionarySyntheticProvider::ExtractPairsFromMemory() {
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp)
    return false;
    
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  // Try to use runtime API for class introspection
  if (m_runtime_api) {
    auto class_info = m_runtime_api->GetObjectClassInfo(m_dict_ptr);
    if (class_info) {
      LLDB_LOG(log, "ExtractPairsFromMemory: Dictionary class is '{0}', proceeding with memory extraction", 
               class_info.value.name);
    } else {
      LLDB_LOG(log, "ExtractPairsFromMemory: Failed to get class info: {0}", class_info.error_message);
    }
  } else {
    LLDB_LOG(log, "ExtractPairsFromMemory: No runtime API available, proceeding with fallback");
  }
  
  // Always fall back to direct memory reading since it's reliable and we now
  // have runtime-discovered offsets to make it more robust
  return ExtractPairsFromMemoryDirect();
}

bool
GNUstepNSDictionarySyntheticProvider::ExtractPairsFromMemoryDirect() {
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp)
    return false;
    
  Status error;
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  // Use discovered offsets or fall back to hardcoded ones
  ptrdiff_t map_offset = (m_map_offset >= 0) ? m_map_offset : m_ptr_size;
  addr_t map_addr = m_dict_ptr + map_offset;
  
  // GSIMapTable layout - these are part of the GSIMapTable struct itself
  // and are consistent regardless of the dictionary class layout
  ptrdiff_t nodeCount_in_map_offset = (m_nodeCount_offset >= 0) ? m_nodeCount_offset : m_ptr_size;
  ptrdiff_t bucketCount_in_map_offset = (m_bucketCount_offset >= 0) ? m_bucketCount_offset : (2 * m_ptr_size);
  ptrdiff_t buckets_in_map_offset = (m_buckets_offset >= 0) ? m_buckets_offset : (3 * m_ptr_size);
  
  uint64_t node_count = process_sp->ReadUnsignedIntegerFromMemory(
      map_addr + nodeCount_in_map_offset, m_ptr_size, 0, error);
  if (error.Fail() || node_count > 100000) {
    LLDB_LOG(log, "Failed to read nodeCount or invalid count: {0}", node_count);
    return false;
  }
    
  uint64_t bucket_count = process_sp->ReadUnsignedIntegerFromMemory(
      map_addr + bucketCount_in_map_offset, m_ptr_size, 0, error);
  if (error.Fail() || bucket_count > 10000) {
    LLDB_LOG(log, "Failed to read bucketCount or invalid count: {0}", bucket_count);
    return false;
  }
    
  addr_t buckets_ptr = process_sp->ReadPointerFromMemory(
      map_addr + buckets_in_map_offset, error);
  if (error.Fail() || !buckets_ptr) {
    LLDB_LOG(log, "Failed to read buckets pointer");
    return false;
  }
  
  if (m_offsets_cached) {
    LLDB_LOG(log, "ExtractPairsFromMemoryDirect: Using runtime-discovered offsets: map_offset={0}, nodeCount={1}, bucketCount={2}, buckets={3}",
             map_offset, nodeCount_in_map_offset, bucketCount_in_map_offset, buckets_in_map_offset);
  } else {
    LLDB_LOG(log, "ExtractPairsFromMemoryDirect: Using fallback hardcoded offsets");
  }
    
  LLDB_LOG(log, "GSIMapTable: nodeCount={0}, bucketCount={1}, buckets=0x{2:x}",
           node_count, bucket_count, buckets_ptr);
  
  // Iterate through buckets to find nodes
  for (uint64_t bucket_idx = 0; 
       bucket_idx < bucket_count && m_key_value_pairs.size() < node_count;
       bucket_idx++) {
    
    // GSIMapBucket layout:
    // - nodeCount at offset 0 (uintptr_t)
    // - firstNode at offset m_ptr_size (pointer)
    addr_t bucket_addr = buckets_ptr + (bucket_idx * 2 * m_ptr_size);
    
    uint64_t bucket_node_count = process_sp->ReadUnsignedIntegerFromMemory(
        bucket_addr, m_ptr_size, 0, error);
    if (error.Fail() || bucket_node_count == 0)
      continue;
      
    addr_t node_ptr = process_sp->ReadPointerFromMemory(
        bucket_addr + m_ptr_size, error);
    if (error.Fail() || !node_ptr)
      continue;
      
    // Traverse linked list of nodes
    uint64_t nodes_read = 0;
    while (node_ptr && nodes_read < bucket_node_count && 
           m_key_value_pairs.size() < node_count) {
      
      // GSIMapNode layout for dictionaries (GSI_MAP_HAS_VALUE = 1):
      // - nextInBucket at offset 0 (pointer)
      // - key at offset m_ptr_size (the key object pointer)
      // - value at offset 2*m_ptr_size (the value object pointer)
      addr_t next_ptr = process_sp->ReadPointerFromMemory(node_ptr, error);
      if (error.Fail())
        break;
        
      addr_t key_addr = process_sp->ReadPointerFromMemory(
          node_ptr + m_ptr_size, error);
      if (error.Fail() || !key_addr) {
        node_ptr = next_ptr;
        nodes_read++;
        continue;
      }
      
      addr_t value_addr = process_sp->ReadPointerFromMemory(
          node_ptr + (2 * m_ptr_size), error);
      if (error.Fail() || !value_addr) {
        node_ptr = next_ptr;
        nodes_read++;
        continue;
      }
      
      // Check if key or value are tagged pointers
      if (IsTaggedPointer(key_addr)) {
        std::string class_name = GetTaggedPointerClassName(key_addr, log);
        LLDB_LOG(log, "Added tagged pointer key: {0} (0x{1:x})", class_name, key_addr);
        
        // For GSTinyString, also try to decode the string content
        if (class_name == "GSTinyString") {
          std::string decoded_string;
          if (DecodeGSTinyString(key_addr, decoded_string, log)) {
            LLDB_LOG(log, "  Decoded key string content: \"{0}\"", decoded_string);
          }
        }
      }
      
      if (IsTaggedPointer(value_addr)) {
        std::string class_name = GetTaggedPointerClassName(value_addr, log);
        LLDB_LOG(log, "Added tagged pointer value: {0} (0x{1:x})", class_name, value_addr);
        
        // For GSTinyString, also try to decode the string content
        if (class_name == "GSTinyString") {
          std::string decoded_string;
          if (DecodeGSTinyString(value_addr, decoded_string, log)) {
            LLDB_LOG(log, "  Decoded value string content: \"{0}\"", decoded_string);
          }
        }
      }
      
      m_key_value_pairs.push_back(std::make_pair(key_addr, value_addr));
      LLDB_LOG(log, "Added key-value pair: key=0x{0:x}, value=0x{1:x}", key_addr, value_addr);
      
      node_ptr = next_ptr;
      nodes_read++;
      
      if (nodes_read > 1000) // Safety
        break;
    }
  }
  
  LLDB_LOG(log, "Extracted {0} key-value pairs from memory", m_key_value_pairs.size());
  return true;
}

size_t 
GNUstepNSDictionarySyntheticProvider::GetIndexOfChildWithName(ConstString name) {
  // Handle dictionary index notation [n] key or [n] value  
  std::string name_str = name.GetStringRef().str();
  
  // Match patterns like "[0] key" or "[0] value"
  if (name_str.size() >= 6) { // Minimum: "[0] key"
    size_t bracket_close = name_str.find(']');
    if (bracket_close != std::string::npos && name_str[0] == '[') {
      std::string index_str = name_str.substr(1, bracket_close - 1);
      size_t pair_idx = 0;
      if (sscanf(index_str.c_str(), "%zu", &pair_idx) == 1) {
        if (pair_idx < m_key_value_pairs.size()) {
          // Check if it's asking for key or value
          size_t space_pos = name_str.find(' ', bracket_close);
          if (space_pos != std::string::npos) {
            std::string suffix = name_str.substr(space_pos + 1);
            if (suffix == "key") {
              return pair_idx * 2;     // Key is at even indices: 0, 2, 4...
            } else if (suffix == "value") {
              return pair_idx * 2 + 1; // Value is at odd indices: 1, 3, 5...
            }
          }
        }
      }
    }
  }
  
  return UINT32_MAX;
}

// Factory function
SyntheticChildrenFrontEnd *
GNUstepNSDictionarySyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                          lldb::ValueObjectSP valobj_sp) {
  ProcessSP process_sp(valobj_sp->GetProcessSP());
  if (!process_sp)
    return nullptr;
    
  ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
  if (!runtime)
    return nullptr;
    
  CompilerType valobj_type(valobj_sp->GetCompilerType());
  Flags flags(valobj_type.GetTypeInfo());
  
  if (flags.IsClear(eTypeIsPointer)) {
    Status error;
    valobj_sp = valobj_sp->AddressOf(error);
    if (error.Fail() || !valobj_sp)
      return nullptr;
  }
  
  ObjCLanguageRuntime::ClassDescriptorSP descriptor(
      runtime->GetClassDescriptor(*valobj_sp));
      
  if (!descriptor || !descriptor->IsValid())
    return nullptr;
    
  ConstString class_name = descriptor->GetClassName();
  
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "GNUstepNSDictionarySyntheticFrontEndCreator for class {0}", class_name);
  
  // Handle any class that looks like a dictionary
  if (class_name.GetStringRef().contains("Dictionary")) {
    return new GNUstepNSDictionarySyntheticProvider(valobj_sp);
  }
  
  return nullptr;
}

} // namespace formatters
} // namespace lldb_private