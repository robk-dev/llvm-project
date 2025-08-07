//===-- GNUstepNSString.h --------------------------------------*- C++ -*-===//
//
// Complete NSString summary provider for GNUstep runtime
// Handles GSTinyString tagged pointers and NSConstantString objects
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEP_GNUSTEPNSSTRING_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEP_GNUSTEPNSSTRING_H

#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/lldb-forward.h"

namespace lldb_private {
namespace formatters {

bool GNUstepNSStringSummaryProvider(ValueObject &valobj, Stream &stream,
                                   const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif