//===-- GNUstepFormatterRegistration.cpp ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../GNUstepFormatterRegistration.h"

// String formatters
#include "../GNUstepStringSummaryProvider.h"
#include "../GNUstepNSString.h"

// Number formatters  
#include "../GNUstepNumberSummaryProvider.h"

// Date formatters
#include "../GNUstepNSDate.h"

// Collection formatters
#include "../GNUstepArraySyntheticProvider.h"
#include "../GNUstepArraySummaryProvider.h"
#include "../GNUstepNSArray.h"
#include "../GNUstepNSSet.h"
#include "../GNUstepNSDictionary.h"

// Universal formatters
#include "../GNUstepUniversalProvider.h"
#include "../GNUstepCustomClass.h"

#include "lldb/DataFormatters/DataVisualization.h"
#include "lldb/DataFormatters/FormatManager.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/DataFormatters/TypeSynthetic.h"
#include "lldb/Utility/ConstString.h"

using namespace lldb;
using namespace lldb_private;

namespace lldb_private {
namespace formatters {

void RegisterGNUstepStringFormatters(lldb::TypeCategoryImplSP objc_category, Log *log) {
  LLDB_LOG(log, "GNUstepFormatterRegistration: Registering NSString formatters");
  
  // Register NSString summary provider to show actual string content
  TypeSummaryImplSP string_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      GNUstepNSStringSummaryProvider,
      "NSString summary"));
  
  if (string_summary_sp) {
    // Register for NSString and related classes
    std::vector<std::string> string_types = {
      "NSString",
      "NSString *",
      "NSConstantString", 
      "NSConstantString *",
      "NSMutableString",
      "NSMutableString *",
      "GSTinyString",
      "GSTinyString *",
      "GSPlaceholderString", 
      "GSPlaceholderString *",
      "GSCString",
      "GSCString *",
      "GSUnicodeString",
      "GSUnicodeString *",
      "id"  // Re-added: needed for array elements to get formatted
    };
    
    for (const std::string &type_name : string_types) {
      objc_category->AddTypeSummary(ConstString(type_name.c_str()), 
                                   lldb::eFormatterMatchExact, 
                                   string_summary_sp);
      LLDB_LOG(log, "GNUstepFormatterRegistration: Registered NSString summary provider for type: {0}", type_name);
    }
  }
}

void RegisterGNUstepNumberFormatters(lldb::TypeCategoryImplSP objc_category, Log *log) {
  LLDB_LOG(log, "GNUstepFormatterRegistration: Registering NSNumber formatters");
  
  // Register NSNumber summary provider to show actual numeric values
  TypeSummaryImplSP number_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      GNUstepNumberSummaryProvider::FormatObject,
      "NSNumber summary"));
  
  if (number_summary_sp) {
    // Register for NSNumber and related classes
    std::vector<std::string> number_types = {
      "NSNumber",
      "NSNumber *", 
      "NSDecimalNumber",
      "NSDecimalNumber *"
    };
    
    for (const std::string &type_name : number_types) {
      objc_category->AddTypeSummary(ConstString(type_name.c_str()), 
                                   lldb::eFormatterMatchExact, 
                                   number_summary_sp);
      LLDB_LOG(log, "GNUstepFormatterRegistration: Registered NSNumber summary provider for type: {0}", type_name);
    }
  }
}

void RegisterGNUstepDateFormatters(lldb::TypeCategoryImplSP objc_category, Log *log) {
  LLDB_LOG(log, "GNUstepFormatterRegistration: Registering NSDate formatters");
  
  // Register NSDate summary provider to show actual date/time values
  TypeSummaryImplSP date_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      GNUstepNSDateSummaryProvider,
      "NSDate summary"));
  
  if (date_summary_sp) {
    // Register for NSDate and related classes
    std::vector<std::string> date_types = {
      "NSDate",
      "NSDate *",
      "NSCalendarDate", 
      "NSCalendarDate *"
      // DO NOT register for "id" - it overrides all other providers!
    };
    
    for (const std::string &type_name : date_types) {
      objc_category->AddTypeSummary(ConstString(type_name.c_str()), 
                                   lldb::eFormatterMatchExact, 
                                   date_summary_sp);
      LLDB_LOG(log, "GNUstepFormatterRegistration: Registered NSDate summary provider for type: {0}", type_name);
    }
  }
}

void RegisterGNUstepCollectionFormatters(lldb::TypeCategoryImplSP objc_category, Log *log) {
  LLDB_LOG(log, "GNUstepFormatterRegistration: Registering collection formatters");
  
  // NSSet formatters
  SyntheticChildrenSP set_synth_sp(new CXXSyntheticChildren(
      SyntheticChildren::Flags().SetCascades(true)
                                .SetSkipPointers(false)
                                .SetSkipReferences(false),
      "NSSet synthetic children",
      GNUstepNSSetSyntheticFrontEndCreator));

  if (set_synth_sp) {
    std::vector<std::string> set_types = {
      "NSSet", "NSSet *", "NSMutableSet", "NSMutableSet *",
      "GSSet", "GSSet *", "GSMutableSet", "GSMutableSet *",
      "__NSSetI", "__NSSetI *", "__NSSetM", "__NSSetM *"
    };
    
    for (const std::string &type_name : set_types) {
      objc_category->AddTypeSynthetic(ConstString(type_name.c_str()),
                                     lldb::eFormatterMatchExact,
                                     set_synth_sp);
      LLDB_LOG(log, "GNUstepFormatterRegistration: Registered NSSet synthetic provider for type: {0}", type_name);
    }
  }

  TypeSummaryImplSP set_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      GNUstepNSSetSummaryProvider,
      "NSSet summary"));
  
  if (set_summary_sp) {
    std::vector<std::string> set_types = {
      "NSSet", "NSSet *", "NSMutableSet", "NSMutableSet *",
      "GSSet", "GSSet *", "GSMutableSet", "GSMutableSet *",
      "__NSSetI", "__NSSetI *", "__NSSetM", "__NSSetM *"
    };
    
    for (const std::string &type_name : set_types) {
      objc_category->AddTypeSummary(ConstString(type_name.c_str()), 
                                   lldb::eFormatterMatchExact, 
                                   set_summary_sp);
      LLDB_LOG(log, "GNUstepFormatterRegistration: Registered NSSet summary provider for type: {0}", type_name);
    }
  }

  // NSDictionary formatters
  SyntheticChildrenSP dict_synth_sp(new CXXSyntheticChildren(
      SyntheticChildren::Flags().SetCascades(true)
                                .SetSkipPointers(false)
                                .SetSkipReferences(false),
      "NSDictionary synthetic children",
      GNUstepNSDictionarySyntheticFrontEndCreator));

  if (dict_synth_sp) {
    std::vector<std::string> dict_types = {
      "NSDictionary", "NSDictionary *", "NSMutableDictionary", "NSMutableDictionary *",
      "GSDictionary", "GSDictionary *", "GSMutableDictionary", "GSMutableDictionary *",
      "__NSDictionaryI", "__NSDictionaryI *", "__NSDictionaryM", "__NSDictionaryM *"
    };
    
    for (const std::string &type_name : dict_types) {
      objc_category->AddTypeSynthetic(ConstString(type_name.c_str()),
                                     lldb::eFormatterMatchExact,
                                     dict_synth_sp);
      LLDB_LOG(log, "GNUstepFormatterRegistration: Registered NSDictionary synthetic provider for type: {0}", type_name);
    }
  }

  TypeSummaryImplSP dict_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      GNUstepNSDictionarySummaryProvider,
      "NSDictionary summary"));
  
  if (dict_summary_sp) {
    std::vector<std::string> dict_types = {
      "NSDictionary", "NSDictionary *", "NSMutableDictionary", "NSMutableDictionary *",
      "GSDictionary", "GSDictionary *", "GSMutableDictionary", "GSMutableDictionary *",
      "__NSDictionaryI", "__NSDictionaryI *", "__NSDictionaryM", "__NSDictionaryM *"
    };
    
    for (const std::string &type_name : dict_types) {
      objc_category->AddTypeSummary(ConstString(type_name.c_str()), 
                                   lldb::eFormatterMatchExact, 
                                   dict_summary_sp);
      LLDB_LOG(log, "GNUstepFormatterRegistration: Registered NSDictionary summary provider for type: {0}", type_name);
    }
  }

  // NSArray formatters
  TypeSummaryImplSP array_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      GNUstepNSArraySummaryProvider,
      "NSArray summary"));
  
  if (array_summary_sp) {
    std::vector<std::string> array_types = {
      "NSArray", "NSArray *", "NSMutableArray", "NSMutableArray *",
      "GSInlineArray", "GSInlineArray *", "GSMutableArray", "GSMutableArray *",
      "GSArray", "GSArray *", "GSArray0", "GSArray0 *", "GSArray1", "GSArray1 *",
      "__NSArrayI", "__NSArrayI *", "__NSArrayM", "__NSArrayM *"
    };
    
    for (const std::string &type_name : array_types) {
      objc_category->AddTypeSummary(ConstString(type_name.c_str()), 
                                   lldb::eFormatterMatchExact, 
                                   array_summary_sp);
      LLDB_LOG(log, "GNUstepFormatterRegistration: Registered NSArray summary provider for type: {0}", type_name);
    }
  }

  SyntheticChildrenSP array_synth_sp(new CXXSyntheticChildren(
      SyntheticChildren::Flags().SetCascades(true)
                                .SetSkipPointers(false)
                                .SetSkipReferences(false),
      "NSArray synthetic children",
      [](CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp) -> SyntheticChildrenFrontEnd * {
        if (!valobj_sp)
          return nullptr;
        return new GNUstepNSArraySyntheticProvider(valobj_sp);
      }));

  if (array_synth_sp) {
    std::vector<std::string> array_types = {
      "NSArray", "NSArray *", "NSMutableArray", "NSMutableArray *",
      "GSInlineArray", "GSInlineArray *", "GSMutableArray", "GSMutableArray *",
      "GSArray0", "GSArray0 *", "GSArray1", "GSArray1 *",
      "__NSArrayI", "__NSArrayI *", "__NSArrayM", "__NSArrayM *"
    };
    
    for (const std::string &type_name : array_types) {
      objc_category->AddTypeSynthetic(ConstString(type_name.c_str()),
                                     lldb::eFormatterMatchExact,
                                     array_synth_sp);
      LLDB_LOG(log, "GNUstepFormatterRegistration: Registered NSArray synthetic provider for type: {0}", type_name);
    }
  }
}

void RegisterGNUstepUniversalFormatters(lldb::TypeCategoryImplSP objc_category, Log *log) {
  LLDB_LOG(log, "GNUstepFormatterRegistration: Registering universal formatters");
  
  // Register universal summary provider first (for any unknown Objective-C object)
  TypeSummaryImplSP universal_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(false)   // LOW priority - don't override specific providers
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      GNUstepUniversalSummaryProvider,
      "Universal summary for unknown classes"));
  
  if (universal_summary_sp) {
    // Register for any class not handled by specific providers (lowest priority)
    std::vector<std::string> universal_patterns = {
      // Catch any custom class that doesn't match Foundation patterns
      "^(?!NS(Array|Dictionary|Set|String|Number|Date|Object)|GS(Array|Dictionary|Set|String|Number|Date))[A-Z][a-zA-Z0-9_]+$",
      "^(?!NS(Array|Dictionary|Set|String|Number|Date|Object)|GS(Array|Dictionary|Set|String|Number|Date))[A-Z][a-zA-Z0-9_]+ \\*$"
    };
    
    for (const std::string &pattern : universal_patterns) {
      objc_category->AddTypeSummary(ConstString(pattern.c_str()),
                                   lldb::eFormatterMatchRegex,
                                   universal_summary_sp);
      LLDB_LOG(log, "GNUstepFormatterRegistration: Registered universal summary provider for pattern: {0}", pattern);
    }
  }
  
  // Register universal synthetic children provider for custom objects
  SyntheticChildrenSP synth_sp(new CXXSyntheticChildren(
      SyntheticChildren::Flags()
          .SetCascades(false)     // LOW priority - don't override specific providers
          .SetSkipPointers(false)
          .SetSkipReferences(false),
      "GNUstep Universal Synthetic Provider for custom objects",
      GNUstepUniversalProviderCreator));
  
  if (synth_sp) {
    // Register for custom classes that don't match Foundation patterns
    std::vector<std::string> custom_class_patterns = {
      // Exclude Foundation and GNUstep internal classes to avoid conflicts
      "^(?!NS(Array|Dictionary|Set|String|Number|Date|Object)|GS(Array|Dictionary|Set|String|Number|Date))[A-Z][a-zA-Z0-9_]+$",
      "^(?!NS(Array|Dictionary|Set|String|Number|Date|Object)|GS(Array|Dictionary|Set|String|Number|Date))[A-Z][a-zA-Z0-9_]+ \\*$"
    };
    
    // Register the provider for each custom object type pattern
    for (const std::string &pattern : custom_class_patterns) {
      objc_category->AddTypeSynthetic(ConstString(pattern.c_str()),
                                     lldb::eFormatterMatchRegex,
                                     synth_sp);
      LLDB_LOG(log, "GNUstepFormatterRegistration: Registered GNUstepUniversalSyntheticProvider for pattern: {0}", pattern);
    }
  }
  
  // Keep the existing custom class summary provider for backward compatibility
  TypeSummaryImplSP custom_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)   // HIGH priority - allow cascading
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      GNUstepCustomClassSummaryProvider,
      "Custom Class summary"));
  
  if (custom_summary_sp) {
    // Register using regex pattern for custom classes (backward compatibility)
    std::vector<std::string> legacy_patterns = {
      "^BankAccount$",        // Specific class for testing
      "^BankAccount \\*$"     // Pointer variant
    };
    
    for (const std::string &pattern : legacy_patterns) {
      objc_category->AddTypeSummary(ConstString(pattern.c_str()),
                                   lldb::eFormatterMatchRegex,
                                   custom_summary_sp);
      LLDB_LOG(log, "GNUstepFormatterRegistration: Registered legacy Custom Class summary provider for pattern: {0}", pattern);
    }
  }
}

} // namespace formatters  
} // namespace lldb_private