//===-- GNUstepUniversalProvider.h ----------------------------*- C++ -*-===//
//
// Universal Synthetic Children Provider for GNUstep Objects
// Uses LLDB's type system to discover field offsets dynamically
// NO HARDCODED OFFSETS - Pure type introspection approach
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPUNIVERSALPROVIDER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPUNIVERSALPROVIDER_H

#include "lldb/DataFormatters/TypeSynthetic.h"
#include "lldb/lldb-forward.h"

namespace lldb_private {

// Forward declarations
class SyntheticChildrenFrontEnd;
class CXXSyntheticChildren;

class GNUstepUniversalProvider : public SyntheticChildrenFrontEnd {
public:
  GNUstepUniversalProvider(lldb::ValueObjectSP valobj_sp);
  
  ~GNUstepUniversalProvider() override = default;
  
  llvm::Expected<uint32_t> CalculateNumChildren() override;
  
  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;
  
  lldb::ChildCacheState Update() override;
  
  bool MightHaveChildren() override;
  
  size_t GetIndexOfChildWithName(ConstString name) override;
  
private:
  // Apple pattern execution context
  ExecutionContextRef m_exe_ctx_ref;
  CompilerType m_id_type;
  uint8_t m_ptr_size;
  
  // Object analysis results
  lldb::addr_t m_object_ptr;
  std::string m_actual_class_name;
  
  // Collection detection results
  bool m_is_collection;
  uint32_t m_count;
  lldb::addr_t m_elements_address;
  
  // Dynamic field discovery using LLDB's type system
  struct FieldInfo {
    std::string name;
    uint32_t offset;
    uint32_t size;
    CompilerType type;
  };
  std::vector<FieldInfo> m_discovered_fields;
  
  // Core methods - NO hardcoded offsets!
  bool AnalyzeObjectType();
  bool DiscoverFieldsFromTypeSystem();
  bool DetectCollectionPattern();
  lldb::addr_t GetDataAddressForCollection();
  
  // Inheritance analysis methods
  bool CheckInheritanceForCollectionTypes();
  bool TraverseInheritanceHierarchy(CompilerType type, const std::vector<std::string>& target_classes, int depth);
  
  // Universal field access
  lldb::ValueObjectSP ReadFieldByName(const std::string& field_name);
  uint32_t ReadUInt32Field(const std::string& field_name);
  lldb::addr_t ReadPointerField(const std::string& field_name);
};

namespace formatters {
// Creator function for LLDB registration
SyntheticChildrenFrontEnd *
GNUstepUniversalProviderCreator(CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp);
} // namespace formatters

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPUNIVERSALPROVIDER_H