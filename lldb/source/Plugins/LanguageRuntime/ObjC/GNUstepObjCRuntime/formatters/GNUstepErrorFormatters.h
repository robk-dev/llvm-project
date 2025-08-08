//===-- GNUstepErrorFormatters.h -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_ERROR_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_ERROR_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// Summary provider for GNUstep NSError objects
class GNUstepNSErrorSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  /// Extract the error domain string
  std::string ExtractDomain(ValueObject &valobj);
  
  /// Extract the error code
  int64_t ExtractCode(ValueObject &valobj);
  
  /// Extract the localized description if available
  std::string ExtractDescription(ValueObject &valobj);
  
  /// Extract a string from an NSString object at given address
  std::string ExtractStringFromAddress(Process *process, lldb::addr_t string_addr);
};

/// Function wrapper for LLDB registration
bool GNUstepNSErrorFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_ERROR_H