//===-- GNUstepNSDate.h ----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNSDATE_H
#define LLDB_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNSDATE_H

#include "lldb/lldb-forward.h"
#include "lldb/Utility/Log.h"
#include <string>

namespace lldb_private {
class ValueObject;
class Stream;

namespace formatters {

/// Decode GNUstep GSSmallDate tagged pointer to actual timestamp
/// GSSmallDate uses tagged pointer format similar to GSTinyString
/// - Tag bits identify it as a date (typically 6 or 7, but may vary)
/// - Timestamp data encoded in upper bits
/// - Converts to seconds since Unix epoch for standard date formatting
/// @param tagged_ptr The tagged pointer value
/// @param timestamp Output timestamp (seconds since Unix epoch)
/// @param log Optional log for debugging output
/// @return true if successfully decoded GSSmallDate, false otherwise
bool DecodeGSSmallDate(uint64_t tagged_ptr, double& timestamp, lldb_private::Log* log = nullptr);

/// Format a timestamp as a human-readable date string
/// Uses standard C++ strftime formatting
/// @param timestamp Seconds since Unix epoch
/// @param formatted_date Output string for formatted date
/// @return true if formatting succeeded
bool FormatTimestamp(double timestamp, std::string& formatted_date);

/// Summary provider for NSDate objects using runtime API (NO MEMORY LAYOUT GUESSING)
/// CRITICAL: Uses GNUstepRuntimeAPI to discover timestamp field dynamically
/// Handles both tagged GSSmallDate pointers and regular NSDate objects
/// For tagged dates: uses runtime introspection first, falls back to decoder
/// For regular NSDate: uses runtime to find timestamp ivar, avoids hardcoded offsets
/// Displays as: "2025-08-06 10:30:45 UTC" or similar readable format
/// @param valobj ValueObject representing the NSDate
/// @param stream Output stream for summary text
/// @param options Formatting options (unused)
/// @return true if summary was generated successfully
bool GNUstepNSDateSummaryProvider(lldb_private::ValueObject &valobj,
                                  lldb_private::Stream &stream,
                                  const lldb_private::TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNSDATE_H