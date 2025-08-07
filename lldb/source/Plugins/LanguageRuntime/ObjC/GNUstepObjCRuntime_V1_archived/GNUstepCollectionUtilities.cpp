//===-- GNUstepCollectionUtilities.cpp ---------------------------------===//
//
// Shared utilities for GNUstep collection formatters (NSSet, NSArray, NSDictionary)
// 
// This implementation extracts working utilities from GNUstepNSSet.cpp
// CRITICAL: All functions copied VERBATIM from working implementation
//
//===----------------------------------------------------------------------===//

#include "GNUstepCollectionUtilities.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/ConstString.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

// Required for clang::ObjCInterfaceDecl template parameter
#include "clang/AST/DeclObjC.h"

#include <algorithm>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace lldb_private {
namespace formatters {
namespace gnustep_collection_utils {

// EXACT COPY from GNUstepNSSet.cpp lines 38-42
// Check if address is a tagged pointer (based on working Python implementation)
bool IsTaggedPointer(uint64_t addr) {
  // CRITICAL FIX: Check both GNUstep (high bits) and Apple (low bits) tagging schemes
  uint8_t tag_low = addr & 0x7;
  uint8_t tag_high = (addr >> 61) & 0x7;
  
  // GNUstep GSTinyString uses tag=4 in high bits
  if (tag_high == 4) return true;
  
  // Apple/traditional tagging in low bits
  return (addr & 0x1) != 0 || tag_low >= 4;
}

// EXACT COPY from GNUstepNSSet.cpp lines 44-67
// Get tagged pointer class name (based on working Python implementation) 
std::string GetTaggedPointerClassName(uint64_t addr, Log* log) {
  // CRITICAL FIX: GNUstep uses different tagging scheme than Apple
  // GNUstep GSTinyString uses tag in bits 61-63, not 0-2
  uint8_t tag_low = addr & 0x7;          // bits 0-2 (original)
  uint8_t tag_high = (addr >> 61) & 0x7; // bits 61-63 (GNUstep)
  
  if (log) {
    LLDB_LOG(log, "Tagged pointer 0x{0:x} with tag_low={1}, tag_high={2}", addr, (int)tag_low, (int)tag_high);
  }
  
  // Try GNUstep high-bit tagging first (more likely for strings)
  switch (tag_high) {
    case 4:
      return "GSTinyString";  // GSTinyString uses tag=4 in high bits
    default:
      break;
  }
  
  // Fall back to low-bit tagging
  switch (tag_low) {
    case 1:
      return "NSSmallInt";  // or NSNumber
    case 4:
      return "GSTinyString";  // Alternative location
    case 6:
      return "GSSmallDate";  // or NSDate
    case 7:
      return "GSSmallDate";  // Another date variant
    default:
      if (log) {
        LLDB_LOG(log, "Unknown tagged pointer tag_low={0}, tag_high={1}", (int)tag_low, (int)tag_high);
      }
      return "NSObject";  // Generic fallback
  }
}

// EXACT COPY from GNUstepNSSet.cpp lines 69-116
// Utility function to decode GNUstep GSTinyString tagged pointers
bool DecodeGSTinyString(uint64_t tagged_ptr, std::string& result, Log* log) {
  // Check if this is a GSTinyString tagged pointer
  uint8_t tag = tagged_ptr & 0x7;  // Tag is in lower bits for GNUstep
  
  if (log) {
    LLDB_LOG(log, "DecodeGSTinyString: tagged_ptr=0x{0:x}, tag={1}", tagged_ptr, (int)tag);
  }
  
  if (tag != 4) {
    return false; // Not a GSTinyString
  }
  
  // Based on working Python implementation:
  // Length is in bits 3-7 (OBJC_SMALL_OBJECT_SHIFT = 3 on 64-bit)
  uint8_t length = (tagged_ptr >> 3) & 0x1F;  // TINY_STRING_LENGTH_MASK = 0x1F
  
  if (length == 0 || length > 8) {
    if (log) {
      LLDB_LOG(log, "Invalid GSTinyString length: {0}", (int)length);
    }
    return false;
  }
  
  result.clear();
  for (int i = 0; i < length; i++) {
    // Use exact formula from GSString.m:
    // TINY_STRING_CHAR(s, x) = ((s & (0xFE00000000000000 >> (x*7))) >> (57-(x*7)))
    uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
    uint8_t ch = (tagged_ptr & mask) >> (57 - (i * 7));
    ch = ch & 0x7F;  // Ensure it's a 7-bit value
    
    if (ch >= 32 && ch < 127) {  // Printable ASCII
      result += (char)ch;
    } else if (ch == 0) {
      break;  // Null terminator
    } else {
      result += '?';  // Non-printable character
    }
  }
  
  if (log) {
    LLDB_LOG(log, "DecodeGSTinyString: length={0}, decoded=\"{1}\"", (int)length, result);
  }
  
  return true;
}

// Helper to decode NSConstantString objects
bool DecodeNSConstantString(uint64_t addr, ProcessSP process, std::string& result, Log* log) {
  if (!process || addr == 0) {
    return false;
  }
  
  Status error;
  size_t ptr_size = process->GetAddressByteSize();
  
  if (log) {
    LLDB_LOG(log, "DecodeNSConstantString: Attempting to decode NSConstantString at 0x{0:x}", addr);
  }
  
  // NSConstantString structure (NEW_ABI):
  // Offset 0x00: isa pointer (8 bytes)
  // Offset 0x08: flags (4 bytes) + padding
  // Offset 0x10: nxcslen (length field)
  // Offset 0x18: size field  
  // Offset 0x20: hash field
  // Offset 0x28: nxcsptr (pointer to C string)
  
  // Read the string pointer at offset 0x28 (40 bytes)
  addr_t string_ptr_addr = addr + 40;  // 0x28 = 40
  uint64_t string_ptr = process->ReadPointerFromMemory(string_ptr_addr, error);
  if (error.Fail() || !string_ptr) {
    if (log) {
      LLDB_LOG(log, "Failed to read string pointer at offset 0x28");
    }
    return false;
  }
  
  // Read the length field at offset 0x10 (16 bytes)
  addr_t length_addr = addr + 16;  // 0x10 = 16
  uint64_t length_data = process->ReadUnsignedIntegerFromMemory(length_addr, 8, 0, error);
  if (error.Fail()) {
    if (log) {
      LLDB_LOG(log, "Failed to read length field at offset 0x10");
    }
    return false;
  }
  
  // The length appears to be duplicated in the field, extract the lower 32 bits
  uint32_t length = static_cast<uint32_t>(length_data & 0xFFFFFFFF);
  
  // Sanity check the length
  if (length > 1000) {  // Reasonable limit for string length
    if (log) {
      LLDB_LOG(log, "String length {0} too large, skipping", length);
    }
    return false;
  }
  
  if (log) {
    LLDB_LOG(log, "NSConstantString: length={0}, string_ptr=0x{1:x}", length, string_ptr);
  }
  
  // Read the C string from the pointer
  result.clear();
  result.reserve(length + 1);
  
  for (uint32_t i = 0; i < length; i++) {
    uint8_t byte_data[1];
    size_t bytes_read = process->ReadMemory(string_ptr + i, byte_data, 1, error);
    if (error.Fail() || bytes_read != 1) {
      if (log) {
        LLDB_LOG(log, "Failed to read string byte at index {0}", i);
      }
      break;
    }
    
    char ch = static_cast<char>(byte_data[0]);
    if (ch == 0) {
      break;  // Null terminator
    }
    result += ch;
  }
  
  if (log) {
    LLDB_LOG(log, "Successfully decoded NSConstantString: \"{0}\"", result);
  }
  
  return !result.empty();
}

// Helper to get class name from ISA pointer  
std::string GetClassNameFromISA(uint64_t isa_ptr, ProcessSP process, TargetSP target, Log* log) {
  if (!process || !target || isa_ptr == 0) {
    return "";
  }
  
  // Try to find class_getName symbol and call it
  SymbolContextList sc_list;
  target->GetImages().FindSymbolsWithNameAndType(ConstString("class_getName"), 
                                                eSymbolTypeCode, sc_list);
  if (sc_list.GetSize() == 0) {
    if (log) {
      LLDB_LOG(log, "class_getName symbol not found");
    }
    return "";
  }
  
  // For now, we'll just try some common heuristics based on known class names
  // A full implementation would use expression evaluation or direct function calls
  
  // This is a simplified approach - in a real implementation we'd call class_getName
  // For now, return empty string to indicate we couldn't determine the class
  return "";
}

// NEW FUNCTION: Generate a summary string for an object at the given address
std::string GetObjectSummary(uint64_t addr, ProcessSP process, TargetSP target, Log* log) {
  // Handle tagged pointers first
  if (IsTaggedPointer(addr)) {
    std::string class_name = GetTaggedPointerClassName(addr, log);
    
    if (class_name == "GSTinyString") {
      std::string decoded;
      if (DecodeGSTinyString(addr, decoded, log)) {
        return "@\"" + decoded + "\"";
      }
      return "GSTinyString (decode failed)";
    }
    
    if (class_name == "NSSmallInt") {
      // For NSSmallInt, extract the integer value
      // The value is typically encoded in the upper bits
      int64_t value = static_cast<int64_t>(addr) >> 3;  // Shift out tag bits
      return std::to_string(value);
    }
    
    if (class_name == "GSSmallDate") {
      // For GSSmallDate, we could decode the timestamp but for now just indicate it's a date
      return "GSSmallDate tagged pointer";
    }
    
    // Handle other tagged pointer types
    return class_name + " tagged pointer";
  }
  
  // Handle regular objects
  if (!process || addr == 0) {
    return "nil";
  }
  
  // Try to read the ISA pointer to validate this is a real object
  Status error;
  uint64_t isa_ptr = process->ReadPointerFromMemory(addr, error);
  if (error.Fail()) {
    return "invalid object";
  }
  
  if (log) {
    LLDB_LOG(log, "GetObjectSummary: Processing regular object at 0x{0:x}, isa=0x{1:x}", addr, isa_ptr);
  }
  
  // CRITICAL FIX: Try to decode as NSConstantString first
  // Most dictionary keys and set elements will be NSConstantString objects
  std::string string_content;
  if (DecodeNSConstantString(addr, process, string_content, log)) {
    return "@\"" + string_content + "\"";
  }
  
  // For other types, we could add more specific decoders here:
  // - NSNumber decoding
  // - NSArray recursive summary
  // - NSDictionary recursive summary
  // - Custom class summaries
  
  // Get class name for better type identification
  std::string class_name = GetClassNameFromISA(isa_ptr, process, target, log);
  if (!class_name.empty()) {
    // Return class-specific summary if we know the class
    char summary[50];
    snprintf(summary, sizeof(summary), "<%s: 0x%llx>", class_name.c_str(), (unsigned long long)addr);
    return std::string(summary);
  }
  
  // Return compact address format as fallback
  // Note: The actual detailed formatting will happen when the child ValueObject is created
  char addr_str[20];
  snprintf(addr_str, sizeof(addr_str), "0x%llx", (unsigned long long)addr);
  return std::string(addr_str);
}

// NEW FUNCTION: Get the most specific CompilerType for an address
CompilerType GetSpecificTypeForAddress(uint64_t addr, TargetSP target, TypeSystemClang* ts, Log* log) {
  if (!target || !ts) {
    return CompilerType();
  }
  
  if (IsTaggedPointer(addr)) {
    std::string class_name = GetTaggedPointerClassName(addr, log);
    
    if (log) {
      LLDB_LOG(log, "GetSpecificTypeForAddress: tagged pointer class = {0}", class_name);
    }
    
    // CRITICAL FIX: Use specific Foundation types instead of generic id
    // This allows existing summary providers to automatically apply
    CompilerType specific_type;
    
    if (class_name == "GSTinyString") {
      // Look for NSString type using TypeSystemClang API
      specific_type = ts->GetTypeForIdentifier<clang::ObjCInterfaceDecl>(ConstString("NSString"));
      if (specific_type.IsValid()) {
        specific_type = specific_type.GetPointerType();
        if (log) {
          LLDB_LOG(log, "Using NSString* type for GSTinyString tagged pointer");
        }
        return specific_type;
      }
    } else if (class_name == "NSSmallInt") {
      // Look for NSNumber type using TypeSystemClang API  
      specific_type = ts->GetTypeForIdentifier<clang::ObjCInterfaceDecl>(ConstString("NSNumber"));
      if (specific_type.IsValid()) {
        specific_type = specific_type.GetPointerType(); 
        if (log) {
          LLDB_LOG(log, "Using NSNumber* type for NSSmallInt tagged pointer");
        }
        return specific_type;
      }
    } else if (class_name == "GSSmallDate") {
      // Look for NSDate type using TypeSystemClang API
      specific_type = ts->GetTypeForIdentifier<clang::ObjCInterfaceDecl>(ConstString("NSDate"));
      if (specific_type.IsValid()) {
        specific_type = specific_type.GetPointerType();
        if (log) {
          LLDB_LOG(log, "Using NSDate* type for GSSmallDate tagged pointer");
        }
        return specific_type;
      }
    }
    
    // Fallback to objc_id if specific type lookup failed
    if (log) {
      LLDB_LOG(log, "Could not find specific type for {0}, using id", class_name);
    }
    return ts->GetBasicType(eBasicTypeObjCID);
  }
  
  // For regular objects, we could potentially do runtime introspection here
  // to get the actual class name and find the corresponding type.
  // For now, use basic objc_id type as the working implementation does
  return ts->GetBasicType(eBasicTypeObjCID);
}

} // namespace gnustep_collection_utils
} // namespace formatters
} // namespace lldb_private