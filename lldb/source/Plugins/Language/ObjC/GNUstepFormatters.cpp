//===-- GNUstepFormatters.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepFormatters.h"

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/Stream.h"
#include "llvm/Support/Error.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool lldb_private::formatters::IsGNUstepObjCRuntime(ValueObject &valobj) {
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;
  return llvm::isa_and_nonnull<GNUstepObjCRuntime>(
      ObjCLanguageRuntime::Get(*process_sp));
}

template <bool is_sel_ptr>
bool lldb_private::formatters::GNUstepObjCSELSummaryProvider(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;
  auto *runtime = llvm::dyn_cast_or_null<GNUstepObjCRuntime>(
      ObjCLanguageRuntime::Get(*process_sp));
  if (!runtime)
    return false;

  // Apple's provider special-cases SEL* by loading through the pointer; a
  // plain SEL holds the selector's address as its value.
  lldb::addr_t sel_addr = LLDB_INVALID_ADDRESS;
  if (is_sel_ptr) {
    const lldb::addr_t ptr_addr =
        valobj.GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
    if (ptr_addr == LLDB_INVALID_ADDRESS)
      return false;
    llvm::Expected<lldb::addr_t> pointee =
        process_sp->ReadPointerFromMemory(ptr_addr);
    if (!pointee) {
      llvm::consumeError(pointee.takeError());
      return false;
    }
    sel_addr = *pointee;
  } else {
    sel_addr = valobj.GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
  }
  if (sel_addr == 0 || sel_addr == LLDB_INVALID_ADDRESS)
    return false;

  ConstString name = runtime->GetSelectorName(sel_addr);
  if (!name)
    return false;
  stream.Printf("\"%s\"", name.AsCString(""));
  return true;
}

template bool lldb_private::formatters::GNUstepObjCSELSummaryProvider<true>(
    ValueObject &, Stream &, const TypeSummaryOptions &);
template bool lldb_private::formatters::GNUstepObjCSELSummaryProvider<false>(
    ValueObject &, Stream &, const TypeSummaryOptions &);
