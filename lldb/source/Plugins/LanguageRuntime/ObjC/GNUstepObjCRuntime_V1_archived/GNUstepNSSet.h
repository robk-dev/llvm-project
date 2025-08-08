//===-- GNUstepNSSet.h ------------------------------------------*- C++ -*-===//
//
// GNUstep NSSet formatters for LLDB
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNSSET_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNSSET_H

#include "lldb/lldb-forward.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/DataFormatters/TypeSynthetic.h"
#include "lldb/Symbol/CompilerType.h"

namespace lldb_private {
namespace formatters {

// Summary provider for GNUstep NSSet classes
bool GNUstepNSSetSummaryProvider(ValueObject &valobj, Stream &stream,
                                const TypeSummaryOptions &options);

// Synthetic children provider for GNUstep NSSet classes
class GNUstepNSSetSyntheticProvider : public SyntheticChildrenFrontEnd {
public:
  GNUstepNSSetSyntheticProvider(lldb::ValueObjectSP valobj_sp);
  
  ~GNUstepNSSetSyntheticProvider() override = default;

  llvm::Expected<uint32_t> CalculateNumChildren() override;

  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;

  lldb::ChildCacheState Update() override;

  bool MightHaveChildren() override { return true; }

  size_t GetIndexOfChildWithName(ConstString name) override;

private:
  ExecutionContextRef m_exe_ctx_ref;
  uint8_t m_ptr_size = 8;
  lldb::addr_t m_set_ptr = LLDB_INVALID_ADDRESS;
  std::vector<lldb::addr_t> m_element_addresses;
  bool m_has_valid_data = false;
  CompilerType m_objc_id_type; // Apple's ObjCBuiltinIdTy for automatic summary provider application
  
  // Try to extract elements using expression evaluation (most reliable)
  bool ExtractElementsUsingExpression(lldb::StackFrameSP frame_sp);
  
  // Fall back to direct memory reading if expressions fail
  bool ExtractElementsFromMemory();
  
  // Direct memory reading implementation (avoids expression evaluation crashes)
  bool ExtractElementsFromMemoryDirect();
};

// Factory function for creating synthetic providers
SyntheticChildrenFrontEnd *
GNUstepNSSetSyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                    lldb::ValueObjectSP valobj_sp);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNSSET_H