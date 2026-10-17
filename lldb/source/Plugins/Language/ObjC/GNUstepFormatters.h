//===-- GNUstepFormatters.h -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Data formatters for values from the GNUstep libobjc2 runtime. Like the Apple
// formatters these never run code in the inferior.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGE_OBJC_GNUSTEPFORMATTERS_H
#define LLDB_SOURCE_PLUGINS_LANGUAGE_OBJC_GNUSTEPFORMATTERS_H

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/lldb-forward.h"

namespace lldb_private {
namespace formatters {

/// True if the process debugging \p valobj uses the GNUstep libobjc2 runtime.
bool IsGNUstepObjCRuntime(ValueObject &valobj);

/// Summary for a SEL, resolved from the symbol that names the selector.
/// Apple's provider reads the selector's name field from memory, and on
/// libobjc2 that field holds a numeric dispatch index once __objc_load has
/// run, so Apple's provider hands GNUstep values to this one.
template <bool is_sel_ptr>
bool GNUstepObjCSELSummaryProvider(ValueObject &valobj, Stream &stream,
                                   const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGE_OBJC_GNUSTEPFORMATTERS_H
