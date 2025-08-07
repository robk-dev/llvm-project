//===-- GNUstepNSSet.cpp -----------------------------------------------===//
//
// GNUstep NSSet formatters for LLDB
// 
// This implementation uses a hybrid approach:
// 1. Try expression evaluation first (most reliable, uses runtime)
// 2. Fall back to direct memory reading if expressions fail
//
//===----------------------------------------------------------------------===//

#include "GNUstepNSSet.h"
#include "GNUstepObjCRuntime.h"
#include "GNUstepCollectionUtilities.h"
#include "GNUstepUtilities.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

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

// Helper function to extract element summaries for preview
bool ExtractElementSummariesForPreview(addr_t set_ptr, ProcessSP process_sp, 
                                     size_t max_elements, 
                                     std::vector<std::string>& summaries,
                                     Log* log) {
  if (!process_sp || !set_ptr) return false;
  
  Status error;
  size_t ptr_size = process_sp->GetAddressByteSize();
  
  // Try to use runtime API for offset discovery (same logic as main provider)
  ptrdiff_t map_offset = ptr_size;  // Default fallback
  
  GNUstepRuntimeAPISP runtime_api = GNUstepRuntimeAPI::Create(process_sp.get());
  if (runtime_api && runtime_api->IsValid()) {
    auto class_info = runtime_api->GetObjectClassInfo(set_ptr);
    if (class_info) {
      // Look for _map or map ivar
      for (const auto& ivar : class_info.value.ivars) {
        if (ivar.name == "_map" || ivar.name == "map") {
          map_offset = ivar.offset;
          LLDB_LOG(log, "Preview: Found _map ivar at offset {0} using runtime API", map_offset);
          break;
        }
      }
    } else {
      LLDB_LOG(log, "Preview: Runtime API class info failed: {0}, using default offset", class_info.error_message);
    }
  } else {
    LLDB_LOG(log, "Preview: Runtime API not available, using default map offset {0}", map_offset);
  }
  
  addr_t map_addr = set_ptr + map_offset;
  
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
  
  // Traverse buckets to extract element summaries
  for (uint64_t bucket_idx = 0; 
       bucket_idx < bucket_count && summaries.size() < max_elements;
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
           summaries.size() < max_elements) {
      
      addr_t next_ptr = process_sp->ReadPointerFromMemory(node_ptr, error);
      if (error.Fail())
        break;
        
      addr_t element_addr = process_sp->ReadPointerFromMemory(
          node_ptr + ptr_size, error);
      if (error.Success() && element_addr) {
        // Get summary for this element using shared utilities
        TargetSP target_sp = process_sp->GetTarget().shared_from_this();
        std::string summary = GetObjectSummary(element_addr, process_sp, target_sp, log);
        if (!summary.empty()) {
          summaries.push_back(summary);
        }
      }
      
      node_ptr = next_ptr;
      nodes_read++;
      
      if (nodes_read > 1000) break; // Safety
    }
  }
  
  return !summaries.empty();
}

// Summary provider implementation
bool GNUstepNSSetSummaryProvider(ValueObject &valobj, Stream &stream,
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
  LLDB_LOG(log, "GNUstepNSSetSummaryProvider: Reading memory for address 0x{0:x}", valobj_addr);
  
  size_t ptr_size = process_sp->GetAddressByteSize();
  Status error;
  
  // Try to use runtime API for dynamic offset discovery
  ptrdiff_t map_offset = ptr_size;  // Default fallback
  ptrdiff_t nodeCount_offset = ptr_size; // Default: nodeCount at map + ptr_size
  
  GNUstepRuntimeAPISP runtime_api = GNUstepRuntimeAPI::Create(process_sp.get());
  if (runtime_api && runtime_api->IsValid()) {
    auto class_info = runtime_api->GetObjectClassInfo(valobj_addr);
    if (class_info) {
      for (const auto& ivar : class_info.value.ivars) {
        if (ivar.name == "_map" || ivar.name == "map") {
          map_offset = ivar.offset;
          LLDB_LOG(log, "Summary: Found _map ivar at offset {0} using runtime API", map_offset);
          break;
        }
      }
    }
  }
  
  addr_t map_addr = valobj_addr + map_offset;
  addr_t node_count_addr = map_addr + nodeCount_offset;
  
  uint64_t count = process_sp->ReadUnsignedIntegerFromMemory(
      node_count_addr, ptr_size, 0, error);
      
  if (error.Fail()) {
    return false;
  }
  
  // Get preview of first 4 elements
  std::vector<std::string> element_summaries;
  bool success = ExtractElementSummariesForPreview(valobj_addr, process_sp, 4, element_summaries, log);
  
  if (success && !element_summaries.empty()) {
    stream.Printf("{");
    for (size_t i = 0; i < element_summaries.size(); i++) {
      if (i > 0) stream.Printf(", ");
      stream.Printf("%s", element_summaries[i].c_str());
    }
    if (count > 4) {
      stream.Printf(", ...");
    }
    stream.Printf("} ");
  }
  
  stream.Printf("(%" PRIu64 " element%s)", count, count == 1 ? "" : "s");
  return true;
}

