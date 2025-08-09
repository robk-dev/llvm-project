//===-- GNUstepDecimalNumberFormatters.cpp -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepDecimalNumberFormatters.h"
#include "GNUstepFormattersBase.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <cstring>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace {

/// Helper class to format NSDecimalNumber objects
class GNUstepNSDecimalNumberSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  struct NSDecimal {
    int8_t  exponent;      // power of 10 exponent (-128 to 127)
    uint8_t isNegative;    // 0=positive, 1=negative  
    uint8_t validNumber;   // 0=invalid (NaN), 1=valid
    uint8_t length;        // digits in mantissa (0-38)
    uint8_t cMantissa[38]; // mantissa as decimal digits (not 16-bit words!)
  };
  
  /// Extract the NSDecimal structure from the NSDecimalNumber
  bool ExtractNSDecimal(ValueObject &valobj, NSDecimal &decimal);
  
  /// Convert NSDecimal to string representation
  std::string FormatNSDecimal(const NSDecimal &decimal);
  
  /// Check for special values (NaN, infinity)
  bool IsSpecialValue(const NSDecimal &decimal, std::string &specialStr);
  
  /// Convert mantissa to string
  std::string MantissaToString(const uint8_t cMantissa[38], uint8_t length);
  
  /// Apply exponent and format final string
  std::string ApplyExponentAndFormat(const std::string &mantissaStr, int8_t exponent, bool isNegative);
};

bool GNUstepNSDecimalNumberSummaryProvider::FormatObject(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  // Validate GNUstep object
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid NSDecimalNumber");
    return false;
  }

  addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    stream.Printf("(null)");
    return true;
  }

  // Extract the NSDecimal structure
  NSDecimal decimal;
  if (!ExtractNSDecimal(valobj, decimal)) {
    WriteErrorSummary(stream, "could not extract decimal value");
    return false;
  }

  // Check for special values first
  std::string specialValue;
  if (IsSpecialValue(decimal, specialValue)) {
    stream.Printf("%s", specialValue.c_str());
    return true;
  }

  // Format the decimal number
  std::string formattedNumber = FormatNSDecimal(decimal);
  if (formattedNumber.empty()) {
    WriteErrorSummary(stream, "could not format decimal");
    return false;
  }

  stream.Printf("%s", formattedNumber.c_str());
  return true;
}

bool GNUstepNSDecimalNumberSummaryProvider::ExtractNSDecimal(ValueObject &valobj, NSDecimal &decimal) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return false;
  }

  addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  Status error;

  // GNUstep NSDecimalNumber structure:
  // The NSDecimal structure is embedded directly in the object
  // after the isa pointer at offset addr_size (8 bytes on x86_64)
  addr_t decimal_addr = obj_addr + addr_size;

  // Read the NSDecimal structure - confirmed layout from memory inspection:
  // offset +0: signed char exponent
  // offset +1: BOOL isNegative  
  // offset +2: BOOL validNumber
  // offset +3: unsigned char length
  // offset +4: unsigned char cMantissa[38]
  
  uint8_t buffer[sizeof(NSDecimal)];
  size_t bytes_read = process->ReadMemory(decimal_addr, buffer, sizeof(NSDecimal), error);
  if (error.Fail() || bytes_read != sizeof(NSDecimal)) {
    return false;
  }

  // Copy the buffer to our decimal structure
  memcpy(&decimal, buffer, sizeof(NSDecimal));
  
  return true;
}

std::string GNUstepNSDecimalNumberSummaryProvider::FormatNSDecimal(const NSDecimal &decimal) {
  // Check for zero
  if (decimal.length == 0) {
    return "0";
  }

  // Convert mantissa to string
  std::string mantissaStr = MantissaToString(decimal.cMantissa, decimal.length);
  if (mantissaStr.empty()) {
    return "";
  }

  // Apply exponent and sign
  return ApplyExponentAndFormat(mantissaStr, decimal.exponent, decimal.isNegative != 0);
}

bool GNUstepNSDecimalNumberSummaryProvider::IsSpecialValue(const NSDecimal &decimal, std::string &specialStr) {
  // Check for invalid number (NaN) using validNumber field
  if (decimal.validNumber == 0) {
    specialStr = "NaN";
    return true;
  }
  
  return false;
}

std::string GNUstepNSDecimalNumberSummaryProvider::MantissaToString(
    const uint8_t cMantissa[38], uint8_t length) {
  if (length == 0 || length > 38) {
    return "";
  }

  // Convert mantissa bytes to a decimal string
  // Each byte in cMantissa represents a decimal digit (0-9)
  std::string result;
  result.reserve(length);
  
  // Convert each decimal digit
  for (uint8_t i = 0; i < length; i++) {
    if (cMantissa[i] > 9) {
      // Invalid digit - corrupted data
      return "";
    }
    result += ('0' + cMantissa[i]);
  }
  
  return result;
}

std::string GNUstepNSDecimalNumberSummaryProvider::ApplyExponentAndFormat(
    const std::string &mantissaStr, int8_t exponent, bool isNegative) {
  
  std::ostringstream result;
  
  if (isNegative) {
    result << "-";
  }
  
  // Handle different exponent cases
  if (exponent == 0) {
    // No decimal point needed
    result << mantissaStr;
  } else if (exponent > 0) {
    // Positive exponent - multiply by powers of 10
    result << mantissaStr;
    for (int i = 0; i < exponent; i++) {
      result << "0";
    }
  } else {
    // Negative exponent - decimal point
    int absExponent = -exponent;
    if (absExponent >= (int)mantissaStr.length()) {
      // Need leading zeros
      result << "0.";
      for (int i = 0; i < absExponent - (int)mantissaStr.length(); i++) {
        result << "0";
      }
      result << mantissaStr;
    } else {
      // Decimal point within the mantissa
      int pointPos = mantissaStr.length() - absExponent;
      result << mantissaStr.substr(0, pointPos);
      result << ".";
      result << mantissaStr.substr(pointPos);
    }
  }
  
  return result.str();
}

} // anonymous namespace

// Public API implementation
namespace lldb_private {
namespace formatters {

bool GNUstepNSDecimalNumberFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  GNUstepNSDecimalNumberSummaryProvider provider;
  return provider.FormatObject(valobj, stream, options);
}

} // namespace formatters
} // namespace lldb_private