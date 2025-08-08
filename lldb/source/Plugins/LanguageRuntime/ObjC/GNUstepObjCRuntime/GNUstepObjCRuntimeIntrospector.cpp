//===-- GNUstepObjCRuntimeIntrospector.cpp ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCRuntimeIntrospector.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Core/Module.h"
#include "lldb/ValueObject/ValueObject.h"

using namespace lldb;
using namespace lldb_private;

GNUstepObjCRuntimeIntrospector::GNUstepObjCRuntimeIntrospector(Process *process)
    : m_process(process) {
  // Cache some runtime constants
  if (m_process) {
    m_address_size = m_process->GetAddressByteSize();
    m_byte_order = m_process->GetByteOrder();
  } else {
    m_address_size = 0;
    m_byte_order = lldb::eByteOrderInvalid;
  }
}

lldb::addr_t GNUstepObjCRuntimeIntrospector::GetISAFromObject(ValueObject &valobj) {
  if (!m_process) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Get the object address
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Check if this is a tagged pointer first
  if (IsTaggedPointer(obj_addr)) {
    // For tagged pointers, we need to extract the class information differently
    // GNUstep tagged pointers encode class information in the lower bits
    return obj_addr; // Return the tagged pointer itself for now
  }
  
  // For regular objects, the ISA is the first pointer-sized value
  Status error;
  lldb::addr_t isa_addr = m_process->ReadPointerFromMemory(obj_addr, error);
  
  if (error.Fail()) {
    return LLDB_INVALID_ADDRESS;
  }
  
  return isa_addr;
}

