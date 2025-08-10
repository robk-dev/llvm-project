//===-- GNUstepOrderedSetFormatters.h -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_ORDEREDSET_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_ORDEREDSET_H

#include "GNUstepFormattersBase.h"
#include <vector>

namespace lldb_private {

class CXXSyntheticChildren;

namespace formatters {

/// Summary provider for GNUstep NSOrderedSet objects
/// Combines ordered access (like NSArray) with uniqueness (like NSSet)
class GNUstepNSOrderedSetSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, 
                   const TypeSummaryOptions &options) override;

private:
  /// Extract ordered set count from NSOrderedSet object
  uint32_t ExtractOrderedSetCount(ValueObject &valobj);
  
  /// Get inline preview of first few elements in order (like Apple's formatters)
  std::string GetInlineElementsPreview(ValueObject &valobj, uint32_t count, FormatterContext &context);
  
  /// Get summary for a single element with recursion protection
  std::string GetElementSummary(Process *process, lldb::addr_t element_addr, FormatterContext &context);
  
  /// Try to extract string content from an object
  std::string TryExtractStringContent(Process *process, lldb::addr_t obj_addr);
};

/// Synthetic children provider for GNUstep NSOrderedSet objects  
/// Provides ordered indexed access [0], [1], [2]... like NSArray
class GNUstepNSOrderedSetSyntheticProvider : public GNUstepSyntheticProvider {
public:
  GNUstepNSOrderedSetSyntheticProvider(lldb::ValueObjectSP valobj_sp);
  ~GNUstepNSOrderedSetSyntheticProvider() override = default;

  llvm::Expected<uint32_t> CalculateNumChildren() override;
  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;

protected:
  bool UpdateImpl() override;

private:
  /// Read the ordered set elements from memory
  bool ReadOrderedSetElements();
  
  /// Get object at specific index (ordered access)
  lldb::addr_t GetElementAtIndex(uint32_t idx);
  
  /// Get the concrete type for an object address
  CompilerType GetConcreteTypeForObject(lldb::addr_t obj_addr);

  // Cached ordered set information
  lldb::addr_t m_objects_array_ptr;       // Pointer to _objects array
  uint32_t m_count;                       // Number of elements
  std::vector<lldb::addr_t> m_elements;   // Cached element addresses in order
  bool m_is_mutable;                      // Whether this is NSMutableOrderedSet
  uint32_t m_capacity;                    // Capacity (for mutable ordered sets)
  
  // Execution context and type information for creating child ValueObjects
  ExecutionContextRef m_exe_ctx_ref;
  CompilerType m_id_type;
};

/// Function wrappers for LLDB registration
bool GNUstepNSOrderedSetFormatterFunction(ValueObject &valobj, Stream &stream, 
                                         const TypeSummaryOptions &options);

SyntheticChildrenFrontEnd *
GNUstepNSOrderedSetSyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                           lldb::ValueObjectSP valobj_sp);

// Compatibility functions for ObjC language plugin
bool GNUstepOrderedSetSummaryProvider(ValueObject &valobj, Stream &stream,
                                     const TypeSummaryOptions &options);

SyntheticChildrenFrontEnd *
GNUstepOrderedSetSyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                         lldb::ValueObjectSP valobj_sp);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_ORDEREDSET_H