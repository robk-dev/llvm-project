//===-- GNUstepStringFormatters.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepStringFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/DataFormatters/StringPrinter.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Target/ExecutionContext.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSStringSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream, 
                                                  const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid object");
    return false;
  }
  
  std::string content = ExtractStringContent(valobj);
  if (content.empty()) {
    WriteErrorSummary(stream, "could not extract string content");
    return false;
  }
  
  WriteQuotedString(stream, content);
  return true;
}

std::string GNUstepNSStringSummaryProvider::ExtractStringContent(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Use runtime introspector to check if this is a tagged pointer
  GNUstepObjCRuntimeIntrospector introspector(process);
  if (introspector.IsTaggedPointer(obj_addr)) {
    return introspector.DecodeTaggedString(obj_addr);
  }
  
  // Get the class name to determine how to extract the string
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  // Handle different GNUstep string types based on their specific memory layouts
  if (class_name == "GSCInlineString" || class_name == "GSUInlineString") {
    // DISABLED: Return empty to prevent hanging
    return "";
  } else if (class_name.find("NSConstantString") != std::string::npos || 
      class_name.find("__NSConstantString") != std::string::npos) {
    return ExtractConstantString(valobj);
  } else if (class_name.find("NSMutableString") != std::string::npos ||
             class_name == "GSMutableString") {
    return ExtractMutableString(valobj);
  } else if (class_name == "NSString") {
    // Try both constant and mutable string formats
    std::string content = ExtractConstantString(valobj);
    if (!content.empty()) {
      return content;
    }
    return ExtractMutableString(valobj);
  } else {
    // Default NSString handling - try inline first (common in GNUstep),
    // then constant string format, then mutable string format
    std::string content = ExtractInlineString(valobj);
    if (!content.empty()) {
      return content;
    }
    content = ExtractConstantString(valobj);
    if (!content.empty()) {
      return content;
    }
    // Last resort: try mutable string format (for GSString and other variants)
    return ExtractMutableString(valobj);
  }
}

std::string GNUstepNSStringSummaryProvider::ExtractConstantString(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // GNUstep NSConstantString structure (based on memory analysis):
  // struct {
  //   Class isa;          // Object's class pointer (offset 0)
  //   uint32_t flags;     // Encoding flags (offset 8)
  //   uint32_t len;       // String length in bytes (offset 12)
  //   uint64_t len2;      // Length again (offset 16)
  //   const void *str;    // String data pointer (offset 24)
  // };
  
  // Read the encoding flags to determine if UTF-16
  lldb::addr_t flags_addr = obj_addr + 8;
  uint32_t flags = 0;
  Status error;
  if (!GNUstepRuntimeHelper::ReadMemory(process, flags_addr, &flags, sizeof(flags))) {
    // Continue without flags
  }
  
  // Check if UTF-16 encoding (flag 0x02 in lower byte)
  bool is_utf16 = (flags & 0xFF) == 0x02;
  
  // Get dynamic offset for str field
  ptrdiff_t str_offset = GNUstepRuntimeHelper::GetIvarOffset(process, "NSConstantString", "str");
  if (str_offset < 0) {
    str_offset = 24; // Fallback to hardcoded offset
  }
  
  // Read the string pointer
  lldb::addr_t str_ptr_addr = obj_addr + str_offset;
  lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  if (error.Fail() || str_data_addr == 0 || str_data_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read the string length (in bytes, not characters)
  lldb::addr_t len_addr = obj_addr + 16;  // Length is at offset 16
  uint64_t byte_length = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, len_addr, &byte_length, sizeof(byte_length))) {
    // Try reading 32-bit length at offset 12
    lldb::addr_t len32_addr = obj_addr + 12;
    uint32_t len32 = 0;
    if (!GNUstepRuntimeHelper::ReadMemory(process, len32_addr, &len32, sizeof(len32))) {
      byte_length = 0;
    } else {
      byte_length = len32;
    }
  }
  
  // Limit string length for safety
  if (byte_length > 2048) {
    byte_length = 2048;
  }
  
  if (byte_length > 0) {
    if (is_utf16) {
      // Read UTF-16 data and convert to UTF-8
      size_t char_count = byte_length / sizeof(uint16_t);
      std::vector<uint16_t> buffer(char_count);
      if (!GNUstepRuntimeHelper::ReadMemory(process, str_data_addr, buffer.data(), byte_length)) {
        return "";
      }
      
      // Convert UTF-16 to UTF-8
      std::string result;
      result.reserve(char_count * 2);
      for (size_t i = 0; i < char_count; ++i) {
        uint16_t ch = buffer[i];
        if (ch == 0) break;  // Stop at null terminator
        
        if (ch < 0x80) {
          result.push_back(static_cast<char>(ch));
        } else if (ch < 0x800) {
          result.push_back(static_cast<char>(0xC0 | (ch >> 6)));
          result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        } else if ((ch & 0xFC00) == 0xD800 && i + 1 < char_count) {
          // Handle UTF-16 surrogate pair for emoji and other 4-byte UTF-8 chars
          uint16_t ch2 = buffer[i + 1];
          if ((ch2 & 0xFC00) == 0xDC00) {
            uint32_t codepoint = 0x10000 + (((ch & 0x3FF) << 10) | (ch2 & 0x3FF));
            result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            i++; // Skip the second surrogate
          } else {
            // Invalid surrogate pair, output replacement character
            result.push_back('?');
          }
        } else {
          result.push_back(static_cast<char>(0xE0 | (ch >> 12)));
          result.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
          result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
      }
      return result;
    } else {
      // UTF-8 string
      return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, byte_length);
    }
  }
  
  // Fallback: try to read a null-terminated string
  return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 256);
}

std::string GNUstepNSStringSummaryProvider::ExtractMutableString(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Get the class name to handle different mutable string implementations
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  // GSMutableString structure (from actual memory inspection):
  // struct {
  //   Class isa;              // offset 0 (8 bytes)
  //   void *_contents;        // offset 8 (8 bytes) - pointer to character data
  //   unsigned int _count;    // offset 16 (4 bytes) - string length
  //   unsigned int _flags;    // offset 20 (4 bytes) - encoding flags
  //   unsigned int _capacity; // offset 24 (4 bytes)
  //   NSZone *_zone;         // offset 32 (8 bytes)
  // };
  
  if (class_name == "GSMutableString") {
    // Read the _contents pointer
    lldb::addr_t contents_addr = obj_addr + 8;
    Status error;
    lldb::addr_t contents_ptr = GNUstepRuntimeHelper::ReadPointer(process, contents_addr, error);
    if (error.Fail() || contents_ptr == 0 || contents_ptr == LLDB_INVALID_ADDRESS) {
      return "";
    }
    
    // Read the string length
    lldb::addr_t count_addr = obj_addr + 16;
    uint32_t string_length = 0;
    if (!GNUstepRuntimeHelper::ReadMemory(process, count_addr, &string_length, sizeof(string_length))) {
      return "";
    }
    
    // Sanity check the length
    if (string_length == 0 || string_length > 10000) {
      return "";
    }
    
    // Read the flags to check encoding
    lldb::addr_t flags_addr = obj_addr + 20;
    uint32_t flags = 0;
    if (!GNUstepRuntimeHelper::ReadMemory(process, flags_addr, &flags, sizeof(flags))) {
      // Continue without flags
    }
    
    // Check if it's wide (16-bit) characters
    bool is_wide = (flags & 0x1) != 0;
    
    if (is_wide) {
      // 16-bit Unicode characters
      size_t byte_size = string_length * sizeof(uint16_t);
      std::vector<uint16_t> buffer(string_length);
      if (!GNUstepRuntimeHelper::ReadMemory(process, contents_ptr, buffer.data(), byte_size)) {
        return "";
      }
      
      // Convert UTF-16 to UTF-8
      std::string result;
      result.reserve(string_length * 2);
      for (uint32_t i = 0; i < string_length; ++i) {
        uint16_t ch = buffer[i];
        if (ch < 0x80) {
          result.push_back(static_cast<char>(ch));
        } else if (ch < 0x800) {
          result.push_back(static_cast<char>(0xC0 | (ch >> 6)));
          result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        } else if ((ch & 0xFC00) == 0xD800 && i + 1 < string_length) {
          // Handle UTF-16 surrogate pair for emoji and other 4-byte UTF-8 chars
          uint16_t ch2 = buffer[i + 1];
          if ((ch2 & 0xFC00) == 0xDC00) {
            uint32_t codepoint = 0x10000 + (((ch & 0x3FF) << 10) | (ch2 & 0x3FF));
            result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            i++; // Skip the second surrogate
          } else {
            // Invalid surrogate pair, output replacement character
            result.append("�");
          }
        } else {
          result.push_back(static_cast<char>(0xE0 | (ch >> 12)));
          result.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
          result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
      }
      return result;
    } else {
      // 8-bit characters (UTF-8 or ASCII)
      return GNUstepRuntimeHelper::ReadUTF8String(process, contents_ptr, string_length);
    }
  }
  
  // For other mutable string types, try inline string format first
  std::string result = ExtractInlineString(valobj);
  if (!result.empty()) {
    return result;
  }
  
  // Then try constant string format
  return ExtractConstantString(valobj);
}

std::string GNUstepNSStringSummaryProvider::ExtractInlineString(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // GSCInlineString/GSUInlineString structure (from GSPrivate.h):
  // struct {
  //   Class isa;              // offset 0 (8 bytes)
  //   union {                 // offset 8 (8 bytes) - _contents
  //     unsigned char *c;
  //     unichar *u;
  //   } _contents;
  //   unsigned int _count;    // offset 16 (4 bytes)
  //   struct {                 // offset 20 (4 bytes) - _flags
  //     unsigned int wide: 1;  // 0 = 8-bit chars, 1 = 16-bit chars
  //     unsigned int owned: 1;
  //     unsigned int unused: 2;
  //     unsigned int hash: 28;
  //   } _flags;
  // };
  // 
  // For inline strings, the actual character data is stored immediately
  // after the object structure in memory. The _contents pointer points
  // to this inline data.
  
  Status error;
  
  // Get the class name to determine the inline string class
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  // Get dynamic offsets for GSInlineString fields
  ptrdiff_t count_offset = GNUstepRuntimeHelper::GetIvarOffset(process, class_name, "_count");
  if (count_offset < 0) {
    count_offset = 16; // Fallback to hardcoded offset
  }
  
  ptrdiff_t flags_offset = GNUstepRuntimeHelper::GetIvarOffset(process, class_name, "_flags");
  if (flags_offset < 0) {
    flags_offset = 20; // Fallback to hardcoded offset
  }
  
  // Read the string length using dynamic offset
  lldb::addr_t count_addr = obj_addr + count_offset;
  uint32_t string_length = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, count_addr, &string_length, sizeof(string_length))) {
    return "";
  }
  
  // Sanity check the length
  if (string_length == 0 || string_length > 10000) {
    return "";
  }
  
  // Read the flags to check if it's wide (16-bit) characters using dynamic offset
  lldb::addr_t flags_addr = obj_addr + flags_offset;
  uint32_t flags = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, flags_addr, &flags, sizeof(flags))) {
    return "";
  }
  
  bool is_wide = (flags & 0x1) != 0;
  
  // For inline strings, data starts right after the object structure
  // Calculate the object size dynamically based on the highest ivar offset + size
  size_t object_size = std::max({
    static_cast<size_t>(count_offset + 4),  // _count is uint32_t (4 bytes)
    static_cast<size_t>(flags_offset + 4),  // _flags is uint32_t (4 bytes)
    static_cast<size_t>(24)  // Minimum fallback size
  });
  
  // The inline data starts immediately after the object
  lldb::addr_t data_addr = obj_addr + object_size;
  
  if (is_wide || class_name == "GSUInlineString") {
    // 16-bit Unicode characters
    size_t byte_size = string_length * sizeof(uint16_t);
    if (byte_size > 10000 * sizeof(uint16_t)) {
      byte_size = 10000 * sizeof(uint16_t);
    }
    
    std::vector<uint16_t> buffer(string_length);
    if (!GNUstepRuntimeHelper::ReadMemory(process, data_addr, buffer.data(), byte_size)) {
      return "";
    }
    
    // Convert UTF-16 to UTF-8
    std::string result;
    result.reserve(string_length * 2);
    for (uint32_t i = 0; i < string_length; ++i) {
      uint16_t ch = buffer[i];
      if (ch < 0x80) {
        result.push_back(static_cast<char>(ch));
      } else if (ch < 0x800) {
        result.push_back(static_cast<char>(0xC0 | (ch >> 6)));
        result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
      } else {
        result.push_back(static_cast<char>(0xE0 | (ch >> 12)));
        result.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
      }
    }
    return result;
  } else {
    // 8-bit characters (UTF-8 or ASCII)
    return GNUstepRuntimeHelper::ReadUTF8String(process, data_addr, string_length);
  }
}

// Function wrapper for LLDB registration
bool lldb_private::formatters::GNUstepNSStringFormatterFunction(ValueObject &valobj, Stream &stream, 
                                     const TypeSummaryOptions &options) {
  GNUstepNSStringSummaryProvider provider_instance;
  return provider_instance.FormatObject(valobj, stream, options);
}

// Smart id formatter that checks runtime type and delegates to appropriate formatter
bool lldb_private::formatters::GNUstepIdFormatterFunction(ValueObject &valobj, Stream &stream, 
                                                          const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    return false; // Let LLDB handle it with default formatting
  }
  
  // Get the actual runtime class name
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  // Delegate to the appropriate formatter based on runtime type
  if (class_name == "NSString" || class_name.find("String") != std::string::npos) {
    // Use string formatter
    GNUstepNSStringSummaryProvider string_provider;
    return string_provider.FormatObject(valobj, stream, options);
  }
  
  // For other types, return false to let LLDB handle with default formatting
  return false;
}

// ===== GSCInlineString Synthetic Provider Implementation =====

GSCInlineStringSyntheticProvider::GSCInlineStringSyntheticProvider(lldb::ValueObjectSP valobj_sp)
    : GNUstepSyntheticProvider(valobj_sp), m_obj_addr(LLDB_INVALID_ADDRESS) {
  memset(static_cast<void*>(&m_string_info), 0, sizeof(m_string_info));
}

bool GSCInlineStringSyntheticProvider::UpdateImpl() {
  // Clear previous state
  memset(static_cast<void*>(&m_string_info), 0, sizeof(m_string_info));
  
  if (!m_process)
    return false;
    
  m_obj_addr = m_backend.GetPointerValue();
  if (m_obj_addr == 0 || m_obj_addr == LLDB_INVALID_ADDRESS)
    return false;
  
  // Get the class name to verify this is an inline string
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(m_backend);
  if (class_name != "GSCInlineString" && class_name != "GSUInlineString") {
    return false;
  }
  
  // Extract string information using the existing ExtractInlineString logic
  // GSCInlineString memory layout:
  // Offset 0:  ISA pointer (8 bytes)
  // Offset 8:  _contents union pointer (8 bytes) - points to inline data
  // Offset 16: _count (4 bytes) - string length  
  // Offset 20: _flags (4 bytes) - encoding flags
  // Offset 24+: Inline string data
  
  // Get dynamic offsets
  ptrdiff_t count_offset = GNUstepRuntimeHelper::GetIvarOffset(m_process, class_name, "_count");
  if (count_offset < 0) {
    count_offset = 16; // Fallback
  }
  
  ptrdiff_t flags_offset = GNUstepRuntimeHelper::GetIvarOffset(m_process, class_name, "_flags");
  if (flags_offset < 0) {
    flags_offset = 20; // Fallback
  }
  
  // Read count
  lldb::addr_t count_addr = m_obj_addr + count_offset;
  if (!GNUstepRuntimeHelper::ReadMemory(m_process, count_addr, &m_string_info.count, sizeof(m_string_info.count))) {
    return false;
  }
  
  // Read flags
  lldb::addr_t flags_addr = m_obj_addr + flags_offset;
  if (!GNUstepRuntimeHelper::ReadMemory(m_process, flags_addr, &m_string_info.flags, sizeof(m_string_info.flags))) {
    return false;
  }
  
  // Determine if wide characters
  m_string_info.is_wide = (m_string_info.flags & 0x1) != 0;
  
  // Extract the actual string content using existing logic
  GNUstepNSStringSummaryProvider string_provider;
  m_string_info.content = string_provider.ExtractInlineString(m_backend);
  
  return true;
}

llvm::Expected<uint32_t> GSCInlineStringSyntheticProvider::CalculateNumChildren() {
  if (!m_update_called)
    return 0;
  
  // Show 3 children: _contents (as string), _count, _flags
  return 3;
}

lldb::ValueObjectSP GSCInlineStringSyntheticProvider::GetChildAtIndex(uint32_t idx) {
  if (!m_update_called || idx >= 3)
    return nullptr;
  
  // Use the safer approach similar to other working synthetic providers
  CompilerType backend_type = m_backend.GetCompilerType();
  if (!backend_type.IsValid()) {
    return nullptr;
  }
  
  auto type_system = backend_type.GetTypeSystem();
  if (!type_system) {
    return nullptr;
  }
  
  switch (idx) {
    case 0: {
      // _contents - Create a simple string representation
      CompilerType char_ptr_type = type_system->GetBasicTypeFromAST(eBasicTypeChar).GetPointerType();
      if (!char_ptr_type.IsValid()) {
        return nullptr;
      }
      
      // For now, just show the string content as a description rather than creating complex data
      // This avoids the crash and still provides useful information
      lldb::addr_t content_addr = m_obj_addr + 24; // Inline data starts at offset 24
      return CreateValueObjectFromAddress("_contents", content_addr, char_ptr_type);
    }
    
    case 1: {
      // _count - show the string length at its actual memory location
      CompilerType uint32_type = type_system->GetBasicTypeFromAST(eBasicTypeUnsignedInt);
      if (!uint32_type.IsValid()) {
        return nullptr;
      }
      
      lldb::addr_t count_addr = m_obj_addr + 16; // _count is at offset 16
      return CreateValueObjectFromAddress("_count", count_addr, uint32_type);
    }
    
    case 2: {
      // _flags - show the encoding flags at its actual memory location
      CompilerType uint32_type = type_system->GetBasicTypeFromAST(eBasicTypeUnsignedInt);
      if (!uint32_type.IsValid()) {
        return nullptr;
      }
      
      lldb::addr_t flags_addr = m_obj_addr + 20; // _flags is at offset 20
      return CreateValueObjectFromAddress("_flags", flags_addr, uint32_type);
    }
    
    default:
      return nullptr;
  }
}

// Creator function for GSCInlineString synthetic provider
SyntheticChildrenFrontEnd *
lldb_private::formatters::GSCInlineStringSyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                                                  lldb::ValueObjectSP valobj_sp) {
  return new GSCInlineStringSyntheticProvider(valobj_sp);
}