std::string GNUstepObjCRuntimeIntrospector::GetClassName(lldb::addr_t isa_addr) {
  if (!m_process || isa_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  // Check if this is a tagged pointer
  if (IsTaggedPointer(isa_addr)) {
    // For tagged pointers, decode the class based on the tag
    uint64_t tag = isa_addr & 0x7; // Lower 3 bits are the tag
    switch (tag) {
    case 1: // Tagged number
      return "NSNumber";
    case 2: // Tagged date
      return "NSDate";
    case 4: // Tagged string (GNUstep uses tag 4 for strings based on our observation)
      return "NSString";
    default:
      return "<tagged[" + std::to_string(tag) + "]>";
    }
  }

  // Based on libobjc2/class.h, the structure of an objc_class is:
  // struct objc_class {
  //   Class isa;          // 0 * address_size
  //   Class super_class;  // 1 * address_size
  //   const char *name;   // 2 * address_size  <- This is what we want!
  //   ...
  // };
  // We need to read the 'name' pointer, which is the third pointer in the
  // structure.

  Status error;

  // The 'name' field is at an offset of 2 * address_size from the start of the
  // class structure.
  const lldb::addr_t name_ptr_addr = isa_addr + (2 * m_address_size);

  const lldb::addr_t name_addr = m_process->ReadPointerFromMemory(name_ptr_addr, error);

  if (error.Fail() || name_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }

  // Now read the C-string from the 'name' pointer.
  std::string class_name;
  m_process->ReadCStringFromMemory(name_addr, class_name, error);

  if (error.Fail()) {
    return "";
  }

  return class_name;
}

std::string GNUstepObjCRuntimeIntrospector::GetClassNameFromObject(ValueObject &valobj) {
  lldb::addr_t isa_addr = GetISAFromObject(valobj);
  if (isa_addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  return GetClassName(isa_addr);
}

lldb::addr_t GNUstepObjCRuntimeIntrospector::FindClass(const std::string &class_name) {
  if (!m_process || class_name.empty()) {
    return LLDB_INVALID_ADDRESS;
  }

  // Try to call objc_lookup_class function in the target
  // This is more reliable than trying to parse the class table ourselves
  std::vector<lldb::addr_t> args;
  
  // First, we need to create a string in the target process memory
  Status error;
  lldb::addr_t string_addr = m_process->AllocateMemory(class_name.length() + 1, 
                                                       lldb::ePermissionsReadable, error);
  if (error.Fail() || string_addr == LLDB_INVALID_ADDRESS) {
    return LLDB_INVALID_ADDRESS;
  }

  // Write the class name to target memory
  size_t bytes_written = m_process->WriteMemory(string_addr, class_name.c_str(), 
                                               class_name.length() + 1, error);
  if (error.Fail() || bytes_written != class_name.length() + 1) {
    m_process->DeallocateMemory(string_addr);
    return LLDB_INVALID_ADDRESS;
  }

  args.push_back(string_addr);
  lldb::addr_t class_addr = CallRuntimeFunction("objc_lookup_class", args);

  // Clean up the allocated string
  m_process->DeallocateMemory(string_addr);

  return class_addr;
}

bool GNUstepObjCRuntimeIntrospector::IsValidGNUstepRuntime() {
  if (!m_process) {
    return false;
  }

  // Try to find objc_lookup_class function - this indicates GNUstep runtime
  Target &target = m_process->GetTarget();
  SymbolContextList sc_list;
  target.GetImages().FindSymbolsWithNameAndType(ConstString("objc_lookup_class"),
                                               lldb::eSymbolTypeCode, sc_list);
  
  return sc_list.GetSize() > 0;
}

lldb::addr_t GNUstepObjCRuntimeIntrospector::CallRuntimeFunction(
    const std::string &function_name, const std::vector<lldb::addr_t> &args) {
  
  if (!m_process) {
    return LLDB_INVALID_ADDRESS;
  }

  Target &target = m_process->GetTarget();
  
  // Find the function symbol
  SymbolContextList sc_list;
  target.GetImages().FindSymbolsWithNameAndType(ConstString(function_name),
                                               lldb::eSymbolTypeCode, sc_list);
  
  if (sc_list.GetSize() == 0) {
    return LLDB_INVALID_ADDRESS;
  }

  SymbolContext sc;
  sc_list.GetContextAtIndex(0, sc);
  
  if (!sc.symbol) {
    return LLDB_INVALID_ADDRESS;
  }

  lldb::addr_t func_addr = sc.symbol->GetLoadAddress(&target);
  if (func_addr == LLDB_INVALID_ADDRESS) {
    return LLDB_INVALID_ADDRESS;
  }

  // For now, return the function address. In a full implementation,
  // we would use the process's thread to actually call the function.
  // This requires more complex setup with the expression evaluator.
  
  // TODO: Implement actual function calling using ExecutionContext
  // and ThreadPlanCallFunction
  
  return LLDB_INVALID_ADDRESS; // Stub for now
}

bool GNUstepObjCRuntimeIntrospector::IsTaggedPointer(lldb::addr_t obj_addr) {
  if (!m_process || obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // GNUstep uses tagged pointers for small objects to avoid allocations.
  // The tag is in the lower 3 bits:
  // - Tag 4: Tiny strings (up to 8 characters)
  // - Tag 1: Small integers (NSNumber)
  // - Tag 2: Dates or other small objects
  // 
  // Check if any of the lower 3 bits are set (indicating a tagged pointer)
  uint64_t tag = obj_addr & 0x7;
  
  // Valid tags are 1, 2, 4 (not 0, 3, 5, 6, 7)
  if (tag == 1 || tag == 2 || tag == 4) {
    return true;
  }
  
  return false;
}

std::string GNUstepObjCRuntimeIntrospector::DecodeTaggedString(lldb::addr_t obj_addr) {
  // GNUstep uses "tiny strings" with tag value 4 for short compile-time constants.
  // Based on analysis of GNUstep source code (GSString.m):
  //
  // Bit layout for 64-bit systems:
  // - Bits 0-2: Tag (must be 4 for tiny strings)
  // - Bits 3-7: Length (5 bits, can store 0-31 but max is 9 characters)
  // - Bits 8-56: Unused/padding
  // - Bits 57-63, 50-56, 43-49, etc: Characters stored from high bits down
  //   Each character uses 7 bits, stored at bit position (57 - i*7)
  //
  // The macro from GNUstep source:
  // #define TINY_STRING_CHAR(s, x) ((s & (0xFE00000000000000 >> (x*7))) >> (57-(x*7)))
  // #define TINY_STRING_LENGTH_SHIFT 3
  // #define TINY_STRING_LENGTH_MASK 0x1f
  //
  // Note: The runtime may set additional high bits (e.g., 0xc instead of 0x8 prefix)
  // for metadata. We mask these off when decoding.
  
  // Verify this is a tagged string (tag = 4)
  if ((obj_addr & 0x7) != 4) {
    return "";
  }
  
  // Don't mask the address - the character extraction already handles the right bits
  // Extract length from bits 3-7 (after the tag)
  int length = (obj_addr >> 3) & 0x1f;
  
  // Sanity check - tiny strings can't be longer than 9 characters
  if (length > 9 || length == 0) {
    return "";
  }
  
  // Decode characters - each uses 7 bits, stored from bit 57 downward
  std::string result;
  result.reserve(length);
  
  for (int i = 0; i < length; i++) {
    // Extract character at position i using the GNUstep formula
    // Characters are stored at bits (57 - i*7) for 7 bits each
    // The mask 0xFE means 7 bits (1111110 in binary), shifted to the right position
    uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
    char c = (obj_addr & mask) >> (57 - (i * 7));
    
    // Validate it's a printable ASCII character
    if (c >= 0x20 && c <= 0x7e) {
      result += c;
    } else if (c == 0) {
      // Unexpected null in the middle - stop
      break;
    } else {
      // Non-printable character - this shouldn't happen with valid tiny strings
      // Return what we have so far or indicate error
      if (result.empty()) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "<tagged_%llx...>", 
                 (unsigned long long)(obj_addr & 0xffffffffffff));
        return buffer;
      }
      break;
    }
  }
  
  // Return the decoded string
  return result;
}

bool GNUstepObjCRuntimeIntrospector::IsValidObjectPointer(lldb::addr_t obj_addr) {
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // Tagged pointers are always valid if they have the right tag bits
  if (IsTaggedPointer(obj_addr)) {
    return true;
  }
  
  // For regular pointers, do some basic sanity checks
  if (m_address_size == 8) {
    // On 64-bit systems, valid object pointers should be:
    // - Aligned to at least 8 bytes
    // - In a reasonable memory range
    if ((obj_addr & 0x7) != 0) {
      return false;
    }
    // Very low addresses are likely invalid
    if (obj_addr < 0x1000) {
      return false;
    }
  } else {
    // On 32-bit systems, align to 4 bytes
    if ((obj_addr & 0x3) != 0) {
      return false;
    }
    if (obj_addr < 0x1000) {
      return false;
    }
  }
  
  // Try to read the ISA pointer - if this fails, it's not a valid object
  Status error;
  lldb::addr_t isa = m_process->ReadPointerFromMemory(obj_addr, error);
  if (error.Fail() || isa == 0 || isa == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  return true;
}
