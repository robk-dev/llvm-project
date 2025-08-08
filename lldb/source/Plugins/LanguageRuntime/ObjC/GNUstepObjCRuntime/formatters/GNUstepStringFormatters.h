//===-- GNUstepStringFormatters.h ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_STRING_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_STRING_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// Summary provider for GNUstep NSString objects
class GNUstepNSStringSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  /// Extract string content from a GNUstep NSString object
  std::string ExtractStringContent(ValueObject &valobj);
  
  /// Handle constant strings (NSConstantString)
  std::string ExtractConstantString(ValueObject &valobj);
  
  /// Handle mutable strings (NSMutableString)
  std::string ExtractMutableString(ValueObject &valobj);
};

/// Function wrapper for LLDB registration
bool GNUstepNSStringFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

/// Smart formatter for id types that delegates based on runtime type  
bool GNUstepIdFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_STRING_H
