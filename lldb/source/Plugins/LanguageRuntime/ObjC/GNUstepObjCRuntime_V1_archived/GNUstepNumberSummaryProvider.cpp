//===-- GNUstepNumberSummaryProvider.cpp -----------------------*- C++ -*-===//
//
// Number summary provider for GNUstep NSNumber objects
// Shows actual numeric value instead of internal formatter fields
//
//===----------------------------------------------------------------------===//

#include "GNUstepNumberSummaryProvider.h"
#include "GNUstepObjCRuntime.h"

#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Utility/Log.h"

#include <algorithm>
#include <vector>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNumberSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream,
                                               const TypeSummaryOptions &options) {
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "GNUstepNumberSummaryProvider::FormatObject called");
  
  // Get the number object pointer
  addr_t number_ptr = valobj.GetPointerValue();
  if (number_ptr == 0 || number_ptr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }
  
  // Get architecture to handle pointer size differences
  ProcessSP process_sp = valobj.GetProcessSP();
  uint32_t addr_size = process_sp ? process_sp->GetAddressByteSize() : 8;
  bool is_64bit = (addr_size == 8);
  
  // Check if this is a tagged pointer (GNUstep small object)
  // Tagged pointers have non-zero low bits (OBJC_SMALL_OBJECT_SHIFT = 3)
  // This is the same for both 32-bit and 64-bit GNUstep
  const uint64_t SMALL_OBJECT_MASK = 0x07;
  const int SMALL_OBJECT_SHIFT = 3;
  
  if ((number_ptr & SMALL_OBJECT_MASK) != 0) {
    // This is a tagged pointer - handle small number types
    // GNUstep uses these tags:
    // 1 = NSSmallInt
    // 2 = NSSmallExtendedDouble
    // 3 = NSSmallRepeatingDouble
    // 4 = GSTinyString (not a number)
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
    }
  }
  
  // Process was already obtained above for architecture detection
  if (!process_sp) {
    stream.Printf("NSNumber <no process>");
    return true;
  }
  
  // Regular NSNumber object - try to extract value
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

bool GNUstepNumberSummaryProvider::TryExtractDouble(ValueObject &valobj, double &value) {
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;
    
  addr_t number_ptr = valobj.GetPointerValue();
  if (number_ptr == 0 || number_ptr == LLDB_INVALID_ADDRESS)
    return false;
    
  // Get the runtime to determine the exact NSNumber subclass
  GNUstepObjCRuntime *runtime = nullptr;
  if (ObjCLanguageRuntime *objc_runtime = ObjCLanguageRuntime::Get(*process_sp)) {
    runtime = static_cast<GNUstepObjCRuntime*>(objc_runtime);
  }
  
  if (!runtime)
    return false;
    
  std::string class_name = runtime->GetClassNameFromObject(number_ptr);
  
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

bool GNUstepNumberSummaryProvider::TryExtractInteger(ValueObject &valobj, int64_t &value) {
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;
    
  addr_t number_ptr = valobj.GetPointerValue();
  if (number_ptr == 0 || number_ptr == LLDB_INVALID_ADDRESS)
    return false;
    
  // Get the runtime to determine the exact NSNumber subclass
  GNUstepObjCRuntime *runtime = nullptr;
  if (ObjCLanguageRuntime *objc_runtime = ObjCLanguageRuntime::Get(*process_sp)) {
    runtime = static_cast<GNUstepObjCRuntime*>(objc_runtime);
  }
  
  if (!runtime)
    return false;
    
  std::string class_name = runtime->GetClassNameFromObject(number_ptr);
  
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
