//===-- GNUstepDecimalNumberFormatters.h -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPDECIMALNUMBERFORMATTERS_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPDECIMALNUMBERFORMATTERS_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// NSDecimalNumber summary provider for GNUstep runtime
/// 
/// Provides precise decimal display with locale support:
/// - Full precision for large numbers beyond double range
/// - Proper handling of scientific notation
/// - Financial precision (no floating point errors)
/// - Special values: NaN, infinity
/// - Locale-aware formatting where applicable
bool GNUstepNSDecimalNumberFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPDECIMALNUMBERFORMATTERS_H