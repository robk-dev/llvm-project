//===-- GNUstepJSONSerializationFormatters.cpp --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepJSONSerializationFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/StreamString.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/lldb-enumerations.h"
#include <sstream>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSJSONSerializationSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream,
                                                            const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid NSJSONSerialization object");
    return false;
  }

  // NSJSONSerialization is typically a class object (static utility class)
  // But we may also encounter instances with JSON data or options
  if (IsClassObject(valobj)) {
    FormatClassObject(valobj, stream);
  } else {
    FormatJSONInstance(valobj, stream);
  }
  
  return true;
}

bool GNUstepNSJSONSerializationSummaryProvider::IsClassObject(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return false;

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return false;

  // Read the isa pointer
  Status error;
  lldb::addr_t isa_addr = GNUstepRuntimeHelper::ReadPointer(process, object_addr, error);
  
  if (error.Fail() || isa_addr == 0 || isa_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }

  // For class objects, isa points to the metaclass
  // We can try to read the class name to determine if it's NSJSONSerialization
  GNUstepObjCRuntimeIntrospector introspector(process);
  std::string class_name = introspector.GetClassName(object_addr);
  
  return class_name == "NSJSONSerialization" || class_name.find("JSONSerialization") != std::string::npos;
}

void GNUstepNSJSONSerializationSummaryProvider::FormatClassObject(ValueObject &valobj, Stream &stream) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    WriteErrorSummary(stream, "no process");
    return;
  }

  lldb::addr_t object_addr = valobj.GetPointerValue();
  GNUstepObjCRuntimeIntrospector introspector(process);
  std::string class_name = introspector.GetClassName(object_addr);
  
  if (class_name.empty()) {
    class_name = "NSJSONSerialization";
  }
  
  // Format as static utility class
  std::ostringstream oss;
  oss << class_name << " (static utility class)";
  WriteQuotedString(stream, oss.str());
}

void GNUstepNSJSONSerializationSummaryProvider::FormatJSONInstance(ValueObject &valobj, Stream &stream) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    WriteErrorSummary(stream, "no process");
    return;
  }

  // Try to extract JSON data
  std::string json_data = ExtractJSONData(valobj);
  
  // Try to extract options
  uint64_t reading_options = ExtractOptionsValue(valobj, "_readingOptions");
  uint64_t writing_options = ExtractOptionsValue(valobj, "_writingOptions");
  
  std::ostringstream oss;
  
  if (!json_data.empty()) {
    std::string json_type = DetectJSONType(json_data);
    std::string preview = CreateJSONPreview(json_data);
    
    oss << "JSON " << json_type << " (" << json_data.length() << " bytes)";
    if (!preview.empty()) {
      oss << ": " << preview;
    }
  } else if (reading_options != 0) {
    oss << "JSONReading(" << FormatReadingOptions(reading_options) << ")";
  } else if (writing_options != 0) {
    oss << "JSONWriting(" << FormatWritingOptions(writing_options) << ")";
  } else {
    // Fallback: show class name
    lldb::addr_t object_addr = valobj.GetPointerValue();
    GNUstepObjCRuntimeIntrospector introspector(process);
    std::string class_name = introspector.GetClassName(object_addr);
    oss << (class_name.empty() ? "JSON object" : class_name);
  }
  
  WriteQuotedString(stream, oss.str());
}

std::string GNUstepNSJSONSerializationSummaryProvider::ExtractJSONData(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return "";

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return "";

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // Look for common JSON-related ivar names
  // _jsonData, _data, _sourceData, etc.
  std::vector<std::string> possible_ivars = {"_jsonData", "_data", "_sourceData"};
  
  for (const auto& ivar_name : possible_ivars) {
    // Try to find NSData object containing JSON
    // This is a simplified approach - in practice would need proper ivar layout
    (void)ivar_name; // Suppress unused variable warning
    for (int offset_multiplier = 1; offset_multiplier <= 5; ++offset_multiplier) {
      lldb::addr_t data_ptr_addr = object_addr + (offset_multiplier * addr_size);
      
      Status error;
      lldb::addr_t data_obj_addr = GNUstepRuntimeHelper::ReadPointer(process, data_ptr_addr, error);
      
      if (error.Success() && data_obj_addr != 0 && data_obj_addr != LLDB_INVALID_ADDRESS) {
        // Try to read as NSData
        std::string potential_json = ExtractDataFromNSData(process, data_obj_addr);
        if (!potential_json.empty() && IsValidJSONPreview(potential_json)) {
          return potential_json;
        }
      }
    }
  }
  
  return "";
}

std::string GNUstepNSJSONSerializationSummaryProvider::FormatReadingOptions(uint64_t options) {
  if (options == 0) {
    return "None";
  }
  
  std::vector<std::string> flags;
  
  if (options & 1) flags.push_back("MutableContainers");     // NSJSONReadingMutableContainers
  if (options & 2) flags.push_back("MutableLeaves");         // NSJSONReadingMutableLeaves
  if (options & 4) flags.push_back("FragmentsAllowed");      // NSJSONReadingFragmentsAllowed
  
  // Join flags with |
  std::ostringstream oss;
  for (size_t i = 0; i < flags.size(); ++i) {
    if (i > 0) oss << "|";
    oss << flags[i];
  }
  
  return oss.str();
}

std::string GNUstepNSJSONSerializationSummaryProvider::FormatWritingOptions(uint64_t options) {
  if (options == 0) {
    return "None";
  }
  
  std::vector<std::string> flags;
  
  if (options & 1) flags.push_back("PrettyPrinted");          // NSJSONWritingPrettyPrinted
  if (options & 2) flags.push_back("SortedKeys");             // NSJSONWritingSortedKeys
  if (options & 4) flags.push_back("FragmentsAllowed");       // NSJSONWritingFragmentsAllowed
  if (options & 8) flags.push_back("WithoutEscapingSlashes"); // NSJSONWritingWithoutEscapingSlashes
  
  // Join flags with |
  std::ostringstream oss;
  for (size_t i = 0; i < flags.size(); ++i) {
    if (i > 0) oss << "|";
    oss << flags[i];
  }
  
  return oss.str();
}

uint64_t GNUstepNSJSONSerializationSummaryProvider::ExtractOptionsValue(ValueObject &valobj, 
                                                                        const char* ivar_name) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return 0;

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return 0;

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // Try different offsets to find the options value
  // This is simplified - real implementation would use proper ivar introspection
  for (int offset_multiplier = 1; offset_multiplier <= 4; ++offset_multiplier) {
    lldb::addr_t options_addr = object_addr + (offset_multiplier * addr_size);
    
    Status error;
    uint64_t options_value = 0;
    
    size_t bytes_read = process->ReadMemory(options_addr, &options_value, sizeof(uint64_t), error);
    
    if (bytes_read == sizeof(uint64_t) && error.Success()) {
      // Check if this looks like valid options (reasonable small value)
      if (options_value > 0 && options_value < 256) {
        return options_value;
      }
    }
  }
  
  return 0;
}

std::string GNUstepNSJSONSerializationSummaryProvider::DetectJSONType(const std::string& json_data) {
  if (json_data.empty()) {
    return "Empty";
  }
  
  // Trim whitespace
  size_t start = 0;
  while (start < json_data.length() && isspace(json_data[start])) {
    start++;
  }
  
  if (start >= json_data.length()) {
    return "Empty";
  }
  
  char first_char = json_data[start];
  
  switch (first_char) {
    case '{': return "Object";
    case '[': return "Array";
    case '"': return "String";
    case 't': case 'f': return "Boolean"; // true/false
    case 'n': return "Null";              // null
    default:
      if (isdigit(first_char) || first_char == '-' || first_char == '+') {
        return "Number";
      }
      return "Unknown";
  }
}

