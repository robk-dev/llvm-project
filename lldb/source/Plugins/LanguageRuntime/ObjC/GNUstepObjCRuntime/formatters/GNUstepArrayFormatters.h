//===-- GNUstepArrayFormatters.h -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_ARRAY_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_ARRAY_H

#include "GNUstepFormattersBase.h"
#include <vector>

namespace lldb_private {

class CXXSyntheticChildren;

namespace formatters {

/// Summary provider for GNUstep NSArray objects
class GNUstepNSArraySummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, 
                   const TypeSummaryOptions &options) override;

private:
  /// Extract array count from NSArray object
  uint32_t ExtractArrayCount(ValueObject &valobj);
  
  /// Get inline preview of first few elements (like Apple's formatters)
  std::string GetInlineElementsPreview(ValueObject &valobj, uint32_t count, FormatterContext &context);
  
  /// Get summary for a single element with recursion protection
  std::string GetElementSummary(Process *process, lldb::addr_t element_addr, FormatterContext &context);
  
  /// Check if address is a GNUstep tagged pointer
  static bool IsGNUstepTaggedPointer(lldb::addr_t addr);
  
  /// Get summary for tagged pointer objects
  std::string GetTaggedPointerSummary(lldb::addr_t addr);
  
  /// Try to extract string content from an object (simplified)
  std::string TryExtractStringContent(Process *process, lldb::addr_t obj_addr);
  
  /// Try to extract summary from collection objects (removed to prevent recursion)
};

/// Synthetic children provider for GNUstep NSArray objects
class GNUstepNSArraySyntheticProvider : public GNUstepSyntheticProvider {
public:
  GNUstepNSArraySyntheticProvider(lldb::ValueObjectSP valobj_sp);
  ~GNUstepNSArraySyntheticProvider() override = default;

  llvm::Expected<uint32_t> CalculateNumChildren() override;
  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;

protected:
  bool UpdateImpl() override;

private:
  /// Read the array elements from memory
  bool ReadArrayElements();
  
  /// Get object at specific index
  lldb::addr_t GetElementAtIndex(uint32_t idx);
  
  /// Get the concrete type for an object address
  CompilerType GetConcreteTypeForObject(lldb::addr_t obj_addr);

  // Cached array information
  lldb::addr_t m_contents_array_ptr;  // Pointer to _contents_array
  uint32_t m_count;                   // Number of elements
  std::vector<lldb::addr_t> m_elements; // Cached element addresses
  bool m_is_mutable;                  // Whether this is NSMutableArray
  uint32_t m_capacity;                 // Capacity (for mutable arrays)
  
  // Execution context and type information for creating child ValueObjects
  ExecutionContextRef m_exe_ctx_ref;
  CompilerType m_id_type;
};

/// Function wrappers for LLDB registration
bool GNUstepNSArrayFormatterFunction(ValueObject &valobj, Stream &stream, 
                                     const TypeSummaryOptions &options);

SyntheticChildrenFrontEnd *
GNUstepNSArraySyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                       lldb::ValueObjectSP valobj_sp);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_ARRAY_H