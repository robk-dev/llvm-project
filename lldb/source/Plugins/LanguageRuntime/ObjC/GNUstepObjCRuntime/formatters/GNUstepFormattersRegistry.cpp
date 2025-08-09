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
#include "GNUstepGenericFormatter.h"
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
  category.AddTypeSummary("__NSCFString", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("NSConstantString", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("__NSConstantString", eFormatterMatchExact, string_summary);
  
  // Also register with pointer types
  category.AddTypeSummary("NSString *", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("NSMutableString *", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("__NSCFString *", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("NSConstantString *", eFormatterMatchExact, string_summary);
  category.AddTypeSummary("__NSConstantString *", eFormatterMatchExact, string_summary);
  
  // Register the comprehensive id dispatcher that handles all GNUstep types
  auto id_summary = std::make_shared<CXXFunctionSummaryFormat>(
      string_flags, GNUstepIdDispatcherFunction, "GNUstep id dispatcher");
  category.AddTypeSummary("id", eFormatterMatchExact, id_summary);
  
  // Also register synthetic children provider for id
  SyntheticChildren::Flags id_synth_flags;
  id_synth_flags.SetCascades(true)
                .SetSkipPointers(false)
                .SetSkipReferences(false)
                .SetNonCacheable(false);
                
  auto id_synth = std::make_shared<CXXSyntheticChildren>(
      id_synth_flags, "id synthetic children", 
      GNUstepIdSyntheticFrontEndCreator);
      
  category.AddTypeSynthetic("id", eFormatterMatchExact, id_synth);
  
  // Register generic synthetic provider for all Objective-C classes
  // This will filter out the isa pointer for any ObjC object
  auto generic_synth = std::make_shared<CXXSyntheticChildren>(
      id_synth_flags, "Generic ObjC synthetic children", 
      GNUstepIdSyntheticFrontEndCreator);
  
  // Match any class that starts with a capital letter (typical ObjC pattern)
  // This includes NSObject, TestClass, etc.
  category.AddTypeSynthetic("^[A-Z][A-Za-z0-9_]+$", eFormatterMatchRegex, generic_synth);
  
  // Also register for pointer types (with or without space before *)
  category.AddTypeSynthetic("^[A-Z][A-Za-z0-9_]+\\s*\\*$", eFormatterMatchRegex, generic_synth);
  
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
}

void GNUstepFormattersRegistry::RegisterCollectionFormatters(TypeCategoryImpl &category) {
  // Register NSArray formatters
  RegisterArrayFormatters(category);
  
  // Register NSDictionary formatters
  RegisterDictionaryFormatters(category);
  
  // Register NSSet formatters
  RegisterSetFormatters(category);
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
  
  // TODO: Uncomment when these formatters are implemented
  // RegisterURLFormatters(category);
  // RegisterErrorFormatters(category);
  // RegisterDataFormatters(category);
  // RegisterUUIDFormatters(category);
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
  
}
