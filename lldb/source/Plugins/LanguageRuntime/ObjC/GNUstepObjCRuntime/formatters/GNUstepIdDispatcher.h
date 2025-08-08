//===-- GNUstepIdDispatcher.h ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_IDDISPATCHER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_IDDISPATCHER_H

#include "GNUstepFormattersBase.h"
#include "lldb/DataFormatters/TypeSynthetic.h"

namespace lldb_private {
namespace formatters {

/// Tagged pointer types used by GNUstep runtime
enum class TaggedPointerType {
  Unknown,
  NSSmallInt,
  NSSmallFloat,
  NSSmallExtendedDouble,
  NSSmallRepeatingDouble
};

/// Dispatcher function for id types that delegates to appropriate formatter based on runtime type
/// This is the main entry point for formatting any Objective-C object with 'id' type
bool GNUstepIdDispatcherFunction(ValueObject &valobj, Stream &stream, 
                                 const TypeSummaryOptions &options);

/// Synthetic children dispatcher for id types
SyntheticChildrenFrontEnd *GNUstepIdSyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                                             lldb::ValueObjectSP valobj_sp);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_IDDISPATCHER_H