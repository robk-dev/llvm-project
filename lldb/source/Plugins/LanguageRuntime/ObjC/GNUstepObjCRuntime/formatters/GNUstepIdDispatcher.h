//===-- GNUstepIdDispatcher.h ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_IDDISPATCHER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_IDDISPATCHER_H

#include "lldb/lldb-forward.h"

namespace lldb_private {

class ValueObject;
class Stream;
class TypeSummaryOptions;

namespace formatters {

/// Minimal dispatcher function for id types - returns false to let LLDB handle formatting
bool GNUstepIdDispatcherFunction(ValueObject &valobj, Stream &stream, 
                                 const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_IDDISPATCHER_H