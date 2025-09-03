//===-- GNUstepFormattersRegistry.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepFormattersRegistry.h"
#include "GNUstepIdDispatcher.h"
#include "GNUstepStringFormatters.h"
#include "GNUstepNumberFormatters.h"
#include "GNUstepArrayFormatters.h"
#include "GNUstepDictionaryFormatters.h"
#include "GNUstepSetFormatters.h"
#include "GNUstepDateFormatters.h"
#include "GNUstepURLFormatters.h"
#include "GNUstepDataFormatters.h"
#include "GNUstepUUIDFormatters.h"
#include "GNUstepNullFormatter.h"
#include "GNUstepGenericFormatter.h"
#include "lldb/DataFormatters/DataVisualization.h"
#include "lldb/DataFormatters/TypeCategory.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/DataFormatters/TypeSynthetic.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

void GNUstepFormattersRegistry::RegisterFormatters(TypeCategoryImpl &category) {
  
  // Register core Foundation formatters only (Apple parity)
  RegisterStringFormatters(category);
  RegisterNumberFormatters(category);
  RegisterCollectionFormatters(category);
  RegisterFoundationFormatters(category);
  
  // Register generic formatter as fallback for all other Objective-C objects
  RegisterGenericFormatter(category);
  
}

// === String Formatters ===

void GNUstepFormattersRegistry::RegisterStringFormatters(TypeCategoryImpl &category) {
  // Register NSString summary provider
  TypeSummaryImpl::Flags string_flags;
  string_flags.SetCascades(true)
             .SetSkipPointers(false)
             .SetSkipReferences(false)
             .SetDontShowChildren(true)
             .SetDontShowValue(true)
             .SetShowMembersOneLiner(false)
             .SetHideItemNames(true);

  // Create the summary provider
  auto string_summary = std::make_shared<CXXFunctionSummaryFormat>(
      string_flags, GNUstepNSStringFormatterFunction, "NSString summary provider");

  // Register for various NSString type names
  category.AddTypeSummary("NSString", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("NSMutableString", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("GSMutableString", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("__NSCFString", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("NSConstantString", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("__NSConstantString", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("GSCInlineString", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("GSUInlineString", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("GSCString", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("GSUnicodeString", eFormatterMatchExact, string_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSString *", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("NSMutableString *", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("GSMutableString *", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("__NSCFString *", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("NSConstantString *", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("__NSConstantString *", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("GSCInlineString *", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("GSUInlineString *", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("GSCString *", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("GSUnicodeString *", eFormatterMatchExact, string_summary);
  
  // Register the comprehensive id dispatcher that handles all GNUstep types
  auto id_summary = std::make_shared<CXXFunctionSummaryFormat>(
      string_flags, GNUstepIdDispatcherFunction, "GNUstep id dispatcher");
  category.AddTypeSummary("id", eFormatterMatchExact, id_summary);
}

// === Number Formatters ===

void GNUstepFormattersRegistry::RegisterNumberFormatters(TypeCategoryImpl &category) {
  // Register NSNumber summary provider
  TypeSummaryImpl::Flags number_flags;
  number_flags.SetCascades(true)
             .SetSkipPointers(false)
             .SetSkipReferences(false)
             .SetDontShowChildren(true)
             .SetDontShowValue(true)
             .SetShowMembersOneLiner(false)
             .SetHideItemNames(true);

  // Create the summary provider
  auto number_summary = std::make_shared<CXXFunctionSummaryFormat>(
      number_flags, GNUstepNSNumberFormatterFunction, "NSNumber summary provider");

  // Register for various NSNumber type names including tagged pointers
  category.AddTypeSummary("NSNumber", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSNumber *", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("__NSCFNumber", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("__NSCFNumber *", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("GSDouble", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("GSFloat", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("GSInt", eFormatterMatchExact, number_summary);
}

// === Collection Formatters ===

void GNUstepFormattersRegistry::RegisterCollectionFormatters(TypeCategoryImpl &category) {
  RegisterArrayFormatters(category);
  RegisterDictionaryFormatters(category);
  RegisterSetFormatters(category);
}

void GNUstepFormattersRegistry::RegisterArrayFormatters(TypeCategoryImpl &category) {
  // Register NSArray summary provider
  TypeSummaryImpl::Flags array_flags;
  array_flags.SetCascades(true)
            .SetSkipPointers(false)
            .SetSkipReferences(false)
            .SetDontShowChildren(false)
            .SetDontShowValue(true)
            .SetShowMembersOneLiner(false)
            .SetHideItemNames(false);

  auto array_summary = std::make_shared<CXXFunctionSummaryFormat>(
      array_flags, GNUstepNSArrayFormatterFunction, "NSArray summary provider");

  category.AddTypeSummary("NSArray", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("NSMutableArray", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("NSArray *", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("NSMutableArray *", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("__NSArrayI", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("__NSArrayM", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("GSMutableArray", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("GSArray", eFormatterMatchExact, array_summary);

  // Register synthetic children provider
  SyntheticChildren::Flags array_synth_flags;
  array_synth_flags.SetCascades(true).SetSkipPointers(false).SetSkipReferences(false);
  
  auto array_synth = std::make_shared<CXXSyntheticChildren>(
      array_synth_flags, "NSArray synthetic children",
      GNUstepNSArraySyntheticFrontEndCreator);
      
  category.AddTypeSynthetic("NSArray", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("NSMutableArray", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("__NSArrayI", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("__NSArrayM", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("GSMutableArray", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("GSArray", eFormatterMatchExact, array_synth);
}

void GNUstepFormattersRegistry::RegisterDictionaryFormatters(TypeCategoryImpl &category) {
  // Register NSDictionary summary provider
  TypeSummaryImpl::Flags dict_flags;
  dict_flags.SetCascades(true)
           .SetSkipPointers(false)
           .SetSkipReferences(false)
           .SetDontShowChildren(false)
           .SetDontShowValue(true)
           .SetShowMembersOneLiner(false)
           .SetHideItemNames(false);

  auto dict_summary = std::make_shared<CXXFunctionSummaryFormat>(
      dict_flags, GNUstepNSDictionaryFormatterFunction, "NSDictionary summary provider");

  category.AddTypeSummary("NSDictionary", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("NSMutableDictionary", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("NSDictionary *", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("NSMutableDictionary *", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("__NSDictionaryI", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("__NSDictionaryM", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("GSMutableDictionary", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("GSDictionary", eFormatterMatchExact, dict_summary);

  // Register synthetic children provider
  SyntheticChildren::Flags dict_synth_flags;
  dict_synth_flags.SetCascades(true).SetSkipPointers(false).SetSkipReferences(false);
  
  auto dict_synth = std::make_shared<CXXSyntheticChildren>(
      dict_synth_flags, "NSDictionary synthetic children",
      GNUstepNSDictionarySyntheticFrontEndCreator);
      
  category.AddTypeSynthetic("NSDictionary", eFormatterMatchExact, dict_synth);
  category.AddTypeSynthetic("NSMutableDictionary", eFormatterMatchExact, dict_synth);
  category.AddTypeSynthetic("__NSDictionaryI", eFormatterMatchExact, dict_synth);
  category.AddTypeSynthetic("__NSDictionaryM", eFormatterMatchExact, dict_synth);
  category.AddTypeSynthetic("GSMutableDictionary", eFormatterMatchExact, dict_synth);
  category.AddTypeSynthetic("GSDictionary", eFormatterMatchExact, dict_synth);
}

void GNUstepFormattersRegistry::RegisterSetFormatters(TypeCategoryImpl &category) {
  // Register NSSet summary provider
  TypeSummaryImpl::Flags set_flags;
  set_flags.SetCascades(true)
          .SetSkipPointers(false)
          .SetSkipReferences(false)
          .SetDontShowChildren(false)
          .SetDontShowValue(true)
          .SetShowMembersOneLiner(false)
          .SetHideItemNames(false);

  auto set_summary = std::make_shared<CXXFunctionSummaryFormat>(
      set_flags, GNUstepNSSetFormatterFunction, "NSSet summary provider");

  category.AddTypeSummary("NSSet", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("NSMutableSet", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("NSSet *", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("NSMutableSet *", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("__NSSetI", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("__NSSetM", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("GSMutableSet", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("GSSet", eFormatterMatchExact, set_summary);

  // Register synthetic children provider
  SyntheticChildren::Flags set_synth_flags;
  set_synth_flags.SetCascades(true).SetSkipPointers(false).SetSkipReferences(false);
  
  auto set_synth = std::make_shared<CXXSyntheticChildren>(
      set_synth_flags, "NSSet synthetic children",
      GNUstepNSSetSyntheticFrontEndCreator);
      
  category.AddTypeSynthetic("NSSet", eFormatterMatchExact, set_synth);
  category.AddTypeSynthetic("NSMutableSet", eFormatterMatchExact, set_synth);
  category.AddTypeSynthetic("__NSSetI", eFormatterMatchExact, set_synth);
  category.AddTypeSynthetic("__NSSetM", eFormatterMatchExact, set_synth);
  category.AddTypeSynthetic("GSMutableSet", eFormatterMatchExact, set_synth);
  category.AddTypeSynthetic("GSSet", eFormatterMatchExact, set_synth);
}

// === Foundation Utility Formatters ===

void GNUstepFormattersRegistry::RegisterFoundationFormatters(TypeCategoryImpl &category) {
  // Core Foundation formatters only (essential types)
  RegisterDateFormatters(category);
  RegisterURLFormatters(category);
  RegisterDataFormatters(category);
  RegisterUUIDFormatters(category);
  RegisterNullFormatter(category);
}

void GNUstepFormattersRegistry::RegisterDateFormatters(TypeCategoryImpl &category) {
  TypeSummaryImpl::Flags date_flags;
  date_flags.SetCascades(true)
           .SetSkipPointers(false)
           .SetSkipReferences(false)
           .SetDontShowChildren(true)
           .SetDontShowValue(true)
           .SetShowMembersOneLiner(false)
           .SetHideItemNames(true);

  auto date_summary = std::make_shared<CXXFunctionSummaryFormat>(
      date_flags, GNUstepNSDateFormatterFunction, "NSDate summary provider");

  category.AddTypeSummary("NSDate", eFormatterMatchExact, date_summary);
  category.AddTypeSummary("NSDate *", eFormatterMatchExact, date_summary);
  category.AddTypeSummary("__NSDate", eFormatterMatchExact, date_summary);
  category.AddTypeSummary("GSDate", eFormatterMatchExact, date_summary);
  category.AddTypeSummary("NSCalendarDate", eFormatterMatchExact, date_summary);
  category.AddTypeSummary("NSCalendarDate *", eFormatterMatchExact, date_summary);
}

void GNUstepFormattersRegistry::RegisterURLFormatters(TypeCategoryImpl &category) {
  TypeSummaryImpl::Flags url_flags;
  url_flags.SetCascades(true)
          .SetSkipPointers(false)
          .SetSkipReferences(false)
          .SetDontShowChildren(true)
          .SetDontShowValue(true)
          .SetShowMembersOneLiner(false)
          .SetHideItemNames(true);

  auto url_summary = std::make_shared<CXXFunctionSummaryFormat>(
      url_flags, GNUstepNSURLFormatterFunction, "NSURL summary provider");

  category.AddTypeSummary("NSURL", eFormatterMatchExact, url_summary);
  category.AddTypeSummary("NSURL *", eFormatterMatchExact, url_summary);
  category.AddTypeSummary("__NSURL", eFormatterMatchExact, url_summary);
  category.AddTypeSummary("GSURL", eFormatterMatchExact, url_summary);
}

void GNUstepFormattersRegistry::RegisterDataFormatters(TypeCategoryImpl &category) {
  TypeSummaryImpl::Flags data_flags;
  data_flags.SetCascades(true)
           .SetSkipPointers(false)
           .SetSkipReferences(false)
           .SetDontShowChildren(true)
           .SetDontShowValue(true)
           .SetShowMembersOneLiner(false)
           .SetHideItemNames(true);

  auto data_summary = std::make_shared<CXXFunctionSummaryFormat>(
      data_flags, GNUstepNSDataFormatterFunction, "NSData summary provider");

  category.AddTypeSummary("NSData", eFormatterMatchExact, data_summary);
  category.AddTypeSummary("NSMutableData", eFormatterMatchExact, data_summary);
  category.AddTypeSummary("NSData *", eFormatterMatchExact, data_summary);
  category.AddTypeSummary("NSMutableData *", eFormatterMatchExact, data_summary);
  category.AddTypeSummary("__NSCFData", eFormatterMatchExact, data_summary);
  category.AddTypeSummary("GSMutableData", eFormatterMatchExact, data_summary);
  category.AddTypeSummary("GSData", eFormatterMatchExact, data_summary);
}

void GNUstepFormattersRegistry::RegisterUUIDFormatters(TypeCategoryImpl &category) {
  TypeSummaryImpl::Flags uuid_flags;
  uuid_flags.SetCascades(true)
           .SetSkipPointers(false)
           .SetSkipReferences(false)
           .SetDontShowChildren(true)
           .SetDontShowValue(true)
           .SetShowMembersOneLiner(false)
           .SetHideItemNames(true);

  auto uuid_summary = std::make_shared<CXXFunctionSummaryFormat>(
      uuid_flags, GNUstepNSUUIDFormatterFunction, "NSUUID summary provider");

  category.AddTypeSummary("NSUUID", eFormatterMatchExact, uuid_summary);
  category.AddTypeSummary("NSUUID *", eFormatterMatchExact, uuid_summary);
  category.AddTypeSummary("__NSUUID", eFormatterMatchExact, uuid_summary);
  category.AddTypeSummary("GSUUID", eFormatterMatchExact, uuid_summary);
}

void GNUstepFormattersRegistry::RegisterNullFormatter(TypeCategoryImpl &category) {
  TypeSummaryImpl::Flags null_flags;
  null_flags.SetCascades(true)
           .SetSkipPointers(false)
           .SetSkipReferences(false)
           .SetDontShowChildren(true)
           .SetDontShowValue(true)
           .SetShowMembersOneLiner(false)
           .SetHideItemNames(true);

  auto null_summary = std::make_shared<CXXFunctionSummaryFormat>(
      null_flags, GNUstepNSNullFormatterFunction, "NSNull summary provider");

  category.AddTypeSummary("NSNull", eFormatterMatchExact, null_summary);
  category.AddTypeSummary("NSNull *", eFormatterMatchExact, null_summary);
  category.AddTypeSummary("__NSNull", eFormatterMatchExact, null_summary);
  category.AddTypeSummary("GSNull", eFormatterMatchExact, null_summary);
}

// === Generic Formatter ===

void GNUstepFormattersRegistry::RegisterGenericFormatter(TypeCategoryImpl &category) {
  // Register generic formatter for any unhandled Objective-C object
  TypeSummaryImpl::Flags generic_flags;
  generic_flags.SetCascades(true)
              .SetSkipPointers(false)
              .SetSkipReferences(false)
              .SetDontShowChildren(false)
              .SetDontShowValue(false)
              .SetShowMembersOneLiner(false)
              .SetHideItemNames(false);

  auto generic_summary = std::make_shared<CXXFunctionSummaryFormat>(
      generic_flags, GNUstepGenericFormatterFunction, "GNUstep generic object formatter");

  // Register with lower priority so specific formatters take precedence
  // Use regex to match any objc object type
  category.AddTypeSummary("^(GS|NS|__NS).*", eFormatterMatchRegex, generic_summary);
  
  // Also register synthetic children for generic objects
  SyntheticChildren::Flags generic_synth_flags;
  generic_synth_flags.SetCascades(true).SetSkipPointers(false).SetSkipReferences(false);
  
  auto generic_synth = std::make_shared<CXXSyntheticChildren>(
      generic_synth_flags, "GNUstep generic object synthetic children",
      GNUstepGenericObjectSyntheticFrontEndCreator);
      
  category.AddTypeSynthetic("^(GS|NS|__NS).*", eFormatterMatchRegex, generic_synth);
}
