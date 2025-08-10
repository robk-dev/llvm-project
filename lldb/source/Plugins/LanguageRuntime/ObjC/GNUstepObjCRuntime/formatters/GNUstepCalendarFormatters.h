//===-- GNUstepCalendarFormatters.h ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_CALENDAR_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_CALENDAR_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// Summary provider for GNUstep NSCalendar objects
class GNUstepNSCalendarSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  /// Extract calendar information from a GNUstep NSCalendar object
  struct CalendarInfo {
    std::string identifier;
    std::string locale;
    bool valid;
    
    CalendarInfo() : valid(false) {}
  };
  
  /// Extract calendar information from NSCalendar object
  CalendarInfo ExtractCalendarInfo(ValueObject &valobj);
  
  /// Extract calendar identifier string from internal structure
  std::string ExtractCalendarIdentifier(ValueObject &valobj);
  
  /// Extract locale information if available from internal structure
  std::string ExtractCalendarLocale(ValueObject &valobj);
  
  /// Convert calendar identifier constant to human-readable name
  std::string ConvertIdentifierToDisplayName(const std::string &identifier);
  
  /// Check if a string looks like a calendar identifier
  bool IsLikelyCalendarIdentifier(const std::string &str);
  
  /// Extract string content from NSString object at given address
  std::string ExtractStringFromAddress(Process *process, lldb::addr_t str_addr);
  
  /// Map of known calendar identifiers to display names
  static const std::map<std::string, std::string> s_calendar_display_names;
};

/// Function wrapper for LLDB registration
bool GNUstepNSCalendarFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_CALENDAR_H