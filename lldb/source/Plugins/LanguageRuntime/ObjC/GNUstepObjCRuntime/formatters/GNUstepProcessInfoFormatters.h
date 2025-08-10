//===-- GNUstepProcessInfoFormatters.h ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_PROCESSINFO_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_PROCESSINFO_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// Summary provider for GNUstep NSProcessInfo objects
/// 
/// NSProcessInfo has no exposed instance variables, so we need to call
/// methods via CallRuntimeFunction to extract process information.
/// Expected format: NSProcessInfo(name='process_name', pid=1234, args=3)
class GNUstepNSProcessInfoSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, 
                    const TypeSummaryOptions &options) override;

private:
  /// Call -processName on the NSProcessInfo object
  std::string GetProcessName(ValueObject &valobj);
  
  /// Call -processIdentifier on the NSProcessInfo object  
  int32_t GetProcessIdentifier(ValueObject &valobj);
  
  /// Call -arguments on the NSProcessInfo object and get count
  size_t GetArgumentCount(ValueObject &valobj);
  
  /// Fallback method using system calls if CallRuntimeFunction fails
  std::string GetProcessNameFromSystem();
  int32_t GetProcessIdentifierFromSystem();
  size_t GetArgumentCountFromSystem(ValueObject &valobj);
};

/// Function wrapper for LLDB registration
bool GNUstepNSProcessInfoFormatterFunction(ValueObject &valobj, Stream &stream, 
                                           const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_PROCESSINFO_H