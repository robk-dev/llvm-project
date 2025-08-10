//===-- GNUstepScannerFormatters.h ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_SCANNER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_SCANNER_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// Summary provider for GNUstep NSScanner objects
class GNUstepNSScannerSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  /// Extract scanner information from a GNUstep NSScanner object
  struct ScannerInfo {
    std::string string;        // The string being scanned
    uint64_t scanLocation;     // Current scan position
    std::string remaining;     // Remaining text to scan
    bool caseSensitive;        // Case sensitivity setting
    bool valid;                // Whether extraction was successful
    
    ScannerInfo() : scanLocation(0), caseSensitive(true), valid(false) {}
  };
  
  /// Extract scanner information from NSScanner object
  ScannerInfo ExtractScannerInfo(ValueObject &valobj);
  
  /// Extract scan string from _string instance variable
  std::string ExtractScanString(ValueObject &valobj);
  
  /// Extract scan location from _scanLocation instance variable
  uint64_t ExtractScanLocation(ValueObject &valobj);
  
  /// Extract case sensitivity from _caseSensitive instance variable
  bool ExtractCaseSensitive(ValueObject &valobj);
  
  /// Calculate remaining text based on string and scan location
  std::string CalculateRemainingText(const std::string &fullString, uint64_t location);
};

/// Function wrapper for LLDB registration
bool GNUstepNSScannerFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_SCANNER_H