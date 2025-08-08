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
class GNUstepFormattersRegistry {
public:
  /// Register all GNUstep formatters with LLDB
  static void RegisterFormatters(TypeCategoryImpl &category);
  
  /// Register string formatters
  static void RegisterStringFormatters(TypeCategoryImpl &category);
  
  /// Register number formatters (to be implemented)
  static void RegisterNumberFormatters(TypeCategoryImpl &category);
  
  /// Register collection formatters (to be implemented)
  static void RegisterCollectionFormatters(TypeCategoryImpl &category);
  
  /// Register array formatters
  static void RegisterArrayFormatters(TypeCategoryImpl &category);
  
  /// Register dictionary formatters
  static void RegisterDictionaryFormatters(TypeCategoryImpl &category);
  
  /// Register set formatters
  static void RegisterSetFormatters(TypeCategoryImpl &category);
  
  /// Register foundation formatters
  static void RegisterFoundationFormatters(TypeCategoryImpl &category);
  
  /// Register date formatters (NSDate, NSCalendarDate)
  static void RegisterDateFormatters(TypeCategoryImpl &category);
  
  /// Register URL formatters (NSURL)
  static void RegisterURLFormatters(TypeCategoryImpl &category);
  
  /// Register error formatters (NSError)
  static void RegisterErrorFormatters(TypeCategoryImpl &category);
  
  /// Register data formatters (NSData, NSMutableData)
  static void RegisterDataFormatters(TypeCategoryImpl &category);
  
  /// Register UUID formatters (NSUUID)
  static void RegisterUUIDFormatters(TypeCategoryImpl &category);
  
  /// Register generic formatter for any Objective-C object
  static void RegisterGenericFormatter(TypeCategoryImpl &category);

private:
  GNUstepFormattersRegistry() = delete; // Static class only
};

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_REGISTRY_H
