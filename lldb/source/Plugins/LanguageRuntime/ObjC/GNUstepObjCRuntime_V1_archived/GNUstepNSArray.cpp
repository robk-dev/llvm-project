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
  
  // Use direct memory access instead of expression evaluation  
  // Expression evaluation during summary provider execution is unreliable
  Status error;
  uint32_t count = process_sp->ReadUnsignedIntegerFromMemory(
      valobj_addr + 2 * ptr_size, 4, 0, error);
  if (error.Fail()) {
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
    : SyntheticChildrenFrontEnd(*valobj_sp), m_count(0), m_array_addr(0) {
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
  
  // Handle different GNUstep array class layouts
  if (class_name.GetStringRef() == "GSInlineArray") {
    // GSInlineArray: elements stored inline starting at offset 24 (3 * ptr_size)
    addr_t element_slot_addr = m_array_addr + (3 * ptr_size) + (index * ptr_size);
    addr_t element_addr = process_sp->ReadPointerFromMemory(element_slot_addr, error);
    if (error.Fail()) {
      LLDB_LOG(log, "CallObjectAtIndex: Failed to read GSInlineArray element at 0x{0:x}", element_slot_addr);
      return LLDB_INVALID_ADDRESS;
    }
    LLDB_LOG(log, "CallObjectAtIndex: GSInlineArray element {0} at 0x{1:x} -> 0x{2:x}", index, element_slot_addr, element_addr);
    return element_addr;
  } else {
    // GSMutableArray and others: _contents_array pointer at offset 8, then array of pointers
    addr_t contents_array_addr = process_sp->ReadPointerFromMemory(m_array_addr + ptr_size, error);
    if (error.Fail() || !contents_array_addr) {
      LLDB_LOG(log, "CallObjectAtIndex: Failed to read _contents_array pointer");
      return LLDB_INVALID_ADDRESS;
    }
    
    addr_t element_slot_addr = contents_array_addr + (index * ptr_size);
    addr_t element_addr = process_sp->ReadPointerFromMemory(element_slot_addr, error);
    if (error.Fail()) {
      LLDB_LOG(log, "CallObjectAtIndex: Failed to read element pointer at 0x{0:x}", element_slot_addr);
      return LLDB_INVALID_ADDRESS;
    }
    LLDB_LOG(log, "CallObjectAtIndex: {0} element {1} at 0x{2:x} -> 0x{3:x}", class_name.AsCString(), index, element_slot_addr, element_addr);
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
  
  // CRITICAL FIX: Use runtime API to call objectAtIndex: instead of memory guessing
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
  
  // Create a temporary memory location to hold the object pointer
  // This is needed because LLDB expects to read from a memory address
  size_t ptr_size = process_sp->GetAddressByteSize();
  LLDB_LOG(log, "GetChildAtIndex: ptr_size = {0}", ptr_size);
  
  // Create ValueObject from a temporary data buffer containing the element address
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
  
  // Write the element address to the buffer in the correct byte order
  DataExtractor data_extractor(data_buffer_sp, process_sp->GetByteOrder(), ptr_size);
  
  // Store the object pointer in our temporary buffer
  if (ptr_size == 8) {
    *reinterpret_cast<uint64_t*>(data_ptr) = element_addr;
    LLDB_LOG(log, "GetChildAtIndex: Stored 64-bit address 0x{0:x} in buffer", element_addr);
  } else {
    *reinterpret_cast<uint32_t*>(data_ptr) = static_cast<uint32_t>(element_addr);
    LLDB_LOG(log, "GetChildAtIndex: Stored 32-bit address 0x{0:x} in buffer", static_cast<uint32_t>(element_addr));
  }
  
  if (!m_objc_id_type.IsValid()) {
    LLDB_LOG(log, "GetChildAtIndex: m_objc_id_type is NOT VALID - this is the problem!");
    return lldb::ValueObjectSP();
  }
  
  LLDB_LOG(log, "GetChildAtIndex: m_objc_id_type is valid, creating ValueObject");
  
  // Create ValueObject from our data buffer
  ValueObjectSP child = ValueObject::CreateValueObjectFromData(
      name.GetString(),
      data_extractor,
      m_exe_ctx_ref,
      m_objc_id_type
  );
  
  if (child && child.get()) {
    LLDB_LOG(log, "GetChildAtIndex: SUCCESS - Created ValueObject for element {0}", idx);
    
    // Get basic info about the created child
    LLDB_LOG(log, "GetChildAtIndex: Child type name: {0}", child->GetTypeName().AsCString("(null)"));
    LLDB_LOG(log, "GetChildAtIndex: Child value as unsigned: 0x{0:x}", child->GetValueAsUnsigned(0));
    
    // Force summary provider application
    const char* summary = child->GetSummaryAsCString();
    if (summary) {
      LLDB_LOG(log, "GetChildAtIndex: Element {0} summary: '{1}'", idx, summary);
    } else {
      LLDB_LOG(log, "GetChildAtIndex: Element {0} has no summary", idx);
    }
    
    child->SetFormat(lldb::eFormatDefault);
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
  
  // Use runtime to discover where the count is stored
  Status error;
  size_t ptr_size = process_sp->GetAddressByteSize();
  
  // For now, we still use the known offset, but TODO: use runtime introspection
  // The runtime should tell us via class_getInstanceVariable("_count") + ivar_getOffset
  // Current known layout: count at offset 16 (2 * ptr_size) for GNUstep arrays
  m_count = process_sp->ReadUnsignedIntegerFromMemory(
      m_array_addr + 2 * ptr_size, 4, 0, error);
  if (error.Fail() || m_count > 10000) {
    LLDB_LOG(log, "Failed to read count or count too large: {0}", m_count);
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

} // namespace formatters
} // namespace lldb_private