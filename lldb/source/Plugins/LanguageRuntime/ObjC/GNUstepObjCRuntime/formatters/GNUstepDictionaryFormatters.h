//===-- GNUstepDictionaryFormatters.h ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GNUSTEPDICTIONARYFORMATTERS_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GNUSTEPDICTIONARYFORMATTERS_H

#include "GNUstepFormattersBase.h"
#include <vector>
#include <utility>

namespace lldb_private {
namespace formatters {

//===----------------------------------------------------------------------===//
// NSDictionary Summary Provider
//===----------------------------------------------------------------------===//

class GNUstepNSDictionarySummaryProvider : public GNUstepSummaryProvider {
public:
  static bool WouldFormat(ValueObject &valobj) {
    // Check if this is a dictionary class
    const char *class_name = valobj.GetTypeName().GetCString();
    if (!class_name) return false;
    
    return (strstr(class_name, "NSDictionary") ||
            strstr(class_name, "NSMutableDictionary") ||
            strstr(class_name, "GSDictionary") ||
            strstr(class_name, "GSMutableDictionary"));
  }
  
  bool FormatObject(ValueObject &valobj, 
                    Stream &stream,
                    const TypeSummaryOptions &options) override;
  
  /// Get summary for a single element (key or value) - made public for sharing
  std::string GetElementSummary(Process *process, lldb::addr_t element_addr, FormatterContext &context);

private:
  // Structures for key-value pairs and map table info (shared with synthetic provider)
  struct KeyValuePair {
    lldb::addr_t key_addr;
    lldb::addr_t value_addr;
  };
  
  struct MapTableInfo {
    lldb::addr_t buckets_ptr;      // Pointer to buckets array
    uint32_t bucket_count;          // Number of buckets
    uint32_t node_count;            // Total nodes in dictionary
  };
  
  uint32_t ExtractDictionaryCount(ValueObject &valobj);
  
  /// Get inline preview of first few key-value pairs (like Apple's formatters)
  std::string GetInlinePairsPreview(ValueObject &valobj, uint32_t count);
  
  /// Read map table information
  bool ReadMapTableInfo(Process *process, lldb::addr_t obj_addr, MapTableInfo &map_info);
  
  /// Extract key-value pairs for preview (limited number)
  bool ExtractKeyValuePairsForPreview(Process *process, const MapTableInfo &map_info,
                                      std::vector<KeyValuePair> &pairs, uint32_t max_pairs);
  
  /// Check if address is a GNUstep tagged pointer
  static bool IsGNUstepTaggedPointer(lldb::addr_t addr);
  
  /// Get summary for tagged pointer objects
  std::string GetTaggedPointerSummary(lldb::addr_t addr);
  
  /// Try to extract string content from an object (simplified)
  std::string TryExtractStringContent(Process *process, lldb::addr_t obj_addr);
  
  /// Try to extract summary from collection objects (arrays, dictionaries, etc.)
  std::string TryExtractCollectionSummary(Process *process, lldb::addr_t obj_addr);
};

//===----------------------------------------------------------------------===//
// NSDictionary Synthetic Children Provider
//===----------------------------------------------------------------------===//

class GNUstepNSDictionarySyntheticProvider : public GNUstepSyntheticProvider {
public:
  GNUstepNSDictionarySyntheticProvider(lldb::ValueObjectSP valobj_sp);
  
  ~GNUstepNSDictionarySyntheticProvider() override = default;
  
  llvm::Expected<uint32_t> CalculateNumChildren() override;
  
  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;
  
  bool MightHaveChildren() override;
  
  size_t GetIndexOfChildWithName(ConstString name) override;

protected:
  bool UpdateImpl() override;

private:
  // GSIMapTable structure info
  struct MapTableInfo {
    lldb::addr_t buckets_ptr;      // Pointer to buckets array
    uint32_t bucket_count;          // Number of buckets
    uint32_t node_count;            // Total nodes in dictionary
  };
  
  // Key-value pair
  struct KeyValuePair {
    lldb::addr_t key_addr;
    lldb::addr_t value_addr;
  };
  
  MapTableInfo m_map_info;
  std::vector<KeyValuePair> m_pairs;
  bool m_is_mutable;
  
  // Execution context and type information for creating child ValueObjects
  ExecutionContextRef m_exe_ctx_ref;
  CompilerType m_id_type;
  
  bool ReadMapTableInfo(Process *process, lldb::addr_t obj_addr);
  bool ExtractKeyValuePairs(Process *process);
  
  /// Get the concrete type for an object address
  CompilerType GetConcreteTypeForObject(lldb::addr_t obj_addr);
  
  /// Get summary for a single element (key or value)
  std::string GetElementSummary(Process *process, lldb::addr_t element_addr, FormatterContext &context);
  
  /// Extract string content from an object (for use as child name)
  std::string ExtractStringFromObject(lldb::addr_t obj_addr);
};

/// Function wrappers for LLDB registration
bool GNUstepNSDictionaryFormatterFunction(ValueObject &valobj, Stream &stream, 
                                          const TypeSummaryOptions &options);

SyntheticChildrenFrontEnd *
GNUstepNSDictionarySyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                            lldb::ValueObjectSP valobj_sp);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GNUSTEPDICTIONARYFORMATTERS_H