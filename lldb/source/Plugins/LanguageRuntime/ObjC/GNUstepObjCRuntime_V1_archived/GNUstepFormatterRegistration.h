//===-- GNUstepFormatterRegistration.h --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_PLUGINS_GNUSTEPOBJCRUNTIME_GNUSTEPFORMATTERREGISTRATION_H
#define LLDB_PLUGINS_GNUSTEPOBJCRUNTIME_GNUSTEPFORMATTERREGISTRATION_H

#include "lldb/DataFormatters/TypeCategory.h"
#include "lldb/Utility/Log.h"
#include "lldb/lldb-forward.h"

namespace lldb_private {
namespace formatters {

/// Register GNUstep NSString formatters (summary providers)
/// Handles NSString, NSConstantString, NSMutableString, GSTinyString, etc.
void RegisterGNUstepStringFormatters(lldb::TypeCategoryImplSP objc_category, Log *log);

/// Register GNUstep NSNumber formatters (summary providers)
/// Handles NSNumber, NSDecimalNumber, etc.
void RegisterGNUstepNumberFormatters(lldb::TypeCategoryImplSP objc_category, Log *log);

/// Register GNUstep NSDate formatters (summary providers)
/// Handles NSDate, NSCalendarDate, etc.
void RegisterGNUstepDateFormatters(lldb::TypeCategoryImplSP objc_category, Log *log);

/// Register GNUstep collection formatters (both summary and synthetic providers)
/// Handles NSArray, NSDictionary, NSSet and their variants
void RegisterGNUstepCollectionFormatters(lldb::TypeCategoryImplSP objc_category, Log *log);

/// Register GNUstep universal formatter for custom classes
/// Handles custom user-defined classes with generic ivar introspection
void RegisterGNUstepUniversalFormatters(lldb::TypeCategoryImplSP objc_category, Log *log);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_PLUGINS_GNUSTEPOBJCRUNTIME_GNUSTEPFORMATTERREGISTRATION_H