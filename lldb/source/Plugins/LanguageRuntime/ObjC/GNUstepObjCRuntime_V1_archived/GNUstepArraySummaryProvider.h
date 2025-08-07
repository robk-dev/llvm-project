//===-- GNUstepArraySummaryProvider.h --------------------------*- C++ -*-===//
//
// Summary provider for GNUstep NSArray objects
// Shows count and type information
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEP_GNUSTEPARRAYSUMMARYPROVIDER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEP_GNUSTEPARRAYSUMMARYPROVIDER_H

#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/lldb-forward.h"

namespace lldb_private {
namespace formatters {

bool GNUstepArraySummaryProvider(ValueObject &valobj, Stream &stream,
                                const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif