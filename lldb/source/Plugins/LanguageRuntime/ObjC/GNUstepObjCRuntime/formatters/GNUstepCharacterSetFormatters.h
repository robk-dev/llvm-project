//===-- GNUstepCharacterSetFormatters.h --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPCHARACTERSETFORMATTERS_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPCHARACTERSETFORMATTERS_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// NSCharacterSet summary provider for GNUstep runtime
/// 
/// Shows character sets in user-friendly formats:
/// - "Letters" for [NSCharacterSet letterCharacterSet]
/// - "Digits" for [NSCharacterSet decimalDigitCharacterSet]  
/// - "Whitespace" for [NSCharacterSet whitespaceCharacterSet]
/// - "10 characters: aeiouAEIOU" for custom sets
/// - "Inverted Letters" for inverted standard sets
/// - "Empty character set" for empty sets
bool GNUstepNSCharacterSetFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

/// NSMutableCharacterSet summary provider (uses same logic as NSCharacterSet)
bool GNUstepNSMutableCharacterSetFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPCHARACTERSETFORMATTERS_H