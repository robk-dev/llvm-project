//===-- GNUstepArrayFormatters.cpp ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepArrayFormatters.h"
#include "GNUstepNumberFormatters.h"
#include "GNUstepDictionaryFormatters.h"
#include "GNUstepSetFormatters.h"
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
#include <cstdio>
#include <cstring>
#include <sstream>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

//===----------------------------------------------------------------------===//
// NSArray Summary Provider
//===----------------------------------------------------------------------===//

bool GNUstepNSArraySummaryProvider::FormatObject(ValueObject &valobj, 
                                                 Stream &stream, 
                                                 const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid object");
    return false;
  }
  
  uint32_t count = ExtractArrayCount(valobj);
  
  // Create formatter context to prevent infinite recursion
  FormatterContext context;
  
  // Format the summary of inline elements
  if (count == 0) {
    stream.Printf("()");
    return true;
  }
  
  // Show inline element preview
  // Limit to reasonable counts to avoid performance issues
  if (count <= MAX_COLLECTION_ELEMENTS_INLINE) {
    std::string inline_elements = GetInlineElementsPreview(valobj, count, context);
    if (!inline_elements.empty()) {
      stream.Printf("%s", inline_elements.c_str());
    } else {
      // Fallback to just showing count if preview fails
      stream.Printf("(%u elements)", count);
    }
  } else {
    // For large arrays, just show count
    stream.Printf("(%u elements)", count);
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
  
  
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read _contents_array pointer at offset 8
  lldb::addr_t contents_ptr_addr = obj_addr + 8;
  Status error;
  lldb::addr_t contents_array_ptr = GNUstepRuntimeHelper::ReadPointer(process, contents_ptr_addr, error);
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
    if (error.Fail() || element_addr == 0) {
      result += "<nil>";
      continue;
    }
    
    // Try to get a string representation of the element with recursion protection
    std::string element_summary = GetElementSummary(process, element_addr, context);
    
    // Debug: If we got a raw address back, it means formatting failed
    if (!element_summary.empty() && element_summary.find("0x") == 0) {
      // Try direct string extraction as a fallback
      std::string direct_string = TryExtractStringContent(process, element_addr);
      if (!direct_string.empty()) {
        element_summary = "\"" + direct_string + "\"";
      }
    }
    
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
  
  // Always try to extract string content first (handles both tagged pointers and regular strings)
  // This includes tagged strings which are the most common case
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
    // For non-string tagged pointers, try to get proper formatting
    uint64_t tag = element_addr & 7;
    
    // For tagged numbers, decode directly using the same logic as GNUstepNumberFormatters
    if (tag == 1 || tag == 2 || tag == 3 || tag == 5) {
      const int SMALL_OBJECT_SHIFT = 3;
      
      if (tag == 1) {
        // NSSmallInt - integer value is ptr >> 3
        int64_t int_value = ((int64_t)element_addr) >> SMALL_OBJECT_SHIFT;
        context.ExitObject(element_addr);
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
        context.ExitObject(element_addr);
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
        context.ExitObject(element_addr);
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
        context.ExitObject(element_addr);
        return std::string(buffer);
      }
      
      // Unknown tagged number type
      context.ExitObject(element_addr);
      return "<NSNumber>";
    }
    
    context.ExitObject(element_addr);
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
      ExecutionContextScope *exe_scope = process->GetTarget().GetProcessSP().get();
      if (exe_scope) {
        TypeSystemClangSP scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(
            process->GetTarget());
        if (scratch_ts_sp) {
          CompilerType id_type = scratch_ts_sp->GetType(
              scratch_ts_sp->getASTContext().ObjCBuiltinIdTy);
          
          // CRITICAL FIX: Create ValueObject from ADDRESS, not from data containing pointer
          // This allows LLDB to properly resolve the object and apply formatters
          ExecutionContext exe_ctx;
          exe_scope->CalculateExecutionContext(exe_ctx);
          ValueObjectSP valobj_sp = ValueObject::CreateValueObjectFromAddress(
              "element", element_addr, exe_ctx, id_type);
          
          if (valobj_sp) {
            // Use the GNUstep NSNumber formatter directly
            GNUstepNSNumberSummaryProvider number_formatter;
            StreamString number_stream;
            TypeSummaryOptions number_options;
            if (number_formatter.FormatObject(*valobj_sp, number_stream, number_options)) {
              context.ExitObject(element_addr);
              return number_stream.GetString().str();
            }
          }
        }
      }
      
      // Fallback for NSNumber if formatter fails
      context.ExitObject(element_addr);
      return "<NSNumber>";
    }
  }
  
  // For other regular objects, create a ValueObject
  // and use LLDB's formatting system to get the proper summary
  ExecutionContextScope *exe_scope = process->GetTarget().GetProcessSP().get();
  if (exe_scope) {
    // Get the ObjC id type from the scratch TypeSystem
    TypeSystemClangSP scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(
        process->GetTarget());
    if (scratch_ts_sp) {
      CompilerType id_type = scratch_ts_sp->GetType(
          scratch_ts_sp->getASTContext().ObjCBuiltinIdTy);
      
      // CRITICAL FIX: Create ValueObject from ADDRESS, not from data containing pointer
      // This allows LLDB to properly resolve nested objects and apply formatters recursively
      ExecutionContext exe_ctx;
      exe_scope->CalculateExecutionContext(exe_ctx);
      ValueObjectSP valobj_sp = ValueObject::CreateValueObjectFromAddress(
          "element", element_addr, exe_ctx, id_type);
      
      if (valobj_sp) {
        // CRITICAL FIX: Manually apply GNUstep formatters since LLDB may not
        // automatically select them for nested objects created programmatically
        
        // Try to get the class name to determine which formatter to use
        std::string class_name = introspector.GetClassName(isa_addr);
        
        // For nested collections, extract count directly using the same logic as the main formatters
        if (class_name.find("Dictionary") != std::string::npos ||
            class_name.find("NSDictionary") != std::string::npos) {
          // Use same count extraction as GNUstepNSDictionarySummaryProvider::ExtractDictionaryCount
          // Dictionary count is at obj_addr + 16 (8 for isa + 8 for map ptr + 8 for nodeCount)
          lldb::addr_t count_addr = element_addr + 16;
          uint64_t count64 = 0;
          if (GNUstepRuntimeHelper::ReadMemory(process, count_addr, &count64, sizeof(count64))) {
            uint32_t nested_count = static_cast<uint32_t>(count64);
            if (nested_count > 0 && nested_count < 1000000) { // Sanity check
              context.ExitObject(element_addr);
              if (nested_count == 1) {
                return "@{1 pair}";
              } else {
                return "@{" + std::to_string(nested_count) + " pairs}";
              }
            }
          }
          context.ExitObject(element_addr);
          return "@{...}";
        } else if (class_name.find("Array") != std::string::npos ||
                   class_name.find("NSArray") != std::string::npos) {
          // Use same count extraction as GNUstepNSArraySummaryProvider::ExtractArrayCount
          // Array count is at obj_addr + 16 (8 for isa + 8 for contents_array ptr)
          lldb::addr_t count_addr = element_addr + 16;
          uint32_t nested_count = 0;
          if (GNUstepRuntimeHelper::ReadMemory(process, count_addr, &nested_count, sizeof(nested_count))) {
            if (nested_count > 0 && nested_count < 1000000) { // Sanity check
              context.ExitObject(element_addr);
              if (nested_count == 1) {
                return "@[1 object]";
              } else {
                return "@[" + std::to_string(nested_count) + " objects]";
              }
            }
          }
          context.ExitObject(element_addr);
          return "@[...]";
        } else if (class_name.find("Set") != std::string::npos ||
                   class_name.find("NSSet") != std::string::npos) {
          // For nested sets, show simple placeholder (Set count extraction is more complex)
          context.ExitObject(element_addr);
          return "{set}";
        }
        
        // Fallback to LLDB's automatic summary
        const char *summary = valobj_sp->GetSummaryAsCString();
        if (summary && strlen(summary) > 0) {
          context.ExitObject(element_addr);
          return summary;
        }
        
        // If no summary, try to get the value as a string
        StreamString value_stream;
        DumpValueObjectOptions options;
        options.SetHideRootType(true).SetHideName(true).SetHideValue(false);
        llvm::Error error = valobj_sp->Dump(value_stream, options);
        if (!error) {
          std::string result = value_stream.GetString().str();
          if (!result.empty()) {
            // CRITICAL FIX: Clean up the result - remove newlines and extra whitespace
            // that could break the inline array display format
            size_t pos = result.find('\n');
            if (pos != std::string::npos) {
              result = result.substr(0, pos);
            }
            // Trim trailing whitespace
            while (!result.empty() && std::isspace(result.back())) {
              result.pop_back();
            }
            context.ExitObject(element_addr);
            return result;
          }
        } else {
          // Consume the error to avoid assertion failures
          llvm::consumeError(std::move(error));
        }
      }
    }
  }
  
  // Exit object tracking
  context.ExitObject(element_addr);
  
  // For non-string objects where we couldn't get a better summary
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
    
    // Check if it's a tagged number - return empty to let GetElementSummary handle it properly
    if (tag == 2 || tag == 3 || tag == 5 || tag == 1) {
      // Don't handle tagged numbers here - let GetElementSummary handle them
      // This function is specifically for string content extraction
      return "";
    }
    
    // Not a string tagged pointer
    return "";
  }
  
  // For regular NSString objects, we need to properly extract the content
  // by using the same logic as the standalone string formatter
  
  // Special check: if obj_addr points to a known class object, not an instance
  // This is a workaround for a compiler/linker bug where NSConstantString class
  // is stored in arrays instead of string instances
  // Check if this address looks like it's from the library's data section
  if ((obj_addr & 0x7ffff7000000) == 0x7ffff7000000) {
    // This looks like a library address, check if it's actually the NSConstantString class
    Status test_error;
    lldb::addr_t test_isa = GNUstepRuntimeHelper::ReadPointer(process, obj_addr, test_error);
    if (!test_error.Fail() && test_isa != 0) {
      // Check if what we read as ISA is actually pointing to a metaclass
      // For the NSConstantString class object, the ISA would be NSConstantString metaclass
      std::string test_name = introspector.GetClassName(test_isa);
      if (test_name.find("METACLASS") != std::string::npos || test_name.empty()) {
        // This is likely a class object, not an instance
        // This is a compiler bug - the array literal syntax is storing the class object
        // instead of a string instance. Check if this is NSConstantString class.
        std::string class_name = introspector.GetClassName(obj_addr);
        if (class_name == "NSConstantString" || class_name == "__NSConstantString") {
          return "<NSConstantString class>";
        }
        return "<class object>";
      }
    }
  }
  
  // First, read the ISA pointer to determine the exact string type
  Status error;
  lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, obj_addr, error);
  if (error.Fail() || isa_addr == 0) {
    return "";
  }
  
  // Get the class name from the ISA
  std::string class_name = introspector.GetClassName(isa_addr);
  
  // DEBUG: Print what we got
  if (obj_addr == 0x00007ffff7d9f6f8) {
    printf("[DEBUG] TryExtractStringContent for 0x%llx: ISA=0x%llx, class_name='%s'\n", 
           (unsigned long long)obj_addr, (unsigned long long)isa_addr, class_name.c_str());
  }
  
  // If we couldn't get the class name, try checking if it looks like NSConstantString
  // by its memory layout or known ISA addresses
  if (class_name.empty()) {
    // Try a fallback - check if this could be NSConstantString
    // by reading what should be the string pointer at offset 8
    lldb::addr_t potential_str_ptr = GNUstepRuntimeHelper::ReadPointer(process, obj_addr + 8, error);
    if (!error.Fail() && potential_str_ptr != 0) {
      // Try to read a few bytes to see if it looks like a string
      char test_buf[16] = {0};
      size_t bytes_read = process->ReadMemory(potential_str_ptr, test_buf, 15, error);
      if (!error.Fail() && bytes_read > 0) {
        // Check if it looks like printable text
        bool looks_like_string = true;
        for (size_t i = 0; i < bytes_read && test_buf[i] != 0; i++) {
          if (!isprint(test_buf[i]) && test_buf[i] != '\n' && test_buf[i] != '\t') {
            looks_like_string = false;
            break;
          }
        }
        if (looks_like_string) {
          // Treat it as NSConstantString
          class_name = "NSConstantString";
        }
      }
    }
  }
  
  // Handle different string types based on their class
  if (class_name.find("NSConstantString") != std::string::npos || 
      class_name.find("__NSConstantString") != std::string::npos ||
      class_name.find("_NSConstantString") != std::string::npos) {
    // NSConstantString layout (compile-time constant strings that aren't tagged):
    // struct {
    //   Class isa;          // offset 0
    //   const char *str;    // offset 8  <-- String pointer is HERE
    //   uint32_t len;       // offset 16
    // };
    
    // Read the string pointer at offset 8
    lldb::addr_t str_ptr_addr = obj_addr + 8;
    lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
    
    if (error.Success() && str_data_addr != 0 && str_data_addr != LLDB_INVALID_ADDRESS) {
      // Read the length from offset 16
      lldb::addr_t len_addr = obj_addr + 16;
      uint32_t string_length = 0;
      GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length));
      
      // Limit string length for inline preview
      uint32_t preview_length = (string_length > 0 && string_length < 100) ? string_length : 100;
      
      std::string result = GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, preview_length);
      if (!result.empty()) {
        return result;
      }
    }
  } else if (class_name.find("GSCString") != std::string::npos ||
             class_name.find("GSCInlineString") != std::string::npos ||
             class_name.find("GSString") != std::string::npos ||
             class_name.find("NSString") != std::string::npos) {
    
    // GSString and other runtime strings layout:
    // struct {
    //   Class isa;          // offset 0
    //   uint32_t len;       // offset 8
    //   uint32_t padding;   // offset 12
    //   uint64_t hash;      // offset 16
    //   const char *str;    // offset 24  <-- String pointer is HERE
    // };
    
    // Read the string pointer at offset 24
    lldb::addr_t str_ptr_addr = obj_addr + 24;
    lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
    if (error.Fail() || str_data_addr == 0 || str_data_addr == LLDB_INVALID_ADDRESS) {
      return "";
    }
    
    // Read the string length from offset 8
    lldb::addr_t len_addr = obj_addr + 8;
    uint32_t string_length = 0;
    
    if (!GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length))) {
      // If we can't read the length, try to read as null-terminated
      string_length = 0;
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
  
  // CRITICAL: Store the process reference for use in GetChildAtIndex
  m_process = process;
  
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
  
  // Check the actual runtime class name to determine array type
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(m_backend);
  m_is_mutable = (class_name.find("NSMutableArray") != std::string::npos ||
                  class_name.find("GSMutableArray") != std::string::npos);
  
  // Handle GSInlineArray differently from regular GSArray
  bool is_inline_array = (class_name.find("GSInlineArray") != std::string::npos);
  
  if (is_inline_array) {
    // GSInlineArray structure (elements stored inline):
    // struct {
    //   Class isa;          // offset 0
    //   id *_contents;      // offset 8  (points to inline elements)
    //   unsigned _count;    // offset 16
    // }
    // The elements are stored immediately after the object header
    
    // Read count first from offset 16 - use same logic as summary provider
    lldb::addr_t count_addr = obj_addr + 16;
    uint32_t temp_count = 0;
    if (!GNUstepRuntimeHelper::ReadMemory(process, count_addr, &temp_count, sizeof(temp_count))) {
      return false;
    }
    m_count = temp_count;
    
    // For inline arrays, the _contents pointer at offset 8 points to where
    // the elements are stored (usually right after the object structure)
    lldb::addr_t contents_ptr_addr = obj_addr + 8;
    Status error;
    m_contents_array_ptr = GNUstepRuntimeHelper::ReadPointer(process, contents_ptr_addr, error);
    if (error.Fail() || m_contents_array_ptr == 0 || m_contents_array_ptr == LLDB_INVALID_ADDRESS) {
      return false;
    }
    
  } else {
    // Regular GSArray structure:
    // struct {
    //   Class isa;             // offset 0
    //   id *_contents_array;   // offset 8
    //   unsigned _count;       // offset 16
    // }
    
    // Read _contents_array pointer at offset 8
    lldb::addr_t contents_ptr_addr = obj_addr + 8;
    Status error;
    m_contents_array_ptr = GNUstepRuntimeHelper::ReadPointer(process, contents_ptr_addr, error);
    if (error.Fail() || m_contents_array_ptr == 0 || m_contents_array_ptr == LLDB_INVALID_ADDRESS) {
      return false;
    }
    
    // Read _count at offset 16 - use same logic as summary provider
    lldb::addr_t count_addr = obj_addr + 16;
    uint32_t temp_count = 0;
    if (!GNUstepRuntimeHelper::ReadMemory(process, count_addr, &temp_count, sizeof(temp_count))) {
      return false;
    }
    m_count = temp_count;
    
    // For mutable arrays, also read _capacity at offset 20
    if (m_is_mutable) {
      lldb::addr_t capacity_addr = obj_addr + 20;
      GNUstepRuntimeHelper::ReadMemory(process, capacity_addr, &m_capacity, sizeof(m_capacity));
    }
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

CompilerType GNUstepNSArraySyntheticProvider::GetConcreteTypeForObject(lldb::addr_t obj_addr) {
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

lldb::ValueObjectSP GNUstepNSArraySyntheticProvider::GetChildAtIndex(uint32_t idx) {
  if (idx >= m_count) {
    return nullptr;
  }
  
  // Get the element value (could be a real pointer or tagged pointer)
  lldb::addr_t element_value = GetElementAtIndex(idx);
  if (element_value == LLDB_INVALID_ADDRESS || element_value == 0) {
    return nullptr;
  }
  
  // Create the child name
  StreamString idx_name;
  idx_name.Printf("[%u]", idx);
  
  // Get execution context
  ExecutionContext exe_ctx(m_exe_ctx_ref);
  
  // Use the generic 'id' type for all synthetic children
  CompilerType element_type = GetConcreteTypeForObject(element_value);
  if (!element_type.IsValid()) {
    element_type = m_id_type;
  }
  
  // CRITICAL FIX: Handle tagged pointers vs. real object pointers differently
  // GNUstep uses tagged pointers extensively for strings and numbers
  
  // Check if this is a tagged pointer (low 3 bits set)
  bool is_tagged_pointer = (element_value & 0x7) != 0;
  
  if (is_tagged_pointer) {
    // For tagged pointers, create ValueObject from DATA, not ADDRESS
    // The tagged pointer value IS the data, not a pointer to memory
    
    // Create a data buffer containing the tagged pointer value (following Apple's pattern)
    size_t ptr_size = exe_ctx.GetAddressByteSize();
    DataBufferSP buffer_sp;
    
    if (ptr_size == 8) {
      uint64_t value64 = element_value;
      buffer_sp = DataBufferSP(new DataBufferHeap(&value64, sizeof(uint64_t)));
    } else {
      uint32_t value32 = static_cast<uint32_t>(element_value);
      buffer_sp = DataBufferSP(new DataBufferHeap(&value32, sizeof(uint32_t)));
    }
    
    // Create DataExtractor from the buffer
    DataExtractor data_extractor(buffer_sp, exe_ctx.GetByteOrder(), ptr_size);
    
    // Create ValueObject from the data buffer containing the tagged pointer
    return ValueObject::CreateValueObjectFromData(idx_name.GetString(), 
                                                  data_extractor, exe_ctx, element_type);
  } else {
    // For regular object pointers, calculate the memory address where the pointer is stored
    // and let LLDB read it and apply dynamic type resolution normally
    
    // Calculate the address in the contents array where this pointer is stored
    if (!m_process || m_contents_array_ptr == LLDB_INVALID_ADDRESS) {
      return nullptr;
    }
    
    size_t ptr_size = GNUstepRuntimeHelper::GetAddressByteSize(m_process);
    lldb::addr_t pointer_storage_addr = m_contents_array_ptr + (idx * ptr_size);
    
    // Create ValueObject from the ADDRESS where the pointer is stored
    // LLDB will read the pointer from that address and apply dynamic type resolution
    return ValueObject::CreateValueObjectFromAddress(idx_name.GetString(), 
                                                     pointer_storage_addr, 
                                                     exe_ctx, element_type);
  }
}

//===----------------------------------------------------------------------===//
// Function Wrappers for Registration
//===----------------------------------------------------------------------===//

bool formatters::GNUstepNSArrayFormatterFunction(ValueObject &valobj, Stream &stream, 
                                                 const TypeSummaryOptions &options) {
  GNUstepNSArraySummaryProvider provider;
  return provider.FormatObject(valobj, stream, options);
}

// Compatibility function for ObjC language plugin
bool formatters::GNUstepArraySummaryProvider(ValueObject &valobj, Stream &stream,
                                            const TypeSummaryOptions &options) {
  return GNUstepNSArrayFormatterFunction(valobj, stream, options);
}

SyntheticChildrenFrontEnd *
formatters::GNUstepNSArraySyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                                   lldb::ValueObjectSP valobj_sp) {
  return new GNUstepNSArraySyntheticProvider(valobj_sp);
}

// Compatibility function for ObjC language plugin
SyntheticChildrenFrontEnd *
formatters::GNUstepArraySyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                                lldb::ValueObjectSP valobj_sp) {
  return GNUstepNSArraySyntheticFrontEndCreator(synth, valobj_sp);
}