// Dynamic offset discovery using runtime API
bool GNUstepNSSetSyntheticProvider::DiscoverOffsets() {
  if (m_offsets_cached) {
    return true;
  }
  
  if (!m_runtime_api || !m_runtime_api->IsValid()) {
    return false;
  }
  
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "GNUstepNSSet: Discovering offsets using runtime API");
  
  auto class_info = m_runtime_api->GetObjectClassInfo(m_set_ptr);
  if (!class_info) {
    LLDB_LOG(log, "GNUstepNSSet: Failed to get class info: {0}", class_info.error_message);
    return false;
  }
  
  LLDB_LOG(log, "GNUstepNSSet: Found class {0} with {1} ivars", 
           class_info.value.name, class_info.value.ivars.size());
  
  // Find _map ivar
  for (const auto& ivar : class_info.value.ivars) {
    LLDB_LOG(log, "GNUstepNSSet: Examining ivar '{0}' at offset {1}", ivar.name, ivar.offset);
    if (ivar.name == "_map" || ivar.name == "map") {
      m_map_offset = ivar.offset;
      LLDB_LOG(log, "GNUstepNSSet: Found _map ivar at offset {0}", m_map_offset);
      break;
    }
  }
  
  // GSIMapTable offsets are well-defined in the GNUstep source
  // See libs-base/Source/GSIMap.h for the structure definition
  m_nodeCount_offset = m_ptr_size;         // nodeCount at offset 8 (after zone pointer)
  m_buckets_offset = 3 * m_ptr_size;       // buckets at offset 24 (after zone, nodeCount, bucketCount)
  
  LLDB_LOG(log, "GNUstepNSSet: Using offsets - _map: {0}, nodeCount: {1}, buckets: {2}",
           m_map_offset, m_nodeCount_offset, m_buckets_offset);
  
  m_offsets_cached = true;
  return true;
}

// Main synthetic provider implementation
GNUstepNSSetSyntheticProvider::GNUstepNSSetSyntheticProvider(
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
    
    // Initialize runtime API
    ProcessSP process_sp = valobj_sp->GetProcessSP();
    if (process_sp) {
      m_runtime_api = GNUstepRuntimeAPI::Create(process_sp.get());
      if (m_runtime_api && m_runtime_api->IsValid()) {
        Log *log = GetLog(LLDBLog::DataFormatters);
        LLDB_LOG(log, "GNUstepNSSet: Runtime API initialized successfully");
      }
    }
    
    Update();
  }
}

llvm::Expected<uint32_t> 
GNUstepNSSetSyntheticProvider::CalculateNumChildren() {
  return m_element_addresses.size();
}

lldb::ValueObjectSP 
GNUstepNSSetSyntheticProvider::GetChildAtIndex(uint32_t idx) {
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "GNUstepNSSetSyntheticProvider::GetChildAtIndex idx={0}, m_element_addresses.size()={1}", idx, m_element_addresses.size());
  
  if (idx >= m_element_addresses.size())
    return lldb::ValueObjectSP();
    
  addr_t element_addr = m_element_addresses[idx];
  LLDB_LOG(log, "Getting child at index {0}, element_addr=0x{1:x}", idx, element_addr);
  
  // Validate address before creating child to prevent "read memory failed" errors
  if (!element_addr || element_addr == LLDB_INVALID_ADDRESS) {
    LLDB_LOG(log, "Invalid address (null/invalid) at index {0}", idx);
    return lldb::ValueObjectSP();
  }
  
  // Check if it's a corrupted address (exceeds valid memory range)
  if (element_addr >= 0x8000000000000000ULL && !IsTaggedPointer(element_addr)) {
    LLDB_LOG(log, "Corrupted address 0x{0:x} at index {1} - exceeds valid range", element_addr, idx);
    
    // Return a placeholder showing the issue instead of crashing
    StreamString error_desc;
    error_desc.Printf("<invalid address: 0x%" PRIx64 ">", element_addr);
    
    ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
    if (process_sp) {
      DataExtractor error_data(error_desc.GetData(), 
                              error_desc.GetSize(),
                              process_sp->GetByteOrder(), 
                              process_sp->GetAddressByteSize());
      
      StreamString name;
      name.Printf("[%" PRIu32 "]", idx);
      
      return CreateValueObjectFromData(name.GetString(), error_data, m_exe_ctx_ref,
                                      m_backend.GetCompilerType());
    }
    return lldb::ValueObjectSP();
  }
  
  // Additional check for very low addresses (except tagged pointers)
  if (element_addr < 0x1000 && !IsTaggedPointer(element_addr)) {
    LLDB_LOG(log, "Low address 0x{0:x} at index {1} - likely invalid", element_addr, idx);
    return lldb::ValueObjectSP();
  }
  
  StreamString name;
  name.Printf("[%" PRIu32 "]", idx);
  
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp)
    return lldb::ValueObjectSP();
    
  TargetSP target_sp = m_exe_ctx_ref.GetTargetSP();
  if (!target_sp)
    return lldb::ValueObjectSP();
  
  // Log tagged pointer detection for debugging
  if (IsTaggedPointer(element_addr)) {
    std::string class_name = GetTaggedPointerClassName(element_addr, log);
    LLDB_LOG(log, "Detected tagged pointer: {0} (0x{1:x})", class_name, element_addr);
  }
  
  // CRITICAL FIX: Use working CreateValueObjectFromAddress pattern (same as arrays)
  // We need to create a temporary memory slot that contains the element pointer,
  // then use CreateValueObjectFromAddress on that slot address
  LLDB_LOG(log, "Creating set child using working CreateValueObjectFromAddress pattern");
  
  if (m_objc_id_type.IsValid()) {
    // Allocate a temporary slot in process memory to hold the element pointer
    auto ptr_size = process_sp->GetAddressByteSize();
    
    // Write the element pointer to a temporary buffer in our address space
    std::vector<uint8_t> slot_buffer(ptr_size);
    switch (ptr_size) {
    case 4:
      *reinterpret_cast<uint32_t *>(slot_buffer.data()) = static_cast<uint32_t>(element_addr);
      break;
    case 8:
      *reinterpret_cast<uint64_t *>(slot_buffer.data()) = static_cast<uint64_t>(element_addr);
      break;
    default:
      LLDB_LOG(log, "Unsupported pointer size: {0}", ptr_size);
      return lldb::ValueObjectSP();
    }
    
    // Try to allocate memory in the target process to create a temporary "slot"
    // This is the same approach that makes arrays work
    Status error;
    addr_t temp_slot_addr = process_sp->AllocateMemory(ptr_size, ePermissionsReadable | ePermissionsWritable, error);
    if (error.Success() && temp_slot_addr != LLDB_INVALID_ADDRESS) {
      // Write our element pointer to the temporary slot
      size_t bytes_written = process_sp->WriteMemory(temp_slot_addr, slot_buffer.data(), ptr_size, error);
      if (error.Success() && bytes_written == ptr_size) {
        LLDB_LOG(log, "Created temporary slot at 0x{0:x} containing element pointer 0x{1:x}", temp_slot_addr, element_addr);
        
        // Now create ValueObject from the slot address (same as arrays)
        ValueObjectSP child = CreateValueObjectFromAddress(
            name.GetString(),
            temp_slot_addr,     // Address OF the slot containing the pointer  
            m_exe_ctx_ref,
            m_objc_id_type      // ObjCBuiltinIdTy will handle dereferencing and summaries
        );
        
        if (child && child.get()) {
          LLDB_LOG(log, "CreateValueObjectFromAddress succeeded for set element {0} at slot 0x{1:x}", idx, temp_slot_addr);
          return child;
        } else {
          LLDB_LOG(log, "CreateValueObjectFromAddress failed for set element {0}", idx);
        }
        
        // Clean up the temporary slot
        process_sp->DeallocateMemory(temp_slot_addr);
      } else {
        LLDB_LOG(log, "Failed to write to temporary slot at 0x{0:x}", temp_slot_addr);
        process_sp->DeallocateMemory(temp_slot_addr);
      }
    } else {
      LLDB_LOG(log, "Failed to allocate temporary slot: {0}", error.AsCString());
    }
  }
  
  // Fallback to CreateValueObjectFromData if CreateValueObjectFromAddress fails
  LLDB_LOG(log, "CreateValueObjectFromAddress failed, falling back to CreateValueObjectFromData");
  
  auto ptr_size = process_sp->GetAddressByteSize();
  DataBufferHeap buffer(ptr_size, 0);
  switch (ptr_size) {
  case 0: // architecture has no clue - fail
    return lldb::ValueObjectSP();
  case 4:
    *reinterpret_cast<uint32_t *>(buffer.GetBytes()) =
        static_cast<uint32_t>(element_addr);
    break;
  case 8:
    *reinterpret_cast<uint64_t *>(buffer.GetBytes()) =
        static_cast<uint64_t>(element_addr);
    break;
  default:
    lldbassert(false && "pointer size is not 4 nor 8");
  }

  DataExtractor data(buffer.GetBytes(), buffer.GetByteSize(),
                     process_sp->GetByteOrder(),
                     process_sp->GetAddressByteSize());

  // Use CreateValueObjectFromData as fallback
  // Use the already initialized m_objc_id_type instead of GetBasicTypeFromAST
  return CreateValueObjectFromData(
      name.GetString(), data, m_exe_ctx_ref, m_objc_id_type);
}

lldb::ChildCacheState
GNUstepNSSetSyntheticProvider::Update() {
  m_element_addresses.clear();
  m_has_valid_data = false;
  
  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (!valobj_sp)
    return lldb::ChildCacheState::eRefetch;
    
  m_exe_ctx_ref = valobj_sp->GetExecutionContextRef();
  ProcessSP process_sp = valobj_sp->GetProcessSP();
  if (!process_sp)
    return lldb::ChildCacheState::eRefetch;
    
  m_ptr_size = process_sp->GetAddressByteSize();
  m_set_ptr = valobj_sp->GetValueAsUnsigned(0);
  
  if (!m_set_ptr)
    return lldb::ChildCacheState::eRefetch;
    
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "GNUstepNSSetSyntheticProvider: Updating for address 0x{0:x}", m_set_ptr);
  
  // Skip expression evaluation - go directly to memory reading to avoid selector issues
  bool success = ExtractElementsFromMemory();
  
  m_has_valid_data = success;
  return success ? lldb::ChildCacheState::eReuse : lldb::ChildCacheState::eRefetch;
}

bool
GNUstepNSSetSyntheticProvider::ExtractElementsUsingExpression(StackFrameSP frame_sp) {
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "Trying to extract set elements using expression evaluation");
  
  ExecutionContext exe_ctx(frame_sp);
  
  // First get the count
  StreamString count_expr;
  count_expr.Printf("(NSUInteger)[(id)0x%" PRIx64 " count]", m_set_ptr);
  
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
    return true; // Empty set is valid
    
  LLDB_LOG(log, "Set has {0} elements", count);
  
  // Get all objects using allObjects
  StreamString all_objects_expr;
  all_objects_expr.Printf("(id)[(id)0x%" PRIx64 " allObjects]", m_set_ptr);
  
  ValueObjectSP array_result_sp;
  expr_result = UserExpression::Evaluate(
      exe_ctx, eval_options, all_objects_expr.GetString(), "", array_result_sp, nullptr);
  
  if (expr_result != eExpressionCompleted || !array_result_sp) {
    return false;
  }
  
  addr_t array_addr = array_result_sp->GetValueAsUnsigned(0);
  if (!array_addr)
    return false;
    
  // Now extract elements from the array
  for (uint64_t i = 0; i < count && i < 1000; i++) { // Safety limit
    StreamString element_expr;
    element_expr.Printf("(id)[(id)0x%" PRIx64 " objectAtIndex:%" PRIu64 "]", 
                       array_addr, i);
    
    ValueObjectSP element_result_sp;
    expr_result = UserExpression::Evaluate(
        exe_ctx, eval_options, element_expr.GetString(), "", element_result_sp, nullptr);
    
    if (expr_result == eExpressionCompleted && element_result_sp) {
      addr_t element_addr = element_result_sp->GetValueAsUnsigned(0);
      if (element_addr) {
        m_element_addresses.push_back(element_addr);
      }
    }
  }
  
  LLDB_LOG(log, "Extracted {0} elements using expressions", m_element_addresses.size());
  return !m_element_addresses.empty();
}

bool
GNUstepNSSetSyntheticProvider::ExtractElementsFromMemory() {
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp)
    return false;
    
  Status error;
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  // Try to use runtime APIs instead of hardcoded memory layouts
  // First, get the runtime class name to verify we're dealing with a set
  addr_t isa_ptr = process_sp->ReadPointerFromMemory(m_set_ptr, error);
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
    return ExtractElementsFromMemoryDirect();
  }
  
  LLDB_LOG(log, "Found class_getName symbol, attempting runtime introspection");
  
  // For now, fall back to direct memory reading since expression evaluation 
  // was causing crashes. In the future, we could implement direct runtime
  // function calls here using the symbol addresses
  return ExtractElementsFromMemoryDirect();
}

bool
GNUstepNSSetSyntheticProvider::ExtractElementsFromMemoryDirect() {
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp)
    return false;
    
  Status error;
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  // Use dynamic offset discovery instead of hardcoded layout assumptions
  if (!DiscoverOffsets()) {
    LLDB_LOG(log, "Failed to discover offsets using runtime API, falling back to defaults");
  }
  
  // Apply discovered or default offsets
  ptrdiff_t map_offset = (m_map_offset >= 0) ? m_map_offset : m_ptr_size;
  ptrdiff_t nodeCount_offset = (m_nodeCount_offset >= 0) ? m_nodeCount_offset : m_ptr_size;
  ptrdiff_t buckets_offset = (m_buckets_offset >= 0) ? m_buckets_offset : (3 * m_ptr_size);
  
  addr_t map_addr = m_set_ptr + map_offset;
  
  LLDB_LOG(log, "Using offsets - map: {0}, nodeCount: {1}, buckets: {2}", 
           map_offset, nodeCount_offset, buckets_offset);
  
  uint64_t node_count = process_sp->ReadUnsignedIntegerFromMemory(
      map_addr + nodeCount_offset, m_ptr_size, 0, error);
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
      map_addr + buckets_offset, error);
  if (error.Fail() || !buckets_ptr) {
    LLDB_LOG(log, "Failed to read buckets pointer");
    return false;
  }
    
  LLDB_LOG(log, "GSIMapTable: nodeCount={0}, bucketCount={1}, buckets=0x{2:x}",
           node_count, bucket_count, buckets_ptr);
  
  // Iterate through buckets to find nodes
  for (uint64_t bucket_idx = 0; 
       bucket_idx < bucket_count && m_element_addresses.size() < node_count;
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
           m_element_addresses.size() < node_count) {
      
      LLDB_LOG(log, "=== DEBUGGING NODE at 0x{0:x} ===", node_ptr);
      
      // First, dump raw memory at node to see actual layout
      uint8_t node_memory[64];
      Status dump_error;
      size_t bytes_read = process_sp->ReadMemory(node_ptr, node_memory, 64, dump_error);
      if (dump_error.Success() && bytes_read >= 32) {
        LLDB_LOG(log, "Raw node memory dump:");
        for (size_t i = 0; i < std::min(bytes_read, (size_t)32); i += 8) {
          uint64_t *ptr = (uint64_t*)(node_memory + i);
          LLDB_LOG(log, "  +{0}: 0x{1:x}", i, *ptr);
        }
      }
      
      // GSIMapNode layout (from GSIMap.h):
      // struct _GSIMapNode {
      // #if defined(GSI_MAP_NODE_CLASS)
      //   void *isa;              // +0 (may or may not be present!)
      // #endif
      //   GSIMapNode nextInBucket; // +0 or +8 depending on GSI_MAP_NODE_CLASS
      //   GSIMapKey key;          // +8 or +16
      // #if GSI_MAP_HAS_VALUE    // (0 for sets)
      //   GSIMapVal value;
      // #endif
      // };
      
      // Try both layouts - first assume no GSI_MAP_NODE_CLASS
      addr_t next_ptr_attempt1 = process_sp->ReadPointerFromMemory(node_ptr, error);
      addr_t element_addr_attempt1 = 0;
      if (error.Success()) {
        element_addr_attempt1 = process_sp->ReadPointerFromMemory(node_ptr + m_ptr_size, error);
      }
      
      // Try second layout - assume GSI_MAP_NODE_CLASS is defined (adds 8-byte isa)
      addr_t next_ptr_attempt2 = 0;
      addr_t element_addr_attempt2 = 0;
      Status error2;
      next_ptr_attempt2 = process_sp->ReadPointerFromMemory(node_ptr + m_ptr_size, error2);
      if (error2.Success()) {
        element_addr_attempt2 = process_sp->ReadPointerFromMemory(node_ptr + (2 * m_ptr_size), error2);
      }
      
      LLDB_LOG(log, "Layout attempt 1 (no isa): next=0x{0:x}, element=0x{1:x}",
               next_ptr_attempt1, element_addr_attempt1);
      LLDB_LOG(log, "Layout attempt 2 (with isa): next=0x{0:x}, element=0x{1:x}",
               next_ptr_attempt2, element_addr_attempt2);
      
      // Heuristic: valid pointers should be in reasonable ranges
      // CRITICAL FIX: Must handle tagged pointers correctly!
      auto is_valid_address = [log](addr_t addr) -> bool {
        if (addr == 0) return false;
        
        // Check if it's a tagged pointer first - these are always valid if properly tagged
        if (IsTaggedPointer(addr)) {
          LLDB_LOG(log, "Address 0x{0:x} is a valid tagged pointer", addr);
          return true;
        }
        
        // For regular pointers, apply standard validation
        if (addr < 0x1000) return false;  // Too low
        if (addr > 0x7FFFFFFFFFFF) return false;  // Too high for user space
        // Object addresses usually 8-byte aligned, but don't be too strict
        if ((addr & 0x7) != 0) {
          LLDB_LOG(log, "Address 0x{0:x} not 8-byte aligned, might be invalid", addr);
          return false;
        }
        return true;
      };
      
      addr_t next_ptr, element_addr;
      bool using_layout2 = false;
      
      // Choose the layout that gives more reasonable addresses
      if (is_valid_address(element_addr_attempt1) && 
          (next_ptr_attempt1 == 0 || is_valid_address(next_ptr_attempt1))) {
        next_ptr = next_ptr_attempt1;
        element_addr = element_addr_attempt1;
        LLDB_LOG(log, "Using layout 1 (no isa field)");
      } else if (is_valid_address(element_addr_attempt2) &&
                 (next_ptr_attempt2 == 0 || is_valid_address(next_ptr_attempt2))) {
        next_ptr = next_ptr_attempt2;
        element_addr = element_addr_attempt2;
        using_layout2 = true;
        LLDB_LOG(log, "Using layout 2 (with isa field)");
      } else {
        LLDB_LOG(log, "Both layouts produced invalid addresses, skipping node");
        break;
      }
      
      if (element_addr && is_valid_address(element_addr)) {
        // Check if this is a tagged pointer
        if (IsTaggedPointer(element_addr)) {
          std::string class_name = GetTaggedPointerClassName(element_addr, log);
          LLDB_LOG(log, "Added tagged pointer element: {0} (0x{1:x})", class_name, element_addr);
          
          // For GSTinyString, also try to decode the string content
          if (class_name == "GSTinyString") {
            std::string decoded_string;
            if (DecodeGSTinyString(element_addr, decoded_string, log)) {
              LLDB_LOG(log, "  Decoded string content: \"{0}\"", decoded_string);
            }
          }
          
          m_element_addresses.push_back(element_addr);
        } else {
          m_element_addresses.push_back(element_addr);
          LLDB_LOG(log, "Added regular element at 0x{0:x}", element_addr);
        }
      }
      
      node_ptr = next_ptr;
      nodes_read++;
      
      if (nodes_read > 1000) // Safety
        break;
    }
  }
  
  LLDB_LOG(log, "Extracted {0} elements from memory", m_element_addresses.size());
  return true;
}

size_t 
GNUstepNSSetSyntheticProvider::GetIndexOfChildWithName(ConstString name) {
  // Handle set index notation [n] - similar to array pattern
  std::string name_str = name.GetStringRef().str();
  if (name_str.size() >= 3 && name_str[0] == '[' && name_str[name_str.size()-1] == ']') {
    std::string index_str = name_str.substr(1, name_str.size()-2);
    size_t idx = 0;
    if (sscanf(index_str.c_str(), "%zu", &idx) == 1) {
      if (idx < m_element_addresses.size())
        return idx;
    }
  }
  return UINT32_MAX;
}

// Factory function
SyntheticChildrenFrontEnd *
GNUstepNSSetSyntheticFrontEndCreator(CXXSyntheticChildren *synth,
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
  LLDB_LOG(log, "GNUstepNSSetSyntheticFrontEndCreator for class {0}", class_name);
  
  // Handle any class that looks like a set
  if (class_name.GetStringRef().contains("Set")) {
    return new GNUstepNSSetSyntheticProvider(valobj_sp);
  }
  
  return nullptr;
}

} // namespace formatters
} // namespace lldb_private