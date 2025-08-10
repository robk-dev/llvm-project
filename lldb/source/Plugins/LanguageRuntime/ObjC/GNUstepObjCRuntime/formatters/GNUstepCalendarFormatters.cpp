//===-- GNUstepCalendarFormatters.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepCalendarFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include <map>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

// Static calendar identifier to display name mapping
const std::map<std::string, std::string> 
GNUstepNSCalendarSummaryProvider::s_calendar_display_names = {
  {"NSCalendarIdentifierGregorian", "Gregorian"},
  {"NSGregorianCalendar", "Gregorian"}, // Legacy identifier
  {"NSCalendarIdentifierBuddhist", "Buddhist"},
  {"NSBuddhistCalendar", "Buddhist"}, // Legacy identifier  
  {"NSCalendarIdentifierChinese", "Chinese"},
  {"NSChineseCalendar", "Chinese"}, // Legacy identifier
  {"NSCalendarIdentifierCoptic", "Coptic"},
  {"NSCalendarIdentifierEthiopicAmeteMihret", "Ethiopic Amete Mihret"},
  {"NSCalendarIdentifierEthiopicAmeteAlem", "Ethiopic Amete Alem"},
  {"NSCalendarIdentifierHebrew", "Hebrew"},
  {"NSHebrewCalendar", "Hebrew"}, // Legacy identifier
  {"NSCalendarIdentifierISO8601", "ISO8601"},
  {"NSISO8601Calendar", "ISO8601"}, // Legacy identifier
  {"NSCalendarIdentifierIndian", "Indian"},
  {"NSIndianCalendar", "Indian"}, // Legacy identifier
  {"NSCalendarIdentifierIslamic", "Islamic"},
  {"NSIslamicCalendar", "Islamic"}, // Legacy identifier
  {"NSCalendarIdentifierIslamicCivil", "Islamic Civil"},
  {"NSIslamicCivilCalendar", "Islamic Civil"}, // Legacy identifier
  {"NSCalendarIdentifierJapanese", "Japanese"},
  {"NSJapaneseCalendar", "Japanese"}, // Legacy identifier
  {"NSCalendarIdentifierPersian", "Persian"},
  {"NSPersianCalendar", "Persian"}, // Legacy identifier
  {"NSCalendarIdentifierRepublicOfChina", "Republic of China"},
  {"NSRepublicOfChinaCalendar", "Republic of China"}, // Legacy identifier
  {"NSCalendarIdentifierIslamicTabular", "Islamic Tabular"},
  {"NSCalendarIdentifierIslamicUmmAlQura", "Islamic Umm al-Qura"}
};

bool GNUstepNSCalendarSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream, 
                                                   const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid object");
    return false;
  }
  
  CalendarInfo info = ExtractCalendarInfo(valobj);
  if (!info.valid) {
    WriteErrorSummary(stream, "could not extract calendar info");
    return false;
  }
  
  // Convert identifier to display name
  std::string display_name = ConvertIdentifierToDisplayName(info.identifier);
  
  // Format: NSCalendar(type='Gregorian', locale='en_US')
  if (!info.locale.empty()) {
    stream.Printf("NSCalendar(type='%s', locale='%s')", 
                  display_name.c_str(), info.locale.c_str());
  } else {
    stream.Printf("NSCalendar(type='%s')", display_name.c_str());
  }
  
  return true;
}

GNUstepNSCalendarSummaryProvider::CalendarInfo 
GNUstepNSCalendarSummaryProvider::ExtractCalendarInfo(ValueObject &valobj) {
  CalendarInfo info;
  
  // Extract calendar identifier
  info.identifier = ExtractCalendarIdentifier(valobj);
  
  // Extract locale information if available
  info.locale = ExtractCalendarLocale(valobj);
  
  // Calendar is valid if we can extract at least the identifier
  info.valid = !info.identifier.empty() || valobj.GetPointerValue() != 0;
  
  return info;
}

std::string GNUstepNSCalendarSummaryProvider::ExtractCalendarIdentifier(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // GNUstep NSCalendar structure analysis:
  // struct NSCalendar {
  //   Class isa;                    // Object's class pointer (offset 0)
  //   void *_NSCalendarInternal;    // Internal calendar data (offset 8 on 64-bit)
  //   void *_dummy1;                // Padding (offset 16)
  //   void *_dummy2;                // Padding (offset 24)
  //   void *_dummy3;                // Padding (offset 32)
  // };
  
  // The _NSCalendarInternal pointer points to the real calendar implementation
  lldb::addr_t internal_ptr_addr = obj_addr + 8;  // Skip isa pointer
  
  Status error;
  lldb::addr_t internal_obj_addr = GNUstepRuntimeHelper::ReadPointer(process, internal_ptr_addr, error);
  if (error.Fail() || internal_obj_addr == 0 || internal_obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // The internal structure typically contains the calendar identifier as a string
  // Based on GNUstep implementation analysis, the identifier is usually stored
  // at the beginning of the internal structure or as an instance variable
  
  // Try to call the calendarIdentifier method if possible via runtime introspection
  GNUstepObjCRuntimeIntrospector introspector(process);
  
  // Attempt 1: Try to call [calendar calendarIdentifier] method
  // This is the most reliable approach if CallRuntimeFunction is available
  // For now, we'll use memory layout analysis as fallback
  
  // Attempt 2: Memory layout analysis
  // The internal structure likely contains a pointer to the identifier string
  // Let's try reading the first few pointers in the internal structure
  
  for (size_t offset = 0; offset < 64; offset += 8) { // Try first 8 pointers
    lldb::addr_t potential_str_ptr_addr = internal_obj_addr + offset;
    lldb::addr_t str_obj_addr = GNUstepRuntimeHelper::ReadPointer(process, potential_str_ptr_addr, error);
    
    if (error.Fail() || str_obj_addr == 0 || str_obj_addr == LLDB_INVALID_ADDRESS) {
      continue;
    }
    
    // Check if this looks like a string object
    if (introspector.IsTaggedPointer(str_obj_addr)) {
      std::string tagged_str = introspector.DecodeTaggedString(str_obj_addr);
      if (IsLikelyCalendarIdentifier(tagged_str)) {
        return tagged_str;
      }
    }
    
    // Try to read as NSString/NSConstantString
    std::string str_content = ExtractStringFromAddress(process, str_obj_addr);
    if (IsLikelyCalendarIdentifier(str_content)) {
      return str_content;
    }
  }
  
  // Attempt 3: Default to "Gregorian" if we can't extract the identifier
  // Most calendars are Gregorian by default
  return "Gregorian";
}

std::string GNUstepNSCalendarSummaryProvider::ExtractCalendarLocale(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Try to extract locale information from the internal structure
  // This is more complex and may not always be available
  lldb::addr_t internal_ptr_addr = obj_addr + 8;  // Skip isa pointer
  
  Status error;
  lldb::addr_t internal_obj_addr = GNUstepRuntimeHelper::ReadPointer(process, internal_ptr_addr, error);
  if (error.Fail() || internal_obj_addr == 0 || internal_obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // For now, return empty string - locale extraction would require deeper
  // understanding of the internal calendar structure
  return "";
}

std::string GNUstepNSCalendarSummaryProvider::ConvertIdentifierToDisplayName(const std::string &identifier) {
  auto it = s_calendar_display_names.find(identifier);
  if (it != s_calendar_display_names.end()) {
    return it->second;
  }
  
  // If not found in map, return the identifier as-is but clean it up
  std::string clean_id = identifier;
  
  // Remove common prefixes
  if (clean_id.find("NSCalendarIdentifier") == 0) {
    clean_id = clean_id.substr(20); // Remove "NSCalendarIdentifier"
  } else if (clean_id.find("NS") == 0 && clean_id.find("Calendar") != std::string::npos) {
    // Remove NS prefix and Calendar suffix for legacy identifiers
    clean_id = clean_id.substr(2); // Remove "NS"
    size_t calendar_pos = clean_id.find("Calendar");
    if (calendar_pos != std::string::npos) {
      clean_id = clean_id.substr(0, calendar_pos);
    }
  }
  
  return clean_id.empty() ? "Unknown" : clean_id;
}

bool GNUstepNSCalendarSummaryProvider::IsLikelyCalendarIdentifier(const std::string &str) {
  if (str.empty()) {
    return false;
  }
  
  // Check if it matches known calendar identifier patterns
  return (str.find("Calendar") != std::string::npos || 
          str.find("Gregorian") != std::string::npos ||
          str.find("Buddhist") != std::string::npos ||
          str.find("Chinese") != std::string::npos ||
          str.find("Hebrew") != std::string::npos ||
          str.find("Islamic") != std::string::npos ||
          str.find("Japanese") != std::string::npos ||
          str.find("ISO8601") != std::string::npos);
}

std::string GNUstepNSCalendarSummaryProvider::ExtractStringFromAddress(Process *process, lldb::addr_t str_addr) {
  if (!process || str_addr == 0 || str_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Try to read as NSConstantString structure:
  // struct {
  //   Class isa;          // Object's class pointer (offset 0)
  //   uint32_t len;       // String length (offset 8)
  //   uint32_t padding;   // Padding (offset 12)
  //   uint64_t len2;      // Length again? (offset 16)
  //   const char *str;    // C string data pointer (offset 24)
  // };
  
  lldb::addr_t str_ptr_addr = str_addr + 24;  // Skip to string pointer
  Status error;
  lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  if (error.Fail() || str_data_addr == 0 || str_data_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read the actual string content (limit to 128 chars for safety)
  return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 128);
}

// Function wrapper for LLDB registration
bool lldb_private::formatters::GNUstepNSCalendarFormatterFunction(ValueObject &valobj, Stream &stream, 
                                                                 const TypeSummaryOptions &options) {
  GNUstepNSCalendarSummaryProvider provider;
  return provider.FormatObject(valobj, stream, options);
}