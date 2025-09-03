//===-- GNUstepIdDispatcher.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepIdDispatcher.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Utility/Stream.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

// Minimal implementation for testing - always returns false to let LLDB
// handle object formatting through expression evaluation
bool lldb_private::formatters::GNUstepIdDispatcherFunction(ValueObject &valobj, Stream &stream, 
                                                           const TypeSummaryOptions &options) {
  // TEST: Minimal approach - let LLDB's expression evaluation handle everything
  (void)valobj;   // Suppress unused parameter warning  
  (void)stream;   // Suppress unused parameter warning
  (void)options;  // Suppress unused parameter warning
  
  // Return false to indicate we didn't handle the formatting,
  // allowing LLDB to use expression evaluation instead
  return false;
}