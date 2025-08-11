//===-- GNUstepStringFormatters.h ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_STRING_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_STRING_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// Summary provider for GNUstep NSString objects
class GNUstepNSStringSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

public:
  /// Extract string content from a GNUstep NSString object
  std::string ExtractStringContent(ValueObject &valobj);
  
  /// Handle inline strings (GSCInlineString/GSUInlineString)
  /// These store string data immediately after the object in memory
  std::string ExtractInlineString(ValueObject &valobj);

private:
  /// Handle constant strings (NSConstantString)
  std::string ExtractConstantString(ValueObject &valobj);
  
  /// Handle mutable strings (NSMutableString)
  std::string ExtractMutableString(ValueObject &valobj);
};

/// Function wrapper for LLDB registration
bool GNUstepNSStringFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

/// Smart formatter for id types that delegates based on runtime type  
bool GNUstepIdFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

/// Synthetic children provider for GSCInlineString objects
/// Shows the actual string content instead of raw memory fields
class GSCInlineStringSyntheticProvider : public GNUstepSyntheticProvider {
public:
  GSCInlineStringSyntheticProvider(lldb::ValueObjectSP valobj_sp);
  ~GSCInlineStringSyntheticProvider() override = default;
  
  llvm::Expected<uint32_t> CalculateNumChildren() override;
  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;
  
protected:
  bool UpdateImpl() override;
  
private:
  struct InlineStringInfo {
    uint32_t count;       // String length
    uint32_t flags;       // Encoding flags
    bool is_wide;         // Wide character encoding
    std::string content;  // Actual string content
  };
  
  InlineStringInfo m_string_info;
  lldb::addr_t m_obj_addr;
};

/// Creator function for GSCInlineString synthetic provider
SyntheticChildrenFrontEnd *
GSCInlineStringSyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                        lldb::ValueObjectSP valobj_sp);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_STRING_H
