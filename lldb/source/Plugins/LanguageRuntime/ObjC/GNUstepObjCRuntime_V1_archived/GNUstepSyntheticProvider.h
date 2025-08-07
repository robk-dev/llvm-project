//===-- GNUstepSyntheticProvider.h -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEP_GNUSTEPSYNTHETICPROVIDER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEP_GNUSTEPSYNTHETICPROVIDER_H

#include "lldb/DataFormatters/TypeSynthetic.h"

namespace lldb_private {

/// Minimal stub synthetic provider to avoid build errors
/// This bypasses the offset calculation issues entirely
class GNUstepSyntheticProvider : public SyntheticChildrenFrontEnd {
public:
  explicit GNUstepSyntheticProvider(lldb::ValueObjectSP backend);
  ~GNUstepSyntheticProvider() override = default;

  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;
  lldb::ChildCacheState Update() override;
  bool MightHaveChildren() override;
  size_t GetIndexOfChildWithName(ConstString name) override;
  llvm::Expected<uint32_t> CalculateNumChildren() override;

private:
  /// Get the runtime offset for an ivar by looking up the offset symbol
  lldb::addr_t GetIvarOffsetFromRuntime(ConstString ivar_name);
  
  /// Fallback: Get ivar offset by parsing class metadata
  lldb::addr_t GetIvarOffsetFromClassMetadata(ConstString ivar_name);
  
  /// Parse the Objective-C class metadata to get actual ivar names
  /// instead of relying on LLDB's type system which stops at NSObject
  void GetClassIvarNames();
  
  /// Cache of ivar names discovered from class metadata
  std::vector<std::string> m_cached_ivar_names;
};

} // namespace lldb_private

#endif