std::string GNUstepNSJSONSerializationSummaryProvider::CreateJSONPreview(const std::string& json_data, 
                                                                        size_t max_length) {
  if (json_data.empty()) {
    return "";
  }
  
  std::string preview = json_data;
  
  // Remove excessive whitespace for preview
  std::string compressed;
  bool in_string = false;
  bool escaped = false;
  
  for (char c : preview) {
    if (!in_string && isspace(c)) {
      // Skip extra whitespace outside strings
      if (!compressed.empty() && !isspace(compressed.back())) {
        compressed += ' ';
      }
    } else {
      if (c == '"' && !escaped) {
        in_string = !in_string;
      }
      escaped = (c == '\\' && !escaped);
      compressed += c;
    }
  }
  
  preview = compressed;
  
  // Truncate if too long
  if (preview.length() > max_length) {
    preview = preview.substr(0, max_length - 3) + "...";
  }
  
  return preview;
}

bool GNUstepNSJSONSerializationSummaryProvider::IsValidJSONPreview(const std::string& data) {
  if (data.empty()) {
    return false;
  }
  
  // Simple JSON validation for preview purposes
  // Just check if it starts with valid JSON characters
  char first_char = data[0];
  
  // Valid JSON can start with: { [ " digit - true false null
  if (first_char == '{' || first_char == '[' || first_char == '"') {
    return true;
  }
  
  if (first_char == 't' && data.substr(0, 4) == "true") {
    return true;
  }
  
  if (first_char == 'f' && data.substr(0, 5) == "false") {
    return true;
  }
  
  if (first_char == 'n' && data.substr(0, 4) == "null") {
    return true;
  }
  
  if (isdigit(first_char) || first_char == '-') {
    return true; // Likely a number
  }
  
  return false;
}

std::string GNUstepNSJSONSerializationSummaryProvider::ExtractDataFromNSData(Process *process, 
                                                                             lldb::addr_t data_addr) {
  if (!process || data_addr == 0 || data_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // NSData layout: [isa][length][bytes_pointer] or [isa][length][inline_data]
  // Length is at offset addr_size (after isa)
  lldb::addr_t length_addr = data_addr + addr_size;
  
  Status error;
  uint64_t data_length = 0;
  
  size_t bytes_read = process->ReadMemory(length_addr, &data_length, sizeof(uint64_t), error);
  
  if (bytes_read != sizeof(uint64_t) || error.Fail() || data_length == 0 || data_length > 10240) {
    // Try 32-bit length
    uint32_t data_length_32 = 0;
    bytes_read = process->ReadMemory(length_addr, &data_length_32, sizeof(uint32_t), error);
    
    if (bytes_read == sizeof(uint32_t) && error.Success() && data_length_32 > 0 && data_length_32 <= 10240) {
      data_length = data_length_32;
    } else {
      return "";
    }
  }
  
  // Read data pointer (after isa and length)
  lldb::addr_t data_ptr_addr = data_addr + addr_size + sizeof(uint64_t);
  lldb::addr_t data_bytes_addr = GNUstepRuntimeHelper::ReadPointer(process, data_ptr_addr, error);
  
  if (error.Fail() || data_bytes_addr == 0 || data_bytes_addr == LLDB_INVALID_ADDRESS) {
    // Try inline data (some NSData implementations store small data inline)
    data_bytes_addr = data_ptr_addr;
  }
  
  // Read the actual data
  std::vector<uint8_t> buffer(std::min(data_length, static_cast<uint64_t>(1024)));
  bytes_read = process->ReadMemory(data_bytes_addr, buffer.data(), buffer.size(), error);
  
  if (bytes_read == 0 || error.Fail()) {
    return "";
  }
  
  // Convert to string (assuming UTF-8 JSON data)
  std::string result(reinterpret_cast<const char*>(buffer.data()), bytes_read);
  
  // Basic validation that this looks like text/JSON
  bool has_printable = false;
  for (char c : result) {
    if (isprint(c) || isspace(c)) {
      has_printable = true;
    } else if (c == 0) {
      break; // Stop at null terminator
    } else {
      return ""; // Found non-printable character, probably not JSON
    }
  }
  
  return has_printable ? result : "";
}

bool lldb_private::formatters::GNUstepNSJSONSerializationFormatterFunction(ValueObject &valobj, 
                                                                           Stream &stream,
                                                                           const TypeSummaryOptions &options) {
  GNUstepNSJSONSerializationSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}