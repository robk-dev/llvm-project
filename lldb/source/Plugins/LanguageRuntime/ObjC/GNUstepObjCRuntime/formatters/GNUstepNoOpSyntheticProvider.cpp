//===-- GNUstepNoOpSyntheticProvider.cpp --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepNoOpSyntheticProvider.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Utility/ConstString.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

GNUstepNoOpSyntheticProvider::GNUstepNoOpSyntheticProvider(lldb::ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp), m_backend(valobj_sp) {}

lldb::ChildCacheState GNUstepNoOpSyntheticProvider::Update() {
  // Always return that we're up to date - nothing to update
  return lldb::ChildCacheState::eRefetch;
}

bool GNUstepNoOpSyntheticProvider::MightHaveChildren() {
  // Indicate that this object has no children
  return false;
}

size_t GNUstepNoOpSyntheticProvider::GetIndexOfChildWithName(ConstString name) {
  // No children, so no child can match any name
  return UINT32_MAX;
}

llvm::Expected<uint32_t> GNUstepNoOpSyntheticProvider::CalculateNumChildren() {
  // Always return 0 children
  return 0;
}

lldb::ValueObjectSP GNUstepNoOpSyntheticProvider::GetChildAtIndex(uint32_t idx) {
  // No children, so always return null
  return nullptr;
}

SyntheticChildrenFrontEnd *
lldb_private::formatters::GNUstepNoOpSyntheticFrontEndCreator(
    CXXSyntheticChildren *synth, lldb::ValueObjectSP valobj_sp) {
  return new GNUstepNoOpSyntheticProvider(valobj_sp);
}