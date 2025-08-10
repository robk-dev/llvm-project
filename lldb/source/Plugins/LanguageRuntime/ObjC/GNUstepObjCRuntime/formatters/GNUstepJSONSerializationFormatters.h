//===-- GNUstepJSONSerializationFormatters.h ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_JSONSERIALIZATION_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_JSONSERIALIZATION_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// Summary provider for NSJSONSerialization objects
/// NSJSONSerialization is typically a static utility class, but may also
/// handle instances containing JSON data or options
class GNUstepNSJSONSerializationSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  /// Determine if this is a class object or instance
  bool IsClassObject(ValueObject &valobj);
  
  /// Format a class object (static NSJSONSerialization)
  void FormatClassObject(ValueObject &valobj, Stream &stream);
  
  /// Format an instance containing JSON data
  void FormatJSONInstance(ValueObject &valobj, Stream &stream);
  
  /// Extract JSON data from NSData object if present
  std::string ExtractJSONData(ValueObject &valobj);
  
  /// Extract and format JSON reading options
  std::string FormatReadingOptions(uint64_t options);
  
  /// Extract and format JSON writing options
  std::string FormatWritingOptions(uint64_t options);
  
  /// Extract options value from object
  uint64_t ExtractOptionsValue(ValueObject &valobj, const char* ivar_name);
  
  /// Detect JSON content type from data preview
  std::string DetectJSONType(const std::string& json_data);
  
  /// Create a preview of JSON data with truncation
  std::string CreateJSONPreview(const std::string& json_data, size_t max_length = 100);
  
  /// Validate if string contains valid JSON
  bool IsValidJSONPreview(const std::string& data);
  
  /// Extract string data from NSData object
  std::string ExtractDataFromNSData(Process *process, lldb::addr_t data_addr);
};

/// Function wrapper for LLDB registration
bool GNUstepNSJSONSerializationFormatterFunction(ValueObject &valobj, Stream &stream, 
                                                 const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_JSONSERIALIZATION_H