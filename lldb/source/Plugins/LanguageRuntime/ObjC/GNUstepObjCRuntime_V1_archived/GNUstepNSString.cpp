//===-- GNUstepNSString.cpp --------------------------------------------===//
//
// Complete NSString summary provider for GNUstep runtime
// Based on Apple NSString.cpp but adapted for GNUstep specifics
//
//===----------------------------------------------------------------------===//

#include "GNUstepNSString.h"
#include "GNUstepCollectionUtilities.h"
#include "GNUstepObjCRuntime.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/DataFormatters/StringPrinter.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"
#include "lldb/ValueObject/ValueObject.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::gnustep_collection_utils;

namespace lldb_private {
namespace formatters {

// Helper function to check if an object is actually a string-like class
static bool IsStringLikeObject(ValueObject &valobj, lldb::addr_t addr, ProcessSP process_sp) {
  // Check for tagged pointers first - tag 4 is GSTinyString
  uint8_t tag = addr & 0x7;
  if (tag == 4) {
    return true; // GSTinyString tagged pointer
  }
  
  // For regular heap objects, try to get the runtime class name
  if (auto *runtime = llvm::dyn_cast_or_null<GNUstepObjCRuntime>(
        process_sp->GetLanguageRuntime(lldb::eLanguageTypeObjC))) {
    std::string class_name = runtime->GetClassNameFromObject(addr);
    
    // Check against known string class names
    if (class_name.find("String") != std::string::npos ||  // NSString, NSMutableString, GSString, etc.
        class_name == "NSConstantString" ||
        class_name.find("GSString") != std::string::npos ||
        class_name.find("GSTinyString") != std::string::npos ||
        class_name.find("GSUnicodeString") != std::string::npos) {
      return true;
    }
    
    // If we got a concrete class name and it's not string-like, reject it
    if (!class_name.empty() && class_name != "id" && class_name != "NSObject") {
      return false;
    }
  }
  
  // Fallback: Check the declared type if we couldn't get runtime class
  CompilerType type = valobj.GetCompilerType();
  if (type.IsValid()) {
    std::string type_name = type.GetTypeName().AsCString("");
    
    // Allow if declared as a string type
    if (type_name.find("String") != std::string::npos ||
        type_name == "NSConstantString" || type_name == "NSConstantString *") {
      return true;
    }
    
    // If declared as generic id, allow it (might be a string)
    if (type_name == "id" || type_name.empty()) {
      return true;
    }
    
    // If declared as something specific that's not string-like, reject
    return false;
  }
  
  // If we can't determine, be conservative and allow it
  return true;
}

static bool ReadStringFromMemory(Process &process, lldb::addr_t addr, 
                                std::string &result, size_t max_len = 1000) {
  Status error;
  result.clear();
  
  for (size_t i = 0; i < max_len; i++) {
    uint8_t byte = process.ReadUnsignedIntegerFromMemory(addr + i, 1, 0, error);
    if (error.Fail()) {
      return false;
    }
    if (byte == 0) {
      break; // Null terminator
    }
    if (byte >= 32 && byte < 127) {
      result += (char)byte;
    } else {
      result += '?'; // Non-printable
    }
  }
  
  return !result.empty();
}

bool GNUstepNSStringSummaryProvider(ValueObject &valobj, Stream &stream,
                                   const TypeSummaryOptions &options) {
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;

  lldb::addr_t string_addr = valobj.GetValueAsUnsigned(0);
  if (string_addr == 0 || string_addr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }

  // CRITICAL FIX: Check if this is actually a string-like object before formatting
  // This prevents dictionaries, arrays, and other objects from being misinterpreted as corrupted strings
  if (!IsStringLikeObject(valobj, string_addr, process_sp)) {
    // Not a string object - don't apply string formatting
    return false;
  }

  // 1. Check for tagged pointers
  uint8_t tag = string_addr & 0x7;
  
  if (tag == 4) {
    // GSTinyString tagged pointer - use shared decoder that works for array summaries
    std::string decoded;
    if (DecodeGSTinyString(string_addr, decoded, nullptr)) {
      stream.Printf("@\"%s\"", decoded.c_str());
      return true;
    }
    // If decoding fails, fall through to NSConstantString handling
  }
  else if (tag != 0) {
    // Other tagged pointer types (NSSmallInt, GSSmallDate, etc.) - use shared utilities
    ProcessSP process_sp = valobj.GetProcessSP();
    TargetSP target_sp = valobj.GetTargetSP();
    if (process_sp && target_sp) {
      std::string summary = GetObjectSummary(string_addr, process_sp, target_sp);
      if (!summary.empty()) {
        stream.Printf("%s", summary.c_str());
        return true;
      }
    }
    // Fallback for unknown tagged pointer types
    stream.Printf("<tagged pointer: tag=%d, value=0x%llx>", tag, string_addr);
    return true;
  }
  // tag == 0 means regular heap object - continue to NSConstantString handling

  // 2. Handle NSConstantString or corrupted tagged pointers - try different possible offsets
  Status error;
  uint32_t ptr_size = process_sp->GetAddressByteSize();
  
  // NSConstantString typical layout: ISA@0, flags@8, length@12, data@16 or data@24
  std::vector<size_t> possible_offsets = {16, 24, ptr_size * 2, ptr_size * 3};
  
  for (size_t offset : possible_offsets) {
    error.Clear();
    lldb::addr_t cstring_ptr = process_sp->ReadPointerFromMemory(string_addr + offset, error);
    
    if (!error.Fail() && cstring_ptr > 0x1000 && cstring_ptr < 0x7FFFFFFFFFFF) {
      std::string content;
      if (ReadStringFromMemory(*process_sp, cstring_ptr, content)) {
        // Quick validation - reasonable string length and content
        if (content.length() > 0 && content.length() < 10000) {
          bool looks_valid = true;
          size_t non_printable = 0;
          
          for (char c : content) {
            if (c < 32 || c > 126) {
              non_printable++;
              if (non_printable > content.length() / 4) { // Too many non-printable chars
                looks_valid = false;
                break;
              }
            }
          }
          
          if (looks_valid) {
            stream.Printf("@\"%s\"", content.c_str());
            return true;
          }
        }
      }
    }
  }

  // 3. Try using the address correction from original provider
  // LLDB sometimes gives us the ISA instead of the object address
  lldb::addr_t object_addr = valobj.GetAddressOf();
  if (object_addr != LLDB_INVALID_ADDRESS && object_addr != string_addr) {
    // Try with the object address instead
    for (size_t offset : possible_offsets) {
      error.Clear();
      lldb::addr_t cstring_ptr = process_sp->ReadPointerFromMemory(object_addr + offset, error);
      
      if (!error.Fail() && cstring_ptr > 0x1000 && cstring_ptr < 0x7FFFFFFFFFFF) {
        std::string content;
        if (ReadStringFromMemory(*process_sp, cstring_ptr, content)) {
          if (content.length() > 0 && content.length() < 10000) {
            stream.Printf("@\"%s\"", content.c_str());
            return true;
          }
        }
      }
    }
  }

  // 4. Fallback
  stream.Printf("@\"...\"");
  return true;
}

} // namespace formatters
} // namespace lldb_private