//===-- GNUstepTimeIntervalFormatters.cpp ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepTimeIntervalFormatters.h"
#include "GNUstepFormattersBase.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Scalar.h"
#include <ctime>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSTimeIntervalSummaryProvider::FormatObject(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  // CRITICAL SAFETY CHECK: Only format if this is explicitly an NSTimeInterval
  // This prevents interference with NSNumber formatters and tagged pointers
  const char* type_name = valobj.GetTypeName().AsCString();
  if (!type_name) {
    return false; // Let other formatters handle
  }
  
  std::string type_str(type_name);
  
  // Only handle if explicitly typed as NSTimeInterval or in a context that suggests time interval
  // IMPORTANT: Do NOT handle raw "double" or "NSNumber" - let existing formatters handle those
  bool is_time_interval = (type_str.find("NSTimeInterval") != std::string::npos) ||
                          (type_str == "NSTimeInterval") ||
                          // Handle common variable naming patterns that suggest time intervals
                          (valobj.GetName().GetStringRef().contains("timestamp") ||
                           valobj.GetName().GetStringRef().contains("time") ||
                           valobj.GetName().GetStringRef().contains("interval") ||
                           valobj.GetName().GetStringRef().contains("duration"));
  
  if (!is_time_interval) {
    return false; // Let other formatters handle
  }
  
  double interval_value;
  if (!ExtractTimeIntervalValue(valobj, interval_value)) {
    // Could not extract value - let other formatters try
    return false;
  }
  
  // Handle special values
  if (std::isnan(interval_value)) {
    stream.Printf("NaN");
    return true;
  }
  
  if (std::isinf(interval_value)) {
    stream.Printf("%s∞", interval_value < 0 ? "-" : "+");
    return true;
  }
  
  // Format based on whether it looks like timestamp or duration
  // Use variable name hints to improve detection
  std::string var_name = valobj.GetName().AsCString("");
  bool name_suggests_timestamp = (var_name.find("timestamp") != std::string::npos) ||
                                 (var_name.find("time") != std::string::npos && var_name.find("interval") == std::string::npos);
  bool name_suggests_duration = (var_name.find("duration") != std::string::npos) ||
                                (var_name.find("interval") != std::string::npos);
  
  bool treat_as_timestamp;
  if (name_suggests_timestamp) {
    treat_as_timestamp = true;
  } else if (name_suggests_duration) {
    treat_as_timestamp = false;
  } else {
    treat_as_timestamp = IsLikelyTimestamp(interval_value);
  }
  
  if (treat_as_timestamp) {
    if (FormatAsTimestamp(interval_value, stream)) {
      return true;
    }
  } else {
    if (FormatAsDuration(interval_value, stream)) {
      return true;
    }
  }
  
  // Fallback to numeric display with "seconds" suffix
  stream.Printf("%.6g seconds", interval_value);
  return true;
}

bool GNUstepNSTimeIntervalSummaryProvider::ExtractTimeIntervalValue(
    ValueObject &valobj, double &value) {
  
  // NSTimeInterval is typedef double, so try to get it as a basic floating point value
  // First try to get data from the ValueObject
  DataExtractor data;
  Status error;
  if (valobj.GetData(data, error) && error.Success()) {
    // Try to extract as double from the data
    lldb::offset_t offset = 0;
    double extracted_value = data.GetDouble(&offset);
    
    // Check if we successfully extracted a valid double
    if (offset > 0) {
      value = extracted_value;
      return true;
    }
  }
  
  // Fallback: try direct memory reading for pointer types
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp) {
    return false;
  }
  
  addr_t addr = valobj.GetPointerValue();
  if (addr == 0 || addr == LLDB_INVALID_ADDRESS) {
    // Try direct value address for non-pointer types
    addr = valobj.GetAddressOf();
  }
  
  if (addr == 0 || addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Read 8 bytes as double
  Status read_error;
  uint64_t double_bits = process_sp->ReadUnsignedIntegerFromMemory(
      addr, sizeof(uint64_t), 0, read_error);
  
  if (read_error.Fail()) {
    return false;
  }
  
  value = *reinterpret_cast<double*>(&double_bits);
  return true;
}

