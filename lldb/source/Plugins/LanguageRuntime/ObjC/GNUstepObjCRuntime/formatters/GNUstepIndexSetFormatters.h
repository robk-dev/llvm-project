//===-- GNUstepIndexSetFormatters.h -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPINDEXSETFORMATTERS_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPINDEXSETFORMATTERS_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// NSIndexSet summary provider for GNUstep runtime
/// 
/// Shows index sets in formats like:
/// - "3 indexes in [0-2]" for contiguous ranges
/// - "5 indexes" for scattered indexes
/// - "1 index: 42" for single indexes
/// - "0 indexes" for empty sets
bool GNUstepNSIndexSetFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

/// NSMutableIndexSet summary provider (uses same logic as NSIndexSet)
bool GNUstepNSMutableIndexSetFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPINDEXSETFORMATTERS_H