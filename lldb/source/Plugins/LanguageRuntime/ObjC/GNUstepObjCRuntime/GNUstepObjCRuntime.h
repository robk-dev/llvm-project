//===-- GNUstepObjCRuntime.h ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "lldb/lldb-private.h"

#include "GNUstepObjCRuntimeIntrospector.h"
#include "GNUstepObjCRuntime.h"
#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_H

#include "lldb/Target/ObjCLanguageRuntime.h"
#include "lldb/lldb-private.h"

#include "GNUstepObjCRuntimeIntrospector.h"

namespace lldb_private {

class GNUstepObjCRuntime : public ObjCLanguageRuntime {
public:
  ~GNUstepObjCRuntime() override;

  // Static Functions
  static void Initialize();
  static void Terminate();
  static lldb::LanguageRuntimeSP CreateInstance(Process *process,
                                                lldb::LanguageType language);
  static llvm::StringRef GetPluginName() { return "gnu-objc-v2"; }

  // LanguageRuntime
  bool GetObjectDescription(Stream &str, ValueObject &object) override;
  bool GetObjectDescription(Stream &str, Value &value,
                            lldb::DynamicValueType use_dynamic) override;

  lldb::BreakpointResolverSP
  CreateExceptionBreakpointResolver(const Breakpoint &bkpt, bool is_catch,
                                    bool is_throw) override;

  lldb::ThreadPlanSP GetStepThroughTrampolinePlan(Thread &thread,
                                                  bool stop_others) override;

protected:
  // Classes and Types
  bool IsValid() override { return m_process != nullptr; }

private:
  GNUstepObjCRuntime(Process *process);
  std::unique_ptr<GNUstepObjCRuntimeIntrospector> m_introspector_up;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_H
