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
#include "GNUstepPerformanceTimer.h"
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

// Forward declarations
static std::string DecodeTaggedStringOptimized(lldb::addr_t tagged_addr);
static std::string DecodeTaggedNumberOptimized(lldb::addr_t tagged_addr);

//===----------------------------------------------------------------------===//
// Global Recursion Protection
//===----------------------------------------------------------------------===//

// CRITICAL FIX: Thread-local static recursion guard that persists across all
// formatter invocations to prevent infinite loops when LLDB's formatting
// system creates new ValueObjects that trigger our formatters recursively
namespace {
  thread_local std::unordered_set<lldb::addr_t> g_formatting_addresses;
  thread_local uint32_t g_formatting_depth = 0;
  
  class GlobalFormatterGuard {
  public:
    explicit GlobalFormatterGuard(lldb::addr_t addr) 
      : m_addr(addr), m_should_track(false) {
      g_formatting_depth++;
      
      // Only track if not already being formatted and depth is reasonable
      if (g_formatting_depth < MAX_FORMATTER_DEPTH && 
          g_formatting_addresses.find(addr) == g_formatting_addresses.end()) {
        g_formatting_addresses.insert(addr);
        m_should_track = true;
      }
    }
    
    ~GlobalFormatterGuard() {
      if (m_should_track) {
        g_formatting_addresses.erase(m_addr);
      }
      if (g_formatting_depth > 0) {
        g_formatting_depth--;
      }
    }
    
    bool should_stop() const {
      return g_formatting_depth >= MAX_FORMATTER_DEPTH || 
             (!m_should_track && g_formatting_addresses.count(m_addr) > 0);
    }
    
  private:
    lldb::addr_t m_addr;
    bool m_should_track;
  };
}

//===----------------------------------------------------------------------===//
// NSArray Summary Provider
//===----------------------------------------------------------------------===//

bool GNUstepNSArraySummaryProvider::FormatObject(ValueObject &valobj, 
                                                 Stream &stream, 
                                                 const TypeSummaryOptions &options) {
  GNUSTEP_PERFORMANCE_TIMER("NSArraySummary");
  
  // Debug logging removed for performance - use LLDB logging system if needed
  
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
  
  // ExtractArrayCount called
  
  // CRITICAL FIX: For custom class properties, we need to get the actual class name 
  // of the object at obj_addr, not the ValueObject's declared type
  std::string class_name;
  
  // First, try to get the runtime class name by reading the ISA pointer
  Status error;
  lldb::addr_t isa_addr = process->ReadPointerFromMemory(obj_addr, error);
  // ISA read attempted
  
  if (!error.Fail() && isa_addr != 0 && isa_addr != LLDB_INVALID_ADDRESS) {
    // Try to get class name from introspector using the ISA
    GNUstepObjCRuntimeIntrospector introspector(process);
    class_name = introspector.GetClassName(isa_addr);
    // Got class name from introspector
  }
  
  // If introspector failed, fall back to ValueObject's class name
  if (class_name.empty() || class_name == "<unknown>") {
    class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
    // Got class name from ValueObject
  }
  
  // If still no luck, use fallback
  if (class_name.empty() || class_name == "<unknown>") {
    class_name = "GSArray"; // Fallback to most common array class
    // Using fallback class name
  }
  
  // CRITICAL WORKAROUND: Detect and handle ISA pointer bug
  // Check if this looks like an ISA pointer issue by examining the class name
  if (class_name == "GSMutableArray" || class_name == "GSArray") {
    // This suggests we got a valid ISA and found a real array class
    // But if obj_addr + 16 gives us garbage, we know obj_addr is wrong
    
    // Try reading at the supposed count location  
    lldb::addr_t test_count_addr = obj_addr + 16;
    uint32_t test_count = 0;
    bool read_success = GNUstepRuntimeHelper::ReadMemory(process, test_count_addr, &test_count, sizeof(test_count));
    
    // Testing count read
    
    // If the count is clearly corrupted (> 1 million), we likely have the ISA pointer problem
    if (!read_success || test_count > 1000000) {
      // ISA pointer bug detected
      
      // WORKAROUND: Since we can't easily find the real object address from the ISA,
      // we'll try to extract the count from the ValueObject directly if possible
      // This is a fallback that should work for most cases
      
      // Try to get count from ValueObject's summary or child count if available
      if (valobj.MightHaveChildren()) {
        llvm::Expected<uint32_t> child_count = valobj.GetNumChildren();
        if (child_count && *child_count <= 1000000) {
          // Using ValueObject child count
          return *child_count;
        }
      }
      
      // If that fails, return 0 to avoid showing garbage
      // Cannot recover from ISA pointer bug
      return 0;
    }
  }
  
  // GNUstep GSArray structure (from GSPrivate.h):
  // @interface GSArray : NSArray
  // {
  // @public
  //   id         *_contents_array;
  //   unsigned   _count;
  // }
  // @end
  
  ptrdiff_t count_offset = GNUstepRuntimeHelper::GetIvarOffset(process, class_name, "_count");
  if (count_offset < 0) {
    // Fallback to hardcoded offset for backward compatibility
    count_offset = 16; // 8 bytes for isa + 8 bytes for _contents_array pointer
    // Using fallback offset
  } else {
    // Dynamic offset for _count found
  }
  
  lldb::addr_t count_addr = obj_addr + count_offset;
  uint32_t count = 0;
  
  // Reading count from calculated address
  
  if (!GNUstepRuntimeHelper::ReadMemory(process, count_addr, &count, sizeof(count))) {
    // Failed to read count
    return 0;
  }
  
  // Successfully read count
  
  // CRITICAL FIX: Detect and correct ISA pointer bug
  if (count > 1000000) {
    // Detected corrupted count - ISA pointer bug
           
    // WORKAROUND: Try to get the correct count from the ValueObject
    if (valobj.MightHaveChildren()) {
      llvm::Expected<uint32_t> child_count = valobj.GetNumChildren();
      if (child_count && *child_count < 1000000) {
        // Fixed: Using ValueObject child count
        return *child_count;
      }
    }
    
    // If we can't get a valid count, return 0 to avoid showing garbage
    // Fixed: Returning 0 instead of corrupted count
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
  
  // Get dynamic offset for _contents_array field
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  if (class_name.empty() || class_name == "<unknown>") {
    class_name = "GSArray"; // Fallback to most common array class
  }
  
  ptrdiff_t contents_offset = GNUstepRuntimeHelper::GetIvarOffset(process, class_name, "_contents_array");
  if (contents_offset < 0) {
    // Fallback to hardcoded offset for backward compatibility
    contents_offset = 8; // After isa pointer
  }
  
  lldb::addr_t contents_ptr_addr = obj_addr + contents_offset;
  Status error;
  lldb::addr_t contents_array_ptr = GNUstepRuntimeHelper::ReadPointer(process, contents_ptr_addr, error);
  if (error.Fail() || contents_array_ptr == 0 || contents_array_ptr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Build inline preview - show up to MAX_COLLECTION_ELEMENTS_INLINE elements to avoid performance issues
  uint32_t preview_limit = std::min(count, MAX_COLLECTION_ELEMENTS_INLINE);
  
  // PERFORMANCE OPTIMIZATION: Pre-allocate string with estimated capacity
  size_t estimated_capacity = 2 + (preview_limit * 12) + 10; // "@[" + elements + margin
  std::string result;
  result.reserve(estimated_capacity);
  result = "@[";
  
  size_t ptr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // Add safety check to prevent infinite loops
  if (preview_limit == 0 || ptr_size == 0) {
    result += "]";
    return result;
  }
  
  // Performance optimized - removed debug file I/O
  
  for (uint32_t i = 0; i < preview_limit && i < MAX_LOOP_ITERATIONS; ++i) {
    if (i > 0) {
      result += ", ";
    }
    
    // PERFORMANCE OPTIMIZATION: Early termination for deeply nested contexts
    if (context.depth >= MAX_FORMATTER_DEPTH - 1) {
      result += "...";
      break;
    }
    
    // Read element pointer with bounds checking
    lldb::addr_t element_ptr_addr = contents_array_ptr + (i * ptr_size);
    lldb::addr_t element_addr = GNUstepRuntimeHelper::ReadPointer(process, element_ptr_addr, error);
    if (error.Fail() || element_addr == 0) {
      result += "<nil>";
      continue;
    }
    
    // Tagged pointer detection optimized
    
    // Try to get a string representation of the element with recursion protection
    std::string element_summary = GetElementSummary(process, element_addr, context);
    
    // Element summary generation optimized
    
    // PERFORMANCE OPTIMIZATION: Limit nested collection detail in deep contexts
    if (context.depth >= 2 && !element_summary.empty() && 
        (element_summary.find("@[") == 0 || element_summary.find("@{") == 0)) {
      // For nested collections at depth 2+, show simplified representation
      if (element_summary.find("@[") == 0) {
        result += "@[...]";
      } else {
        result += "@{...}";
      }
      continue;
    }
    
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
  
  // Performance optimization: debug logging removed
  
  return result;
}

std::string GNUstepNSArraySummaryProvider::GetElementSummary(Process *process, lldb::addr_t element_addr, FormatterContext &context) {
  if (!process || element_addr == 0 || element_addr == LLDB_INVALID_ADDRESS) {
    return "<nil>";
  }
  
  // CRITICAL FIX: Use global recursion guard to prevent infinite loops
  // This persists across LLDB's ValueObject creation and formatting calls
  GlobalFormatterGuard global_guard(element_addr);
  if (global_guard.should_stop()) {
    return "<...>"; // Stop infinite recursion
  }
  
  // Check for recursion depth limit and cycle detection (local context)
  if (context.ShouldStopRecursion(element_addr)) {
    return "<...>"; // Indicate recursion was stopped
  }
  
  // Enter this object in our recursion tracking
  context.EnterObject(element_addr);
  
  // CRITICAL FIX: Check for tagged pointers FIRST, before trying to read ISA
  // Tagged pointers encode data directly in the pointer value, not as memory addresses
  if ((element_addr & 0x7) != 0) {
    // It's a tagged pointer - handle immediately without trying to read ISA
    GNUstepObjCRuntimeIntrospector introspector(process);
    uint64_t tag = element_addr & 0x7;
    
    if (tag == 4) {
      // Tagged string - decode it properly
      std::string decoded = introspector.DecodeTaggedString(element_addr);
      context.ExitObject(element_addr);
      if (!decoded.empty()) {
        return "\"" + decoded + "\"";
      }
      // If decoding failed, return empty to let fallback handle it
      return "";
    }
    
    if (tag == 1 || tag == 3) {
      // Tagged number - decode it properly
      std::string decoded = DecodeTaggedNumberOptimized(element_addr);
      context.ExitObject(element_addr);
      return decoded.empty() ? "<NSNumber>" : decoded;
    }
    
    // Other tagged pointer types
    context.ExitObject(element_addr);
    return "<tagged>";
  }
  
  // For regular objects, read the ISA pointer
  Status isa_error;
  lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, element_addr, isa_error);
  if (isa_error.Fail()) {
    // Failed to read ISA for a non-tagged pointer - this is an error
    context.ExitObject(element_addr);
    return "<invalid>";
  }
  
  GNUstepObjCRuntimeIntrospector introspector(process);
  
  if (isa_addr != 0) {
    std::string class_name = introspector.GetClassName(isa_addr);
    
    // Handle NSNull immediately without further processing
    if (class_name.find("NSNull") != std::string::npos) {
      context.ExitObject(element_addr);
      return "<null>";
    }
    
    // Handle other special cases that shouldn't be processed recursively
    if (class_name.empty() || class_name == "<unknown>") {
      context.ExitObject(element_addr);
      return "<object>";
    }
  } else {
    // ISA is null/zero
    context.ExitObject(element_addr);
    return "<nil>";
  }
  
  // Try to extract string content for regular (non-tagged) strings
  std::string string_content = TryExtractStringContent(process, element_addr);
  if (!string_content.empty()) {
    context.ExitObject(element_addr);
    // Return quoted string, truncated for inline display
    if (string_content.length() > MAX_STRING_PREVIEW_LENGTH) {
      return "\"" + string_content.substr(0, MAX_STRING_PREVIEW_LENGTH - 3) + "...\"";
    }
    return "\"" + string_content + "\"";
  }
  
  // For regular objects (non-tagged), check if it's an NSNumber and handle it specially
  // We already have isa_addr and introspector from above
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
            // CRITICAL FIX: For NSNumber objects in collections, manually extract the value
            // instead of relying on the complex NSNumber formatter which may fail in nested contexts
            
            // Read the ISA pointer first to get the exact class name
            Status isa_error;
            lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, element_addr, isa_error);
            if (isa_error.Success() && isa_addr != 0) {
              std::string exact_class_name = introspector.GetClassName(isa_addr);
              
              // Handle different NSNumber subclass types by reading value directly from memory
              if (exact_class_name.find("IntNumber") != std::string::npos) {
                // NSIntNumber stores int32_t at offset 8
                int32_t int_value = 0;
                if (GNUstepRuntimeHelper::ReadMemory(process, element_addr + 8, &int_value, sizeof(int_value))) {
                  context.ExitObject(element_addr);
                  return std::to_string(int_value);
                }
              } else if (exact_class_name.find("LongLongNumber") != std::string::npos) {
                // NSLongLongNumber stores int64_t at offset 8  
                int64_t ll_value = 0;
                if (GNUstepRuntimeHelper::ReadMemory(process, element_addr + 8, &ll_value, sizeof(ll_value))) {
                  context.ExitObject(element_addr);
                  return std::to_string(ll_value);
                }
              } else if (exact_class_name.find("FloatNumber") != std::string::npos) {
                // NSFloatNumber stores float at offset 8
                float float_value = 0.0f;
                if (GNUstepRuntimeHelper::ReadMemory(process, element_addr + 8, &float_value, sizeof(float_value))) {
                  context.ExitObject(element_addr);
                  char buffer[32];
                  snprintf(buffer, sizeof(buffer), "%g", float_value);
                  return std::string(buffer);
                }
              } else if (exact_class_name.find("DoubleNumber") != std::string::npos) {
                // NSDoubleNumber stores double at offset 8
                double double_value = 0.0;
                if (GNUstepRuntimeHelper::ReadMemory(process, element_addr + 8, &double_value, sizeof(double_value))) {
                  context.ExitObject(element_addr);
                  char buffer[32];  
                  snprintf(buffer, sizeof(buffer), "%g", double_value);
                  return std::string(buffer);
                }
              } else if (exact_class_name.find("BoolNumber") != std::string::npos) {
                // NSBoolNumber stores BOOL at offset 8
                uint32_t bool_value = 0;
                if (GNUstepRuntimeHelper::ReadMemory(process, element_addr + 8, &bool_value, sizeof(bool_value))) {
                  context.ExitObject(element_addr);
                  return (bool_value != 0) ? "YES" : "NO";
                }
              }
            }
            
            // Fallback: Use the GNUstep NSNumber formatter
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
        
        // CRITICAL FIX: AVOID GetSummaryAsCString() which can cause infinite recursion
        // Instead, try to extract basic information directly without triggering formatters
        
        // For unknown object types, just show the class name if we have it
        if (!class_name.empty() && class_name != "<unknown>") {
          context.ExitObject(element_addr);
          return "<" + class_name + ">";
        }
        
        // CRITICAL FIX: AVOID Dump() which can also trigger recursive formatting
        // For unknown objects, just return a generic placeholder
      }
    }
  }
  
  // Exit object tracking
  context.ExitObject(element_addr);
  
  // For non-string objects where we couldn't get a better summary
  return "";
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
      
      // Enhanced fallback: try different decoding approaches if standard fails
      // Some tagged strings might use slightly different encoding
      
      // Try manual decode with different parameters
      int length = (obj_addr >> 3) & 0x1f;
      if (length > 0 && length <= 9) {
        std::string manual_result;
        manual_result.reserve(length);
        bool all_printable = true;
        
        for (int i = 0; i < length; i++) {
          uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
          char c = (obj_addr & mask) >> (57 - (i * 7));
          if (c >= 0x20 && c <= 0x7e) {
            manual_result += c;
          } else {
            all_printable = false;
            break;
          }
        }
        
        if (all_printable && !manual_result.empty()) {
          return manual_result;
        }
      }
      
      // Final fallback: show the raw tagged pointer value for debugging
      char buffer[64];
      snprintf(buffer, sizeof(buffer), "<tagged_str_0x%llx>", 
               (unsigned long long)obj_addr);
      return std::string(buffer);
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
    // NEW_ABI (GNUstep 2.1+):
    // struct {
    //   Class isa;          // offset 0
    //   uint32_t flags;     // offset 8
    //   uint32_t length;    // offset 12 <-- String length
    //   uint32_t size;      // offset 16
    //   uint32_t hash;      // offset 20
    //   const char *str;    // offset 24 <-- String pointer
    // };
    
    // Get dynamic offsets for NSConstantString
    ptrdiff_t str_offset = GNUstepRuntimeHelper::GetIvarOffset(process, "NSConstantString", "str");
    if (str_offset < 0) {
      str_offset = 24; // Fallback to hardcoded offset for NEW_ABI
    }
    
    ptrdiff_t len_offset = GNUstepRuntimeHelper::GetIvarOffset(process, "NSConstantString", "length");
    if (len_offset < 0) {
      len_offset = 12; // Fallback to hardcoded offset for NEW_ABI
    }
    
    // Read the string pointer using dynamic offset
    lldb::addr_t str_ptr_addr = obj_addr + str_offset;
    lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
    
    if (error.Success() && str_data_addr != 0 && str_data_addr != LLDB_INVALID_ADDRESS) {
      // Read the length using dynamic offset
      lldb::addr_t len_addr = obj_addr + len_offset;
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
    //   Class isa;
    //   uint32_t len;
    //   uint32_t padding;
    //   uint64_t hash;
    //   const char *str;    <-- String pointer
    // };
    
    // Get dynamic offsets for GSString-based classes
    std::string base_class_name = class_name;
    if (base_class_name.find("GSC") == 0) {
      base_class_name = "GSString"; // GSCString, GSCInlineString map to GSString
    }
    
    ptrdiff_t str_offset = GNUstepRuntimeHelper::GetIvarOffset(process, base_class_name, "str");
    if (str_offset < 0) {
      str_offset = 24; // Fallback to hardcoded offset
    }
    
    ptrdiff_t len_offset = GNUstepRuntimeHelper::GetIvarOffset(process, base_class_name, "len");
    if (len_offset < 0) {
      len_offset = 8; // Fallback to hardcoded offset
    }
    
    // Read the string pointer using dynamic offset
    lldb::addr_t str_ptr_addr = obj_addr + str_offset;
    lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
    if (error.Fail() || str_data_addr == 0 || str_data_addr == LLDB_INVALID_ADDRESS) {
      return "";
    }
    
    // Read the string length using dynamic offset
    lldb::addr_t len_addr = obj_addr + len_offset;
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

// Helper function to get summary for tagged pointers without creating ValueObjects
std::string GetTaggedPointerSummary(Process *process, lldb::addr_t tagged_ptr) {
  if (!process || tagged_ptr == 0) {
    return "<nil>";
  }
  
  // Check the tag in the low 3 bits
  uint8_t tag = tagged_ptr & 0x7;
  
  // GSTinyString (tag 4)
  if (tag == 4) {
    // Length is in bits 3-7 (5 bits)
    int length = (tagged_ptr >> 3) & 0x1F;
    
    if (length > 0 && length <= 9) {
      std::string result = "\"";
      // Characters are extracted using the TINY_STRING_CHAR macro
      for (int i = 0; i < length; i++) {
        uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
        char c = (tagged_ptr & mask) >> (57 - (i * 7));
        if (c >= 0x20 && c <= 0x7e) {
          result += c;
        } else if (c != 0) {
          result += '?';
        }
      }
      result += "\"";
      return result;
    }
  }
  
  // GSTaggedNumber (tag 1 or 3)
  if (tag == 1 || tag == 3) {
    // For tagged numbers, extract the value
    int64_t value = (int64_t)(tagged_ptr >> 3);  // Remove tag bits
    return std::to_string(value);
  }
  
  // Other tagged types - show raw value for debugging
  return llvm::formatv("<tagged:0x{0:x}>", tagged_ptr);
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
  // Performance optimized - removed debug file I/O
  
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
    //   Class isa;
    //   id *_contents;      (points to inline elements)
    //   unsigned _count;
    // }
    // The elements are stored immediately after the object header
    
    // Get dynamic offsets for GSInlineArray
    ptrdiff_t count_offset = GNUstepRuntimeHelper::GetIvarOffset(process, class_name, "_count");
    if (count_offset < 0) {
      // Dynamic offset lookup failed for _count, using fallback
      count_offset = 16; // Fallback to hardcoded offset
    } else {
      // Dynamic offset for _count found
    }
    
    ptrdiff_t contents_offset = GNUstepRuntimeHelper::GetIvarOffset(process, class_name, "_contents");
    if (contents_offset < 0) {
      // Dynamic offset lookup failed for _contents, using fallback
      contents_offset = 8; // Fallback to hardcoded offset
    } else {
      // Dynamic offset for _contents found
    }
    
    // Read count first using dynamic offset
    lldb::addr_t count_addr = obj_addr + count_offset;
    uint32_t temp_count = 0;
    if (!GNUstepRuntimeHelper::ReadMemory(process, count_addr, &temp_count, sizeof(temp_count))) {
      return false;
    }
    m_count = temp_count;
    
    // For inline arrays, the _contents pointer points to where
    // the elements are stored (usually right after the object structure)
    lldb::addr_t contents_ptr_addr = obj_addr + contents_offset;
    Status error;
    m_contents_array_ptr = GNUstepRuntimeHelper::ReadPointer(process, contents_ptr_addr, error);
    if (error.Fail() || m_contents_array_ptr == 0 || m_contents_array_ptr == LLDB_INVALID_ADDRESS) {
      return false;
    }
    
  } else {
    // Regular GSArray structure:
    // struct {
    //   Class isa;
    //   id *_contents_array;
    //   unsigned _count;
    // }
    
    // Get dynamic offsets for GSArray
    ptrdiff_t contents_offset = GNUstepRuntimeHelper::GetIvarOffset(process, class_name, "_contents_array");
    if (contents_offset < 0) {
      contents_offset = 8; // Fallback to hardcoded offset
    }
    
    ptrdiff_t count_offset = GNUstepRuntimeHelper::GetIvarOffset(process, class_name, "_count");
    if (count_offset < 0) {
      count_offset = 16; // Fallback to hardcoded offset
    }
    
    // Read _contents_array pointer using dynamic offset
    lldb::addr_t contents_ptr_addr = obj_addr + contents_offset;
    Status error;
    m_contents_array_ptr = GNUstepRuntimeHelper::ReadPointer(process, contents_ptr_addr, error);
    if (error.Fail() || m_contents_array_ptr == 0 || m_contents_array_ptr == LLDB_INVALID_ADDRESS) {
      return false;
    }
    
    // Read _count using dynamic offset
    lldb::addr_t count_addr = obj_addr + count_offset;
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
  
  m_elements.clear();
  m_elements.reserve(elements_to_read);
  
  size_t ptr_size = GNUstepRuntimeHelper::GetAddressByteSize(m_process);
  
  // Safety check for ptr_size
  if (ptr_size == 0 || ptr_size > 16) {
    return false;
  }
  
  // PERFORMANCE OPTIMIZATION: Batch read all element pointers at once
  // This reduces memory read operations from N to 1, significantly improving performance
  size_t total_bytes = elements_to_read * ptr_size;
  DataBufferSP buffer_sp(new DataBufferHeap(total_bytes, 0));
  
  Status batch_error;
  size_t bytes_read = m_process->ReadMemory(m_contents_array_ptr, 
                                            const_cast<void*>(static_cast<const void*>(buffer_sp->GetBytes())), 
                                            total_bytes, 
                                            batch_error);
  
  if (batch_error.Success() && bytes_read >= ptr_size) {
    // Use DataExtractor for efficient, endian-safe pointer extraction
    DataExtractor extractor(buffer_sp, 
                           m_process->GetByteOrder(), 
                           ptr_size);
    
    lldb::offset_t offset = 0;
    uint32_t elements_read = bytes_read / ptr_size;
    
    for (uint32_t i = 0; i < elements_read && i < elements_to_read; ++i) {
      lldb::addr_t element_addr = extractor.GetAddress(&offset);
      m_elements.push_back(element_addr);
    }
  } else {
    // Fallback to individual reads if batch read fails
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
  
  // Ensure we have a valid process
  if (!m_process) {
    return nullptr;
  }
  
  // Calculate the address where the element pointer is stored (not the element value itself!)
  // This follows Apple's NSArray formatter pattern exactly
  size_t ptr_size = GNUstepRuntimeHelper::GetAddressByteSize(m_process);
  lldb::addr_t element_ptr_addr = m_contents_array_ptr + (idx * ptr_size);
  
  // Create the child name
  StreamString idx_name;
  idx_name.Printf("[%u]", idx);
  
  // Get execution context
  ExecutionContext exe_ctx(m_exe_ctx_ref);
  if (!exe_ctx.GetBestExecutionContextScope()) {
    return nullptr;
  }
  
  // CRITICAL FIX: Pass the ADDRESS where the pointer is stored, not the pointer value!
  // This allows LLDB to properly read the pointer and handle tagged pointers automatically.
  // Apple's NSArray formatters do exactly this - they calculate the address where the
  // element pointer is stored and pass that to CreateValueObjectFromAddress.
  // LLDB then reads the pointer from that address and correctly handles both:
  // - Tagged pointers (where the "address" contains encoded data)  
  // - Regular pointers (where the address points to actual objects)
  return SyntheticChildrenFrontEnd::CreateValueObjectFromAddress(
      idx_name.GetString(), element_ptr_addr, exe_ctx, m_id_type);
}

//===----------------------------------------------------------------------===//
// Performance Optimization Functions
//===----------------------------------------------------------------------===//

// PERFORMANCE OPTIMIZATION: Optimized tagged string decoder with fast paths
static std::string DecodeTaggedStringOptimized(lldb::addr_t tagged_addr) {
  // Fast validation: check tag bits and length
  if ((tagged_addr & 0x7) != 4) return "";
  
  int length = (tagged_addr >> 3) & 0x1F;  // Extract 5-bit length
  if (length == 0 || length > 9) return "";
  
  // Pre-allocate result string to avoid repeated allocations
  std::string result;
  result.reserve(length);
  
  // Optimized character extraction using bit manipulation
  // Use lookup table approach for common characters
  static const uint64_t CHAR_MASKS[9] = {
    0xFE00000000000000ULL,      // i=0: shift 57
    0x01FC000000000000ULL,      // i=1: shift 50  
    0x0003F80000000000ULL,      // i=2: shift 43
    0x00000700000000ULL,        // i=3: shift 36
    0x000000FE0000ULL,          // i=4: shift 29
    0x000000001FC00ULL,         // i=5: shift 22
    0x00000000003F8ULL,         // i=6: shift 15
    0x0000000000007ULL,         // i=7: shift 8
    0x000000000000FEULL         // i=8: shift 1
  };
  
  static const int CHAR_SHIFTS[9] = {57, 50, 43, 36, 29, 22, 15, 8, 1};
  
  for (int i = 0; i < length; i++) {
    char c = (tagged_addr & CHAR_MASKS[i]) >> CHAR_SHIFTS[i];
    if (c >= 0x20 && c <= 0x7E) {
      result += c;
    } else if (c == 0) {
      break;  // Null terminator
    } else {
      // Non-printable character, abort decoding
      return "";
    }
  }
  
  return result;
}

// PERFORMANCE OPTIMIZATION: Fast path for tagged number decoding
static std::string DecodeTaggedNumberOptimized(lldb::addr_t tagged_addr) {
  uint8_t tag = tagged_addr & 0x7;
  
  switch (tag) {
    case 1: {
      // NSSmallInt - optimized arithmetic shift
      int64_t value = static_cast<int64_t>(tagged_addr) >> 3;
      return std::to_string(value);
    }
    case 5: {
      // NSSmallFloat - optimized union conversion
      union { uint64_t bits; double d; } converter;
      converter.bits = tagged_addr & ~0x7ULL;
      float float_val = static_cast<float>(converter.d);
      
      // Use faster formatting for common float values
      if (float_val == 0.0f) return "0";
      if (float_val == 1.0f) return "1";
      if (float_val == -1.0f) return "-1";
      
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%.6g", float_val);
      return std::string(buffer);
    }
    case 2: {
      // NSSmallExtendedDouble - optimized bit manipulation
      uint64_t mask = tagged_addr & 8;
      union { uint64_t bits; double d; } converter;
      converter.bits = (tagged_addr & ~7ULL) | (mask >> 1) | (mask >> 2) | (mask >> 3);
      
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%.6g", converter.d);
      return std::string(buffer);
    }
    case 3: {
      // NSSmallRepeatingDouble - optimized bit manipulation
      uint64_t mask = tagged_addr & 56;
      union { uint64_t bits; double d; } converter;
      converter.bits = (tagged_addr & ~7ULL) | (mask >> 3);
      
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%.6g", converter.d);
      return std::string(buffer);
    }
    default:
      return "";
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