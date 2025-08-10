//===-- GNUstepProxyFormatters.h --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_PROXY_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_PROXY_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// Summary provider for GNUstep NSProxy objects and subclasses
class GNUstepNSProxySummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  /// Determine proxy type and format accordingly
  bool FormatProxyByType(ValueObject &valobj, Stream &stream, const std::string &class_name);
  
  /// Format NSDistantObject proxy
  bool FormatDistantObject(ValueObject &valobj, Stream &stream);
  
  /// Format NSProtocolChecker proxy  
  bool FormatProtocolChecker(ValueObject &valobj, Stream &stream);
  
  /// Format generic proxy (including custom subclasses)
  bool FormatGenericProxy(ValueObject &valobj, Stream &stream, const std::string &class_name);
  
  /// Extract target object information if available
  std::string ExtractTargetInfo(ValueObject &valobj);
  
  /// Extract target object from common ivar names
  lldb::addr_t FindTargetObject(ValueObject &valobj);
  
  /// Extract connection information for distant objects
  std::string ExtractConnectionInfo(ValueObject &valobj);
  
  /// Extract protocol information for protocol checkers
  std::string ExtractProtocolInfo(ValueObject &valobj);
  
  /// Check if proxy appears to be in a valid state
  bool ValidateProxyState(ValueObject &valobj);
  
  /// Extract a string from an NSString object at given address
  std::string ExtractStringFromAddress(Process *process, lldb::addr_t string_addr);
  
  /// Get object description from target if accessible
  std::string GetTargetDescription(Process *process, lldb::addr_t target_addr);
};

/// Function wrapper for LLDB registration
bool GNUstepNSProxyFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_PROXY_H