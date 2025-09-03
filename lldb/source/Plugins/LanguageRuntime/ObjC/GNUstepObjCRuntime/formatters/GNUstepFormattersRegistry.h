//===-- GNUstepFormattersRegistry.h ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_REGISTRY_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_REGISTRY_H

#include "lldb/lldb-forward.h"

namespace lldb_private {

class TypeCategoryImpl;

namespace formatters {

/// Central registry for all GNUstep formatters
/// TEST: Minimal approach - relying on LLDB's expression evaluation
class GNUstepFormattersRegistry {
public:
  /// Register all GNUstep formatters with LLDB
  /// Currently empty - testing if expression evaluation handles everything
  static void RegisterFormatters(TypeCategoryImpl &category);

private:
  GNUstepFormattersRegistry() = delete; // Static class only
};

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_REGISTRY_H
