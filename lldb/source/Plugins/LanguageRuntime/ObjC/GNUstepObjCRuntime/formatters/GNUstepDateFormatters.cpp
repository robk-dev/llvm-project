//===-- GNUstepDateFormatters.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepDateFormatters.h"
#include "GNUstepFormattersBase.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include <cstring>
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

  // Check if this is a tagged pointer NSDate (tag 6)
  if ((object_addr & 0x7) == 6) {
    // Tagged NSDate uses compressed double format based on libs-base/Source/NSDate.m
    // The format is:
    // bits 0-2: tag (6)
    // bits 3-54: 52-bit mantissa/fraction  
    // bits 55-62: 8-bit signed exponent (biased by -0x3EF from standard bias)
    // bit 63: sign bit
    
    // Extract components using bit manipulation (avoiding bitfields for portability)
    uint64_t compressed = object_addr;
    
    // Extract fraction (bits 3-54, which is 52 bits)
    uint64_t fraction = (compressed >> 3) & 0xFFFFFFFFFFFFFULL;
    
    // Extract exponent (bits 55-62, which is 8 bits)
    uint64_t exp_bits = (compressed >> 55) & 0xFF;
    
    // Extract sign (bit 63)
    uint64_t sign = (compressed >> 63) & 0x1;
    
    // Sign extend the 8-bit exponent to 64-bit
    int64_t signed_exp = static_cast<int64_t>(exp_bits);
    if (signed_exp & 0x80) {
      signed_exp |= 0xFFFFFFFFFFFFFF00ULL;
    }
    
    // Add back the bias (0x3EF) to get the standard IEEE 754 exponent
    int64_t biased_exponent = signed_exp + 0x3EF;
    
    // Clamp to valid IEEE 754 double exponent range [0, 0x7FF]
    if (biased_exponent < 0) {
      biased_exponent = 0;
    } else if (biased_exponent > 0x7FF) {
      biased_exponent = 0x7FF;
    }
    
    // Reconstruct the IEEE 754 double
    uint64_t ieee_bits = 0;
    ieee_bits |= fraction;                                    // bits 0-51: fraction
    ieee_bits |= (static_cast<uint64_t>(biased_exponent) << 52);  // bits 52-62: exponent
    ieee_bits |= (sign << 63);                               // bit 63: sign
    
    // Interpret as double
    double result;
    memcpy(&result, &ieee_bits, sizeof(double));
    
    return result;
  }

  // Non-tagged NSDate: read from memory
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

bool lldb_private::formatters::GNUstepNSDateFormatterFunction(ValueObject &valobj, Stream &stream,
                                   const TypeSummaryOptions &options) {
  GNUstepNSDateSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}

bool GNUstepNSTimeZoneSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream,
                                                    const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid NSTimeZone object");
    return false;
  }

  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  // Handle different TimeZone subclasses
  if (class_name.find("GSAbsTimeZone") != std::string::npos) {
    std::string formatted_tz = FormatAbsoluteTimeZone(valobj);
    if (!formatted_tz.empty()) {
      stream.Printf("%s", formatted_tz.c_str());
      return true;
    }
  }
  else if (class_name.find("GSTimeZone") != std::string::npos) {
    std::string formatted_tz = FormatComplexTimeZone(valobj);
    if (!formatted_tz.empty()) {
      stream.Printf("%s", formatted_tz.c_str());
      return true;
    }
  }
  else if (class_name.find("NSLocalTimeZone") != std::string::npos) {
    std::string formatted_tz = FormatLocalTimeZone(valobj);
    if (!formatted_tz.empty()) {
      stream.Printf("%s", formatted_tz.c_str());
      return true;
    }
  }
  
  // Fallback: try to extract basic info
  std::string timezone_name = ExtractTimeZoneName(valobj);
  if (timezone_name.empty()) {
    WriteErrorSummary(stream, "could not extract timezone name");
    return false;
  }
  
  bool has_dst = false;
  int32_t offset = ExtractTimeZoneOffset(valobj, has_dst);
  std::string offset_str = FormatTimeZoneOffset(offset);
  
  stream.Printf("%s(name=\"%s\", offset=%s)", 
                class_name.c_str(), timezone_name.c_str(), offset_str.c_str());
  return true;
}

std::string GNUstepNSTimeZoneSummaryProvider::ExtractTimeZoneName(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return "";

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return "";

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  // For GSAbsTimeZone, the name field is at offset addr_size from the object start
  // For GSTimeZone, the timeZoneName field is at offset addr_size from the object start  
  // For NSLocalTimeZone, we need to get the name from the actual timezone it proxies
  
  lldb::addr_t name_field_addr = object_addr + addr_size;
  
  Status error;
  lldb::addr_t name_obj_addr = 0;
  size_t bytes_read = process->ReadMemory(name_field_addr, &name_obj_addr, 
                                         addr_size, error);
  
  if (bytes_read != addr_size || error.Fail() || name_obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Try to extract as NSString using the existing string extraction method
  return TryExtractStringContent(process, name_obj_addr);
}

int32_t GNUstepNSTimeZoneSummaryProvider::ExtractTimeZoneOffset(ValueObject &valobj, bool &has_dst) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return 0;

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return 0;

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  has_dst = false;
  
  if (class_name.find("GSAbsTimeZone") != std::string::npos) {
    // GSAbsTimeZone layout: [isa][name][offset(int)]
    lldb::addr_t offset_addr = object_addr + addr_size + addr_size; // isa + name pointer
    
    Status error;
    int32_t offset = 0;
    size_t bytes_read = process->ReadMemory(offset_addr, &offset, sizeof(int32_t), error);
    
    if (bytes_read == sizeof(int32_t) && error.Success()) {
      return offset;
    }
  }
  else if (class_name.find("GSTimeZone") != std::string::npos) {
    // GSTimeZone has complex zoneinfo data - we need to compute current offset
    // This would require parsing the timezone data, which is complex
    // For now, return a placeholder indicating DST-capable timezone
    has_dst = true;
    return 0; // Would need to compute based on current date
  }
  
  return 0;
}

std::string GNUstepNSTimeZoneSummaryProvider::FormatAbsoluteTimeZone(ValueObject &valobj) {
  std::string name = ExtractTimeZoneName(valobj);
  if (name.empty()) {
    name = "Unknown";
  }
  
  bool has_dst = false;
  int32_t offset = ExtractTimeZoneOffset(valobj, has_dst);
  std::string offset_str = FormatTimeZoneOffset(offset);
  
  return "GSAbsTimeZone(name=\"" + name + "\", offset=" + offset_str + ")";
}

std::string GNUstepNSTimeZoneSummaryProvider::FormatComplexTimeZone(ValueObject &valobj) {
  std::string name = ExtractTimeZoneName(valobj);
  if (name.empty()) {
    name = "Unknown";
  }
  
  bool has_dst = false;
  int32_t offset = ExtractTimeZoneOffset(valobj, has_dst);
  std::string offset_str = FormatTimeZoneOffset(offset);
  
  std::string result = "GSTimeZone(name=\"" + name + "\", offset=" + offset_str;
  if (has_dst) {
    result += ", dst=true";
  }
  result += ")";
  
  return result;
}

std::string GNUstepNSTimeZoneSummaryProvider::FormatLocalTimeZone(ValueObject &valobj) {
  // NSLocalTimeZone is a proxy - it forwards to the actual default timezone
  // We can try to extract the current timezone name and offset
  std::string name = ExtractTimeZoneName(valobj);
  if (name.empty()) {
    name = "Local";
  }
  
  bool has_dst = false;
  int32_t offset = ExtractTimeZoneOffset(valobj, has_dst);
  std::string offset_str = FormatTimeZoneOffset(offset);
  
  return "NSLocalTimeZone(name=\"" + name + "\", current_offset=" + offset_str + ")";
}

std::string GNUstepNSTimeZoneSummaryProvider::FormatTimeZoneOffset(int32_t offset_seconds) {
  if (offset_seconds == 0) {
    return "0";
  }
  
  // Convert seconds to hours and minutes
  int hours = offset_seconds / 3600;
  int minutes = (abs(offset_seconds) % 3600) / 60;
  
  char buffer[16];
  if (minutes == 0) {
    snprintf(buffer, sizeof(buffer), "%+d", hours);
  } else {
    snprintf(buffer, sizeof(buffer), "%+03d%02d", hours, minutes);
  }
  
  return std::string(buffer);
}

std::string GNUstepNSTimeZoneSummaryProvider::TryExtractStringContent(Process *process, lldb::addr_t obj_addr) {
  if (!process || obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // First check if this is a tagged pointer
  GNUstepObjCRuntimeIntrospector introspector(process);
  if (introspector.IsTaggedPointer(obj_addr)) {
    uint64_t tag = obj_addr & 0x7;
    if (tag == 4) {
      // This is a tagged string - decode it properly
      std::string decoded = introspector.DecodeTaggedString(obj_addr);
      if (!decoded.empty()) {
        return decoded;
      }
      return "<tagged_string>";
    }
    return "";
  }
  
  // For regular NSString objects, extract the string content
  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // Read the string object layout: [isa][flags/count][capacity][data_ptr or inline_data]
  lldb::addr_t len_addr = obj_addr + addr_size;
  
  Status error;
  uint32_t string_length = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, len_addr, &string_length, sizeof(string_length))) {
    return "";
  }
  
  // Sanity check the length
  if (string_length == 0 || string_length > 65536) {
    return "";
  }
  
  // Read the data pointer
  lldb::addr_t data_ptr_addr = obj_addr + addr_size + sizeof(uint32_t) + sizeof(uint32_t);
  lldb::addr_t string_data_addr = 0;
  if (!GNUstepRuntimeHelper::ReadMemory(process, data_ptr_addr, &string_data_addr, addr_size)) {
    return "";
  }
  
  if (string_data_addr == 0 || string_data_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Read the actual string data
  std::vector<char> buffer(string_length + 1, 0);
  size_t bytes_read = process->ReadMemory(string_data_addr, buffer.data(), string_length, error);
  if (bytes_read != string_length || error.Fail()) {
    return "";
  }
  
  return std::string(buffer.data(), string_length);
}

bool lldb_private::formatters::GNUstepNSTimeZoneFormatterFunction(ValueObject &valobj, Stream &stream,
                                     const TypeSummaryOptions &options) {
  GNUstepNSTimeZoneSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}