//===-- GNUstepStringSummaryProvider.h -------------------------*- C++ -*-===//
//
// String summary provider for GNUstep NSString objects
// Shows actual string content instead of raw pointers
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEP_GNUSTEPSTRINGSUMMARYPROVIDER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEP_GNUSTEPSTRINGSUMMARYPROVIDER_H

#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/lldb-forward.h"

namespace lldb_private {
namespace formatters {

bool GNUstepStringSummaryProvider(ValueObject &valobj, Stream &stream,
                                 const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif