//===-- GNUstepNumberFormatters.cpp -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepNumberFormatters.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Utility/Status.h"
#include "../../ObjCLanguageRuntime.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSNumberSummaryProvider::FormatObject(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  printf("[GNUstepNSNumber] FormatObject called\n");
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;

  // Get the number object pointer
  addr_t number_ptr = valobj.GetPointerValue();
  if (number_ptr == 0 || number_ptr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }
  
  // Get architecture to handle pointer size differences
  uint32_t addr_size = process_sp->GetAddressByteSize();
  
  // Check if this is a tagged pointer (GNUstep small object)
  // Tagged pointers have non-zero low bits (OBJC_SMALL_OBJECT_SHIFT = 3)
  const uint64_t SMALL_OBJECT_MASK = 0x07;
  const int SMALL_OBJECT_SHIFT = 3;
  
  if ((number_ptr & SMALL_OBJECT_MASK) != 0) {
    // This is a tagged pointer - handle small number types
    // GNUstep uses these tags:
    // 1 = NSSmallInt
    // 2 = NSSmallExtendedDouble  
    // 3 = NSSmallRepeatingDouble
    // 5 = NSSmallFloat
    uint64_t tag = number_ptr & SMALL_OBJECT_MASK;
    
    if (tag == 1) {
      // NSSmallInt - integer value is ptr >> 3
      int64_t int_value = ((int64_t)number_ptr) >> SMALL_OBJECT_SHIFT;
      stream.Printf("%lld", (long long)int_value);
      return true;
    } else if (tag == 5) {
      // NSSmallFloat - float encoded in double format with tag 5
      union {
        uint64_t bits;
        double d;
      } converter;
      converter.bits = number_ptr & ~SMALL_OBJECT_MASK;  // Clear tag bits
      float float_value = (float)converter.d;
      stream.Printf("%g", float_value);
      return true;
    } else if (tag == 2) {
      // NSSmallExtendedDouble
      // Decode according to unboxSmallExtendedDouble
      uint64_t mask = number_ptr & 8;
      union {
        uint64_t bits;
        double d;
      } converter;
      converter.bits = (number_ptr & ~7) | (mask >> 1) | (mask >> 2) | (mask >> 3);
      stream.Printf("%g", converter.d);
      return true;
    } else if (tag == 3) {
      // NSSmallRepeatingDouble
      // Decode according to unboxSmallRepeatingDouble
      uint64_t mask = number_ptr & 56;
      union {
        uint64_t bits;
        double d;
      } converter;
      converter.bits = (number_ptr & ~7) | (mask >> 3);
      stream.Printf("%g", converter.d);
      return true;
    } else {
      // Unknown tagged pointer type - show tag for debugging
      stream.Printf("<tagged_number:tag=%llu>", (unsigned long long)tag);
      return true;
    }
  }
  
  // Regular NSNumber object - try to extract value
  // First check if it's a boolean
  std::string class_name = GetNumberClassName(valobj);
  
  // Check if the type name contains Bool (for cases where it's explicitly typed)
  const char* type_name = valobj.GetTypeName().AsCString();
  printf("[GNUstepNSNumber] Type name: '%s', Class name: '%s'\n", 
         type_name ? type_name : "null", class_name.c_str());
  bool is_bool = (class_name.find("BoolNumber") != std::string::npos) ||
                 (type_name && strstr(type_name, "BoolNumber") != nullptr);
  
  if (is_bool) {
    // This is a boolean - format as YES/NO
    int64_t bool_value;
    if (TryExtractInteger(valobj, bool_value)) {
      stream.Printf("%s", bool_value ? "YES" : "NO");
      return true;
    }
  }
  
  // Not a boolean, try other types
  double double_value;
  if (TryExtractDouble(valobj, double_value)) {
    stream.Printf("%g", double_value);
  } else {
    int64_t int_value;
    if (TryExtractInteger(valobj, int_value)) {
      stream.Printf("%lld", (long long)int_value);
    } else {
      // Fallback to showing address
      stream.Printf("NSNumber @ 0x%llx", (unsigned long long)number_ptr);
    }
  }
  
  return true;
}

std::string GNUstepNSNumberSummaryProvider::GetNumberClassName(
    ValueObject &valobj) {
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return "";

  addr_t number_ptr = valobj.GetPointerValue();
  if (number_ptr == 0 || number_ptr == LLDB_INVALID_ADDRESS)
    return "";
    
  // Get the runtime to determine the exact NSNumber subclass
  ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
  if (!runtime)
    return "";

  ObjCLanguageRuntime::ClassDescriptorSP class_descriptor_sp =
      runtime->GetClassDescriptor(valobj);
  if (!class_descriptor_sp) {
    return "";
  }

  return class_descriptor_sp->GetClassName().AsCString("");
}

bool GNUstepNSNumberSummaryProvider::TryExtractDouble(ValueObject &valobj, double &value) {
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;
    
  addr_t number_ptr = valobj.GetPointerValue();
  if (number_ptr == 0 || number_ptr == LLDB_INVALID_ADDRESS)
    return false;
    
  std::string class_name = GetNumberClassName(valobj);
  
  // GNUstep NSNumber subclasses store their value at offset 8 (after ISA pointer)
  // We determine the type based on the class name pattern
  
  if (class_name.find("FloatNumber") != std::string::npos) {
    // Float value is stored at offset 8 as 32-bit float
    Status error;
    uint32_t float_bits = process_sp->ReadUnsignedIntegerFromMemory(
        number_ptr + 8, sizeof(uint32_t), 0, error);
    if (!error.Fail()) {
      float float_val = *reinterpret_cast<float*>(&float_bits);
      value = static_cast<double>(float_val);
      return true;
    }
  } else if (class_name.find("DoubleNumber") != std::string::npos) {
    // Double value is stored at offset 8 as 64-bit double
    Status error;
    uint64_t double_bits = process_sp->ReadUnsignedIntegerFromMemory(
        number_ptr + 8, sizeof(uint64_t), 0, error);
    if (!error.Fail()) {
      value = *reinterpret_cast<double*>(&double_bits);
      return true;
    }
  }
  
  return false;
}

bool GNUstepNSNumberSummaryProvider::TryExtractInteger(ValueObject &valobj, int64_t &value) {
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;
    
  addr_t number_ptr = valobj.GetPointerValue();
  if (number_ptr == 0 || number_ptr == LLDB_INVALID_ADDRESS)
    return false;
    
  std::string class_name = GetNumberClassName(valobj);
  
  // GNUstep NSNumber subclasses store their value at offset 8 (after ISA pointer)
  // We determine the type and size based on the class name pattern
  
  if (class_name.find("BoolNumber") != std::string::npos) {
    // Bool stored as int at offset 8
    Status error;
    uint32_t int_val = process_sp->ReadUnsignedIntegerFromMemory(
        number_ptr + 8, sizeof(uint32_t), 0, error);
    if (!error.Fail()) {
      value = (int_val != 0) ? 1 : 0;
      return true;
    }
  } else if (class_name.find("IntNumber") != std::string::npos) {
    // Int value is stored at offset 8 as 32-bit int
    Status error;
    uint32_t int_val = process_sp->ReadUnsignedIntegerFromMemory(
        number_ptr + 8, sizeof(uint32_t), 0, error);
    if (!error.Fail()) {
      value = static_cast<int32_t>(int_val);  // Sign extend
      return true;
    }
  } else if (class_name.find("LongLongNumber") != std::string::npos) {
    // Long long value is stored at offset 8 as 64-bit integer
    Status error;
    uint64_t ll_val = process_sp->ReadUnsignedIntegerFromMemory(
        number_ptr + 8, sizeof(uint64_t), 0, error);
    if (!error.Fail()) {
      // Check if it's unsigned based on class name
      if (class_name.find("Unsigned") != std::string::npos) {
        // Note: This might overflow if the value is > INT64_MAX
        value = static_cast<int64_t>(ll_val);
      } else {
        value = static_cast<int64_t>(ll_val);
      }
      return true;
    }
  } else if (class_name.find("Number") != std::string::npos) {
    // Generic fallback - try reading as 64-bit integer
    Status error;
    uint64_t generic_val = process_sp->ReadUnsignedIntegerFromMemory(
        number_ptr + 8, sizeof(uint64_t), 0, error);
    if (!error.Fail()) {
      value = static_cast<int64_t>(generic_val);
      return true;
    }
  }
  
  return false;
}


// Function wrapper for LLDB registration
bool lldb_private::formatters::GNUstepNSNumberFormatterFunction(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  GNUstepNSNumberSummaryProvider provider;
  return provider.FormatObject(valobj, stream, options);
}