//===-- GNUstepCustomClass.h ------------------------------------*- C++ -*-===//
//
// Generic custom class summary provider for user-defined Objective-C classes
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJ_RUNTIME_GNUSTEPCUSTOMCLASS_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJ_RUNTIME_GNUSTEPCUSTOMCLASS_H

#include "lldb/lldb-private.h"

namespace lldb_private {
namespace formatters {

bool GNUstepCustomClassSummaryProvider(ValueObject &valobj, 
                                      Stream &stream, 
                                      const TypeSummaryOptions &options);

bool GNUstepCustomClassSyntheticProvider(ValueObject &valobj,
                                        Stream &stream,
                                        const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJ_RUNTIME_GNUSTEPCUSTOMCLASS_H