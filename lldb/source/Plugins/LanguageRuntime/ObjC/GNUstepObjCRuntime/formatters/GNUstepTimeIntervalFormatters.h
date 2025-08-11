//===-- GNUstepTimeIntervalFormatters.h -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPTIMEINTERVALFORMATTERS_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPTIMEINTERVALFORMATTERS_H

#include "GNUstepFormattersBase.h"
#include "lldb/DataFormatters/TypeSummary.h"

namespace lldb_private {
namespace formatters {

/// \class GNUstepNSTimeIntervalSummaryProvider
///
/// Provides summary formatting for NSTimeInterval values in GNUstep runtime.
/// NSTimeInterval is typedef double representing seconds since reference date.
/// 
/// This formatter specifically handles NSTimeInterval contexts to provide
/// readable time representations while not interfering with general NSNumber
/// or double formatting.
///
/// Formatting Strategy:
/// - Values > 978307200 (Jan 1, 2001) treated as absolute timestamps 
/// - Values < 978307200 treated as time durations
/// - Edge cases handled gracefully with fallback to numeric display
///
/// CRITICAL: This formatter is designed to NOT interfere with existing
/// NSNumber formatters, especially tagged pointer handling.
class GNUstepNSTimeIntervalSummaryProvider : public GNUstepSummaryProvider {
public:
  GNUstepNSTimeIntervalSummaryProvider() = default;

  bool FormatObject(ValueObject &valobj, Stream &stream,
                    const TypeSummaryOptions &options) override;

private:
  /// Extract double value from NSTimeInterval variable
  /// \param valobj The ValueObject representing the NSTimeInterval
  /// \param value Output parameter to store the extracted double value
  /// \return true if successful, false otherwise
  bool ExtractTimeIntervalValue(ValueObject &valobj, double &value);

  /// Format a time interval as an absolute timestamp (date/time)
  /// \param interval_value The time interval value (seconds since reference)
  /// \param stream Output stream for formatted result
  /// \return true if successfully formatted as timestamp
  bool FormatAsTimestamp(double interval_value, Stream &stream);

  /// Format a time interval as a duration (relative time span)
  /// \param interval_value The duration value in seconds
  /// \param stream Output stream for formatted result
  /// \return true if successfully formatted as duration
  bool FormatAsDuration(double interval_value, Stream &stream);

  /// Check if the value appears to be a valid timestamp
  /// \param interval_value The time interval value to check
  /// \return true if value is likely a timestamp, false if likely duration
  bool IsLikelyTimestamp(double interval_value);
};

/// Function called by LLDB to format NSTimeInterval values
bool GNUstepNSTimeIntervalFormatterFunction(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPTIMEINTERVALFORMATTERS_H