//===-- GNUstepAttributedStringFormatter.h ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GNUSTEPATTRIBUTEDSTRINGFORMATTER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GNUSTEPATTRIBUTEDSTRINGFORMATTER_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
class Process;
namespace formatters {

class GNUstepNSAttributedStringSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, 
                   const TypeSummaryOptions &options) override;

private:
  // Extract the underlying string content from NSAttributedString
  std::string ExtractStringContent(ValueObject &valobj);
  
  // Extract string content from a string pointer (using NSString logic)
  std::string ExtractStringFromPointer(lldb::addr_t string_ptr, Process *process);
  
  // Estimate the number of attributes
  size_t EstimateAttributeCount(ValueObject &valobj);
};

// Function entry point for LLDB's formatting system
bool GNUstepNSAttributedStringFormatterFunction(ValueObject &valobj, Stream &stream,
                                                const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GNUSTEPATTRIBUTEDSTRINGFORMATTER_H