//===-- GNUstepSetFormatters.h ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_SET_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_SET_H

#include "GNUstepFormattersBase.h"
#include <vector>

namespace lldb_private {

class CXXSyntheticChildren;

namespace formatters {

/// Summary provider for GNUstep NSSet objects
class GNUstepNSSetSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, 
                   const TypeSummaryOptions &options) override;

private:
  /// Extract set count from NSSet object
  uint32_t ExtractSetCount(ValueObject &valobj);
  
  /// Get inline preview of first few elements (like Apple's formatters)
  std::string GetInlineElementsPreview(ValueObject &valobj, uint32_t count);
  
  /// Extract set elements for preview (limited number)
  bool ExtractSetElementsForPreview(Process *process, lldb::addr_t obj_addr,
                                    std::vector<lldb::addr_t> &elements, uint32_t max_elements);
  
  /// Get summary for a single element
  std::string GetElementSummary(Process *process, lldb::addr_t element_addr);
  
  /// Check if address is a GNUstep tagged pointer
  static bool IsGNUstepTaggedPointer(lldb::addr_t addr);
  
  /// Get summary for tagged pointer objects
  std::string GetTaggedPointerSummary(lldb::addr_t addr);
  
  /// Try to extract string content from an object (simplified)
  std::string TryExtractStringContent(Process *process, lldb::addr_t obj_addr);
  
  /// Try to extract summary from collection objects (arrays, dictionaries, etc.)
  std::string TryExtractCollectionSummary(Process *process, lldb::addr_t obj_addr);
};

/// Synthetic children provider for GNUstep NSSet objects
class GNUstepNSSetSyntheticProvider : public GNUstepSyntheticProvider {
public:
  GNUstepNSSetSyntheticProvider(lldb::ValueObjectSP valobj_sp);
  ~GNUstepNSSetSyntheticProvider() override = default;

  llvm::Expected<uint32_t> CalculateNumChildren() override;
  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;

protected:
  bool UpdateImpl() override;

private:
  /// Read the set elements from memory
  bool ReadSetElements();
  
  /// Get object at specific index
  lldb::addr_t GetElementAtIndex(uint32_t idx);

  // Cached set information
  lldb::addr_t m_map_ptr;              // Pointer to GSIMapTable
  uint32_t m_count;                    // Number of elements
  std::vector<lldb::addr_t> m_elements; // Cached element addresses
  bool m_is_mutable;                   // Whether this is NSMutableSet
};

/// Function wrappers for LLDB registration
bool GNUstepNSSetFormatterFunction(ValueObject &valobj, Stream &stream, 
                                   const TypeSummaryOptions &options);

SyntheticChildrenFrontEnd *
GNUstepNSSetSyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                    lldb::ValueObjectSP valobj_sp);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_SET_H