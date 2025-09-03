//===-- GNUstepFormattersRegistry.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepFormattersRegistry.h"

using namespace lldb_private::formatters;

void GNUstepFormattersRegistry::RegisterFormatters(TypeCategoryImpl &category) {
  // Minimal implementation - LLDB's expression evaluation handles GNUstep objects
  (void)category; // Suppress unused parameter warning
}
