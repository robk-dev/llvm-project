//===-- GNUstepUniversalFormatter.h -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Universal formatter that replaces ALL specific formatters using expression
// evaluation. This single formatter handles all GNUstep/Objective-C objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPUNIVERSALFORMATTER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPUNIVERSALFORMATTER_H

#include "GNUstepObjCRuntimeIntrospector.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/DataFormatters/TypeSynthetic.h"

namespace lldb_private {
namespace formatters {

// Universal summary provider - handles ALL objects including tagged pointers
bool GNUstepUniversalSummaryProvider(ValueObject &valobj, Stream &stream,
                                     const TypeSummaryOptions &options);

// Universal synthetic children provider - handles collections
class GNUstepUniversalSyntheticProvider : public SyntheticChildrenFrontEnd {
public:
  GNUstepUniversalSyntheticProvider(ValueObject &valobj);

  ~GNUstepUniversalSyntheticProvider() override = default;

  llvm::Expected<uint32_t> CalculateNumChildren() override;

  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;

  lldb::ChildCacheState Update() override;

  bool MightHaveChildren() override;

  size_t GetIndexOfChildWithName(ConstString name) override;

private:
  ExecutionContext m_exe_ctx;
  lldb::addr_t m_obj_addr;
  std::string m_class_name;
  uint32_t m_count;

  enum ObjectType {
    Unknown,
    TaggedPointer,
    Array,
    Dictionary,
    Set,
    Other,
    CustomClass
  };

  ObjectType m_type;
  ObjCLanguageRuntime::ClassDescriptorSP m_class_descriptor;

  // Helper methods
  lldb::ValueObjectSP EvaluateExpression(const std::string &expr);
  lldb::ValueObjectSP GetArrayElementViaFunctionCaller(uint32_t idx);
  lldb::ValueObjectSP GetSetElementViaFunctionCaller(uint32_t idx);
  uint32_t GetCollectionCountViaFunctionCaller();
  ObjectType DetectObjectType();
  bool IsTaggedPointer() const { return (m_obj_addr & 0x1) != 0; }
};

// Creator function for synthetic provider
SyntheticChildrenFrontEnd *
GNUstepUniversalSyntheticProviderCreator(CXXSyntheticChildren *synth,
                                         lldb::ValueObjectSP valobj_sp);

} // namespace formatters
} // namespace lldb_private

#endif