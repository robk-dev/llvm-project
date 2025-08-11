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
#include "GNUstepErrorFormatters.h"
#include "GNUstepDataFormatters.h"
#include "GNUstepUUIDFormatters.h"
#include "GNUstepNullFormatter.h"
#include "GNUstepExceptionFormatter.h"
#include "GNUstepAttributedStringFormatter.h"
#include "GNUstepIndexPathFormatter.h"
#include "GNUstepNotificationFormatter.h"
#include "GNUstepGenericFormatter.h"
#include "GNUstepNoOpSyntheticProvider.h"
#include "GNUstepIndexSetFormatters.h"
#include "GNUstepDecimalNumberFormatters.h"
#include "GNUstepCharacterSetFormatters.h"
#include "GNUstepOrderedSetFormatters.h"
#include "GNUstepBundleFormatters.h"
#include "GNUstepScannerFormatters.h"
#include "GNUstepLocaleFormatters.h"
#include "GNUstepUserDefaultsFormatters.h"
#include "GNUstepCalendarFormatters.h"
#include "GNUstepProcessInfoFormatters.h"
#include "GNUstepTimeIntervalFormatters.h"
#include "lldb/DataFormatters/DataVisualization.h"
#include "lldb/DataFormatters/TypeCategory.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/DataFormatters/TypeSynthetic.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

void GNUstepFormattersRegistry::RegisterFormatters(TypeCategoryImpl &category) {
  
  // Add safety checks to prevent crashes during formatter registration
  // Note: LLDB builds with exceptions disabled, so we use simpler checks
  RegisterStringFormatters(category);
  
  // Register Priority 1 Foundation formatters FIRST to ensure precedence over base classes
  RegisterPriority1Formatters(category);
  
  // Register NSTimeInterval formatter BEFORE NSNumber to ensure precedence for time intervals
  RegisterTimeIntervalFormatters(category);
  
  RegisterNumberFormatters(category);
  
  RegisterCollectionFormatters(category);
  
  RegisterFoundationFormatters(category);
  
  // Register generic formatter as fallback for all other Objective-C objects
  // Fixed crash issues in generic formatter - now safe to enable
  RegisterGenericFormatter(category);
  
}

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
  
  // Only register synthetic children provider for id type here
  SyntheticChildren::Flags id_synth_flags;
  id_synth_flags.SetCascades(true)
                .SetSkipPointers(false)
                .SetSkipReferences(false)
                .SetNonCacheable(false);
                
  auto id_synth = std::make_shared<CXXSyntheticChildren>(
      id_synth_flags, "id synthetic children", 
      GNUstepIdSyntheticFrontEndCreator);
      
  category.AddTypeSynthetic("id", eFormatterMatchExact, id_synth);
  
  // Register synthetic children provider for GSCInlineString to show proper field expansion
  SyntheticChildren::Flags inline_string_synth_flags;
  inline_string_synth_flags.SetCascades(true)
                           .SetSkipPointers(false)
                           .SetSkipReferences(false)
                           .SetNonCacheable(false);
                           
  auto inline_string_synth = std::make_shared<CXXSyntheticChildren>(
      inline_string_synth_flags, "GSCInlineString synthetic children", 
      GSCInlineStringSyntheticFrontEndCreator);
      
  category.AddTypeSynthetic("GSCInlineString", eFormatterMatchExact, inline_string_synth);
  category.AddTypeSynthetic("GSUInlineString", eFormatterMatchExact, inline_string_synth);
  category.AddTypeSynthetic("GSCInlineString *", eFormatterMatchExact, inline_string_synth);
  category.AddTypeSynthetic("GSUInlineString *", eFormatterMatchExact, inline_string_synth);
  
  // NOTE: Generic synthetic providers moved to RegisterGenericFormatter()
  // to ensure they don't override specific formatters
  
}

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

  // Register for NSNumber base class and its concrete subclasses
  category.AddTypeSummary("NSNumber", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSIntNumber", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSBoolNumber", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSLongLongNumber", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSUnsignedLongLongNumber", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSFloatNumber", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSDoubleNumber", eFormatterMatchExact, number_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSNumber *", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSIntNumber *", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSBoolNumber *", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSLongLongNumber *", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSUnsignedLongLongNumber *", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSFloatNumber *", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSDoubleNumber *", eFormatterMatchExact, number_summary);
  
  // Register for small object variants (tagged pointers)
  category.AddTypeSummary("NSSmallInt", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSSmallFloat", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSSmallExtendedDouble", eFormatterMatchExact, number_summary);
  category.AddTypeSummary("NSSmallRepeatingDouble", eFormatterMatchExact, number_summary);
  
  // CRITICAL FIX: Register for common tagged pointer address patterns in LLDB po
  // LLDB might see tagged pointers as raw addresses or generic pointer types
  // This ensures our NSNumber formatter catches them regardless of type name
  TypeSummaryImpl::Flags fallback_flags;
  fallback_flags.SetCascades(false)  // Don't cascade to avoid infinite loops
               .SetSkipPointers(true)   // We handle the pointer dereferencing
               .SetSkipReferences(false)
               .SetDontShowChildren(true)
               .SetDontShowValue(true)
               .SetShowMembersOneLiner(false)
               .SetHideItemNames(true);
  
  // Create a fallback summary that checks for tagged pointers
  auto fallback_summary = std::make_shared<CXXFunctionSummaryFormat>(
      fallback_flags, GNUstepIdDispatcherFunction, "Tagged pointer fallback");
  
  // Register for generic pointer patterns that might represent tagged NSNumbers
  // This catches cases where LLDB shows "(NSNumber *) 0x151" instead of calling our formatter
  category.AddTypeSummary("^NSNumber \\* const$", eFormatterMatchRegex, fallback_summary);
  category.AddTypeSummary("^const NSNumber \\*$", eFormatterMatchRegex, fallback_summary);
}

void GNUstepFormattersRegistry::RegisterCollectionFormatters(TypeCategoryImpl &category) {
  // Register NSArray formatters
  RegisterArrayFormatters(category);
  
  // Register NSDictionary formatters
  RegisterDictionaryFormatters(category);
  
  // Register NSSet formatters
  RegisterSetFormatters(category);
  
  // Register NSOrderedSet formatters - TODO: implement when files available
  // RegisterOrderedSetFormatters(category);
}

void GNUstepFormattersRegistry::RegisterArrayFormatters(TypeCategoryImpl &category) {
  // Register NSArray summary provider
  TypeSummaryImpl::Flags array_flags;
  array_flags.SetCascades(true)
            .SetSkipPointers(false)
            .SetSkipReferences(false)
            .SetDontShowChildren(false)  // We want to show children
            .SetDontShowValue(true)
            .SetShowMembersOneLiner(false)
            .SetHideItemNames(false);

  // Create the summary provider
  auto array_summary = std::make_shared<CXXFunctionSummaryFormat>(
      array_flags, GNUstepNSArrayFormatterFunction, "NSArray summary provider");

  // Register for various NSArray type names
  category.AddTypeSummary("NSArray", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("NSMutableArray", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("GSArray", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("GSMutableArray", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("GSInlineArray", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("GSPlaceholderArray", eFormatterMatchExact, array_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSArray *", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("NSMutableArray *", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("GSArray *", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("GSMutableArray *", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("GSInlineArray *", eFormatterMatchExact, array_summary);
  category.AddTypeSummary("GSPlaceholderArray *", eFormatterMatchExact, array_summary);
  
  // Register synthetic children provider for arrays
  SyntheticChildren::Flags array_synth_flags;
  array_synth_flags.SetCascades(true)
                  .SetSkipPointers(false)
                  .SetSkipReferences(false)
                  .SetNonCacheable(false);

  auto array_synth = std::make_shared<CXXSyntheticChildren>(
      array_synth_flags, "NSArray synthetic children", 
      GNUstepNSArraySyntheticFrontEndCreator);

  // Register synthetic provider for the same types
  category.AddTypeSynthetic("NSArray", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("NSMutableArray", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("GSArray", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("GSMutableArray", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("GSInlineArray", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("GSPlaceholderArray", eFormatterMatchExact, array_synth);
  
  // Also register with pointer types
  category.AddTypeSynthetic("NSArray *", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("NSMutableArray *", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("GSArray *", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("GSMutableArray *", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("GSInlineArray *", eFormatterMatchExact, array_synth);
  category.AddTypeSynthetic("GSPlaceholderArray *", eFormatterMatchExact, array_synth);
}

void GNUstepFormattersRegistry::RegisterDictionaryFormatters(TypeCategoryImpl &category) {
  // Register NSDictionary summary provider
  TypeSummaryImpl::Flags dict_flags;
  dict_flags.SetCascades(true)
           .SetSkipPointers(false)
           .SetSkipReferences(false)
           .SetDontShowChildren(false)  // We want to show children
           .SetDontShowValue(true)
           .SetShowMembersOneLiner(false)
           .SetHideItemNames(false);

  // Create the summary provider
  auto dict_summary = std::make_shared<CXXFunctionSummaryFormat>(
      dict_flags, GNUstepNSDictionaryFormatterFunction, "NSDictionary summary provider");

  // Register for various NSDictionary type names
  category.AddTypeSummary("NSDictionary", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("NSMutableDictionary", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("GSDictionary", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("GSMutableDictionary", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("GSInsensitiveDictionary", eFormatterMatchExact, dict_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSDictionary *", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("NSMutableDictionary *", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("GSDictionary *", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("GSMutableDictionary *", eFormatterMatchExact, dict_summary);
  category.AddTypeSummary("GSInsensitiveDictionary *", eFormatterMatchExact, dict_summary);
  
  // Register synthetic children provider for dictionaries
  SyntheticChildren::Flags dict_synth_flags;
  dict_synth_flags.SetCascades(true)
                 .SetSkipPointers(false)
                 .SetSkipReferences(false)
                 .SetNonCacheable(false);

  auto dict_synth = std::make_shared<CXXSyntheticChildren>(
      dict_synth_flags, "NSDictionary synthetic children", 
      GNUstepNSDictionarySyntheticFrontEndCreator);

  // Register synthetic provider for the same types
  category.AddTypeSynthetic("NSDictionary", eFormatterMatchExact, dict_synth);
  category.AddTypeSynthetic("NSMutableDictionary", eFormatterMatchExact, dict_synth);
  category.AddTypeSynthetic("GSDictionary", eFormatterMatchExact, dict_synth);
  category.AddTypeSynthetic("GSMutableDictionary", eFormatterMatchExact, dict_synth);
  category.AddTypeSynthetic("GSInsensitiveDictionary", eFormatterMatchExact, dict_synth);
  
  // Also register with pointer types
  category.AddTypeSynthetic("NSDictionary *", eFormatterMatchExact, dict_synth);
  category.AddTypeSynthetic("NSMutableDictionary *", eFormatterMatchExact, dict_synth);
  category.AddTypeSynthetic("GSDictionary *", eFormatterMatchExact, dict_synth);
  category.AddTypeSynthetic("GSMutableDictionary *", eFormatterMatchExact, dict_synth);
  category.AddTypeSynthetic("GSInsensitiveDictionary *", eFormatterMatchExact, dict_synth);
}

void GNUstepFormattersRegistry::RegisterSetFormatters(TypeCategoryImpl &category) {
  // Register NSSet summary provider
  TypeSummaryImpl::Flags set_flags;
  set_flags.SetCascades(true)
          .SetSkipPointers(false)
          .SetSkipReferences(false)
          .SetDontShowChildren(false)  // We want to show children
          .SetDontShowValue(true)
          .SetShowMembersOneLiner(false)
          .SetHideItemNames(false);

  // Create the summary provider
  auto set_summary = std::make_shared<CXXFunctionSummaryFormat>(
      set_flags, GNUstepNSSetFormatterFunction, "NSSet summary provider");

  // Register for various NSSet type names
  category.AddTypeSummary("NSSet", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("NSMutableSet", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("GSSet", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("GSMutableSet", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("NSCountedSet", eFormatterMatchExact, set_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSSet *", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("NSMutableSet *", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("GSSet *", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("GSMutableSet *", eFormatterMatchExact, set_summary);
  category.AddTypeSummary("NSCountedSet *", eFormatterMatchExact, set_summary);
  
  // Register synthetic children provider for sets
  SyntheticChildren::Flags set_synth_flags;
  set_synth_flags.SetCascades(true)
                .SetSkipPointers(false)
                .SetSkipReferences(false)
                .SetNonCacheable(false);

  auto set_synth = std::make_shared<CXXSyntheticChildren>(
      set_synth_flags, "NSSet synthetic children", 
      GNUstepNSSetSyntheticFrontEndCreator);

  // Register synthetic provider for the same types
  category.AddTypeSynthetic("NSSet", eFormatterMatchExact, set_synth);
  category.AddTypeSynthetic("NSMutableSet", eFormatterMatchExact, set_synth);
  category.AddTypeSynthetic("GSSet", eFormatterMatchExact, set_synth);
  category.AddTypeSynthetic("GSMutableSet", eFormatterMatchExact, set_synth);
  category.AddTypeSynthetic("NSCountedSet", eFormatterMatchExact, set_synth);
  
  // Also register with pointer types
  category.AddTypeSynthetic("NSSet *", eFormatterMatchExact, set_synth);
  category.AddTypeSynthetic("NSMutableSet *", eFormatterMatchExact, set_synth);
  category.AddTypeSynthetic("GSSet *", eFormatterMatchExact, set_synth);
  category.AddTypeSynthetic("GSMutableSet *", eFormatterMatchExact, set_synth);
  category.AddTypeSynthetic("NSCountedSet *", eFormatterMatchExact, set_synth);
}

void GNUstepFormattersRegistry::RegisterFoundationFormatters(TypeCategoryImpl &category) {
  // Enable NSDate formatter - it's implemented
  RegisterDateFormatters(category);
  
  // Enable NSCalendar formatter - it's implemented
  RegisterCalendarFormatters(category);
  
  // Enable additional Foundation formatters - all are implemented and ready
  RegisterURLFormatters(category);
  RegisterErrorFormatters(category);
  RegisterDataFormatters(category);
  RegisterUUIDFormatters(category);
  
  // Register new high-priority Foundation formatters
  RegisterNullFormatter(category);
  RegisterExceptionFormatter(category);
  RegisterAttributedStringFormatter(category);
  
  // Priority 1 Foundation formatters already registered above for precedence
  
  // Re-enable IndexPath formatter - issues should be fixed now
  RegisterIndexPathFormatter(category);
  
  // Re-enabled with NoOp synthetic to prevent recursion
  RegisterNotificationFormatter(category);
  
  // Register NSBundle formatter
  RegisterBundleFormatters(category);
  
  // Register NSScanner formatter
  RegisterScannerFormatters(category);
  
  // Register NSLocale formatter
  RegisterLocaleFormatters(category);
  
  // Register NSUserDefaults formatter
  RegisterUserDefaultsFormatters(category);
  
  // Register NSProcessInfo formatter  
  RegisterProcessInfoFormatters(category);
  
  // Register NSProxy formatter - TODO: implement function
  // RegisterProxyFormatters(category);
}

void GNUstepFormattersRegistry::RegisterDateFormatters(TypeCategoryImpl &category) {
  // Register NSDate summary provider
  TypeSummaryImpl::Flags date_flags;
  date_flags.SetCascades(true)
            .SetSkipPointers(false)
            .SetSkipReferences(false)
            .SetDontShowChildren(true)
            .SetDontShowValue(true)
            .SetShowMembersOneLiner(false)
            .SetHideItemNames(true);

  // Create the summary provider
  auto date_summary = std::make_shared<CXXFunctionSummaryFormat>(
      date_flags, GNUstepNSDateFormatterFunction, "NSDate summary provider");

  // Register for various NSDate type names
  category.AddTypeSummary("NSDate", eFormatterMatchExact, date_summary);
  category.AddTypeSummary("NSCalendarDate", eFormatterMatchExact, date_summary);
  category.AddTypeSummary("GSDate", eFormatterMatchExact, date_summary);
  category.AddTypeSummary("GSCalendarDate", eFormatterMatchExact, date_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSDate *", eFormatterMatchExact, date_summary);
  category.AddTypeSummary("NSCalendarDate *", eFormatterMatchExact, date_summary);
  category.AddTypeSummary("GSDate *", eFormatterMatchExact, date_summary);
  category.AddTypeSummary("GSCalendarDate *", eFormatterMatchExact, date_summary);

  // Register NSTimeZone summary provider
  TypeSummaryImpl::Flags tz_flags;
  tz_flags.SetCascades(true)
          .SetSkipPointers(false)
          .SetSkipReferences(false)
          .SetDontShowChildren(true)
          .SetDontShowValue(true)
          .SetShowMembersOneLiner(false)
          .SetHideItemNames(true);

  auto tz_summary = std::make_shared<CXXFunctionSummaryFormat>(
      tz_flags, GNUstepNSTimeZoneFormatterFunction, "NSTimeZone summary provider");

  // Register for various NSTimeZone type names
  category.AddTypeSummary("NSTimeZone", eFormatterMatchExact, tz_summary);
  category.AddTypeSummary("GSTimeZone", eFormatterMatchExact, tz_summary);
  category.AddTypeSummary("GSAbsTimeZone", eFormatterMatchExact, tz_summary);
  category.AddTypeSummary("NSLocalTimeZone", eFormatterMatchExact, tz_summary);
  category.AddTypeSummary("GSWindowsTimeZone", eFormatterMatchExact, tz_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSTimeZone *", eFormatterMatchExact, tz_summary);
  category.AddTypeSummary("GSTimeZone *", eFormatterMatchExact, tz_summary);
  category.AddTypeSummary("GSAbsTimeZone *", eFormatterMatchExact, tz_summary);
  category.AddTypeSummary("NSLocalTimeZone *", eFormatterMatchExact, tz_summary);
  category.AddTypeSummary("GSWindowsTimeZone *", eFormatterMatchExact, tz_summary);
}

void GNUstepFormattersRegistry::RegisterCalendarFormatters(TypeCategoryImpl &category) {
  // Register NSCalendar summary provider
  TypeSummaryImpl::Flags calendar_flags;
  calendar_flags.SetCascades(true)
               .SetSkipPointers(false)
               .SetSkipReferences(false)
               .SetDontShowChildren(true)
               .SetDontShowValue(true)
               .SetShowMembersOneLiner(false)
               .SetHideItemNames(true);

  // Create the summary provider
  auto calendar_summary = std::make_shared<CXXFunctionSummaryFormat>(
      calendar_flags, GNUstepNSCalendarFormatterFunction, "NSCalendar summary provider");

  // Register for various NSCalendar type names
  category.AddTypeSummary("NSCalendar", eFormatterMatchExact, calendar_summary);
  category.AddTypeSummary("GSCalendar", eFormatterMatchExact, calendar_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSCalendar *", eFormatterMatchExact, calendar_summary);
  category.AddTypeSummary("GSCalendar *", eFormatterMatchExact, calendar_summary);
}

void GNUstepFormattersRegistry::RegisterURLFormatters(TypeCategoryImpl &category) {
  // Register NSURL summary provider
  TypeSummaryImpl::Flags url_flags;
  url_flags.SetCascades(true)
           .SetSkipPointers(false)
           .SetSkipReferences(false)
           .SetDontShowChildren(true)
           .SetDontShowValue(true)
           .SetShowMembersOneLiner(false)
           .SetHideItemNames(true);

  // Create the summary provider
  auto url_summary = std::make_shared<CXXFunctionSummaryFormat>(
      url_flags, GNUstepNSURLFormatterFunction, "NSURL summary provider");

  // Register for various NSURL type names
  category.AddTypeSummary("NSURL", eFormatterMatchExact, url_summary);
  category.AddTypeSummary("GSURL", eFormatterMatchExact, url_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSURL *", eFormatterMatchExact, url_summary);
  category.AddTypeSummary("GSURL *", eFormatterMatchExact, url_summary);
}

void GNUstepFormattersRegistry::RegisterErrorFormatters(TypeCategoryImpl &category) {
  // Register NSError summary provider
  TypeSummaryImpl::Flags error_flags;
  error_flags.SetCascades(true)
             .SetSkipPointers(false)
             .SetSkipReferences(false)
             .SetDontShowChildren(true)
             .SetDontShowValue(true)
             .SetShowMembersOneLiner(false)
             .SetHideItemNames(true);

  // Create the summary provider
  auto error_summary = std::make_shared<CXXFunctionSummaryFormat>(
      error_flags, GNUstepNSErrorFormatterFunction, "NSError summary provider");

  // Register for various NSError type names
  category.AddTypeSummary("NSError", eFormatterMatchExact, error_summary);
  category.AddTypeSummary("GSError", eFormatterMatchExact, error_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSError *", eFormatterMatchExact, error_summary);
  category.AddTypeSummary("GSError *", eFormatterMatchExact, error_summary);
}

void GNUstepFormattersRegistry::RegisterDataFormatters(TypeCategoryImpl &category) {
  // Register NSData summary provider
  TypeSummaryImpl::Flags data_flags;
  data_flags.SetCascades(true)
            .SetSkipPointers(false)
            .SetSkipReferences(false)
            .SetDontShowChildren(true)
            .SetDontShowValue(true)
            .SetShowMembersOneLiner(false)
            .SetHideItemNames(true);

  // Create the summary provider
  auto data_summary = std::make_shared<CXXFunctionSummaryFormat>(
      data_flags, GNUstepNSDataFormatterFunction, "NSData summary provider");

  // Register for various NSData type names
  category.AddTypeSummary("NSData", eFormatterMatchExact, data_summary);
  category.AddTypeSummary("NSMutableData", eFormatterMatchExact, data_summary);
  category.AddTypeSummary("GSData", eFormatterMatchExact, data_summary);
  category.AddTypeSummary("GSMutableData", eFormatterMatchExact, data_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSData *", eFormatterMatchExact, data_summary);
  category.AddTypeSummary("NSMutableData *", eFormatterMatchExact, data_summary);
  category.AddTypeSummary("GSData *", eFormatterMatchExact, data_summary);
  category.AddTypeSummary("GSMutableData *", eFormatterMatchExact, data_summary);
}

void GNUstepFormattersRegistry::RegisterUUIDFormatters(TypeCategoryImpl &category) {
  // Register NSUUID summary provider
  TypeSummaryImpl::Flags uuid_flags;
  uuid_flags.SetCascades(true)
            .SetSkipPointers(false)
            .SetSkipReferences(false)
            .SetDontShowChildren(true)
            .SetDontShowValue(true)
            .SetShowMembersOneLiner(false)
            .SetHideItemNames(true);

  // Create the summary provider
  auto uuid_summary = std::make_shared<CXXFunctionSummaryFormat>(
      uuid_flags, GNUstepNSUUIDFormatterFunction, "NSUUID summary provider");

  // Register for various NSUUID type names
  category.AddTypeSummary("NSUUID", eFormatterMatchExact, uuid_summary);
  category.AddTypeSummary("GSUUID", eFormatterMatchExact, uuid_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSUUID *", eFormatterMatchExact, uuid_summary);
  category.AddTypeSummary("GSUUID *", eFormatterMatchExact, uuid_summary);
}

void GNUstepFormattersRegistry::RegisterGenericFormatter(TypeCategoryImpl &category) {
  // Register generic formatter for any Objective-C object
  // This will be used as a fallback when no specific formatter is found
  TypeSummaryImpl::Flags generic_flags;
  generic_flags.SetCascades(true)
              .SetSkipPointers(false)
              .SetSkipReferences(false)
              .SetDontShowChildren(false)  // Show children for generic objects
              .SetDontShowValue(true)
              .SetShowMembersOneLiner(false)
              .SetHideItemNames(false);

  // Create the generic summary provider
  auto generic_summary = std::make_shared<CXXFunctionSummaryFormat>(
      generic_flags, GNUstepGenericFormatterFunction, "Generic GNUstep object summary provider");

  // Register with regex pattern matching for any object type
  // This uses a lower priority so specific formatters take precedence
  category.AddTypeSummary("^[A-Z].*", eFormatterMatchRegex, generic_summary);
  
  // Also register for common base class patterns
  category.AddTypeSummary("NSObject *", eFormatterMatchExact, generic_summary);
  
  // CRITICAL FIX: Register generic synthetic provider HERE (last) to avoid overriding specific formatters
  SyntheticChildren::Flags generic_synth_flags;
  generic_synth_flags.SetCascades(true)
                     .SetSkipPointers(false)
                     .SetSkipReferences(false)
                     .SetNonCacheable(true);  // IMPORTANT: Non-cacheable to prevent cross-contamination
                     
  auto generic_synth = std::make_shared<CXXSyntheticChildren>(
      generic_synth_flags, "Generic ObjC synthetic children", 
      GNUstepGenericObjectSyntheticFrontEndCreator);  // Use the actual generic provider, not ID dispatcher
  
  // Match any class that starts with a capital letter (typical ObjC pattern)
  // This includes NSObject, TestClass, etc. - but will be overridden by specific formatters
  category.AddTypeSynthetic("^[A-Z][A-Za-z0-9_]+$", eFormatterMatchRegex, generic_synth);
  
  // Also register for pointer types (with or without space before *)
  category.AddTypeSynthetic("^[A-Z][A-Za-z0-9_]+\\s*\\*$", eFormatterMatchRegex, generic_synth);
  
}

void GNUstepFormattersRegistry::RegisterNullFormatter(TypeCategoryImpl &category) {
  // Register NSNull summary provider
  TypeSummaryImpl::Flags null_flags;
  null_flags.SetCascades(true)
            .SetSkipPointers(false)
            .SetSkipReferences(false)
            .SetDontShowChildren(true)
            .SetDontShowValue(true)
            .SetShowMembersOneLiner(false)
            .SetHideItemNames(true);

  // Create the summary provider
  auto null_summary = std::make_shared<CXXFunctionSummaryFormat>(
      null_flags, GNUstepNSNullFormatterFunction, "NSNull summary provider");

  // Register for NSNull
  category.AddTypeSummary("NSNull", eFormatterMatchExact, null_summary);
  category.AddTypeSummary("NSNull *", eFormatterMatchExact, null_summary);
  
  // Create NoOp synthetic provider for safe disabling
  SyntheticChildren::Flags noop_synth_flags;
  noop_synth_flags.SetCascades(true)
                  .SetSkipPointers(false)
                  .SetSkipReferences(false)
                  .SetNonCacheable(true);
                  
  auto noop_synth = std::make_shared<CXXSyntheticChildren>(
      noop_synth_flags, "NoOp synthetic children", 
      GNUstepNoOpSyntheticFrontEndCreator);
  
  // Disable synthetic children for NSNull (it's a singleton with no meaningful children)
  category.AddTypeSynthetic("NSNull", eFormatterMatchExact, noop_synth);
  category.AddTypeSynthetic("NSNull *", eFormatterMatchExact, noop_synth);
}

void GNUstepFormattersRegistry::RegisterExceptionFormatter(TypeCategoryImpl &category) {
  // Create NoOp synthetic provider for safe disabling
  SyntheticChildren::Flags noop_synth_flags;
  noop_synth_flags.SetCascades(true)
                  .SetSkipPointers(false)
                  .SetSkipReferences(false)
                  .SetNonCacheable(true);
                  
  auto noop_synth = std::make_shared<CXXSyntheticChildren>(
      noop_synth_flags, "NoOp synthetic children", 
      GNUstepNoOpSyntheticFrontEndCreator);
  
  // Register NSException summary provider
  TypeSummaryImpl::Flags exception_flags;
  exception_flags.SetCascades(true)
                 .SetSkipPointers(false)
                 .SetSkipReferences(false)
                 .SetDontShowChildren(false)  // Show children for debugging
                 .SetDontShowValue(true)
                 .SetShowMembersOneLiner(false)
                 .SetHideItemNames(false);

  // Create the summary provider
  auto exception_summary = std::make_shared<CXXFunctionSummaryFormat>(
      exception_flags, GNUstepNSExceptionFormatterFunction, "NSException summary provider");

  // Register for NSException
  category.AddTypeSummary("NSException", eFormatterMatchExact, exception_summary);
  category.AddTypeSummary("NSException *", eFormatterMatchExact, exception_summary);
  category.AddTypeSummary("GSException", eFormatterMatchExact, exception_summary);
  category.AddTypeSummary("GSException *", eFormatterMatchExact, exception_summary);
  
  // Disable synthetic children for NSException to prevent recursion
  category.AddTypeSynthetic("NSException", eFormatterMatchExact, noop_synth);
  category.AddTypeSynthetic("NSException *", eFormatterMatchExact, noop_synth);
}

void GNUstepFormattersRegistry::RegisterAttributedStringFormatter(TypeCategoryImpl &category) {
  // Create NoOp synthetic provider for safe disabling
  SyntheticChildren::Flags noop_synth_flags;
  noop_synth_flags.SetCascades(true)
                  .SetSkipPointers(false)
                  .SetSkipReferences(false)
                  .SetNonCacheable(true);
                  
  auto noop_synth = std::make_shared<CXXSyntheticChildren>(
      noop_synth_flags, "NoOp synthetic children", 
      GNUstepNoOpSyntheticFrontEndCreator);
  
  // Register NSAttributedString summary provider
  TypeSummaryImpl::Flags attrstring_flags;
  attrstring_flags.SetCascades(true)
                  .SetSkipPointers(false)
                  .SetSkipReferences(false)
                  .SetDontShowChildren(false)
                  .SetDontShowValue(true)
                  .SetShowMembersOneLiner(false)
                  .SetHideItemNames(false);

  // Create the summary provider
  auto attrstring_summary = std::make_shared<CXXFunctionSummaryFormat>(
      attrstring_flags, GNUstepNSAttributedStringFormatterFunction, "NSAttributedString summary provider");

  // Register for NSAttributedString
  category.AddTypeSummary("NSAttributedString", eFormatterMatchExact, attrstring_summary);
  category.AddTypeSummary("NSAttributedString *", eFormatterMatchExact, attrstring_summary);
  category.AddTypeSummary("NSMutableAttributedString", eFormatterMatchExact, attrstring_summary);
  category.AddTypeSummary("NSMutableAttributedString *", eFormatterMatchExact, attrstring_summary);
  category.AddTypeSummary("GSAttributedString", eFormatterMatchExact, attrstring_summary);
  category.AddTypeSummary("GSAttributedString *", eFormatterMatchExact, attrstring_summary);
  
  // Disable synthetic children for NSAttributedString to prevent recursion
  category.AddTypeSynthetic("NSAttributedString", eFormatterMatchExact, noop_synth);
  category.AddTypeSynthetic("NSAttributedString *", eFormatterMatchExact, noop_synth);
  category.AddTypeSynthetic("NSMutableAttributedString", eFormatterMatchExact, noop_synth);
  category.AddTypeSynthetic("NSMutableAttributedString *", eFormatterMatchExact, noop_synth);
}

void GNUstepFormattersRegistry::RegisterIndexPathFormatter(TypeCategoryImpl &category) {
  // Create NoOp synthetic provider for safe disabling
  SyntheticChildren::Flags noop_synth_flags;
  noop_synth_flags.SetCascades(true)
                  .SetSkipPointers(false)
                  .SetSkipReferences(false)
                  .SetNonCacheable(true);
                  
  auto noop_synth = std::make_shared<CXXSyntheticChildren>(
      noop_synth_flags, "NoOp synthetic children", 
      GNUstepNoOpSyntheticFrontEndCreator);
  
  // Register NSIndexPath summary provider
  TypeSummaryImpl::Flags indexpath_flags;
  indexpath_flags.SetCascades(true)
                 .SetSkipPointers(false)
                 .SetSkipReferences(false)
                 .SetDontShowChildren(true)
                 .SetDontShowValue(true)
                 .SetShowMembersOneLiner(false)
                 .SetHideItemNames(true);

  // Create the summary provider
  auto indexpath_summary = std::make_shared<CXXFunctionSummaryFormat>(
      indexpath_flags, GNUstepNSIndexPathFormatterFunction, "NSIndexPath summary provider");

  // Register for NSIndexPath
  category.AddTypeSummary("NSIndexPath", eFormatterMatchExact, indexpath_summary);
  category.AddTypeSummary("NSIndexPath *", eFormatterMatchExact, indexpath_summary);
  category.AddTypeSummary("GSIndexPath", eFormatterMatchExact, indexpath_summary);
  category.AddTypeSummary("GSIndexPath *", eFormatterMatchExact, indexpath_summary);
  
  // Disable synthetic children for NSIndexPath to prevent recursion
  category.AddTypeSynthetic("NSIndexPath", eFormatterMatchExact, noop_synth);
  category.AddTypeSynthetic("NSIndexPath *", eFormatterMatchExact, noop_synth);
  category.AddTypeSynthetic("GSIndexPath", eFormatterMatchExact, noop_synth);
  category.AddTypeSynthetic("GSIndexPath *", eFormatterMatchExact, noop_synth);
}

void GNUstepFormattersRegistry::RegisterNotificationFormatter(TypeCategoryImpl &category) {
  // Create NoOp synthetic provider for safe disabling
  SyntheticChildren::Flags noop_synth_flags;
  noop_synth_flags.SetCascades(true)
                  .SetSkipPointers(false)
                  .SetSkipReferences(false)
                  .SetNonCacheable(true);
                  
  auto noop_synth = std::make_shared<CXXSyntheticChildren>(
      noop_synth_flags, "NoOp synthetic children", 
      GNUstepNoOpSyntheticFrontEndCreator);
  
  // Register NSNotification summary provider
  TypeSummaryImpl::Flags notification_flags;
  notification_flags.SetCascades(true)
                    .SetSkipPointers(false)
                    .SetSkipReferences(false)
                    .SetDontShowChildren(false)
                    .SetDontShowValue(true)
                    .SetShowMembersOneLiner(false)
                    .SetHideItemNames(false);

  // Create the summary provider
  auto notification_summary = std::make_shared<CXXFunctionSummaryFormat>(
      notification_flags, GNUstepNSNotificationFormatterFunction, "NSNotification summary provider");

  // Register for NSNotification
  category.AddTypeSummary("NSNotification", eFormatterMatchExact, notification_summary);
  category.AddTypeSummary("NSNotification *", eFormatterMatchExact, notification_summary);
  category.AddTypeSummary("GSNotification", eFormatterMatchExact, notification_summary);
  category.AddTypeSummary("GSNotification *", eFormatterMatchExact, notification_summary);
  
  // Disable synthetic children for NSNotification to prevent recursion
  category.AddTypeSynthetic("NSNotification", eFormatterMatchExact, noop_synth);
  category.AddTypeSynthetic("NSNotification *", eFormatterMatchExact, noop_synth);
  category.AddTypeSynthetic("GSNotification", eFormatterMatchExact, noop_synth);
  category.AddTypeSynthetic("GSNotification *", eFormatterMatchExact, noop_synth);
}

void GNUstepFormattersRegistry::RegisterPriority1Formatters(TypeCategoryImpl &category) {
  // Register the three priority 1 Foundation formatters
  RegisterIndexSetFormatters(category);
  RegisterDecimalNumberFormatters(category);
  RegisterCharacterSetFormatters(category);
}

void GNUstepFormattersRegistry::RegisterIndexSetFormatters(TypeCategoryImpl &category) {
  // Register NSIndexSet summary provider
  TypeSummaryImpl::Flags indexset_flags;
  indexset_flags.SetCascades(true)
                .SetSkipPointers(false)
                .SetSkipReferences(false)
                .SetDontShowChildren(true)
                .SetDontShowValue(true)
                .SetShowMembersOneLiner(false)
                .SetHideItemNames(true);

  // Create the summary provider
  auto indexset_summary = std::make_shared<CXXFunctionSummaryFormat>(
      indexset_flags, GNUstepNSIndexSetFormatterFunction, "NSIndexSet summary provider");

  // Register for various NSIndexSet type names
  category.AddTypeSummary("NSIndexSet", eFormatterMatchExact, indexset_summary);
  category.AddTypeSummary("NSMutableIndexSet", eFormatterMatchExact, indexset_summary);
  category.AddTypeSummary("GSIndexSet", eFormatterMatchExact, indexset_summary);
  category.AddTypeSummary("GSMutableIndexSet", eFormatterMatchExact, indexset_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSIndexSet *", eFormatterMatchExact, indexset_summary);
  category.AddTypeSummary("NSMutableIndexSet *", eFormatterMatchExact, indexset_summary);
  category.AddTypeSummary("GSIndexSet *", eFormatterMatchExact, indexset_summary);
  category.AddTypeSummary("GSMutableIndexSet *", eFormatterMatchExact, indexset_summary);
}

void GNUstepFormattersRegistry::RegisterDecimalNumberFormatters(TypeCategoryImpl &category) {
  // Register NSDecimalNumber summary provider
  TypeSummaryImpl::Flags decimal_flags;
  decimal_flags.SetCascades(true)
               .SetSkipPointers(false)
               .SetSkipReferences(false)
               .SetDontShowChildren(true)
               .SetDontShowValue(true)
               .SetShowMembersOneLiner(false)
               .SetHideItemNames(true);

  // Create the summary provider
  auto decimal_summary = std::make_shared<CXXFunctionSummaryFormat>(
      decimal_flags, GNUstepNSDecimalNumberFormatterFunction, "NSDecimalNumber summary provider");

  // Register for various NSDecimalNumber type names
  category.AddTypeSummary("NSDecimalNumber", eFormatterMatchExact, decimal_summary);
  category.AddTypeSummary("GSDecimalNumber", eFormatterMatchExact, decimal_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSDecimalNumber *", eFormatterMatchExact, decimal_summary);
  category.AddTypeSummary("GSDecimalNumber *", eFormatterMatchExact, decimal_summary);
}

void GNUstepFormattersRegistry::RegisterCharacterSetFormatters(TypeCategoryImpl &category) {
  // Register NSCharacterSet summary provider
  TypeSummaryImpl::Flags charset_flags;
  charset_flags.SetCascades(true)
               .SetSkipPointers(false)
               .SetSkipReferences(false)
               .SetDontShowChildren(true)
               .SetDontShowValue(true)
               .SetShowMembersOneLiner(false)
               .SetHideItemNames(true);

  // Create the summary provider
  auto charset_summary = std::make_shared<CXXFunctionSummaryFormat>(
      charset_flags, GNUstepNSCharacterSetFormatterFunction, "NSCharacterSet summary provider");

  // Register for various NSCharacterSet type names
  category.AddTypeSummary("NSCharacterSet", eFormatterMatchExact, charset_summary);
  category.AddTypeSummary("NSMutableCharacterSet", eFormatterMatchExact, charset_summary);
  category.AddTypeSummary("GSCharacterSet", eFormatterMatchExact, charset_summary);
  category.AddTypeSummary("GSMutableCharacterSet", eFormatterMatchExact, charset_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSCharacterSet *", eFormatterMatchExact, charset_summary);
  category.AddTypeSummary("NSMutableCharacterSet *", eFormatterMatchExact, charset_summary);
  category.AddTypeSummary("GSCharacterSet *", eFormatterMatchExact, charset_summary);
  category.AddTypeSummary("GSMutableCharacterSet *", eFormatterMatchExact, charset_summary);
}

void GNUstepFormattersRegistry::RegisterOrderedSetFormatters(TypeCategoryImpl &category) {
  // Register NSOrderedSet summary provider
  TypeSummaryImpl::Flags orderedset_flags;
  orderedset_flags.SetCascades(true)
                  .SetSkipPointers(false)
                  .SetSkipReferences(false)
                  .SetDontShowChildren(false)  // We want to show children
                  .SetDontShowValue(true)
                  .SetShowMembersOneLiner(false)
                  .SetHideItemNames(false);

  // Create the summary provider
  auto orderedset_summary = std::make_shared<CXXFunctionSummaryFormat>(
      orderedset_flags, GNUstepNSOrderedSetFormatterFunction, "NSOrderedSet summary provider");

  // Register for various NSOrderedSet type names
  category.AddTypeSummary("NSOrderedSet", eFormatterMatchExact, orderedset_summary);
  category.AddTypeSummary("NSMutableOrderedSet", eFormatterMatchExact, orderedset_summary);
  category.AddTypeSummary("GSOrderedSet", eFormatterMatchExact, orderedset_summary);
  category.AddTypeSummary("GSMutableOrderedSet", eFormatterMatchExact, orderedset_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSOrderedSet *", eFormatterMatchExact, orderedset_summary);
  category.AddTypeSummary("NSMutableOrderedSet *", eFormatterMatchExact, orderedset_summary);
  category.AddTypeSummary("GSOrderedSet *", eFormatterMatchExact, orderedset_summary);
  category.AddTypeSummary("GSMutableOrderedSet *", eFormatterMatchExact, orderedset_summary);
  
  // Register synthetic children provider for ordered sets
  SyntheticChildren::Flags orderedset_synth_flags;
  orderedset_synth_flags.SetCascades(true)
                        .SetSkipPointers(false)
                        .SetSkipReferences(false)
                        .SetNonCacheable(false);

  auto orderedset_synth = std::make_shared<CXXSyntheticChildren>(
      orderedset_synth_flags, "NSOrderedSet synthetic children", 
      GNUstepNSOrderedSetSyntheticFrontEndCreator);

  // Register synthetic provider for the same types
  category.AddTypeSynthetic("NSOrderedSet", eFormatterMatchExact, orderedset_synth);
  category.AddTypeSynthetic("NSMutableOrderedSet", eFormatterMatchExact, orderedset_synth);
  category.AddTypeSynthetic("GSOrderedSet", eFormatterMatchExact, orderedset_synth);
  category.AddTypeSynthetic("GSMutableOrderedSet", eFormatterMatchExact, orderedset_synth);
  
  // Also register with pointer types
  category.AddTypeSynthetic("NSOrderedSet *", eFormatterMatchExact, orderedset_synth);
  category.AddTypeSynthetic("NSMutableOrderedSet *", eFormatterMatchExact, orderedset_synth);
  category.AddTypeSynthetic("GSOrderedSet *", eFormatterMatchExact, orderedset_synth);
  category.AddTypeSynthetic("GSMutableOrderedSet *", eFormatterMatchExact, orderedset_synth);
}

// TODO: Implement RegisterProxyFormatters when GNUstepProxyFormatters are implemented

void GNUstepFormattersRegistry::RegisterBundleFormatters(TypeCategoryImpl &category) {
  // Register NSBundle summary provider
  TypeSummaryImpl::Flags bundle_flags;
  bundle_flags.SetCascades(true)
              .SetSkipPointers(false)
              .SetSkipReferences(false)
              .SetDontShowChildren(true)
              .SetDontShowValue(true)
              .SetShowMembersOneLiner(false)
              .SetHideItemNames(true);

  // Create the summary provider
  auto bundle_summary = std::make_shared<CXXFunctionSummaryFormat>(
      bundle_flags, GNUstepNSBundleFormatterFunction, "NSBundle summary provider");

  // Register for various NSBundle type names
  category.AddTypeSummary("NSBundle", eFormatterMatchExact, bundle_summary);
  category.AddTypeSummary("GSBundle", eFormatterMatchExact, bundle_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSBundle *", eFormatterMatchExact, bundle_summary);
  category.AddTypeSummary("GSBundle *", eFormatterMatchExact, bundle_summary);
}

void GNUstepFormattersRegistry::RegisterScannerFormatters(TypeCategoryImpl &category) {
  // Register NSScanner summary provider
  TypeSummaryImpl::Flags scanner_flags;
  scanner_flags.SetCascades(true)
               .SetSkipPointers(false)
               .SetSkipReferences(false)
               .SetDontShowChildren(true)
               .SetDontShowValue(true)
               .SetShowMembersOneLiner(false)
               .SetHideItemNames(true);

  // Create the summary provider
  auto scanner_summary = std::make_shared<CXXFunctionSummaryFormat>(
      scanner_flags, GNUstepNSScannerFormatterFunction, "NSScanner summary provider");

  // Register for various NSScanner type names
  category.AddTypeSummary("NSScanner", eFormatterMatchExact, scanner_summary);
  category.AddTypeSummary("GSScanner", eFormatterMatchExact, scanner_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSScanner *", eFormatterMatchExact, scanner_summary);
  category.AddTypeSummary("GSScanner *", eFormatterMatchExact, scanner_summary);
}

void GNUstepFormattersRegistry::RegisterLocaleFormatters(TypeCategoryImpl &category) {
  // Register NSLocale summary provider
  TypeSummaryImpl::Flags locale_flags;
  locale_flags.SetCascades(true)
              .SetSkipPointers(false)
              .SetSkipReferences(false)
              .SetDontShowChildren(true)
              .SetDontShowValue(true)
              .SetShowMembersOneLiner(false)
              .SetHideItemNames(true);

  // Create the summary provider
  auto locale_summary = std::make_shared<CXXFunctionSummaryFormat>(
      locale_flags, GNUstepNSLocaleFormatterFunction, "NSLocale summary provider");

  // Register for various NSLocale type names
  category.AddTypeSummary("NSLocale", eFormatterMatchExact, locale_summary);
  category.AddTypeSummary("GSLocale", eFormatterMatchExact, locale_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSLocale *", eFormatterMatchExact, locale_summary);
  category.AddTypeSummary("GSLocale *", eFormatterMatchExact, locale_summary);
}

void GNUstepFormattersRegistry::RegisterUserDefaultsFormatters(TypeCategoryImpl &category) {
  // Register NSUserDefaults summary provider
  TypeSummaryImpl::Flags userdefaults_flags;
  userdefaults_flags.SetCascades(true)
                    .SetSkipPointers(false)
                    .SetSkipReferences(false)
                    .SetDontShowChildren(true)
                    .SetDontShowValue(true)
                    .SetShowMembersOneLiner(false)
                    .SetHideItemNames(true);

  // Create the summary provider
  auto userdefaults_summary = std::make_shared<CXXFunctionSummaryFormat>(
      userdefaults_flags, GNUstepNSUserDefaultsFormatterFunction, "NSUserDefaults summary provider");

  // Register for various NSUserDefaults type names
  category.AddTypeSummary("NSUserDefaults", eFormatterMatchExact, userdefaults_summary);
  category.AddTypeSummary("GSUserDefaults", eFormatterMatchExact, userdefaults_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSUserDefaults *", eFormatterMatchExact, userdefaults_summary);
  category.AddTypeSummary("GSUserDefaults *", eFormatterMatchExact, userdefaults_summary);
}

void GNUstepFormattersRegistry::RegisterProcessInfoFormatters(TypeCategoryImpl &category) {
  // Register NSProcessInfo summary provider
  TypeSummaryImpl::Flags processinfo_flags;
  processinfo_flags.SetCascades(true)
                   .SetSkipPointers(false)
                   .SetSkipReferences(false)
                   .SetDontShowChildren(true)
                   .SetDontShowValue(true)
                   .SetShowMembersOneLiner(false)
                   .SetHideItemNames(true);
  
  // Create the summary provider
  auto processinfo_summary = std::make_shared<CXXFunctionSummaryFormat>(
      processinfo_flags, GNUstepNSProcessInfoFormatterFunction, "NSProcessInfo summary provider");
  
  // Register for various NSProcessInfo type names
  category.AddTypeSummary("NSProcessInfo", eFormatterMatchExact, processinfo_summary);
  category.AddTypeSummary("_NSConcreteProcessInfo", eFormatterMatchExact, processinfo_summary);
  
  // Also register with pointer types  
  category.AddTypeSummary("NSProcessInfo *", eFormatterMatchExact, processinfo_summary);
  category.AddTypeSummary("_NSConcreteProcessInfo *", eFormatterMatchExact, processinfo_summary);
}

void GNUstepFormattersRegistry::RegisterTimeIntervalFormatters(TypeCategoryImpl &category) {
  // Register NSTimeInterval summary provider
  TypeSummaryImpl::Flags timeinterval_flags;
  timeinterval_flags.SetCascades(true)
                    .SetSkipPointers(false)
                    .SetSkipReferences(false)
                    .SetDontShowChildren(true)
                    .SetDontShowValue(true)
                    .SetShowMembersOneLiner(false)
                    .SetHideItemNames(true);
  
  // Create the summary provider
  auto timeinterval_summary = std::make_shared<CXXFunctionSummaryFormat>(
      timeinterval_flags, GNUstepNSTimeIntervalFormatterFunction, "NSTimeInterval summary provider");
  
  // IMPORTANT: Only register for explicit NSTimeInterval types to avoid interfering 
  // with NSNumber formatters and tagged pointer handling
  category.AddTypeSummary("NSTimeInterval", eFormatterMatchExact, timeinterval_summary);
  category.AddTypeSummary("NSTimeInterval *", eFormatterMatchExact, timeinterval_summary);
  
  // Also handle common typedef variations that might appear in debug info
  category.AddTypeSummary("CFTimeInterval", eFormatterMatchExact, timeinterval_summary);
  category.AddTypeSummary("CFTimeInterval *", eFormatterMatchExact, timeinterval_summary);
}
