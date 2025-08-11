//===-- GNUstepEnhancedDictionaryFormatter.h -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GNUSTEPENHANCEDDICTIONARYFORMATTER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GNUSTEPENHANCEDDICTIONARYFORMATTER_H

#include "GNUstepFormattersBase.h"
#include <vector>

namespace lldb_private {

class CXXSyntheticChildren;

namespace formatters {

/// Summary provider for GNUstep enhanced dictionary display
class GNUstepEnhancedDictionarySummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, 
                   const TypeSummaryOptions &options) override;

private:
  uint32_t ExtractDictionaryCount(ValueObject &valobj);
  std::string GetInlineElementsPreview(ValueObject &valobj, uint32_t count, FormatterContext &context);
};

/// Enhanced synthetic provider for GNUstep NSDictionary objects
class GNUstepEnhancedDictionarySyntheticProvider : public GNUstepSyntheticProvider {
public:
  GNUstepEnhancedDictionarySyntheticProvider(lldb::ValueObjectSP valobj_sp);
  ~GNUstepEnhancedDictionarySyntheticProvider() override = default;

  llvm::Expected<uint32_t> CalculateNumChildren() override;
  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;

protected:
  bool UpdateImpl() override;

private:
  struct DictionaryItem {
    lldb::addr_t key_ptr;
    lldb::addr_t value_ptr;
    std::string key_summary;
    std::string value_summary;
  };

  /// Read dictionary structure from memory
  bool ReadDictionaryElements();
  
  /// Get key/value pair at specific index
  DictionaryItem GetItemAtIndex(uint32_t idx);
  
  /// Get summary for an object address
  std::string GetObjectSummary(lldb::addr_t obj_addr);

  // Cached dictionary information
  uint32_t m_count;
  std::vector<DictionaryItem> m_items;
  bool m_is_mutable;
  
  // Execution context and type information
  ExecutionContextRef m_exe_ctx_ref;
  CompilerType m_id_type;
};

/// Function wrappers for LLDB registration
bool GNUstepEnhancedDictionaryFormatterFunction(ValueObject &valobj, Stream &stream, 
                                                const TypeSummaryOptions &options);

SyntheticChildrenFrontEnd *
GNUstepEnhancedDictionarySyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                                  lldb::ValueObjectSP valobj_sp);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GNUSTEPENHANCEDDICTIONARYFORMATTER_H