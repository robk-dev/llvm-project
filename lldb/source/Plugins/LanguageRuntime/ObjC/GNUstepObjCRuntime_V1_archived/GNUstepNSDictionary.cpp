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

// Create special NSPair type for dictionary key-value pairs (following Apple's approach)
// This ensures proper synthetic children and allows nested expansion to work
static CompilerType GetGNUstepNSPairType(TargetSP target_sp) {
  CompilerType compiler_type;
  TypeSystemClangSP scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(*target_sp);

  if (!scratch_ts_sp)
    return compiler_type;

  static constexpr llvm::StringLiteral g_gnustep_autogen_nspair("__gnustep_autogen_nspair");

  compiler_type = scratch_ts_sp->GetTypeForIdentifier<clang::CXXRecordDecl>(g_gnustep_autogen_nspair);

  if (!compiler_type) {
    compiler_type = scratch_ts_sp->CreateRecordType(
        nullptr, OptionalClangModuleID(), lldb::eAccessPublic,
        g_gnustep_autogen_nspair, llvm::to_underlying(clang::TagTypeKind::Struct),
        lldb::eLanguageTypeC);

    if (compiler_type) {
      TypeSystemClang::StartTagDeclarationDefinition(compiler_type);
      CompilerType id_compiler_type = scratch_ts_sp->GetBasicType(eBasicTypeObjCID);
      // Create 'key' and 'value' fields - these will become synthetic children
      TypeSystemClang::AddFieldToRecordType(
          compiler_type, "key", id_compiler_type, lldb::eAccessPublic, 0);
      TypeSystemClang::AddFieldToRecordType(
          compiler_type, "value", id_compiler_type, lldb::eAccessPublic, 0);
      TypeSystemClang::CompleteTagDeclarationDefinition(compiler_type);
    }
  }
  return compiler_type;
}

// Helper function to extract key-value pair summaries for preview
bool ExtractKeyValueSummariesForPreview(addr_t dict_ptr, ProcessSP process_sp, 
                                       size_t max_pairs, 
                                       std::vector<std::pair<std::string, std::string>>& key_value_summaries,
                                       Log* log) {
  if (!process_sp || !dict_ptr) return false;
  
  Status error;
  size_t ptr_size = process_sp->GetAddressByteSize();
  
  // Use same GSIMapTable traversal logic as ExtractPairsFromMemoryDirect
  addr_t map_addr = dict_ptr + ptr_size;
  
  uint64_t bucket_count = process_sp->ReadUnsignedIntegerFromMemory(
      map_addr + (2 * ptr_size), ptr_size, 0, error);
  if (error.Fail() || bucket_count > 10000) {
    return false;
  }
    
  addr_t buckets_ptr = process_sp->ReadPointerFromMemory(
      map_addr + (3 * ptr_size), error);
  if (error.Fail() || !buckets_ptr) {
    return false;
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
  
  // GSDictionary layout: GSIMapTable_t map at offset ptr_size
  addr_t map_addr = valobj_addr + ptr_size;
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
  // Initialize ObjCBuiltinIdTy following Apple's pattern
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
    Update();
  }
}

llvm::Expected<uint32_t> 
GNUstepNSDictionarySyntheticProvider::CalculateNumChildren() {
  // Return number of key-value pairs (each pair is one child showing "key" = "value")
  return m_key_value_pairs.size();
}

lldb::ValueObjectSP 
GNUstepNSDictionarySyntheticProvider::GetChildAtIndex(uint32_t idx) {
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "GNUstepNSDictionarySyntheticProvider::GetChildAtIndex idx={0}, m_key_value_pairs.size()={1}", idx, m_key_value_pairs.size());
  
  if (idx >= m_key_value_pairs.size())
    return lldb::ValueObjectSP();
  
  addr_t key_addr = m_key_value_pairs[idx].first;
  addr_t value_addr = m_key_value_pairs[idx].second;
  
  LLDB_LOG(log, "Getting dictionary pair at index {0}, key_addr=0x{1:x}, value_addr=0x{2:x}", 
           idx, key_addr, value_addr);
  
  // Validate both key and value addresses
  if (!key_addr || key_addr == LLDB_INVALID_ADDRESS || !value_addr || value_addr == LLDB_INVALID_ADDRESS) {
    LLDB_LOG(log, "Invalid key or value address at index {0}", idx);
    return lldb::ValueObjectSP();
  }
  
  // Validate addresses are reasonable (not corrupted)
  if ((key_addr >= 0x8000000000000000ULL && !IsTaggedPointer(key_addr)) ||
      (value_addr >= 0x8000000000000000ULL && !IsTaggedPointer(value_addr))) {
    LLDB_LOG(log, "Corrupted key or value address at index {0}", idx);
    return lldb::ValueObjectSP();
  }
  
  // Check for very low addresses (except tagged pointers)
  if ((key_addr < 0x1000 && !IsTaggedPointer(key_addr)) || 
      (value_addr < 0x1000 && !IsTaggedPointer(value_addr))) {
    LLDB_LOG(log, "Low key or value address at index {0} - likely invalid", idx);
    return lldb::ValueObjectSP();
  }
  
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  TargetSP target_sp = m_exe_ctx_ref.GetTargetSP();
  if (!process_sp || !target_sp)
    return lldb::ValueObjectSP();
  
  // APPLE'S APPROACH: Create NSPair struct with key and value fields
  // This allows both key and value to have proper synthetic providers and enables nested expansion
  
  // Initialize pair type if needed
  if (!m_pair_type.IsValid()) {
    m_pair_type = GetGNUstepNSPairType(target_sp);
  }
  
  if (!m_pair_type.IsValid()) {
    LLDB_LOG(log, "Failed to create NSPair type");
    return lldb::ValueObjectSP();
  }

  // Create buffer containing both key and value pointers
  auto ptr_size = process_sp->GetAddressByteSize();
  WritableDataBufferSP buffer_sp(new DataBufferHeap(2 * ptr_size, 0));
  
  if (ptr_size == 8) {
    uint64_t *data_ptr = (uint64_t *)buffer_sp->GetBytes();
    *data_ptr = key_addr;       // First field: key
    *(data_ptr + 1) = value_addr;  // Second field: value
  } else {
    uint32_t *data_ptr = (uint32_t *)buffer_sp->GetBytes();
    *data_ptr = static_cast<uint32_t>(key_addr);
    *(data_ptr + 1) = static_cast<uint32_t>(value_addr);
  }

  // Create child name showing index
  StreamString idx_name;
  idx_name.Printf("[%" PRIu32 "]", idx);
  
  // Create ValueObject using the pair type
  DataExtractor data(buffer_sp, process_sp->GetByteOrder(), ptr_size);
  ValueObjectSP pair_sp = CreateValueObjectFromData(idx_name.GetString(), data, m_exe_ctx_ref, m_pair_type);
  
  if (pair_sp) {
    LLDB_LOG(log, "Created dictionary pair child at index {0} with key=0x{1:x}, value=0x{2:x}", 
             idx, key_addr, value_addr);
    return pair_sp;
  } else {
    LLDB_LOG(log, "Failed to create pair ValueObject at index {0}", idx);
    return lldb::ValueObjectSP();
  }
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
GNUstepNSDictionarySyntheticProvider::ExtractPairsFromMemory() {
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp)
    return false;
    
  Status error;
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  // Try to use runtime APIs instead of hardcoded memory layouts
  // First, get the runtime class name to verify we're dealing with a dictionary
  addr_t isa_ptr = process_sp->ReadPointerFromMemory(m_dict_ptr, error);
  if (error.Fail() || !isa_ptr) {
    LLDB_LOG(log, "Failed to read ISA pointer");
    return false;
  }
  
  // Try to call class_getName via runtime symbols
  TargetSP target_sp = m_exe_ctx_ref.GetTargetSP();
  if (!target_sp) {
    LLDB_LOG(log, "No target available");
    return false;
  }
  
  // Look up class_getName symbol
  SymbolContextList sc_list;
  target_sp->GetImages().FindSymbolsWithNameAndType(ConstString("class_getName"), 
                                                   eSymbolTypeCode, sc_list);
  if (sc_list.GetSize() == 0) {
    LLDB_LOG(log, "class_getName symbol not found, falling back to memory reading");
    return ExtractPairsFromMemoryDirect();
  }
  
  LLDB_LOG(log, "Found class_getName symbol, attempting runtime introspection");
  
  // For now, fall back to direct memory reading since expression evaluation 
  // was causing crashes. In the future, we could implement direct runtime
  // function calls here using the symbol addresses
  return ExtractPairsFromMemoryDirect();
}

bool
GNUstepNSDictionarySyntheticProvider::ExtractPairsFromMemoryDirect() {
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp)
    return false;
    
  Status error;
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  // GSDictionary layout: 
  // - isa at offset 0
  // - GSIMapTable_t map at offset m_ptr_size (embedded struct, not pointer)
  addr_t map_addr = m_dict_ptr + m_ptr_size;
  
  // GSIMapTable layout:
  // - zone at offset 0
  // - nodeCount at offset m_ptr_size
  // - bucketCount at offset 2*m_ptr_size
  // - buckets pointer at offset 3*m_ptr_size
  uint64_t node_count = process_sp->ReadUnsignedIntegerFromMemory(
      map_addr + m_ptr_size, m_ptr_size, 0, error);
  if (error.Fail() || node_count > 100000) {
    LLDB_LOG(log, "Failed to read nodeCount or invalid count: {0}", node_count);
    return false;
  }
    
  uint64_t bucket_count = process_sp->ReadUnsignedIntegerFromMemory(
      map_addr + (2 * m_ptr_size), m_ptr_size, 0, error);
  if (error.Fail() || bucket_count > 10000) {
    LLDB_LOG(log, "Failed to read bucketCount or invalid count: {0}", bucket_count);
    return false;
  }
    
  addr_t buckets_ptr = process_sp->ReadPointerFromMemory(
      map_addr + (3 * m_ptr_size), error);
  if (error.Fail() || !buckets_ptr) {
    LLDB_LOG(log, "Failed to read buckets pointer");
    return false;
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
      size_t idx = 0;
      if (sscanf(index_str.c_str(), "%zu", &idx) == 1) {
        if (idx < m_key_value_pairs.size()) {
          // Check if it's asking for key or value
          size_t space_pos = name_str.find(' ', bracket_close);
          if (space_pos != std::string::npos) {
            std::string suffix = name_str.substr(space_pos + 1);
            if (suffix == "key") {
              return idx * 2; // Key is at even indices
            } else if (suffix == "value") {
              return idx * 2 + 1; // Value is at odd indices
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