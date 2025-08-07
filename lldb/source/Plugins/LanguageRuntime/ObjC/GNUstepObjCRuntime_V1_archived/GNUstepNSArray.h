//===-- GNUstepNSArray.h ---------------------------------------*- C++ -*-===//
//
// GNUstep NSArray formatters for LLDB
// 
// Provides both summary and synthetic providers for NSArray objects
// Based on proven NSSet implementation pattern
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNSARRAY_H
#define LLDB_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNSARRAY_H

#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/DataFormatters/TypeSynthetic.h"
#include "lldb/lldb-forward.h"
#include "lldb/Symbol/CompilerType.h"
#include "GNUstepRuntimeAPI.h"
#include <vector>

namespace lldb_private {
namespace formatters {

/// Summary provider for GNUstep NSArray objects
/// Shows array preview with first 4 elements: [@"first", @"second", @"third", @"fourth", ...] (N elements)
/// @param valobj The NSArray ValueObject
/// @param stream Output stream for summary text
/// @param options Type summary formatting options
/// @return true if summary was generated successfully
bool GNUstepNSArraySummaryProvider(ValueObject &valobj, Stream &stream,
                                   const TypeSummaryOptions &options);

/// Synthetic children provider for GNUstep NSArray objects
/// Provides indexed access to individual array elements
class GNUstepNSArraySyntheticProvider : public SyntheticChildrenFrontEnd {
public:
  GNUstepNSArraySyntheticProvider(lldb::ValueObjectSP valobj_sp);

  ~GNUstepNSArraySyntheticProvider() override = default;

  llvm::Expected<uint32_t> CalculateNumChildren() override;

  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;

  lldb::ChildCacheState Update() override;

  bool MightHaveChildren() override;

  size_t GetIndexOfChildWithName(ConstString name) override;

private:
  ExecutionContextRef m_exe_ctx_ref;
  uint32_t m_count;
  lldb::addr_t m_array_addr; // Address of the NSArray object itself
  CompilerType m_objc_id_type; // Apple's ObjCBuiltinIdTy for automatic summary provider application
  GNUstepRuntimeAPISP m_runtime_api; // Runtime API for safe method calls

  // Dynamic offset caching for performance
  ptrdiff_t m_count_offset;      // Offset of _count ivar
  ptrdiff_t m_contents_offset;   // Offset of _contents_array ivar  
  bool m_offsets_cached;         // Whether offsets have been discovered

  /// Call objectAtIndex: method safely via runtime
  /// @param index The array index to retrieve
  /// @return Object address, or LLDB_INVALID_ADDRESS on failure
  lldb::addr_t CallObjectAtIndex(uint32_t index);

  /// Discover and cache ivar offsets using runtime API
  /// @return true if offsets were successfully discovered
  bool DiscoverOffsets();
};

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNSARRAY_H