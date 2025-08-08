//===-- GNUstepArrayFormatters.cpp ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepArrayFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Symbol/CompilerType.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Utility/StreamString.h"
#include <cstdio>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

//===----------------------------------------------------------------------===//
// NSArray Summary Provider
//===----------------------------------------------------------------------===//

bool GNUstepNSArraySummaryProvider::FormatObject(ValueObject &valobj, 
                                                 Stream &stream, 
                                                 const TypeSummaryOptions &options) {
  printf("[GNUstepArray] FormatObject called\n");
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    printf("[GNUstepArray] Not a valid GNUstep object\n");
    WriteErrorSummary(stream, "invalid object");
    return false;
  }
  
  uint32_t count = ExtractArrayCount(valobj);
  
  // Create formatter context to prevent infinite recursion
  FormatterContext context;
  
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
  // Limit to reasonable counts to avoid performance issues
  if (count <= MAX_COLLECTION_ELEMENTS_INLINE) {
    std::string inline_elements = GetInlineElementsPreview(valobj, count, context);
    if (!inline_elements.empty()) {
      stream.Printf(" %s", inline_elements.c_str());
    }
  }
  
  return true;
}

uint32_t GNUstepNSArraySummaryProvider::ExtractArrayCount(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return 0;
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return 0;
  }
  
  // GNUstep GSArray structure (from GSPrivate.h):
  // @interface GSArray : NSArray
  // {
  // @public
  //   id         *_contents_array;  // offset 8 (after isa)
  //   unsigned   _count;             // offset 16 (after _contents_array pointer)
  // }
  // @end
  
  // Read the count field at offset 16 (8 bytes for isa + 8 bytes for _contents_array pointer)
  lldb::addr_t count_addr = obj_addr + 16;
  uint32_t count = 0;
  
  if (!GNUstepRuntimeHelper::ReadMemory(process, count_addr, &count, sizeof(count))) {
    return 0;
  }
  
  return count;
}

std::string GNUstepNSArraySummaryProvider::GetInlineElementsPreview(ValueObject &valobj, uint32_t count, FormatterContext &context) {
  if (count == 0) {
    return "";
  }
  
  printf("[GNUstepArray] GetInlineElementsPreview called with count=%u\n", count);
  
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    printf("[GNUstepArray] No process\n");
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  printf("[GNUstepArray] Array object at: 0x%llx\n", (unsigned long long)obj_addr);
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read _contents_array pointer at offset 8
  lldb::addr_t contents_ptr_addr = obj_addr + 8;
  Status error;
  lldb::addr_t contents_array_ptr = GNUstepRuntimeHelper::ReadPointer(process, contents_ptr_addr, error);
  printf("[GNUstepArray] Contents array pointer at 0x%llx: 0x%llx\n", 
         (unsigned long long)contents_ptr_addr, (unsigned long long)contents_array_ptr);
  if (error.Fail() || contents_array_ptr == 0 || contents_array_ptr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Build inline preview - show up to MAX_COLLECTION_ELEMENTS_INLINE elements to avoid performance issues
  uint32_t preview_limit = std::min(count, MAX_COLLECTION_ELEMENTS_INLINE);
  std::string result = "@[";
  
  size_t ptr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // Add safety check to prevent infinite loops
  if (preview_limit == 0 || ptr_size == 0) {
    result += "]";
    return result;
  }
  
  for (uint32_t i = 0; i < preview_limit && i < MAX_LOOP_ITERATIONS; ++i) {
    if (i > 0) {
      result += ", ";
    }
    
    // Read element pointer with bounds checking
    lldb::addr_t element_ptr_addr = contents_array_ptr + (i * ptr_size);
    lldb::addr_t element_addr = GNUstepRuntimeHelper::ReadPointer(process, element_ptr_addr, error);
    printf("[GNUstepArray] Element %u at 0x%llx: 0x%llx\n", 
           i, (unsigned long long)element_ptr_addr, (unsigned long long)element_addr);
    if (error.Fail() || element_addr == 0) {
      result += "<nil>";
      continue;
    }
    
    // Try to get a string representation of the element with recursion protection
    std::string element_summary = GetElementSummary(process, element_addr, context);
    printf("[GNUstepArray] Element %u summary: %s\n", i, element_summary.c_str());
    if (element_summary.empty()) {
      result += "<object>";
    } else {
      result += element_summary;
    }
  }
  
  // Add ellipsis if there are more elements
  if (count > preview_limit) {
    result += ", ...";
  }
  
  result += "]";
  return result;
}

std::string GNUstepNSArraySummaryProvider::GetElementSummary(Process *process, lldb::addr_t element_addr, FormatterContext &context) {
  if (!process || element_addr == 0 || element_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Check for recursion depth limit and cycle detection
  if (context.ShouldStopRecursion(element_addr)) {
    return "<...>"; // Indicate recursion was stopped
  }
  
  // Enter this object in our recursion tracking
  context.EnterObject(element_addr);
  
  // Try to extract string content first (handles both tagged pointers and regular strings)
  std::string string_content = TryExtractStringContent(process, element_addr);
  if (!string_content.empty()) {
    context.ExitObject(element_addr);
    // Return quoted string, truncated for inline display
    if (string_content.length() > MAX_STRING_PREVIEW_LENGTH) {
      return "\"" + string_content.substr(0, MAX_STRING_PREVIEW_LENGTH - 3) + "...\"";
    }
    return "\"" + string_content + "\"";
  }
  
  // Check if it's a tagged pointer that couldn't be decoded as string
  GNUstepObjCRuntimeIntrospector introspector(process);
  if (introspector.IsTaggedPointer(element_addr)) {
    context.ExitObject(element_addr);
    // For non-string tagged pointers, show the tag type
    uint64_t tag = element_addr & 7;
    switch (tag) {
      case 1: return "<NSNumber>";
      case 2: return "<NSDate>";
      case 4: return "<NSString>"; // Fallback if decoding failed
      default: return "<tagged>";
    }
  }
  
  // REMOVED: Recursive TryExtractCollectionSummary call to prevent infinite loops
  // This was causing the infinite recursion issue
  
  // Exit object tracking
  context.ExitObject(element_addr);
  
  // For non-string objects, just return a generic marker
  return "<object>";
}

bool GNUstepNSArraySummaryProvider::IsGNUstepTaggedPointer(lldb::addr_t addr) {
  // GNUstep uses the low 3 bits for tagged pointers on 64-bit systems
  return (addr & 7) != 0;
}

std::string GNUstepNSArraySummaryProvider::GetTaggedPointerSummary(lldb::addr_t addr) {
  // This method should not be used anymore - tagged pointer decoding
  // is handled properly in TryExtractStringContent() using the introspector
  return "<tagged>";
}

std::string GNUstepNSArraySummaryProvider::TryExtractStringContent(Process *process, lldb::addr_t obj_addr) {
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
    
    // Check if it's a tagged number
    if (tag == 2) {
      return "<tagged_number>";
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
  printf("[GNUstepArray] Class name: %s, ISA: 0x%llx\n", class_name.c_str(), (unsigned long long)isa_addr);
  
  // Handle different string types based on their class
  if (class_name.find("NSConstantString") != std::string::npos || 
      class_name.find("__NSConstantString") != std::string::npos ||
      class_name.find("GSCString") != std::string::npos ||
      class_name.find("GSString") != std::string::npos ||
      class_name.find("NSString") != std::string::npos) {
    
    // For GNUstep constant strings, the structure is:
    // struct {
    //   Class isa;          // offset 0
    //   uint32_t len;       // offset 8
    //   uint32_t padding;   // offset 12
    //   uint64_t hash;      // offset 16 (sometimes used as second length field)
    //   const char *str;    // offset 24
    // };
    
    // Read the string pointer at offset 24
    lldb::addr_t str_ptr_addr = obj_addr + 24;
    lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
    printf("[GNUstepArray] String data pointer at 0x%llx: 0x%llx\n", 
           (unsigned long long)str_ptr_addr, (unsigned long long)str_data_addr);
    if (error.Fail() || str_data_addr == 0 || str_data_addr == LLDB_INVALID_ADDRESS) {
      printf("[GNUstepArray] Failed to read string data pointer\n");
      return "";
    }
    
    // Read the string length from offset 8
    lldb::addr_t len_addr = obj_addr + 8;
    uint32_t string_length = 0;
    
    if (!GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length))) {
      // If we can't read the length, try to read as null-terminated
      printf("[GNUstepArray] Failed to read string length at 0x%llx\n", (unsigned long long)len_addr);
      string_length = 0;
    } else {
      printf("[GNUstepArray] String length at 0x%llx: %u\n", (unsigned long long)len_addr, string_length);
    }
    
    // Sanity check the length
    if (string_length > 10000) {
      // Probably corrupted, try null-terminated instead
      string_length = 0;
    }
    
    // Limit string length for inline preview
    uint32_t preview_length = (string_length > 0 && string_length < 100) ? string_length : 100;
    
    if (string_length > 0) {
      std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, preview_length);
      // If we truncated, add ellipsis
      if (string_length > preview_length) {
        result += "...";
      }
      return result;
    }
    
    // Fallback: try to read as null-terminated string
    std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 100);
    if (result.length() >= 100) {
      result = result.substr(0, 97) + "...";
    }
    return result;
  }
  
  // For unknown string types, return empty
  return "";
}

// REMOVED: TryExtractCollectionSummary method to prevent infinite recursion
// This method was causing recursive loops when arrays contained other collections

//===----------------------------------------------------------------------===//
// NSArray Synthetic Children Provider
//===----------------------------------------------------------------------===//

GNUstepNSArraySyntheticProvider::GNUstepNSArraySyntheticProvider(
    lldb::ValueObjectSP valobj_sp)
    : GNUstepSyntheticProvider(valobj_sp),
      m_contents_array_ptr(LLDB_INVALID_ADDRESS),
      m_count(0),
      m_is_mutable(false),
      m_capacity(0),
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

bool GNUstepNSArraySyntheticProvider::UpdateImpl() {
  // Update execution context reference
  m_exe_ctx_ref = m_backend.GetExecutionContextRef();
  
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(m_backend);
  if (!process) {
    return false;
  }
  
  lldb::addr_t obj_addr = m_backend.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Clear previous state
  m_elements.clear();
  m_contents_array_ptr = LLDB_INVALID_ADDRESS;
  m_count = 0;
  m_is_mutable = false;
  m_capacity = 0;
  
  // Check if this is a mutable array
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(m_backend);
  m_is_mutable = (class_name.find("NSMutableArray") != std::string::npos ||
                  class_name.find("GSMutableArray") != std::string::npos);
  
  // Read _contents_array pointer at offset 8
  lldb::addr_t contents_ptr_addr = obj_addr + 8;
  Status error;
  m_contents_array_ptr = GNUstepRuntimeHelper::ReadPointer(process, contents_ptr_addr, error);
  if (error.Fail() || m_contents_array_ptr == 0 || m_contents_array_ptr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Read _count at offset 16
  lldb::addr_t count_addr = obj_addr + 16;
  if (!GNUstepRuntimeHelper::ReadMemory(process, count_addr, &m_count, sizeof(m_count))) {
    return false;
  }
  
  // For mutable arrays, also read _capacity at offset 20
  if (m_is_mutable) {
    lldb::addr_t capacity_addr = obj_addr + 20;
    GNUstepRuntimeHelper::ReadMemory(process, capacity_addr, &m_capacity, sizeof(m_capacity));
  }
  
  // Read the array elements
  return ReadArrayElements();
}

bool GNUstepNSArraySyntheticProvider::ReadArrayElements() {
  if (!m_process || m_contents_array_ptr == LLDB_INVALID_ADDRESS || m_count == 0) {
    return true; // Empty array is valid
  }
  
  // Add safety check to prevent excessive memory usage
  if (m_count > 1000000) {
    // Array is suspiciously large, probably corrupted data
    return false;
  }
  
  // Limit the number of elements we read for performance
  uint32_t elements_to_read = std::min(m_count, 256u);
  
  // Read element pointers one by one using the proper API
  // This handles endianness and pointer size correctly
  m_elements.clear();
  m_elements.reserve(elements_to_read);
  
  size_t ptr_size = GNUstepRuntimeHelper::GetAddressByteSize(m_process);
  
  // Safety check for ptr_size
  if (ptr_size == 0 || ptr_size > 16) {
    return false;
  }
  
  Status error;
  
  for (uint32_t i = 0; i < elements_to_read; ++i) {
    lldb::addr_t element_ptr_addr = m_contents_array_ptr + (i * ptr_size);
    lldb::addr_t element_addr = GNUstepRuntimeHelper::ReadPointer(m_process, element_ptr_addr, error);
    
    if (error.Fail()) {
      // If we fail to read an element, stop but keep what we've read so far
      break;
    }
    
    m_elements.push_back(element_addr);
  }
  
  return true;
}

llvm::Expected<uint32_t> GNUstepNSArraySyntheticProvider::CalculateNumChildren() {
  return m_count;
}

lldb::addr_t GNUstepNSArraySyntheticProvider::GetElementAtIndex(uint32_t idx) {
  if (idx >= m_count) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // If we have it cached, return it
  if (idx < m_elements.size()) {
    return m_elements[idx];
  }
  
  // Otherwise, read it from memory
  if (!m_process || m_contents_array_ptr == LLDB_INVALID_ADDRESS) {
    return LLDB_INVALID_ADDRESS;
  }
  
  size_t ptr_size = GNUstepRuntimeHelper::GetAddressByteSize(m_process);
  lldb::addr_t element_ptr_addr = m_contents_array_ptr + (idx * ptr_size);
  
  Status error;
  lldb::addr_t element_addr = GNUstepRuntimeHelper::ReadPointer(m_process, element_ptr_addr, error);
  if (error.Fail()) {
    return LLDB_INVALID_ADDRESS;
  }
  
  return element_addr;
}

lldb::ValueObjectSP GNUstepNSArraySyntheticProvider::GetChildAtIndex(uint32_t idx) {
  if (idx >= m_count) {
    return nullptr;
  }
  
  // CRITICAL FIX: We need to pass the address WHERE the pointer is stored,
  // not the pointer value itself. CreateValueObjectFromAddress will read
  // the pointer from that address and create the proper ValueObject.
  
  // Calculate the address in the array where this element pointer is stored
  size_t ptr_size = GNUstepRuntimeHelper::GetAddressByteSize(m_process);
  lldb::addr_t element_ptr_addr = m_contents_array_ptr + (idx * ptr_size);
  
  StreamString idx_name;
  idx_name.Printf("[%u]", idx);
  ExecutionContext exe_ctx(m_exe_ctx_ref);
  
  // Create a ValueObject from the address where the pointer is stored
  // This will properly read the pointer and format the object it points to
  return ValueObject::CreateValueObjectFromAddress(idx_name.GetString(), element_ptr_addr,
                                                   exe_ctx, m_id_type);
}

//===----------------------------------------------------------------------===//
// Function Wrappers for Registration
//===----------------------------------------------------------------------===//

bool formatters::GNUstepNSArrayFormatterFunction(ValueObject &valobj, Stream &stream, 
                                                 const TypeSummaryOptions &options) {
  printf("[GNUstepArray] Formatter function called\n");
  GNUstepNSArraySummaryProvider provider;
  return provider.FormatObject(valobj, stream, options);
}

SyntheticChildrenFrontEnd *
formatters::GNUstepNSArraySyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                                   lldb::ValueObjectSP valobj_sp) {
  return new GNUstepNSArraySyntheticProvider(valobj_sp);
}