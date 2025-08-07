//===-- GNUstepCollectionUtilities.h -----------------------------------===//
//
// Shared utilities for GNUstep collection formatters (NSSet, NSArray, NSDictionary)
//
//===--------------------------------------------------------------------===//

#ifndef LLDB_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPCOLLECTIONUTILITIES_H
#define LLDB_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPCOLLECTIONUTILITIES_H

#include "lldb/lldb-forward.h"
#include "lldb/Utility/Log.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include <string>

namespace lldb_private {
namespace formatters {
namespace gnustep_collection_utils {

/// Check if address represents a tagged pointer based on GNUstep bit patterns
/// Tagged pointers have specific patterns: (addr & 0x1) != 0 || (addr & 0x7) >= 4
/// @param addr The address to check
/// @return true if address is a tagged pointer, false otherwise
bool IsTaggedPointer(uint64_t addr);

/// Get the class name for a tagged pointer based on its tag bits
/// Uses tag extraction: uint8_t tag = addr & 0x7
/// Maps tags to known GNUstep tagged pointer classes:
/// - tag 1: NSSmallInt (or NSNumber)
/// - tag 4: GSTinyString (or NSString)  
/// - tag 6/7: GSSmallDate (or NSDate)
/// @param addr Tagged pointer address
/// @param log Optional log for debugging output
/// @return Class name string (e.g., "GSTinyString", "NSSmallInt")
std::string GetTaggedPointerClassName(uint64_t addr, Log* log = nullptr);

/// Decode GNUstep GSTinyString tagged pointer to actual string content
/// Uses exact formula from GSString.m:
/// - Tag must be 4 for GSTinyString
/// - Length in bits 3-7: (tagged_ptr >> 3) & 0x1F
/// - Characters using mask: 0xFE00000000000000ULL >> (i * 7)
/// - Character extraction: (tagged_ptr & mask) >> (57 - (i * 7))
/// @param tagged_ptr The tagged pointer value (must have tag=4)
/// @param result Output string to store decoded content
/// @param log Optional log for debugging output
/// @return true if successfully decoded GSTinyString, false if not a GSTinyString or invalid
bool DecodeGSTinyString(uint64_t tagged_ptr, std::string& result, Log* log = nullptr);

/// Determine if an object is NSConstantString by checking its memory layout
/// Performs heuristic checks to identify NSConstantString objects:
/// - Validates flags, length, and string pointer fields
/// - Ensures string pointer points to valid memory
/// - Checks that length is reasonable
/// @param addr Object address to check
/// @param process ProcessSP for memory reading
/// @param log Optional log for debugging output
/// @return true if object appears to be NSConstantString, false otherwise
bool IsNSConstantString(uint64_t addr, lldb::ProcessSP process, Log* log = nullptr);

/// Decode NSConstantString objects to extract string content
/// Handles regular heap-allocated NSConstantString objects (not tagged pointers)
/// Uses NSConstantString NEW_ABI memory layout:
/// - Offset 0x28: nxcsptr (pointer to C string data)
/// - Offset 0x10: nxcslen (length field)
/// @param addr NSConstantString object address
/// @param process ProcessSP for memory reading
/// @param result Output string to store decoded content
/// @param log Optional log for debugging output
/// @return true if successfully decoded NSConstantString, false otherwise
bool DecodeNSConstantString(uint64_t addr, lldb::ProcessSP process, std::string& result, Log* log = nullptr);

/// Generate a summary string for an object at the given address
/// Handles both tagged pointers and regular objects:
/// - For tagged pointers: decodes content (e.g., string value, number value)
/// - For regular objects: attempts to get basic object description
/// @param addr Object address (can be tagged pointer or regular object)
/// @param process ProcessSP for memory reading
/// @param target TargetSP for type system access
/// @param log Optional log for debugging output
/// @return Summary string for display in debugger
std::string GetObjectSummary(uint64_t addr, lldb::ProcessSP process, 
                           lldb::TargetSP target, Log* log = nullptr);

/// Get the most specific CompilerType for an address
/// For tagged pointers: returns appropriate Foundation type (NSString*, NSNumber*, etc.)
/// For regular objects: attempts runtime class name discovery and type lookup
/// Falls back to objc_id_type if specific type cannot be determined
/// @param addr Object address (tagged pointer or regular object)
/// @param target TargetSP for type system access  
/// @param ts TypeSystemClang for type creation
/// @param log Optional log for debugging output
/// @return CompilerType - most specific type available, or objc_id as fallback
CompilerType GetSpecificTypeForAddress(uint64_t addr, lldb::TargetSP target,
                                     TypeSystemClang* ts, Log* log = nullptr);

} // namespace gnustep_collection_utils  
} // namespace formatters
} // namespace lldb_private

#endif // LLDB_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPCOLLECTIONUTILITIES_H