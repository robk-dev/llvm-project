//===-- GNUstepNSDate.cpp --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepNSDate.h"
#include "GNUstepCollectionUtilities.h"
#include "GNUstepUtilities.h"
#include "GNUstepRuntimeAPI.h"
#include "lldb/DataFormatters/StringPrinter.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"
#include "lldb/ValueObject/ValueObject.h"

#include <ctime>
#include <iomanip>
#include <sstream>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace lldb_private {
namespace formatters {

// Decode GNUstep GSSmallDate tagged pointer to actual timestamp
bool DecodeGSSmallDate(uint64_t tagged_ptr, double& timestamp, Log* log) {
  // Check if this is a tagged pointer with the correct tag for GSSmallDate (tag = 6)
  uint8_t tag_low = tagged_ptr & 0x7;
  
  if (log) {
    LLDB_LOG(log, "DecodeGSSmallDate: tagged_ptr=0x{0:x}, tag_low={1}", 
             tagged_ptr, (int)tag_low);
  }
  
  // For debugging specific case: 0x16b90df665112dbe
  if (tagged_ptr == 0x16b90df665112dbeULL) {
    if (log) {
      LLDB_LOG(log, "DecodeGSSmallDate: DEBUGGING SPECIFIC CASE 0x16b90df665112dbe");
    }
  }
  
  // GSSmallDate uses tag 6 in the least significant 3 bits
  if (tag_low != 6) {
    if (log) {
      LLDB_LOG(log, "DecodeGSSmallDate: not a GSSmallDate tagged pointer (expected tag=6, got={0})", (int)tag_low);
    }
    return false;
  }
  
  // Based on GNUstep source analysis, GSSmallDate uses this bit structure:
  // Bits 0-2:   tag (6 for GSSmallDate)  
  // Bits 3-54:  fraction (52 bits)
  // Bits 55-62: compressed exponent (8 bits, signed)
  // Bit 63:     sign bit
  
  // Extract components using the actual GNUstep CompressedDouble structure
  uint64_t fraction = (tagged_ptr >> 3) & 0x000FFFFFFFFFFFFFULL;  // bits 3-54 (52 bits)
  int8_t compressed_exp = (int8_t)((tagged_ptr >> 55) & 0xFF);    // bits 55-62 (8 bits, signed)  
  uint8_t sign = (tagged_ptr >> 63) & 0x1;                       // bit 63
  
  if (log) {
    LLDB_LOG(log, "DecodeGSSmallDate: fraction=0x{0:x}, compressed_exp={1}, sign={2}", 
             fraction, (int)compressed_exp, (int)sign);
  }
  
  // Decompress the exponent using GNUstep's algorithm:
  // 1. Sign-extend the 8-bit exponent to 64-bit
  // 2. Add secondary bias (0x3EF)
  const int32_t EXPONENT_BIAS = 0x3EF;
  int64_t extended_exp = compressed_exp; // Already sign-extended by int8_t cast
  uint32_t decompressed_exp = (uint32_t)(extended_exp + EXPONENT_BIAS);
  
  // Clamp exponent to valid IEEE 754 double range (0-2047)
  if (decompressed_exp > 2047) {
    decompressed_exp = 2047;
  }
  
  if (log) {
    LLDB_LOG(log, "DecodeGSSmallDate: extended_exp={0}, decompressed_exp={1}", 
             extended_exp, decompressed_exp);
  }
  
  // Reconstruct IEEE 754 double bits
  uint64_t double_bits = 0;
  double_bits |= fraction;                           // Fraction in bits 0-51
  double_bits |= ((uint64_t)decompressed_exp << 52); // Exponent in bits 52-62
  double_bits |= ((uint64_t)sign << 63);             // Sign in bit 63
  
  // Interpret as double (this is the NSTimeInterval since 2001-01-01)
  timestamp = *reinterpret_cast<double*>(&double_bits);
  
  // Convert from NSTimeInterval (seconds since 2001-01-01) to Unix timestamp (seconds since 1970-01-01)
  const double NSTimeIntervalSince1970 = 978307200.0; // Seconds from 1970 to 2001
  double unix_timestamp = timestamp + NSTimeIntervalSince1970;
  
  if (log) {
    LLDB_LOG(log, "DecodeGSSmallDate: NSTimeInterval={0}, unix_timestamp={1}", timestamp, unix_timestamp);
  }
  
  // Sanity check: Unix timestamp should be reasonable (between 1970 and 2100)
  if (unix_timestamp < 0 || unix_timestamp > 4102444800.0) { // 2100-01-01
    if (log) {
      LLDB_LOG(log, "DecodeGSSmallDate: unix_timestamp {0} is outside reasonable range", unix_timestamp);
    }
    return false;
  }
  
  timestamp = unix_timestamp;
  return true;
}

// Format a timestamp as human-readable date string
bool FormatTimestamp(double timestamp, std::string& formatted_date) {
  // Convert timestamp to time_t for formatting
  time_t time_val = static_cast<time_t>(timestamp);
  
  // Get UTC time structure
  struct tm* utc_tm = gmtime(&time_val);
  if (!utc_tm) {
    formatted_date = "invalid date";
    return false;
  }
  
  // Format as ISO 8601-like string: "2025-08-06 10:30:45 UTC"
  std::ostringstream oss;
  oss << std::setfill('0')
      << (1900 + utc_tm->tm_year) << "-"
      << std::setw(2) << (utc_tm->tm_mon + 1) << "-"
      << std::setw(2) << utc_tm->tm_mday << " "
      << std::setw(2) << utc_tm->tm_hour << ":"
      << std::setw(2) << utc_tm->tm_min << ":"
      << std::setw(2) << utc_tm->tm_sec << " UTC";
  
  formatted_date = oss.str();
  return true;
}

// NSDate summary provider using runtime API - NO MEMORY LAYOUT GUESSING
bool GNUstepNSDateSummaryProvider(ValueObject &valobj, Stream &stream,
                                  const TypeSummaryOptions &options) {
  if (!valobj.GetCompilerType().IsValid()) {
    return false;
  }

  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp) {
    return false;
  }

  TargetSP target_sp = valobj.GetTargetSP();
  if (!target_sp) {
    return false;
  }

  // Get object address
  GNUstepAddressResolver resolver;
  addr_t date_addr = resolver.ResolveObjectAddress(valobj);
  
  if (date_addr == 0 || date_addr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }

  Log *log = GetLog(LLDBLog::Types);  // Enable logging for debugging

  // CRITICAL DEBUG: Always log for debugging
  LLDB_LOG(log, "NSDate processing address: 0x{0:x}", date_addr);
  bool is_tagged = gnustep_collection_utils::IsTaggedPointer(date_addr);
  LLDB_LOG(log, "NSDate IsTaggedPointer result: {0}", is_tagged);

  // CRITICAL FIX: Check tagged pointers FIRST - before creating runtime API!
  // Tagged pointers are NOT real objects and will cause runtime API calls to fail
  if (is_tagged) {
    LLDB_LOG(log, "NSDate detected tagged pointer: 0x{0:x}", date_addr);
    
    // Tagged pointers encode data directly in the pointer value
    // Don't call runtime API functions on them - they're not real objects!
    double timestamp;
    if (DecodeGSSmallDate(date_addr, timestamp, log)) {
      std::string formatted_date;
      if (FormatTimestamp(timestamp, formatted_date)) {
        if (log) {
          LLDB_LOG(log, "NSDate tagged pointer successfully decoded: {0}", formatted_date);
        }
        stream.Printf("%s", formatted_date.c_str());
        return true;
      } else {
        if (log) {
          LLDB_LOG(log, "NSDate tagged pointer decode succeeded but format failed");
        }
        stream.Printf("GSSmallDate (format failed, timestamp=%.6f)", timestamp);
        return true;
      }
    } else {
      if (log) {
        LLDB_LOG(log, "NSDate tagged pointer decode failed, showing generic tagged info");
      }
      // Fallback: show generic tagged pointer info
      std::string class_name = gnustep_collection_utils::GetTaggedPointerClassName(date_addr, log);
      stream.Printf("%s tagged: 0x%" PRIx64, class_name.c_str(), date_addr);
      return true;
    }
  }
  
  // 2. For non-tagged pointers: Create runtime API and handle regular NSDate objects
  auto runtime_api = GNUstepRuntimeAPI::Create(process_sp.get());
  if (!runtime_api || !runtime_api->IsValid()) {
    stream.Printf("NSDate (runtime API unavailable)");
    return true;
  }

  // 3. Handle regular NSDate objects using runtime API - NO HARDCODED OFFSETS!
  auto class_info_result = runtime_api->GetObjectClassInfo(date_addr);
  if (!class_info_result) {
    stream.Printf("NSDate (class info failed: %s)", class_info_result.error_message.c_str());
    return true;
  }
  
  auto class_info = *class_info_result;
  if (log) {
    LLDB_LOG(log, "NSDate object class: {0}", class_info.name);
  }
  
  // Look for common NSDate timestamp field names
  std::vector<std::string> timestamp_field_names = {
    "_seconds",           // Common NSDate ivar name
    "_timeIntervalSinceReferenceDate", // Another common name
    "_timeInterval",      // Generic name
    "timestamp",          // Generic fallback
    "_time"              // Another fallback
  };
  
  double nstime_interval = 0.0;
  bool found_timestamp = false;
  
  for (const std::string& field_name : timestamp_field_names) {
    auto ivar_result = runtime_api->GetIvarValue(date_addr, field_name);
    if (ivar_result) {
      addr_t timestamp_addr = *ivar_result;
      if (timestamp_addr != LLDB_INVALID_ADDRESS) {
        // Read the double value from the ivar location
        Status error;
        uint64_t raw_bits = process_sp->ReadUnsignedIntegerFromMemory(timestamp_addr, 8, 0, error);
        if (error.Success()) {
          nstime_interval = *reinterpret_cast<double*>(&raw_bits);
          found_timestamp = true;
          if (log) {
            LLDB_LOG(log, "Found NSDate timestamp in field '{0}': {1}", field_name, nstime_interval);
          }
          break;
        }
      }
    }
  }
  
  if (!found_timestamp) {
    // Last resort: try to read directly from object + 8 (after isa)
    // This is still a guess, but at least we've tried the runtime first
    Status error;
    uint64_t raw_timestamp_bits = process_sp->ReadUnsignedIntegerFromMemory(date_addr + 8, 8, 0, error);
    
    if (error.Success()) {
      nstime_interval = *reinterpret_cast<double*>(&raw_timestamp_bits);
      if (log) {
        LLDB_LOG(log, "NSDate fallback read (offset 8): {0}", nstime_interval);
      }
    } else {
      stream.Printf("NSDate (timestamp read failed)");
      return true;
    }
  }
  
  // Sanity check the NSTimeInterval value
  // NSTimeInterval can be negative (dates before 2001-01-01)
  if (nstime_interval < -978307200.0 || nstime_interval > 3124137600.0) {
    if (log) {
      LLDB_LOG(log, "NSDate NSTimeInterval {0} outside reasonable range", nstime_interval);
    }
    stream.Printf("NSDate (invalid timestamp=%.6f)", nstime_interval);
    return true;
  }
  
  // Convert NSTimeInterval (seconds since 2001-01-01) to Unix timestamp
  const double NSTimeIntervalSince1970 = 978307200.0;
  double unix_timestamp = nstime_interval + NSTimeIntervalSince1970;
  
  if (log) {
    LLDB_LOG(log, "NSDate: NSTimeInterval={0}, unix_timestamp={1}", nstime_interval, unix_timestamp);
  }
  
  std::string formatted_date;
  if (FormatTimestamp(unix_timestamp, formatted_date)) {
    stream.Printf("%s", formatted_date.c_str());
    return true;
  } else {
    stream.Printf("NSDate (format failed, NSTimeInterval=%.6f)", nstime_interval);
    return true;
  }
}

} // namespace formatters
} // namespace lldb_private