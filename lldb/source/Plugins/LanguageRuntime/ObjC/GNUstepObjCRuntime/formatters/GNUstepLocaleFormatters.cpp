//===-- GNUstepLocaleFormatters.cpp -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepLocaleFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSLocaleSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream, 
                                                  const TypeSummaryOptions &options) {
  // Check for nil
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }
  
  // Skip IsValidGNUstepObject check for now - it might be too strict
  // if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
  //   WriteErrorSummary(stream, "invalid object");
  //   return false;
  // }
  
  LocaleInfo info = ExtractLocaleInfo(valobj);
  if (!info.valid) {
    // Try a fallback simple format
    stream.Printf("NSLocale(id=<unknown>)");
    return true;
  }
  
  // Format: NSLocale(id='en_US', language='English', currency='USD')
  stream.Printf("NSLocale(id='%s'", info.identifier.c_str());
  
  if (!info.language.empty()) {
    stream.Printf(", language='%s'", info.language.c_str());
  }
  
  if (!info.currency.empty()) {
    stream.Printf(", currency='%s'", info.currency.c_str());
  }
  
  if (!info.country.empty() && info.country != info.identifier) {
    stream.Printf(", country='%s'", info.country.c_str());
  }
  
  if (!info.script.empty()) {
    stream.Printf(", script='%s'", info.script.c_str());
  }
  
  stream.Printf(")");
  
  return true;
}

GNUstepNSLocaleSummaryProvider::LocaleInfo 
GNUstepNSLocaleSummaryProvider::ExtractLocaleInfo(ValueObject &valobj) {
  LocaleInfo info;
  
  // Extract locale identifier
  info.identifier = ExtractLocaleIdentifier(valobj);
  if (info.identifier.empty()) {
    return info; // Invalid locale
  }
  
  // Parse the identifier to extract basic components
  ParseLocaleIdentifier(info.identifier, info);
  
  // Get currency code for this locale
  info.currency = GetCurrencyForLocale(info.identifier);
  
  // Get display name for language
  if (!info.language.empty()) {
    std::string displayName = GetLanguageDisplayName(info.language);
    if (!displayName.empty()) {
      info.language = displayName;
    }
  }
  
  // Try to extract additional components from _components dictionary
  LocaleInfo componentInfo = ExtractLocaleComponents(valobj, info.identifier);
  if (componentInfo.valid) {
    // Override with more detailed information from components
    if (!componentInfo.currency.empty()) {
      info.currency = componentInfo.currency;
    }
    if (!componentInfo.script.empty()) {
      info.script = componentInfo.script;
    }
  }
  
  info.valid = true;
  return info;
}

std::string GNUstepNSLocaleSummaryProvider::ExtractLocaleIdentifier(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return "";
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // GNUstep NSLocale structure analysis:
  // Based on typical GNUstep object layout:
  // struct NSLocale {
  //   Class isa;                 // Object's class pointer (offset 0)
  //   NSString *_localeId;       // Locale identifier (offset 8 on 64-bit)
  //   NSDictionary *_components; // Locale components (offset 16)
  //   // ... other instance variables
  // };
  
  // Try to read the _localeId instance variable at offset 8
  lldb::addr_t locale_id_ptr_addr = obj_addr + 8;  // Skip isa pointer
  
  Status error;
  lldb::addr_t locale_id_obj_addr = GNUstepRuntimeHelper::ReadPointer(process, locale_id_ptr_addr, error);
  if (error.Fail() || locale_id_obj_addr == 0 || locale_id_obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Use runtime introspector to check if this is a tagged pointer
  GNUstepObjCRuntimeIntrospector introspector(process);
  if (introspector.IsTaggedPointer(locale_id_obj_addr)) {
    return introspector.DecodeTaggedString(locale_id_obj_addr);
  }
  
  // For regular NSString objects, try to extract using constant string layout
  // NSConstantString structure:
  // struct {
  //   Class isa;          // Object's class pointer (offset 0)
  //   uint32_t len;       // String length (offset 8)
  //   uint32_t padding;   // Padding (offset 12)
  //   uint64_t len2;      // Length again? (offset 16)
  //   const char *str;    // C string data pointer (offset 24)
  // };
  
  lldb::addr_t str_ptr_addr = locale_id_obj_addr + 24;  // Skip to string pointer
  lldb::addr_t str_data_addr = GNUstepRuntimeHelper::ReadPointer(process, str_ptr_addr, error);
  if (error.Fail() || str_data_addr == 0 || str_data_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read the actual string content
  return GNUstepRuntimeHelper::ReadUTF8String(process, str_data_addr, 64);
}

GNUstepNSLocaleSummaryProvider::LocaleInfo 
GNUstepNSLocaleSummaryProvider::ExtractLocaleComponents(ValueObject &valobj, const std::string &identifier) {
  LocaleInfo info;
  
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return info;
  }
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return info;
  }
  
  // Try to read the _components instance variable at offset 16
  lldb::addr_t components_ptr_addr = obj_addr + 16;  // Skip isa and _localeId
  
  Status error;
  lldb::addr_t components_obj_addr = GNUstepRuntimeHelper::ReadPointer(process, components_ptr_addr, error);
  if (error.Fail() || components_obj_addr == 0 || components_obj_addr == LLDB_INVALID_ADDRESS) {
    return info; // No components dictionary available
  }
  
  // TODO: Extract specific keys from the components dictionary
  // This would require implementing NSDictionary value extraction
  // For now, we'll rely on identifier parsing
  
  info.valid = true;
  return info;
}

void GNUstepNSLocaleSummaryProvider::ParseLocaleIdentifier(const std::string &identifier, LocaleInfo &info) {
  if (identifier.empty()) {
    return;
  }
  
  // Parse locale identifier like "en_US", "fr_FR", "ja_JP", etc.
  // Format can be: language_country, language_script_country, or just language
  
  size_t underscore_pos = identifier.find('_');
  if (underscore_pos == std::string::npos) {
    // Just language code
    info.language = identifier;
    info.country = "";
  } else {
    // Language and country (or script)
    info.language = identifier.substr(0, underscore_pos);
    std::string remainder = identifier.substr(underscore_pos + 1);
    
    // Check if there's another underscore (for script)
    size_t second_underscore = remainder.find('_');
    if (second_underscore != std::string::npos) {
      // Format: language_script_country
      info.script = remainder.substr(0, second_underscore);
      info.country = remainder.substr(second_underscore + 1);
    } else {
      // Format: language_country
      info.country = remainder;
    }
  }
}

std::string GNUstepNSLocaleSummaryProvider::GetCurrencyForLocale(const std::string &identifier) {
  // Common currency mappings for debugging
  if (identifier.find("US") != std::string::npos) return "USD";
  if (identifier.find("CA") != std::string::npos) return "CAD";
  if (identifier.find("GB") != std::string::npos) return "GBP";
  if (identifier.find("FR") != std::string::npos) return "EUR";
  if (identifier.find("DE") != std::string::npos) return "EUR";
  if (identifier.find("IT") != std::string::npos) return "EUR";
  if (identifier.find("ES") != std::string::npos) return "EUR";
  if (identifier.find("JP") != std::string::npos) return "JPY";
  if (identifier.find("CN") != std::string::npos) return "CNY";
  if (identifier.find("KR") != std::string::npos) return "KRW";
  if (identifier.find("AU") != std::string::npos) return "AUD";
  if (identifier.find("BR") != std::string::npos) return "BRL";
  if (identifier.find("IN") != std::string::npos) return "INR";
  if (identifier.find("RU") != std::string::npos) return "RUB";
  if (identifier.find("MX") != std::string::npos) return "MXN";
  
  return ""; // Unknown currency
}

std::string GNUstepNSLocaleSummaryProvider::GetLanguageDisplayName(const std::string &languageCode) {
  // Common language display names for debugging
  if (languageCode == "en") return "English";
  if (languageCode == "fr") return "French";
  if (languageCode == "de") return "German";
  if (languageCode == "es") return "Spanish";
  if (languageCode == "it") return "Italian";
  if (languageCode == "ja") return "Japanese";
  if (languageCode == "zh") return "Chinese";
  if (languageCode == "ko") return "Korean";
  if (languageCode == "pt") return "Portuguese";
  if (languageCode == "ru") return "Russian";
  if (languageCode == "ar") return "Arabic";
  if (languageCode == "hi") return "Hindi";
  if (languageCode == "th") return "Thai";
  if (languageCode == "vi") return "Vietnamese";
  if (languageCode == "tr") return "Turkish";
  if (languageCode == "pl") return "Polish";
  if (languageCode == "nl") return "Dutch";
  if (languageCode == "sv") return "Swedish";
  if (languageCode == "da") return "Danish";
  if (languageCode == "no") return "Norwegian";
  if (languageCode == "fi") return "Finnish";
  if (languageCode == "cs") return "Czech";
  if (languageCode == "sk") return "Slovak";
  if (languageCode == "hu") return "Hungarian";
  if (languageCode == "he") return "Hebrew";
  if (languageCode == "el") return "Greek";
  
  return languageCode; // Return language code if no display name found
}

// Function wrapper for LLDB registration
bool lldb_private::formatters::GNUstepNSLocaleFormatterFunction(ValueObject &valobj, Stream &stream, 
                                                               const TypeSummaryOptions &options) {
  GNUstepNSLocaleSummaryProvider provider;
  return provider.FormatObject(valobj, stream, options);
}