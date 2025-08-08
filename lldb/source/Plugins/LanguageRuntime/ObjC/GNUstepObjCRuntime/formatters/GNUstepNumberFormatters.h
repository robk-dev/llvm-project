//===-- GNUstepNumberFormatters.h ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_NUMBER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_NUMBER_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// Summary provider for GNUstep NSNumber objects
class GNUstepNSNumberSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, 
                    const TypeSummaryOptions &options) override;

private:
  /// Try to extract a double value from NSNumber
  bool TryExtractDouble(ValueObject &valobj, double &value);
  
  /// Try to extract an integer value from NSNumber
  bool TryExtractInteger(ValueObject &valobj, int64_t &value);
  
  /// Helper to determine the concrete NSNumber subclass
  std::string GetNumberClassName(ValueObject &valobj);
};

/// Function wrapper for LLDB registration
bool GNUstepNSNumberFormatterFunction(ValueObject &valobj, Stream &stream, 
                                       const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_NUMBER_H