//===-- GNUstepStringSummaryProvider.cpp ------*- C++ -*-===//
//
// String provider using runtime introspection for dynamic offset discovery
//
//===----------------------------------------------------------------------===//

#include "GNUstepStringSummaryProvider.h"
#include "GNUstepObjCRuntime.h"
#include "RuntimeIntrospector.h"
#include "GNUstepCollectionUtilities.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"
#include "lldb/ValueObject/ValueObject.h"
#include <set>

using namespace lldb;
using namespace lldb_private;

namespace lldb_private {
namespace formatters {

// Guard against recursion in string formatting
static thread_local std::set<addr_t> g_formatting_addresses;

// Helper function to check if an object is actually a string-like class
static bool IsActualStringObject(ValueObject &valobj, addr_t addr, ProcessSP process_sp) {
  // Check for tagged pointers first - tag 4 is GSTinyString
  uint8_t tag = addr & 0x7;
  if (tag == 4) {
    return true; // GSTinyString tagged pointer
  }
  
  // For regular heap objects, try to get the runtime class name
  if (auto *runtime = llvm::dyn_cast_or_null<GNUstepObjCRuntime>(
        ObjCLanguageRuntime::Get(*process_sp))) {
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
      // Specifically reject dictionary, array, and other collection types
      if (class_name.find("Dictionary") != std::string::npos ||
          class_name.find("Array") != std::string::npos ||
          class_name.find("Set") != std::string::npos ||
          class_name.find("GSDictionary") != std::string::npos ||
          class_name.find("GSArray") != std::string::npos ||
          class_name.find("GSSet") != std::string::npos) {
        return false;
      }
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

bool GNUstepStringSummaryProvider(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  addr_t string_ptr = valobj.GetValueAsUnsigned(0);
  if (string_ptr == 0 || string_ptr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }
  
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;

  // CRITICAL FIX: Check if this is actually a string-like object before formatting
  // This prevents dictionaries, arrays, and other objects from being misinterpreted as corrupted strings
  if (!IsActualStringObject(valobj, string_ptr, process_sp)) {
    // Not a string object - don't apply string formatting
    return false;
  }
  
  // CRITICAL FIX: ISA vs Structure Address Resolution
  // ----------------------------------------------------
  // LLDB has a known behavior with NSConstantString where:
  // - GetValueAsUnsigned() returns the ISA/class pointer (e.g., 0x7ffff7eda378)
  // - GetAddressOf() returns the actual structure address (e.g., 0x5555555596a0)
  // 
  // This happens because NSConstantString literals compiled with
  // -fconstant-string-class=NSConstantString are stored in the data segment
  // and LLDB's ValueObject transformation gives us the class pointer instead
  // of the instance address. We detect this case and use the correct address.
  //
  // Verified: NSConstantString layout has nxcsptr at offset 24
  // Structure: ISA@0, flags@8, length@16, nxcsptr@24
  addr_t var_addr = valobj.GetAddressOf();
  
  // If var_addr differs from string_ptr, use var_addr as the structure address
  if (var_addr != LLDB_INVALID_ADDRESS && var_addr != 0 && var_addr != string_ptr) {
    string_ptr = var_addr;
  }
  
  // Check for recursion
  if (g_formatting_addresses.count(string_ptr) > 0) {
    stream.Printf("@\"<recursive>\"");
    return true;
  }
  
  // Guard for recursion
  g_formatting_addresses.insert(string_ptr);
  
  // RAII cleanup guard
  struct CleanupGuard {
    addr_t addr;
    ~CleanupGuard() { g_formatting_addresses.erase(addr); }
  } cleanup{string_ptr};
  
  // Get the runtime introspector for ivar offset discovery
  RuntimeIntrospector *introspector = nullptr;
  if (auto *runtime = llvm::dyn_cast_or_null<GNUstepObjCRuntime>(
          ObjCLanguageRuntime::Get(*process_sp))) {
    introspector = runtime->GetRuntimeIntrospector();
  }
  
  Status error;
  
  
  // Check for GSTinyString tagged pointer (GNUstep uses lower 3 bits for tags)
  if ((string_ptr & 0x7) == 4) {
    // Inline GSTinyString decoding (VERIFIED working algorithm)
    // Based on GNUstep libs-base/Source/GSString.m implementation
    uint8_t length = (string_ptr >> 3) & 0x1F; // Length in bits 3-7
    
    if (length > 0 && length <= 8) { // GSTinyString max 8 chars
      std::string decoded;
      for (int i = 0; i < length; i++) {
        // Character extraction formula from GSString.m:
        // TINY_STRING_CHAR(s, x) = ((s & (0xFE00000000000000 >> (x*7))) >> (57-(x*7)))
        uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
        uint8_t ch = (string_ptr & mask) >> (57 - (i * 7));
        ch = ch & 0x7F; // Ensure 7-bit value
        
        if (ch >= 32 && ch < 127) { // Printable ASCII
          decoded += (char)ch;
        } else if (ch == 0) {
          break; // Null terminator
        } else {
          decoded += '?'; // Non-printable character
        }
      }
      
      if (!decoded.empty()) {
        // Properly escape quotes and backslashes
        std::string escaped;
        for (char c : decoded) {
          if (c == '"') escaped += "\\\"";
          else if (c == '\\') escaped += "\\\\";
          else if (c >= 32 && c < 127) escaped += c;
          else escaped += '?';
        }
        stream.Printf("@\"%s\"", escaped.c_str());
        return true;
      }
    }
  }
  
  // Try direct memory read for NSConstantString first (avoids expression evaluation)
  // NSConstantString has C string pointer at offset 24
  error.Clear();
  addr_t cstring_ptr = process_sp->ReadPointerFromMemory(string_ptr + 24, error);
  
  if (!error.Fail() && cstring_ptr > 0x1000 && cstring_ptr < 0x7FFFFFFFFFFF) {
    std::string content;
    process_sp->ReadCStringFromMemory(cstring_ptr, content, error);
    
    if (!error.Fail() && !content.empty() && content.length() < 10000) {
      // Validate it's printable
      bool valid = true;
      for (char c : content) {
        if (c != '\0' && (c < 32 || c > 126) && c != '\n' && c != '\r' && c != '\t') {
          valid = false;
          break;
        }
      }
      
      if (valid) {
        // Escape quotes
        std::string escaped;
        for (char c : content) {
          if (c == '"') escaped += "\\\"";
          else if (c == '\\') escaped += "\\\\";
          else if (c >= 32 && c < 127) escaped += c;
        }
        stream.Printf("@\"%s\"", escaped.c_str());
        return true;
      }
    }
  }
  
  // If direct read failed, try with runtime introspection (slower but more flexible)
  // Only attempt if we have a reasonable address and have introspector
  if (introspector && string_ptr > 0x100000 && string_ptr < 0x7FFFFFFFFFFF) {
    ptrdiff_t nxcsptr_offset = introspector->GetIvarOffset("NSConstantString", "nxcsptr");
    
    if (nxcsptr_offset > 0) {
      error.Clear();
      cstring_ptr = process_sp->ReadPointerFromMemory(string_ptr + nxcsptr_offset, error);
      
      if (!error.Fail() && cstring_ptr > 0x1000) {
        std::string content;
        process_sp->ReadCStringFromMemory(cstring_ptr, content, error);
        
        if (!error.Fail() && !content.empty()) {
          // Escape quotes
          std::string escaped;
          for (char c : content) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c >= 32 && c < 127) escaped += c;
          }
          stream.Printf("@\"%s\"", escaped.c_str());
          return true;
        }
      }
    }
  }
  
  // Fallback
  stream.Printf("@\"...\"");
  return true;
}

} // namespace formatters
} // namespace lldb_private