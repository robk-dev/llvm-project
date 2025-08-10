//===-- GNUstepBundleFormatters.h -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_BUNDLE_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_BUNDLE_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// Summary provider for GNUstep NSBundle objects
class GNUstepNSBundleSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  /// Extract bundle information from a GNUstep NSBundle object
  struct BundleInfo {
    std::string path;
    std::string version;
    bool loaded;
    bool valid;
    
    BundleInfo() : loaded(false), valid(false) {}
  };
  
  /// Extract bundle information from NSBundle object
  BundleInfo ExtractBundleInfo(ValueObject &valobj);
  
  /// Extract path string from _path instance variable
  std::string ExtractBundlePath(ValueObject &valobj);
  
  /// Extract version string from _frameworkVersion instance variable
  std::string ExtractBundleVersion(ValueObject &valobj);
  
  /// Extract loaded status from _codeLoaded instance variable
  bool ExtractBundleLoadedStatus(ValueObject &valobj);
};

/// Function wrapper for LLDB registration
bool GNUstepNSBundleFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_BUNDLE_H