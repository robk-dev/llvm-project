//===-- GNUstepIdDispatcher.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

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
#include "GNUstepJSONSerializationFormatters.h"
#include "GNUstepGenericFormatter.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "lldb/Utility/Log.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

// Helper function to check if address is a tagged pointer
static bool IsTaggedPointer(lldb::addr_t addr) {
  // GNUstep uses the lower 3 bits for tagging
  // Any of the low 3 bits set = small object (tagged pointer)
  return (addr & 0x7) != 0;
}

// Helper function to decode tagged pointer type
static TaggedPointerType GetTaggedPointerType(lldb::addr_t addr) {
  if (!IsTaggedPointer(addr)) {
    return TaggedPointerType::Unknown;
  }
  
  // Check the tag bits (lower 3 bits)
  uint8_t tag = addr & 0x7;
  
  switch (tag) {
    case 1:  // NSSmallInt
      return TaggedPointerType::NSSmallInt;
    case 4:  // NSSmallString - CRITICAL FIX: Added missing tag 4 handling
      return TaggedPointerType::NSSmallString;
    case 5:  // NSSmallFloat 
      return TaggedPointerType::NSSmallFloat;
    case 2:  // NSSmallExtendedDouble
      return TaggedPointerType::NSSmallExtendedDouble;
    case 3:  // NSSmallRepeatingDouble
      return TaggedPointerType::NSSmallRepeatingDouble;
    default:
      return TaggedPointerType::Unknown;
  }
}

bool lldb_private::formatters::GNUstepIdDispatcherFunction(ValueObject &valobj, Stream &stream, 
                                                           const TypeSummaryOptions &options) {
  // Get the address of the object
  lldb::addr_t obj_addr = valobj.GetValueAsUnsigned(0);
  
  // Check for nil
  if (obj_addr == 0) {
    stream.PutCString("nil");
    return true;
  }
  
  // CRITICAL FIX: Enhanced tagged pointer handling for LLDB po commands
  // Check if it's a tagged pointer first - this is crucial for NSNumber @42 etc.
  if (IsTaggedPointer(obj_addr)) {
    TaggedPointerType tag_type = GetTaggedPointerType(obj_addr);
    
    // Handle tagged pointers directly with proper formatting
    switch (tag_type) {
      case TaggedPointerType::NSSmallInt: {
        // Decode small int value (shift right by 3 bits)
        int64_t value = ((int64_t)obj_addr) >> 3;
        stream.Printf("%lld", value);
        return true;
      }
      case TaggedPointerType::NSSmallString: {
        // CRITICAL FIX: Handle tagged strings properly
        return GNUstepNSStringFormatterFunction(valobj, stream, options);
      }
      case TaggedPointerType::NSSmallFloat: {
        // Use the full NSNumber formatter for proper float handling
        return GNUstepNSNumberFormatterFunction(valobj, stream, options);
      }
      case TaggedPointerType::NSSmallExtendedDouble:
      case TaggedPointerType::NSSmallRepeatingDouble: {
        // Use the full NSNumber formatter for proper double handling
        return GNUstepNSNumberFormatterFunction(valobj, stream, options);
      }
      default:
        // For unknown tagged pointer types, DO NOT try NSNumber formatter
        // This was causing the "tagged_number:tag=X" fallback display
        // Instead, return false to let LLDB handle it
        return false;
    }
  }
  
  // For regular objects, we need to get the runtime class name
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    return false; // Let LLDB handle it with default formatting
  }
  
  // Get the actual runtime class name
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  Log *log = GetLog(LLDBLog::DataFormatters);
  if (log) {
    log->Printf("GNUstepIdDispatcher: Object at 0x%llx has runtime class: %s", 
                obj_addr, class_name.c_str());
  }
  
  // Dispatch to the appropriate formatter based on runtime type
  // Check for NSString and variants - ENHANCED for better NSConstantString detection
  if (class_name == "NSConstantString" || 
      class_name == "__NSConstantString" ||
      class_name == "NSCFConstantString" ||
      class_name.find("NSString") != std::string::npos ||
      class_name.find("NSCFString") != std::string::npos ||
      class_name.find("NSMutableString") != std::string::npos ||
      class_name.find("GSString") != std::string::npos ||
      class_name.find("GSCString") != std::string::npos ||
      class_name.find("GSMutableString") != std::string::npos ||
      (class_name.find("String") != std::string::npos && 
       (class_name.find("Constant") != std::string::npos || 
        class_name.find("Immutable") != std::string::npos))) {
    return GNUstepNSStringFormatterFunction(valobj, stream, options);
  }
  
  // Check for NSNumber and variants
  if (class_name.find("Number") != std::string::npos ||
      class_name.find("IntNumber") != std::string::npos ||
      class_name.find("BoolNumber") != std::string::npos ||
      class_name.find("FloatNumber") != std::string::npos ||
      class_name.find("DoubleNumber") != std::string::npos) {
    return GNUstepNSNumberFormatterFunction(valobj, stream, options);
  }
  
  // Check for NSArray and variants
  if (class_name.find("Array") != std::string::npos && 
      class_name.find("ByteArray") == std::string::npos) { // Exclude NSData variants
    return GNUstepNSArrayFormatterFunction(valobj, stream, options);
  }
  
  // Check for NSDictionary and variants
  if (class_name.find("Dictionary") != std::string::npos) {
    return GNUstepNSDictionaryFormatterFunction(valobj, stream, options);
  }
  
  // Check for NSSet and variants
  if (class_name.find("Set") != std::string::npos &&
      class_name.find("IndexSet") == std::string::npos) { // Exclude NSIndexSet
    return GNUstepNSSetFormatterFunction(valobj, stream, options);
  }
  
  // Check for NSDate and variants
  if (class_name.find("Date") != std::string::npos &&
      class_name.find("DateFormatter") == std::string::npos) { // Exclude formatter classes
    return GNUstepNSDateFormatterFunction(valobj, stream, options);
  }
  
  // Check for NSURL and variants
  if (class_name.find("URL") != std::string::npos &&
      class_name.find("URLRequest") == std::string::npos &&
      class_name.find("URLResponse") == std::string::npos) { // Exclude related classes
    return GNUstepNSURLFormatterFunction(valobj, stream, options);
  }
  
  // Check for NSError and variants
  if (class_name.find("Error") != std::string::npos) {
    return GNUstepNSErrorFormatterFunction(valobj, stream, options);
  }
  
  // Check for NSData and variants
  if (class_name.find("Data") != std::string::npos &&
      class_name.find("Date") == std::string::npos) { // Exclude NSDate
    return GNUstepNSDataFormatterFunction(valobj, stream, options);
  }
  
  // Check for NSUUID and variants
  if (class_name.find("UUID") != std::string::npos) {
    return GNUstepNSUUIDFormatterFunction(valobj, stream, options);
  }
  
  // Check for NSJSONSerialization and JSON-related classes
  if (class_name.find("JSON") != std::string::npos) {
    return GNUstepNSJSONSerializationFormatterFunction(valobj, stream, options);
  }
  
  // Check for NSValue (including NSNumber which inherits from NSValue)
  if (class_name.find("Value") != std::string::npos) {
    // Try NSNumber formatter first (it will reject non-number values)
    if (GNUstepNSNumberFormatterFunction(valobj, stream, options)) {
      return true;
    }
    // Fall through to generic if not a number
  }
  
  // For custom classes and other Objective-C objects, use the generic formatter
  // This provides a reasonable default display
  // Fixed crash issues in generic formatter - now safe to enable
  // But first check if this type has a specialized formatter
  if (class_name == "NSException" || class_name == "NSIndexPath" ||
      class_name == "NSNotification" || class_name == "NSAttributedString" ||
      class_name == "GSException" || class_name == "GSIndexPath" ||
      class_name == "GSNotification" || class_name == "GSAttributedString" ||
      class_name == "NSNull" || class_name == "NSDecimalNumber" ||
      class_name == "NSIndexSet" || class_name == "NSCharacterSet") {
    // These types should use their specialized formatters instead
    return false;
  }
  
  if (class_name[0] >= 'A' && class_name[0] <= 'Z') { // Likely an Objective-C class
    return GNUstepGenericFormatterFunction(valobj, stream, options);
  }
  
  // For anything else, let LLDB handle with default formatting
  return false;
}

// Synthetic children dispatcher for id types
SyntheticChildrenFrontEnd *lldb_private::formatters::GNUstepIdSyntheticFrontEndCreator(CXXSyntheticChildren *synth, 
                                                             lldb::ValueObjectSP valobj_sp) {
  if (!valobj_sp) {
    return nullptr;
  }
  
  // Get the runtime class name
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(*valobj_sp);
  
  // Dispatch to the appropriate synthetic provider
  if (class_name.find("Array") != std::string::npos && 
      class_name.find("ByteArray") == std::string::npos) {
    return GNUstepNSArraySyntheticFrontEndCreator(synth, valobj_sp);
  }
  
  if (class_name.find("Dictionary") != std::string::npos) {
    return GNUstepNSDictionarySyntheticFrontEndCreator(synth, valobj_sp);
  }
  
  if (class_name.find("Set") != std::string::npos &&
      class_name.find("IndexSet") == std::string::npos) {
    return GNUstepNSSetSyntheticFrontEndCreator(synth, valobj_sp);
  }
  
  // NSNumber, NSString, NSDate and other value types should NOT have synthetic children
  // They are simple value objects and showing internal structure is confusing
  if (class_name.find("Number") != std::string::npos ||
      class_name.find("String") != std::string::npos ||
      class_name.find("Date") != std::string::npos ||
      class_name.find("URL") != std::string::npos ||
      class_name.find("Data") != std::string::npos ||
      class_name.find("UUID") != std::string::npos ||
      class_name.find("Error") != std::string::npos) {
    // Return nullptr to indicate no synthetic children should be shown
    return nullptr;
  }
  
  // For other Objective-C objects, use the generic synthetic provider
  // This filters out the isa pointer and shows only user-defined ivars
  if (!class_name.empty() && class_name[0] >= 'A' && class_name[0] <= 'Z') {
    return GNUstepGenericObjectSyntheticFrontEndCreator(synth, valobj_sp);
  }
  
  // For non-objects (e.g., primitive types), no synthetic children
  return nullptr;
}