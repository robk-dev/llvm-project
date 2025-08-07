//===-- GNUstepNSDictionary.h ----------------------------------*- C++ -*-===//
//
// GNUstep NSDictionary formatters for LLDB
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNSDICTIONARY_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNSDICTIONARY_H

#include "lldb/lldb-forward.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/DataFormatters/TypeSynthetic.h"
#include "lldb/Symbol/CompilerType.h"
#include "GNUstepRuntimeAPI.h"

namespace lldb_private {
namespace formatters {

// Summary provider for GNUstep NSDictionary classes
bool GNUstepNSDictionarySummaryProvider(ValueObject &valobj, Stream &stream,
                                       const TypeSummaryOptions &options);

// Synthetic children provider for GNUstep NSDictionary classes
class GNUstepNSDictionarySyntheticProvider : public SyntheticChildrenFrontEnd {
public:
  GNUstepNSDictionarySyntheticProvider(lldb::ValueObjectSP valobj_sp);
  
  ~GNUstepNSDictionarySyntheticProvider() override = default;

  llvm::Expected<uint32_t> CalculateNumChildren() override;

  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;

  lldb::ChildCacheState Update() override;

  bool MightHaveChildren() override { return true; }

  size_t GetIndexOfChildWithName(ConstString name) override;

private:
  ExecutionContextRef m_exe_ctx_ref;
  uint8_t m_ptr_size = 8;
  lldb::addr_t m_dict_ptr = LLDB_INVALID_ADDRESS;
  std::vector<std::pair<lldb::addr_t, lldb::addr_t>> m_key_value_pairs;
  bool m_has_valid_data = false;
  CompilerType m_objc_id_type; // Apple's ObjCBuiltinIdTy for automatic summary provider application
  GNUstepRuntimeAPISP m_runtime_api; // Runtime API for safe method calls
  
  // Dynamic offset caching for performance
  ptrdiff_t m_map_offset = -1;        // Offset of _map ivar in NSDictionary
  ptrdiff_t m_nodeCount_offset = -1;  // Offset of nodeCount in GSIMapTable
  ptrdiff_t m_bucketCount_offset = -1; // Offset of bucketCount in GSIMapTable
  ptrdiff_t m_buckets_offset = -1;     // Offset of buckets pointer in GSIMapTable
  bool m_offsets_cached = false;       // Whether offsets have been discovered
  
  // Try to extract key-value pairs using expression evaluation (most reliable)
  bool ExtractPairsUsingExpression(lldb::StackFrameSP frame_sp);
  
  // Fall back to direct memory reading if expressions fail
  bool ExtractPairsFromMemory();
  
  // Direct memory reading implementation (avoids expression evaluation crashes)
  bool ExtractPairsFromMemoryDirect();
  
  // Discover dynamic offsets using runtime introspection
  bool DiscoverOffsets();
};

// Factory function for creating synthetic providers
SyntheticChildrenFrontEnd *
GNUstepNSDictionarySyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                          lldb::ValueObjectSP valobj_sp);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNSDICTIONARY_H