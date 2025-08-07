//===-- GNUstepArraySyntheticProvider.h ------------------------*- C++ -*-===//
//
// Synthetic children provider for GNUstep NSArray objects
// Provides indexed access to array elements with proper type casting
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPARRAYSYNTHETIC_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPARRAYSYNTHETIC_H

#include "lldb/DataFormatters/TypeSynthetic.h"
#include "lldb/lldb-forward.h"

namespace lldb_private {

class RuntimeIntrospector;

class GNUstepArraySyntheticProvider : public SyntheticChildrenFrontEnd {
public:
  GNUstepArraySyntheticProvider(lldb::ValueObjectSP valobj_sp);
  
  ~GNUstepArraySyntheticProvider() override = default;
  
  llvm::Expected<uint32_t> CalculateNumChildren() override;
  
  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;
  
  lldb::ChildCacheState Update() override;
  
  bool MightHaveChildren() override;
  
  size_t GetIndexOfChildWithName(ConstString name) override;
  
private:
  // Apple pattern members - matching NSArray.cpp
  ExecutionContextRef m_exe_ctx_ref;
  CompilerType m_id_type;
  
  // Array layout information - dynamically discovered via runtime introspection
  lldb::addr_t m_array_ptr;
  uint32_t m_count;
  lldb::addr_t m_data_address;  // _contents pointer or inline elements address
  uint8_t m_ptr_size;
  
  // Runtime introspection for dynamic offset discovery
  RuntimeIntrospector *m_runtime_introspector;
  std::string m_class_name;
  
  // Dynamically discovered offsets (cached after first discovery)
  ptrdiff_t m_contents_array_offset;
  ptrdiff_t m_count_offset;
  bool m_offsets_discovered;
  
  // Helper methods
  bool ExtractArrayInfo();
  bool DiscoverIvarOffsets();
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPARRAYSYNTHETIC_H