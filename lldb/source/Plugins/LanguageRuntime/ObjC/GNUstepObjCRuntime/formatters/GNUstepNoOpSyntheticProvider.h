//===-- GNUstepNoOpSyntheticProvider.h ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGE_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNOOPSYNTHETICPROVIDER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGE_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNOOPSYNTHETICPROVIDER_H

#include "lldb/DataFormatters/TypeSynthetic.h"
#include "lldb/ValueObject/ValueObject.h"

namespace lldb_private {
namespace formatters {

/// A synthetic provider that safely provides no children
/// Used to disable synthetic children for problematic types
class GNUstepNoOpSyntheticProvider : public SyntheticChildrenFrontEnd {
public:
  GNUstepNoOpSyntheticProvider(lldb::ValueObjectSP valobj_sp);
  ~GNUstepNoOpSyntheticProvider() override = default;

  lldb::ChildCacheState Update() override;
  bool MightHaveChildren() override;
  size_t GetIndexOfChildWithName(ConstString name) override;
  llvm::Expected<uint32_t> CalculateNumChildren() override;
  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;

private:
  lldb::ValueObjectSP m_backend;
};

SyntheticChildrenFrontEnd *
GNUstepNoOpSyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                   lldb::ValueObjectSP valobj_sp);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGE_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPNOOPSYNTHETICPROVIDER_H