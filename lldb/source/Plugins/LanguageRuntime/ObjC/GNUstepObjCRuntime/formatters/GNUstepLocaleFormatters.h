//===-- GNUstepLocaleFormatters.h -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_LOCALE_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_LOCALE_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// Summary provider for GNUstep NSLocale objects
class GNUstepNSLocaleSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  /// Extract locale information from a GNUstep NSLocale object
  struct LocaleInfo {
    std::string identifier;
    std::string language;
    std::string country;
    std::string currency;
    std::string script;
    bool valid;
    
    LocaleInfo() : valid(false) {}
  };
  
  /// Extract locale information from NSLocale object
  LocaleInfo ExtractLocaleInfo(ValueObject &valobj);
  
  /// Extract locale identifier string from _localeId instance variable
  std::string ExtractLocaleIdentifier(ValueObject &valobj);
  
  /// Extract locale components from _components dictionary
  LocaleInfo ExtractLocaleComponents(ValueObject &valobj, const std::string &identifier);
  
  /// Parse locale identifier into components (e.g., "en_US" -> language="en", country="US")
  void ParseLocaleIdentifier(const std::string &identifier, LocaleInfo &info);
  
  /// Get currency code for common locales
  std::string GetCurrencyForLocale(const std::string &identifier);
  
  /// Get display name for language code
  std::string GetLanguageDisplayName(const std::string &languageCode);
};

/// Function wrapper for LLDB registration
bool GNUstepNSLocaleFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_LOCALE_H