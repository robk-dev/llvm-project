//===-- GNUstepIndexPathFormatter.h ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GNUSTEPINDEXPATHFORMATTER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GNUSTEPINDEXPATHFORMATTER_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

class GNUstepNSIndexPathSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, 
                   const TypeSummaryOptions &options) override;
};

// Function entry point for LLDB's formatting system
bool GNUstepNSIndexPathFormatterFunction(ValueObject &valobj, Stream &stream,
                                         const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GNUSTEPINDEXPATHFORMATTER_H