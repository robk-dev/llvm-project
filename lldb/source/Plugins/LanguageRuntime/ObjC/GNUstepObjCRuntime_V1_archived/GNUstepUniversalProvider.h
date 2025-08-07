//===-- GNUstepUniversalProvider.h ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef GNUSTEP_UNIVERSAL_PROVIDER_H
#define GNUSTEP_UNIVERSAL_PROVIDER_H

#include "lldb/DataFormatters/TypeSynthetic.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "GNUstepRuntimeAPI.h"

namespace lldb_private {
namespace formatters {

// Universal summary provider for any Objective-C object
bool GNUstepUniversalSummaryProvider(ValueObject &valobj, Stream &stream,
                                     const TypeSummaryOptions &options);

// Universal synthetic provider for any Objective-C object
class GNUstepUniversalSyntheticProvider : public SyntheticChildrenFrontEnd {
public:
  GNUstepUniversalSyntheticProvider(lldb::ValueObjectSP valobj_sp);
  
  llvm::Expected<uint32_t> CalculateNumChildren() override;
  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;
  lldb::ChildCacheState Update() override;
  bool MightHaveChildren() override;
  size_t GetIndexOfChildWithName(ConstString name) override;

private:
  GNUstepRuntimeAPISP m_runtime_api;
  std::vector<GNUstepRuntimeAPI::IvarInfo> m_ivars;
  lldb::addr_t m_object_addr;
  std::string m_class_name;
};

// Creator function for LLDB registration
SyntheticChildrenFrontEnd *
GNUstepUniversalProviderCreator(CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp);

} // namespace formatters
} // namespace lldb_private

#endif