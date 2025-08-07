//===-- GNUstepNumberSummaryProvider.h -------------------------*- C++ -*-===//
//
// Number summary provider for GNUstep NSNumber objects
// Shows actual numeric value instead of internal formatter fields
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEP_GNUSTEPNUMBERSUMMARYPROVIDER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEP_GNUSTEPNUMBERSUMMARYPROVIDER_H

#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/lldb-forward.h"

namespace lldb_private {
namespace formatters {

class GNUstepNumberSummaryProvider {
public:
  static bool FormatObject(ValueObject &valobj, Stream &stream,
                          const TypeSummaryOptions &options);

private:
  // Helper methods for different number types
  static bool TryExtractDouble(ValueObject &valobj, double &value);
  static bool TryExtractInteger(ValueObject &valobj, int64_t &value);
};

} // namespace formatters
} // namespace lldb_private

#endif
