//===-- GNUstepNullFormatter.cpp -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepNullFormatter.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Utility/Stream.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSNullSummaryProvider::FormatObject(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  // NSNull is a singleton, so we just need to check that it's not nil
  addr_t null_ptr = valobj.GetPointerValue();
  if (null_ptr == 0 || null_ptr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }
  
  // For NSNull, always display "(null)"
  stream.Printf("(null)");
  return true;
}

bool lldb_private::formatters::GNUstepNSNullFormatterFunction(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  GNUstepNSNullSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}