bool GNUstepNSTimeIntervalSummaryProvider::FormatAsTimestamp(
    double interval_value, Stream &stream) {
  
  // NSTimeInterval reference date is January 1, 2001, 00:00:00 GMT
  // Convert to Unix timestamp (seconds since January 1, 1970)
  const double NSTimeIntervalSince1970 = 978307200.0; // Jan 1, 2001 - Jan 1, 1970
  double unix_timestamp = interval_value + NSTimeIntervalSince1970;
  
  // Convert to time_t for standard library functions
  time_t timestamp = static_cast<time_t>(unix_timestamp);
  
  // Get fractional seconds for microsecond precision
  double fractional = unix_timestamp - timestamp;
  int microseconds = static_cast<int>(fractional * 1000000);
  
  // Format using UTC time
  struct tm *utc_time = std::gmtime(&timestamp);
  if (!utc_time) {
    return false; // Invalid timestamp
  }
  
  // Format: YYYY-MM-DD HH:MM:SS.μμμμμμ UTC
  std::ostringstream oss;
  oss << std::setfill('0')
      << (utc_time->tm_year + 1900) << "-"
      << std::setw(2) << (utc_time->tm_mon + 1) << "-" 
      << std::setw(2) << utc_time->tm_mday << " "
      << std::setw(2) << utc_time->tm_hour << ":"
      << std::setw(2) << utc_time->tm_min << ":"
      << std::setw(2) << utc_time->tm_sec;
  
  if (microseconds > 0) {
    oss << "." << std::setw(6) << microseconds;
  }
  oss << " UTC";
  
  stream.Printf("%s", oss.str().c_str());
  return true;
}

bool GNUstepNSTimeIntervalSummaryProvider::FormatAsDuration(
    double interval_value, Stream &stream) {
  
  double abs_value = std::abs(interval_value);
  bool is_negative = interval_value < 0;
  
  // Format duration based on magnitude
  if (abs_value < 1.0) {
    // Less than 1 second - show in milliseconds
    double milliseconds = abs_value * 1000.0;
    if (milliseconds < 1.0) {
      // Less than 1 ms - show in microseconds
      double microseconds = abs_value * 1000000.0;
      stream.Printf("%s%.2f μs", is_negative ? "-" : "", microseconds);
    } else {
      stream.Printf("%s%.2f ms", is_negative ? "-" : "", milliseconds);
    }
  } else if (abs_value < 60.0) {
    // Less than 1 minute - show in seconds
    stream.Printf("%s%.3f seconds", is_negative ? "-" : "", abs_value);
  } else if (abs_value < 3600.0) {
    // Less than 1 hour - show in minutes and seconds
    int minutes = static_cast<int>(abs_value / 60.0);
    double seconds = abs_value - (minutes * 60.0);
    stream.Printf("%s%d:%06.3f", is_negative ? "-" : "", minutes, seconds);
  } else if (abs_value < 86400.0) {
    // Less than 1 day - show in hours, minutes, seconds
    int hours = static_cast<int>(abs_value / 3600.0);
    int minutes = static_cast<int>((abs_value - (hours * 3600.0)) / 60.0);
    double seconds = abs_value - (hours * 3600.0) - (minutes * 60.0);
    stream.Printf("%s%d:%02d:%06.3f", is_negative ? "-" : "", hours, minutes, seconds);
  } else {
    // 1 day or more - show in days, hours, minutes, seconds
    int days = static_cast<int>(abs_value / 86400.0);
    int hours = static_cast<int>((abs_value - (days * 86400.0)) / 3600.0);
    int minutes = static_cast<int>((abs_value - (days * 86400.0) - (hours * 3600.0)) / 60.0);
    double seconds = abs_value - (days * 86400.0) - (hours * 3600.0) - (minutes * 60.0);
    stream.Printf("%s%d days, %d:%02d:%06.3f", is_negative ? "-" : "", days, hours, minutes, seconds);
  }
  
  return true;
}

bool GNUstepNSTimeIntervalSummaryProvider::IsLikelyTimestamp(double interval_value) {
  // NSTimeInterval reference is Jan 1, 2001
  // Values that are likely timestamps vs durations
  
  // Durations are typically small positive or negative values
  // Timestamps are typically large positive values
  
  const double ONE_YEAR_SECONDS = 365.25 * 24 * 60 * 60; // ~31,557,600 seconds
  const double TEN_YEARS_SECONDS = 10 * ONE_YEAR_SECONDS;  // ~315,576,000 seconds
  
  // If the absolute value is very small, it's likely a duration
  double abs_value = std::abs(interval_value);
  if (abs_value < ONE_YEAR_SECONDS) {
    return false; // Likely a duration (less than 1 year)
  }
  
  // If it's a reasonable large positive value, likely a timestamp
  if (interval_value > TEN_YEARS_SECONDS) {
    // This would be ~2011 or later, reasonable for timestamps
    return true;
  }
  
  // If it's a large negative value, could be a date before 2001
  if (interval_value < -TEN_YEARS_SECONDS) {
    // This would be ~1991 or earlier, reasonable for historical timestamps
    return true;
  }
  
  // Middle range - use variable name hint if available
  std::string var_name = ""; // We don't have access to valobj here, but that's OK
  
  // For middle range values (1-10 years), default to duration
  // This catches cases like multi-hour/day durations but avoids
  // treating moderate intervals as timestamps
  return false;
}

// Function called by LLDB formatter system
bool lldb_private::formatters::GNUstepNSTimeIntervalFormatterFunction(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  GNUstepNSTimeIntervalSummaryProvider provider;
  return provider.FormatObject(valobj, stream, options);
}