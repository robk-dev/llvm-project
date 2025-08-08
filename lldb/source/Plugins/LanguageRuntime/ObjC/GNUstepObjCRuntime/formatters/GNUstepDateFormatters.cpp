//===-- GNUstepDateFormatters.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepDateFormatters.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include <ctime>
#include <iomanip>
#include <sstream>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSDateSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream,
                                                const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid NSDate object");
    return false;
  }

  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  // Handle NSCalendarDate specially as it has additional formatting information
  if (class_name.find("NSCalendarDate") != std::string::npos) {
    std::string formatted_date = ExtractCalendarDate(valobj);
    if (!formatted_date.empty()) {
      WriteQuotedString(stream, formatted_date);
      return true;
    }
  }
  
  // Extract the time interval for regular NSDate objects
  double time_interval = ExtractTimeInterval(valobj);
  if (time_interval == 0.0) {
    WriteErrorSummary(stream, "could not extract time interval");
    return false;
  }
  
  std::string formatted_date = FormatTimestamp(time_interval);
  WriteQuotedString(stream, formatted_date);
  return true;
}

double GNUstepNSDateSummaryProvider::ExtractTimeInterval(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return 0.0;

  // Get the object's address
  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return 0.0;

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // NSDate stores its time interval as a double immediately after the isa pointer
  // Layout: [isa pointer][double _seconds_since_ref]
  lldb::addr_t time_interval_addr = object_addr + addr_size;
  
  Status error;
  double time_interval = 0.0;
  size_t bytes_read = process->ReadMemory(time_interval_addr, &time_interval, 
                                         sizeof(double), error);
  
  if (bytes_read != sizeof(double) || error.Fail()) {
    return 0.0;
  }

  return time_interval;
}

std::string GNUstepNSDateSummaryProvider::FormatTimestamp(double time_interval) {
  // NSDate uses reference date of 2001-01-01 00:00:00 UTC
  // Unix epoch is 1970-01-01 00:00:00 UTC
  // Difference is 978307200 seconds (31 years)
  const double NSDATE_EPOCH_OFFSET = 978307200.0;
  
  time_t unix_timestamp = static_cast<time_t>(time_interval + NSDATE_EPOCH_OFFSET);
  
  // Format as ISO 8601 datetime string
  struct tm *utc_tm = gmtime(&unix_timestamp);
  if (!utc_tm) {
    return "invalid date";
  }
  
  std::ostringstream oss;
  oss << std::put_time(utc_tm, "%Y-%m-%d %H:%M:%S UTC");
  return oss.str();
}

std::string GNUstepNSDateSummaryProvider::ExtractCalendarDate(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return "";

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return "";

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // NSCalendarDate has the same base time interval as NSDate
  lldb::addr_t time_interval_addr = object_addr + addr_size;
  
  Status error;
  double time_interval = 0.0;
  size_t bytes_read = process->ReadMemory(time_interval_addr, &time_interval, 
                                         sizeof(double), error);
  
  if (bytes_read != sizeof(double) || error.Fail()) {
    return "";
  }

  // For now, format NSCalendarDate the same as NSDate
  // In the future, we could extract timezone info and format string
  return FormatTimestamp(time_interval);
}

bool GNUstepNSDateFormatterFunction(ValueObject &valobj, Stream &stream,
                                   const TypeSummaryOptions &options) {
  GNUstepNSDateSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}