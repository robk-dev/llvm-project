//===-- GNUstepDateFormatters.h --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_DATE_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_DATE_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// Summary provider for GNUstep NSDate objects
class GNUstepNSDateSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  /// Extract time interval since reference date (2001-01-01 00:00:00 UTC)
  double ExtractTimeInterval(ValueObject &valobj);
  
  /// Format a timestamp as a human-readable string
  std::string FormatTimestamp(double time_interval);
  
  /// Handle NSCalendarDate objects
  std::string ExtractCalendarDate(ValueObject &valobj);
};

/// Summary provider for GNUstep NSTimeZone objects
class GNUstepNSTimeZoneSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  /// Extract timezone name from GSTimeZone objects
  std::string ExtractTimeZoneName(ValueObject &valobj);
  
  /// Extract timezone offset from various timezone types
  int32_t ExtractTimeZoneOffset(ValueObject &valobj, bool &has_dst);
  
  /// Handle GSAbsTimeZone objects (fixed offset)
  std::string FormatAbsoluteTimeZone(ValueObject &valobj);
  
  /// Handle GSTimeZone objects (complex zoneinfo-based)
  std::string FormatComplexTimeZone(ValueObject &valobj);
  
  /// Handle NSLocalTimeZone objects
  std::string FormatLocalTimeZone(ValueObject &valobj);
  
  /// Format timezone offset as string (e.g., "-0500", "+0100")
  std::string FormatTimeZoneOffset(int32_t offset_seconds);
  
  /// Extract string content from a GNUstep string object address
  std::string TryExtractStringContent(Process *process, lldb::addr_t obj_addr);
};

/// Function wrapper for LLDB registration
bool GNUstepNSDateFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

/// Function wrapper for NSTimeZone LLDB registration
bool GNUstepNSTimeZoneFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_DATE_H