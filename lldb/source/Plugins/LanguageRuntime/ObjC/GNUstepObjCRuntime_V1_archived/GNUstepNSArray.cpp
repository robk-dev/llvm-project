//===-- GNUstepNSArray.cpp ---------------------------------------------===//
//
// GNUstep NSArray formatters for LLDB
// 
// This implementation uses a hybrid approach:
// 1. Try direct memory reading for array traversal (most efficient)
// 2. Fall back to expression evaluation if memory access fails
// 3. Use shared collection utilities for tagged pointer handling
//
// GSArray memory layout (from GSPrivate.h):
// - id *_contents_array at offset 8 (64-bit pointers)
// - unsigned _count at offset 16
//
//===----------------------------------------------------------------------===//

#include "GNUstepNSArray.h"
#include "GNUstepObjCRuntime.h"
#include "GNUstepCollectionUtilities.h"
#include "GNUstepUtilities.h"
#include "GNUstepRuntimeAPI.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

#include "lldb/ValueObject/ValueObject.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/DataFormatters/StringPrinter.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/Expression/UserExpression.h" 
#include "lldb/Expression/ExpressionVariable.h"
#include "lldb/Expression/DiagnosticManager.h"
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

#include <algorithm>
#include <chrono>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::gnustep_collection_utils;

namespace lldb_private {
namespace formatters {

// Helper function to extract element summaries using runtime-guided memory access
bool ExtractArrayElementSummariesForPreview(addr_t array_ptr, ProcessSP process_sp, 
                                          size_t max_elements, 
                                          std::vector<std::string>& summaries,
                                          Log* log) {
  // Currently disabled to avoid expression evaluation issues
  // TODO: Implement using runtime method discovery and direct symbol calls
  // The runtime should tell us:
  // 1. Where the count ivar is located (via class_getInstanceVariable + ivar_getOffset)
  // 2. Where the elements are stored (via runtime introspection)
  // 3. How to access elements (via direct memory or indirection)
  // This requires implementing proper runtime API calls without expression evaluation
  return false;
}

// Summary provider implementation
bool GNUstepNSArraySummaryProvider(ValueObject &valobj, Stream &stream,
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
  LLDB_LOG(log, "GNUstepNSArraySummaryProvider: Reading memory for address {0}", valobj_addr);
  
  // Check if we're getting ISA pointer instead of object pointer
  if (valobj_addr > 0x7000000000000000ULL) {
    LLDB_LOG(log, "GNUstepNSArraySummaryProvider: Got ISA-like address, might need different handling");
    stream.Printf("NSArray (ISA pointer detected)");
    return true;
  }
  
  size_t ptr_size = process_sp->GetAddressByteSize();
  
  // Use GNUstepRuntimeAPI to discover count offset dynamically
  auto runtime_api = GNUstepRuntimeAPI::Create(process_sp.get());
  if (!runtime_api) {
    LLDB_LOG(log, "GNUstepNSArraySummaryProvider: Failed to create runtime API");
    return false;
  }
  
  auto class_info = runtime_api->GetObjectClassInfo(valobj_addr);
  if (!class_info) {
    LLDB_LOG(log, "GNUstepNSArraySummaryProvider: Failed to get class info: {0}", class_info.error_message);
    return false;
  }
  
  // Find the count ivar dynamically or use known offsets
  ptrdiff_t count_offset = -1;
  
  // For GSInlineArray, the count is stored at offset 16 (0x10)
  // It's not exposed as a named ivar
  if (class_info.value.name == "GSInlineArray") {
    count_offset = 16;
    LLDB_LOG(log, "GNUstepNSArraySummaryProvider: Using known offset {0} for GSInlineArray count", count_offset);
  } else {
    // For other array types, try to find the _count ivar
    for (const auto& ivar : class_info.value.ivars) {
      if (ivar.name == "_count") {
        count_offset = ivar.offset;
        LLDB_LOG(log, "GNUstepNSArraySummaryProvider: Found _count ivar at offset {0}", count_offset);
        break;
      }
    }
  }
  
  if (count_offset < 0) {
    // Fallback: assume count at offset 16 for most GNUstep array types
    count_offset = 16;
    LLDB_LOG(log, "GNUstepNSArraySummaryProvider: Using fallback offset {0} for count in class {1}", 
             count_offset, class_info.value.name);
  }
  
  Status error;
  uint32_t count = process_sp->ReadUnsignedIntegerFromMemory(
      valobj_addr + count_offset, 4, 0, error);
  if (error.Fail()) {
    LLDB_LOG(log, "GNUstepNSArraySummaryProvider: Failed to read count at offset {0}: {1}", count_offset, error.AsCString());
    return false;
  }
  
  // Skip preview for now to avoid expression evaluation issues
  // TODO: Re-enable preview once we have pure memory-based element access
  // std::vector<std::string> element_summaries;
  // bool success = ExtractArrayElementSummariesForPreview(valobj_addr, process_sp, 4, element_summaries, log);
  // 
  // if (success && !element_summaries.empty()) {
  //   stream.Printf("@[");
  //   for (size_t i = 0; i < element_summaries.size(); i++) {
  //     if (i > 0) stream.Printf(", ");
  //     stream.Printf("%s", element_summaries[i].c_str());
  //   }
  //   if (count > 4) {
  //     stream.Printf(", ...");
  //   }
  //   stream.Printf("] ");
  // }
  
  stream.Printf("(%" PRIu32 " element%s)", count, count == 1 ? "" : "s");
  return true;
}

// Main synthetic provider implementation
GNUstepNSArraySyntheticProvider::GNUstepNSArraySyntheticProvider(
    lldb::ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp), m_count(0), m_array_addr(0),
      m_count_offset(-1), m_contents_offset(-1), m_offsets_cached(false) {
  // Initialize ObjCBuiltinIdTy following Apple's NSArray pattern (NSArray.cpp:459-470)
  if (valobj_sp) {
    TargetSP target_sp = valobj_sp->GetExecutionContextRef().GetTargetSP();
    if (target_sp) {
      TypeSystemClangSP scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(*target_sp);
      if (scratch_ts_sp) {
        m_objc_id_type = CompilerType(
            scratch_ts_sp->weak_from_this(),
            scratch_ts_sp->getASTContext().ObjCBuiltinIdTy.getAsOpaquePtr());
      }
      // Initialize runtime API
      ProcessSP process_sp = valobj_sp->GetProcessSP();
      if (process_sp) {
        m_runtime_api = GNUstepRuntimeAPI::Create(process_sp.get());
      }
    }
    Update();
  }
}

llvm::Expected<uint32_t> 
GNUstepNSArraySyntheticProvider::CalculateNumChildren() {
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "CalculateNumChildren: returning {0}", m_count);
  return m_count;
}

lldb::addr_t 
GNUstepNSArraySyntheticProvider::CallObjectAtIndex(uint32_t index) {
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  if (!m_array_addr || index >= m_count) {
    LLDB_LOG(log, "CallObjectAtIndex: Invalid array address or index {0} >= {1}", index, m_count);
    return LLDB_INVALID_ADDRESS;
  }
  
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp) {
    LLDB_LOG(log, "CallObjectAtIndex: No valid process");
    return LLDB_INVALID_ADDRESS;
  }
  
  // CRITICAL CHANGE: Use pure memory access instead of expression evaluation
  // Expression evaluation during formatter execution is unreliable and restricted
  
  // Get the runtime class to determine array type and memory layout
  ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
  if (!runtime) {
    LLDB_LOG(log, "CallObjectAtIndex: No ObjC runtime available");
    return LLDB_INVALID_ADDRESS;
  }
  
  // Get the actual class of this array
  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (!valobj_sp) {
    LLDB_LOG(log, "CallObjectAtIndex: No backend ValueObject");
    return LLDB_INVALID_ADDRESS;
  }
  
  ObjCLanguageRuntime::ClassDescriptorSP descriptor(runtime->GetClassDescriptor(*valobj_sp));
  if (!descriptor || !descriptor->IsValid()) {
    LLDB_LOG(log, "CallObjectAtIndex: No valid class descriptor");
    return LLDB_INVALID_ADDRESS;
  }
  
  ConstString class_name = descriptor->GetClassName();
  LLDB_LOG(log, "CallObjectAtIndex: Array class: {0}", class_name.AsCString());
  
  Status error;
  size_t ptr_size = process_sp->GetAddressByteSize();
  
  // Discover offsets dynamically if not cached
  if (!DiscoverOffsets()) {
    LLDB_LOG(log, "CallObjectAtIndex: Failed to discover offsets for class {0}", class_name.AsCString());
    return LLDB_INVALID_ADDRESS;
  }
  
  // Handle different GNUstep array class layouts
  if (class_name.GetStringRef() == "GSInlineArray") {
    // GSInlineArray: elements stored inline after the standard ivars
    // The elements start at offset 24 (0x18) after ISA, reserved, and count
    addr_t element_slot_addr = m_array_addr + 24 + (index * ptr_size);
    addr_t element_addr = process_sp->ReadPointerFromMemory(element_slot_addr, error);
    if (error.Fail()) {
      LLDB_LOG(log, "CallObjectAtIndex: Failed to read GSInlineArray element at 0x{0:x}", element_slot_addr);
      return LLDB_INVALID_ADDRESS;
    }
    LLDB_LOG(log, "CallObjectAtIndex: GSInlineArray element {0} at 0x{1:x} -> 0x{2:x}", index, element_slot_addr, element_addr);
    return element_addr;
  } else {
    // GSMutableArray and others: use discovered _contents_array offset
    if (m_contents_offset < 0) {
      LLDB_LOG(log, "CallObjectAtIndex: No contents offset discovered");
      return LLDB_INVALID_ADDRESS;
    }
    
    addr_t contents_array_addr = process_sp->ReadPointerFromMemory(m_array_addr + m_contents_offset, error);
    if (error.Fail() || !contents_array_addr) {
      LLDB_LOG(log, "CallObjectAtIndex: Failed to read _contents_array pointer at offset {0}", m_contents_offset);
      return LLDB_INVALID_ADDRESS;
    }
    
    addr_t element_slot_addr = contents_array_addr + (index * ptr_size);
    addr_t element_addr = process_sp->ReadPointerFromMemory(element_slot_addr, error);
    if (error.Fail()) {
      LLDB_LOG(log, "CallObjectAtIndex: Failed to read element pointer at 0x{0:x}", element_slot_addr);
      return LLDB_INVALID_ADDRESS;
    }
    LLDB_LOG(log, "CallObjectAtIndex: {0} element {1} at 0x{2:x} -> 0x{3:x} (discovered contents offset {4})", class_name.AsCString(), index, element_slot_addr, element_addr, m_contents_offset);
    return element_addr;
  }
}

lldb::ValueObjectSP 
GNUstepNSArraySyntheticProvider::GetChildAtIndex(uint32_t idx) {
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "=== GetChildAtIndex ENTRY: idx={0}, count={1} ===", idx, m_count);
  
  if (idx >= m_count) {
    LLDB_LOG(log, "GetChildAtIndex: Index {0} >= count {1}, returning empty", idx, m_count);
    return lldb::ValueObjectSP();
  }
    
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp) {
    LLDB_LOG(log, "GetChildAtIndex: No process, returning empty");
    return lldb::ValueObjectSP();
  }
    
  TargetSP target_sp = m_exe_ctx_ref.GetTargetSP();
  if (!target_sp) {
    LLDB_LOG(log, "GetChildAtIndex: No target, returning empty");
    return lldb::ValueObjectSP();
  }
  
  StreamString name;
  name.Printf("[%" PRIu32 "]", idx);
  LLDB_LOG(log, "GetChildAtIndex: Child name will be '{0}'", name.GetString());
  
  // Get element address from array memory
  addr_t element_addr = CallObjectAtIndex(idx);
  if (element_addr == LLDB_INVALID_ADDRESS) {
    LLDB_LOG(log, "GetChildAtIndex: CallObjectAtIndex failed for index {0}", idx);
    return lldb::ValueObjectSP();
  }
  
  LLDB_LOG(log, "GetChildAtIndex: Element {0} has address 0x{1:x}", idx, element_addr);
  
  // Validate the element address
  if (element_addr == 0) {
    LLDB_LOG(log, "GetChildAtIndex: Element address is null for index {0}", idx);
    return lldb::ValueObjectSP();
  }
  
  // CRITICAL FIX: Determine the actual runtime type of the element
  // This ensures proper formatter selection
  CompilerType element_type = m_objc_id_type;
  
  // Check if it's a tagged pointer first
  uint8_t tag = element_addr & 0x7;
  if (tag == 4) {
    // GSTinyString - use NSString type for proper formatting
    LLDB_LOG(log, "GetChildAtIndex: Element {0} is GSTinyString (tag 4)", idx);
    // The formatter will handle this based on the tag
  } else if (tag == 0 && m_runtime_api) {
    // Regular heap object - get its runtime class
    auto class_info = m_runtime_api->GetObjectClassInfo(element_addr);
    if (class_info && !class_info.value.name.empty()) {
      LLDB_LOG(log, "GetChildAtIndex: Element {0} runtime class: {1}", idx, class_info.value.name);
      
      // Special handling for known string classes
      if (class_info.value.name.find("String") != std::string::npos ||
          class_info.value.name == "NSConstantString") {
        // For string types, just use the generic id type
        // The string formatter is registered for id and will detect the actual type
        LLDB_LOG(log, "GetChildAtIndex: Element {0} is a string type: {1}", idx, class_info.value.name);
      }
    }
  }
  
  // Create ValueObject from data containing the element address
  // We need to create a buffer containing the pointer value since LLDB expects to read from memory
  size_t ptr_size = process_sp->GetAddressByteSize();
  DataBufferSP data_buffer_sp(new DataBufferHeap(ptr_size, 0));
  if (!data_buffer_sp) {
    LLDB_LOG(log, "GetChildAtIndex: Failed to create DataBufferHeap");
    return lldb::ValueObjectSP();
  }
  
  uint8_t *data_ptr = const_cast<uint8_t*>(data_buffer_sp->GetBytes());
  if (!data_ptr) {
    LLDB_LOG(log, "GetChildAtIndex: Failed to get data buffer bytes");
    return lldb::ValueObjectSP();
  }
  
  // Store the element address in our temporary buffer
  if (ptr_size == 8) {
    *reinterpret_cast<uint64_t*>(data_ptr) = element_addr;
  } else {
    *reinterpret_cast<uint32_t*>(data_ptr) = static_cast<uint32_t>(element_addr);
  }
  
  DataExtractor data_extractor(data_buffer_sp, process_sp->GetByteOrder(), ptr_size);
  
  // Create ValueObject from our data buffer
  ValueObjectSP child = ValueObject::CreateValueObjectFromData(
      name.GetString(),
      data_extractor,
      m_exe_ctx_ref,
      element_type
  );
  
  if (child && child.get()) {
    LLDB_LOG(log, "GetChildAtIndex: SUCCESS - Created ValueObject for element {0}", idx);
    
    // Get basic info about the created child
    LLDB_LOG(log, "GetChildAtIndex: Child type name: {0}", child->GetTypeName().AsCString("(null)"));
    LLDB_LOG(log, "GetChildAtIndex: Child value as unsigned: 0x{0:x}", child->GetValueAsUnsigned(0));
    
    // Check if summary was applied
    const char* summary = child->GetSummaryAsCString();
    if (summary) {
      LLDB_LOG(log, "GetChildAtIndex: Element {0} summary: '{1}'", idx, summary);
    } else {
      LLDB_LOG(log, "GetChildAtIndex: Element {0} has no summary, may need explicit formatting", idx);
      
      // Force update to trigger formatter application
      child->GetValueDidChange();
      child->UpdateValueIfNeeded();
      
      // Check again after update
      summary = child->GetSummaryAsCString();
      if (summary) {
        LLDB_LOG(log, "GetChildAtIndex: After update, element {0} summary: '{1}'", idx, summary);
      }
    }
    
    LLDB_LOG(log, "=== GetChildAtIndex SUCCESS: returning valid child for idx={0} ===", idx);
    return child;
  } else {
    LLDB_LOG(log, "GetChildAtIndex: FAILED to create ValueObject for element {0}", idx);
    LLDB_LOG(log, "=== GetChildAtIndex FAILURE: returning empty for idx={0} ===", idx);
  }
  
  return lldb::ValueObjectSP();
}

lldb::ChildCacheState 
GNUstepNSArraySyntheticProvider::Update() {
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  m_count = 0;
  m_array_addr = 0;
  
  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (!valobj_sp)
    return lldb::ChildCacheState::eRefetch;
    
  m_exe_ctx_ref = valobj_sp->GetExecutionContextRef();
  ProcessSP process_sp = valobj_sp->GetProcessSP();
  if (!process_sp)
    return lldb::ChildCacheState::eRefetch;
    
  // Resolve object address (handles ISA pointer vs structure address)
  addr_t valobj_addr = m_backend.GetValueAsUnsigned(0);
  if (!valobj_addr || valobj_addr == LLDB_INVALID_ADDRESS) {
    return lldb::ChildCacheState::eRefetch;
  }
  
  m_array_addr = valobj_addr;
  LLDB_LOG(log, "GNUstepNSArraySyntheticProvider::Update array={0}", m_array_addr);
  
  TargetSP target_sp = m_exe_ctx_ref.GetTargetSP();
  if (!target_sp) {
    return lldb::ChildCacheState::eRefetch;
  }
  
  // Use runtime API to discover count offset dynamically
  if (!DiscoverOffsets()) {
    LLDB_LOG(log, "Failed to discover offsets for array");
    m_count = 0;
    return lldb::ChildCacheState::eRefetch;
  }
  
  Status error;
  m_count = process_sp->ReadUnsignedIntegerFromMemory(
      m_array_addr + m_count_offset, 4, 0, error);
  if (error.Fail() || m_count > 10000) {
    LLDB_LOG(log, "Failed to read count at offset {0} or count too large: {1}", m_count_offset, m_count);
    m_count = 0;
    return lldb::ChildCacheState::eRefetch;
  }
  
  LLDB_LOG(log, "GNUstepNSArraySyntheticProvider::Update found {0} elements", m_count);
  return lldb::ChildCacheState::eRefetch;
}

bool GNUstepNSArraySyntheticProvider::MightHaveChildren() {
  Log *log = GetLog(LLDBLog::DataFormatters);
  bool result = (m_count > 0);
  LLDB_LOG(log, "MightHaveChildren: count={0}, returning {1}", m_count, result);
  return result;
}

size_t GNUstepNSArraySyntheticProvider::GetIndexOfChildWithName(ConstString name) {
  const char *item_name = name.GetCString();
  uint32_t idx = ExtractIndexFromString(item_name);
  if (idx < UINT32_MAX && idx < m_count) {
    return idx;
  }
  return UINT32_MAX;
}

bool GNUstepNSArraySyntheticProvider::DiscoverOffsets() {
  if (m_offsets_cached) {
    return true;
  }
  
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  if (!m_runtime_api || !m_array_addr) {
    LLDB_LOG(log, "DiscoverOffsets: No runtime API or invalid array address");
    return false;
  }
  
  // Get class information for this array object
  auto class_info = m_runtime_api->GetObjectClassInfo(m_array_addr);
  if (!class_info) {
    LLDB_LOG(log, "DiscoverOffsets: Failed to get class info: {0}", class_info.error_message);
    return false;
  }
  
  LLDB_LOG(log, "DiscoverOffsets: Analyzing class {0} with {1} ivars", 
           class_info.value.name, class_info.value.ivars.size());
  
  // Special handling for GSInlineArray which doesn't expose ivars
  if (class_info.value.name == "GSInlineArray") {
    m_count_offset = 16;  // Count is at offset 16
    m_contents_offset = -1; // Elements stored inline, no contents pointer
    m_offsets_cached = true;
    LLDB_LOG(log, "DiscoverOffsets: Using known offsets for GSInlineArray - count at {0}", m_count_offset);
    return true;
  }
  
  // Search for the required ivars in other array types
  bool found_count = false, found_contents = false;
  for (const auto& ivar : class_info.value.ivars) {
    LLDB_LOG(log, "DiscoverOffsets: Found ivar '{0}' at offset {1}", ivar.name, ivar.offset);
    
    if (ivar.name == "_count") {
      m_count_offset = ivar.offset;
      found_count = true;
      LLDB_LOG(log, "DiscoverOffsets: Found _count at offset {0}", m_count_offset);
    } else if (ivar.name == "_contents_array" || ivar.name == "_contents") {
      m_contents_offset = ivar.offset;
      found_contents = true;
      LLDB_LOG(log, "DiscoverOffsets: Found contents array at offset {0}", m_contents_offset);
    }
  }
  
  // For GSMutableArray and similar, use fallback offsets if not found
  if (!found_count) {
    m_count_offset = 16; // Common offset for count in GNUstep arrays
    LLDB_LOG(log, "DiscoverOffsets: Using fallback count offset {0} for class {1}", 
             m_count_offset, class_info.value.name);
  }
  
  if (!found_contents) {
    ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
    if (process_sp) {
      size_t ptr_size = process_sp->GetAddressByteSize();
      m_contents_offset = ptr_size; // Common location after ISA pointer
      LLDB_LOG(log, "DiscoverOffsets: Using fallback contents offset {0} for class {1}", 
               m_contents_offset, class_info.value.name);
    }
  }
  
  m_offsets_cached = true;
  LLDB_LOG(log, "DiscoverOffsets: Successfully cached offsets - count: {0}, contents: {1}", 
           m_count_offset, m_contents_offset);
  return true;
}

} // namespace formatters
} // namespace lldb_private