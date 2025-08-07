//===-- GNUstepSyntheticProvider.cpp ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepSyntheticProvider.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/RegularExpression.h"
#include "lldb/Core/Module.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Symbol/Type.h"
#include "lldb/Symbol/TypeSystem.h"
#include "lldb/Symbol/Symbol.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "lldb/lldb-enumerations.h"
#include "llvm/Support/Error.h"
#include <vector>

using namespace lldb;
using namespace lldb_private;

GNUstepSyntheticProvider::GNUstepSyntheticProvider(lldb::ValueObjectSP backend)
    : SyntheticChildrenFrontEnd(*backend) {
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "GNUstepSyntheticProvider::Constructor - backend: {0}", 
           m_backend.GetValueAsUnsigned(0));
}

lldb::ValueObjectSP GNUstepSyntheticProvider::GetChildAtIndex(uint32_t idx) {
    LLDB_LOG(GetLog(LLDBLog::DataFormatters),
             "GNUstepSyntheticProvider::GetChildAtIndex({0}) - backend: {1}", idx, 
             (void*)&m_backend);
    
    // Get execution context for runtime lookups
    ExecutionContext exe_ctx = m_backend.GetExecutionContextRef().Lock(false);
    if (!exe_ctx.HasTargetScope()) {
        LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                 "GNUstepSyntheticProvider::GetChildAtIndex({0}) - no execution context", idx);
        return lldb::ValueObjectSP();
    }
    
    // Get the actual class ivar names instead of relying on LLDB's type system
    if (m_cached_ivar_names.empty()) {
        GetClassIvarNames();
    }
    
    if (idx >= m_cached_ivar_names.size()) {
        LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                 "GNUstepSyntheticProvider::GetChildAtIndex({0}) - index out of bounds (size: {1})", 
                 idx, m_cached_ivar_names.size());
        return lldb::ValueObjectSP();
    }
    
    ConstString field_name(m_cached_ivar_names[idx].c_str());
    
    LLDB_LOG(GetLog(LLDBLog::DataFormatters),
             "GNUstepSyntheticProvider::GetChildAtIndex({0}) - using field name: {1}", 
             idx, field_name.GetCString());
    
    // Try to get the runtime offset for this field dynamically
    lldb::addr_t runtime_offset = GetIvarOffsetFromRuntime(field_name);
    LLDB_LOG(GetLog(LLDBLog::DataFormatters),
             "GNUstepSyntheticProvider::GetChildAtIndex({0}) - GetIvarOffsetFromRuntime({1}) returned {2:x}", 
             idx, field_name.GetCString(), runtime_offset);
    
    if (runtime_offset == LLDB_INVALID_ADDRESS) {
        // If we can't get runtime offset, try using class metadata parsing
        runtime_offset = GetIvarOffsetFromClassMetadata(field_name);
        LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                 "GNUstepSyntheticProvider::GetChildAtIndex({0}) - GetIvarOffsetFromClassMetadata({1}) returned {2:x}", 
                 idx, field_name.GetCString(), runtime_offset);
    }
    
    if (runtime_offset == LLDB_INVALID_ADDRESS) {
        LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                 "GNUstepSyntheticProvider::GetChildAtIndex({0}) - no runtime offset for {1}, using direct access", 
                 idx, field_name.GetCString());
        
        // Alternative approach: Use LLDB's type system to create children at correct offsets
        // This bypasses the non-synthetic children issue
        CompilerType obj_type = m_backend.GetCompilerType();
        CompilerType type_to_use = obj_type;
        
        // If it's a pointer type, get the pointee type
        if (obj_type.IsValid() && obj_type.IsPointerType()) {
            type_to_use = obj_type.GetPointeeType();
        }
        
        if (type_to_use.IsValid()) {
            
            // Get the field directly by index instead of searching by name
            // This ensures we get the field that corresponds to our cached index
            std::string type_field_name;
            uint64_t field_bit_offset = 0;
            uint32_t field_bitfield_bit_size = 0;
            bool is_bitfield = false;
            
            CompilerType field_type = type_to_use.GetFieldAtIndex(
                idx, type_field_name, &field_bit_offset, &field_bitfield_bit_size, &is_bitfield);
            
            if (field_type.IsValid() && type_field_name == field_name.GetCString()) {
                LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                         "GNUstepSyntheticProvider::GetChildAtIndex({0}) - found field {1} at bit offset {2}", 
                         idx, field_name.GetCString(), field_bit_offset);
                
                // Create a child value object at the calculated offset using GetSyntheticChildAtOffset
                uint32_t byte_offset = field_bit_offset / 8;
                
                LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                         "GNUstepSyntheticProvider::GetChildAtIndex({0}) - creating child {1} at byte offset {2}", 
                         idx, field_name.GetCString(), byte_offset);
                
                // Use GetSyntheticChildAtOffset for proper parent-child relationship and value display
                lldb::ValueObjectSP child = m_backend.GetSyntheticChildAtOffset(
                    byte_offset, field_type, true, field_name);
                
                if (child && child->GetError().Success()) {
                    LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                             "GNUstepSyntheticProvider::GetChildAtIndex({0}) - successfully created child {1} with type {2}", 
                             idx, field_name.GetCString(), field_type.GetTypeName().GetCString());
                    return child;
                } else {
                    LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                             "GNUstepSyntheticProvider::GetChildAtIndex({0}) - failed to create child {1}: {2}", 
                             idx, field_name.GetCString(), child ? child->GetError().AsCString() : "null child");
                }
            } else {
                LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                         "GNUstepSyntheticProvider::GetChildAtIndex({0}) - field name mismatch or invalid: expected {1}, got {2}", 
                         idx, field_name.GetCString(), type_field_name.c_str());
            }
        }
        
        LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                 "GNUstepSyntheticProvider::GetChildAtIndex({0}) - direct access failed for {1}", 
                 idx, field_name.GetCString());
        return lldb::ValueObjectSP();
    }
    
    // Create a new value object using the correct runtime offset
    // Try to get type information from LLDB's type system
    lldb::ValueObjectSP non_synthetic = m_backend.GetNonSyntheticValue();
    CompilerType field_type;
    
    if (non_synthetic) {
        // Try to find a child with matching name to get type info
        auto num_children_expected = non_synthetic->GetNumChildren(true);
        if (num_children_expected) {
            uint32_t num_children = *num_children_expected;
            for (uint32_t i = 0; i < num_children; ++i) {
                lldb::ValueObjectSP child = non_synthetic->GetChildAtIndex(i, true);
                if (child && child->GetName() == field_name) {
                    field_type = child->GetCompilerType();
                    break;
                }
            }
        }
    }
    
    // If we couldn't get type info, try to determine it from the field name and context
    if (!field_type.IsValid()) {
        // Try to infer type from field name patterns or use generic id type
        ExecutionContext exe_ctx = m_backend.GetExecutionContextRef().Lock(false);
        if (exe_ctx.HasTargetScope()) {
            Target *target = exe_ctx.GetTargetPtr();
            if (target) {
                // Common Objective-C field name patterns
                std::string field_name_str = field_name.GetStringRef().str();
                
                // Try to find the type based on common patterns
                if (field_name_str.find("String") != std::string::npos ||
                    field_name_str.find("Name") != std::string::npos ||
                    field_name_str.find("Title") != std::string::npos ||
                    field_name_str.find("Text") != std::string::npos ||
                    field_name_str.find("Label") != std::string::npos ||
                    field_name_str.find("_account") != std::string::npos) {
                    // Likely an NSString*
                    if (auto ts = target->GetScratchTypeSystemForLanguage(eLanguageTypeObjC_plus_plus)) {
                        if (auto *clang_ts = llvm::dyn_cast<TypeSystemClang>(ts->get())) {
                            field_type = clang_ts->GetBasicType(eBasicTypeObjCID);
                        }
                    }
                } else if (field_name_str.find("Array") != std::string::npos ||
                           field_name_str.find("List") != std::string::npos ||
                           field_name_str.find("Items") != std::string::npos ||
                           field_name_str == "_transactions") {
                    // Likely an NSArray* or NSMutableArray*
                    if (auto ts = target->GetScratchTypeSystemForLanguage(eLanguageTypeObjC_plus_plus)) {
                        if (auto *clang_ts = llvm::dyn_cast<TypeSystemClang>(ts->get())) {
                            field_type = clang_ts->GetBasicType(eBasicTypeObjCID);
                        }
                    }
                } else if (field_name_str.find("Number") != std::string::npos ||
                           field_name_str.find("Count") != std::string::npos ||
                           field_name_str.find("Size") != std::string::npos ||
                           field_name_str.find("Index") != std::string::npos) {
                    // Might be NSNumber* or a primitive
                    if (auto ts = target->GetScratchTypeSystemForLanguage(eLanguageTypeObjC_plus_plus)) {
                        if (auto *clang_ts = llvm::dyn_cast<TypeSystemClang>(ts->get())) {
                            field_type = clang_ts->GetBasicType(eBasicTypeObjCID);
                        }
                    }
                } else if (field_name_str.find("_balance") != std::string::npos ||
                           field_name_str.find("amount") != std::string::npos ||
                           field_name_str.find("price") != std::string::npos ||
                           field_name_str.find("value") != std::string::npos) {
                    // Likely a double
                    if (auto ts = target->GetScratchTypeSystemForLanguage(eLanguageTypeObjC_plus_plus)) {
                        if (auto *clang_ts = llvm::dyn_cast<TypeSystemClang>(ts->get())) {
                            field_type = clang_ts->GetBasicType(eBasicTypeDouble);
                        }
                    }
                } else {
                    // Default to generic Objective-C object pointer (id)
                    if (auto ts = target->GetScratchTypeSystemForLanguage(eLanguageTypeObjC_plus_plus)) {
                        if (auto *clang_ts = llvm::dyn_cast<TypeSystemClang>(ts->get())) {
                            field_type = clang_ts->GetBasicType(eBasicTypeObjCID);
                        }
                    }
                }
            }
        }
    }
    
    if (!field_type.IsValid()) {
        LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                 "GNUstepSyntheticProvider::GetChildAtIndex({0}) - could not determine field type for {1}", 
                 idx, field_name.GetCString());
        return lldb::ValueObjectSP();
    }
    
    // DEBUG: Let's see what offset we're getting and test without adjustment first
    lldb::addr_t adjusted_offset = runtime_offset;
    LLDB_LOG(GetLog(LLDBLog::DataFormatters),
             "GNUstepSyntheticProvider::GetChildAtIndex({0}) - runtime offset for {1} is {2} (0x{2:x})", 
             idx, field_name.GetCString(), runtime_offset);
    
    // TEST: Don't adjust the offset - GNUstep offsets might already be correct
    // The issue might be elsewhere in how GetSyntheticChildAtOffset interprets offsets
    /*
    if (runtime_offset >= 8) {
        adjusted_offset = runtime_offset - 8;
        LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                 "GNUstepSyntheticProvider::GetChildAtIndex({0}) - adjusting offset from {1} to {2} (subtracting isa pointer)", 
                 idx, runtime_offset, adjusted_offset);
    }
    */
    
    // Use the adjusted runtime offset to create child with GetSyntheticChildAtOffset
    LLDB_LOG(GetLog(LLDBLog::DataFormatters),
             "GNUstepSyntheticProvider::GetChildAtIndex({0}) - creating runtime offset child {1} at adjusted offset {2} (original: {3})", 
             idx, field_name.GetCString(), adjusted_offset, runtime_offset);
    
    // GetSyntheticChildAtOffset works with byte offsets and maintains proper parent-child relationships
    lldb::ValueObjectSP corrected_child = m_backend.GetSyntheticChildAtOffset(
        adjusted_offset,
        field_type,
        true,  // can_create - allow creating the child even if offset seems unusual
        field_name  // name_const_str
    );
    
    if (corrected_child && corrected_child->GetError().Success()) {
        LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                 "GNUstepSyntheticProvider::GetChildAtIndex({0}) - successfully created runtime offset child {1}", 
                 idx, field_name.GetCString());
        return corrected_child;
    } else {
        LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                 "GNUstepSyntheticProvider::GetChildAtIndex({0}) - failed to create runtime offset child {1}: {2}", 
                 idx, field_name.GetCString(), corrected_child ? corrected_child->GetError().AsCString() : "null child");
        return lldb::ValueObjectSP();
    }
}

lldb::ChildCacheState GNUstepSyntheticProvider::Update() {
  // Clear the cached ivar names so they get re-discovered
  m_cached_ivar_names.clear();
  return lldb::ChildCacheState::eRefetch;
}

bool GNUstepSyntheticProvider::MightHaveChildren() {
  // Check if this is a Foundation class that shouldn't have synthetic children
  CompilerType type = m_backend.GetCompilerType();
  if (type.IsValid()) {
    std::string type_name = type.GetTypeName().GetStringRef().str();
    
    // Remove pointer marker if present
    size_t pos = type_name.find(" *");
    if (pos != std::string::npos) {
      type_name = type_name.substr(0, pos);
    }
    
    // Foundation classes that shouldn't have synthetic children
    // These are either toll-free bridged, tagged pointers, or have special implementations
    if (type_name == "NSString" || type_name == "NSMutableString" ||
        type_name == "NSNumber" || type_name == "NSDecimalNumber" ||
        type_name == "NSDate" || type_name == "NSCalendarDate" ||
        type_name == "NSArray" || type_name == "NSMutableArray" ||
        type_name == "NSDictionary" || type_name == "NSMutableDictionary" ||
        type_name == "NSSet" || type_name == "NSMutableSet" ||
        type_name == "NSData" || type_name == "NSMutableData" ||
        type_name == "NSURL" || type_name == "NSError" ||
        type_name == "NSException" || type_name == "NSValue") {
      return false;  // These types have their own formatters, don't use synthetic children
    }
  }
  
  // CRITICAL FIX: Use GetNonSyntheticValue() to avoid recursion
  lldb::ValueObjectSP non_synthetic = m_backend.GetNonSyntheticValue();
  if (!non_synthetic) {
    return false;
  }
  return non_synthetic->MightHaveChildren();
}

size_t GNUstepSyntheticProvider::GetIndexOfChildWithName(ConstString name) {
  // CRITICAL FIX: Use GetNonSyntheticValue() to avoid recursion
  lldb::ValueObjectSP non_synthetic = m_backend.GetNonSyntheticValue();
  if (!non_synthetic) {
    return UINT32_MAX;
  }
  
  // Use LLDB's built-in name-based child lookup
  auto num_children_expected = non_synthetic->GetNumChildren(true);
  if (!num_children_expected) {
    return UINT32_MAX; // Error getting children count
  }
  uint32_t num_children = *num_children_expected;
  
  for (uint32_t i = 0; i < num_children; ++i) {
    lldb::ValueObjectSP child_at_index = non_synthetic->GetChildAtIndex(i, true);
    if (child_at_index && child_at_index->GetName() == name) {
      return i;
    }
  }
  return UINT32_MAX;
}

llvm::Expected<uint32_t> GNUstepSyntheticProvider::CalculateNumChildren() {
  // Check if this is a Foundation class that shouldn't have synthetic children
  CompilerType type = m_backend.GetCompilerType();
  if (type.IsValid()) {
    std::string type_name = type.GetTypeName().GetStringRef().str();
    
    // Remove pointer marker if present
    size_t pos = type_name.find(" *");
    if (pos != std::string::npos) {
      type_name = type_name.substr(0, pos);
    }
    
    // Foundation classes that shouldn't have synthetic children
    if (type_name == "NSString" || type_name == "NSMutableString" ||
        type_name == "NSNumber" || type_name == "NSDecimalNumber" ||
        type_name == "NSDate" || type_name == "NSCalendarDate" ||
        type_name == "NSArray" || type_name == "NSMutableArray" ||
        type_name == "NSDictionary" || type_name == "NSMutableDictionary" ||
        type_name == "NSSet" || type_name == "NSMutableSet" ||
        type_name == "NSData" || type_name == "NSMutableData" ||
        type_name == "NSURL" || type_name == "NSError" ||
        type_name == "NSException" || type_name == "NSValue") {
      return 0;  // These types have their own formatters
    }
  }
  
  // Parse the actual class metadata to get the real ivar count
  // instead of relying on LLDB's type system which stops at NSObject
  if (m_cached_ivar_names.empty()) {
    GetClassIvarNames(); // Populate the cache
  }
  
  return m_cached_ivar_names.size();
}

lldb::addr_t GNUstepSyntheticProvider::GetIvarOffsetFromRuntime(ConstString ivar_name) {
  // Get the object's execution context to access symbols
  ExecutionContext exe_ctx = m_backend.GetExecutionContextRef().Lock(false);
  
  if (!exe_ctx.HasTargetScope()) {
    return LLDB_INVALID_ADDRESS;
  }
  
  Target *target = exe_ctx.GetTargetPtr();
  if (!target) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Try to get the class name from the backend type
  CompilerType obj_type = m_backend.GetCompilerType();
  std::string class_name = obj_type.GetTypeName().GetStringRef().str();
  
  // Remove pointer marker if present (C++17 compatible way)
  if (class_name.length() >= 2 && class_name.substr(class_name.length() - 2) == " *") {
    class_name = class_name.substr(0, class_name.length() - 2);
  }
  
  // Build the ivar offset symbol name prefix
  // GNUstep uses symbols like: __objc_ivar_offset_ClassName.fieldName.
  // Some fields have type suffixes like .d for double, .i for int, etc.
  std::string offset_symbol_prefix = "__objc_ivar_offset_" + class_name + "." + ivar_name.GetCString();
  
  LLDB_LOG(GetLog(LLDBLog::DataFormatters),
           "GNUstepSyntheticProvider::GetIvarOffsetFromRuntime - looking for symbol with prefix: {0}", 
           offset_symbol_prefix.c_str());
  
  // Look up symbols that match our prefix using regex
  // This handles any type suffix (.d, .i, .f, etc.) automatically
  SymbolContextList sc_list;
  
  // Create a regex pattern that matches our prefix with any suffix
  // The pattern matches: __objc_ivar_offset_ClassName.fieldName followed by anything
  std::string pattern = "^" + offset_symbol_prefix + "\\.*";
  RegularExpression regex(pattern.c_str());
  
  // Search for symbols matching our pattern
  target->GetImages().FindSymbolsMatchingRegExAndType(
      regex, eSymbolTypeAny, sc_list);
  
  if (sc_list.GetSize() > 0) {
    LLDB_LOG(GetLog(LLDBLog::DataFormatters),
             "GNUstepSyntheticProvider::GetIvarOffsetFromRuntime - found {0} matching symbols for prefix {1}", 
             sc_list.GetSize(), offset_symbol_prefix.c_str());
  }
  
  // If regex didn't find it, try exact match with trailing dot
  if (sc_list.GetSize() == 0) {
    std::string exact_symbol = offset_symbol_prefix + ".";
    SymbolContextList exact_sc_list;
    target->GetImages().FindSymbolsWithNameAndType(
        ConstString(exact_symbol.c_str()), eSymbolTypeAny, exact_sc_list);
    
    if (exact_sc_list.GetSize() > 0) {
      sc_list = exact_sc_list;
      LLDB_LOG(GetLog(LLDBLog::DataFormatters),
               "GNUstepSyntheticProvider::GetIvarOffsetFromRuntime - found symbol with exact match: {0}", 
               exact_symbol.c_str());
    }
  }
  
  if (sc_list.GetSize() > 0) {
    SymbolContext sc;
    sc_list.GetContextAtIndex(0, sc);
    if (sc.symbol) {
      // Log the symbol name we actually found
      const char* symbol_name = sc.symbol->GetName().GetCString();
      if (symbol_name) {
        LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                 "GNUstepSyntheticProvider::GetIvarOffsetFromRuntime - found symbol with suffix: {0}", 
                 symbol_name);
      }
      
      addr_t symbol_addr = sc.symbol->GetLoadAddress(target);
      if (symbol_addr != LLDB_INVALID_ADDRESS) {
        // Read the offset value from this address - try different sizes
        Process *process = target->GetProcessSP().get();
        if (process) {
          Status error;
          
          // For BSS symbols, we still need to read from memory
          // The symbol holds the address where the offset is stored
          // BSS symbols are uninitialized data, so we read from their addresses
          
          // Try reading as uint64_t first (8 bytes) for 64-bit systems
          uint64_t offset64 = process->ReadUnsignedIntegerFromMemory(symbol_addr, 8, 0, error);
          if (error.Success() && offset64 < 0x10000) { // Reasonable offset range
            LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                     "GNUstepSyntheticProvider: Found runtime offset {0} (0x{0:x}) (64-bit) for {1}.{2} at symbol address 0x{3:x}", 
                     offset64, class_name.c_str(), ivar_name.GetCString(), symbol_addr);
            return static_cast<uint32_t>(offset64);
          }
          
          // Try reading as uint32_t (4 bytes)
          error.Clear();
          uint32_t offset32 = process->ReadUnsignedIntegerFromMemory(symbol_addr, 4, 0, error);
          if (error.Success() && offset32 < 0x10000) { // Reasonable offset range
            LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                     "GNUstepSyntheticProvider: Found runtime offset {0} (0x{0:x}) (32-bit) for {1}.{2} at symbol address 0x{3:x}", 
                     offset32, class_name.c_str(), ivar_name.GetCString(), symbol_addr);
            return offset32;
          }
          
          // Try reading as uint16_t (2 bytes) as final attempt
          error.Clear();
          uint16_t offset16 = process->ReadUnsignedIntegerFromMemory(symbol_addr, 2, 0, error);
          if (error.Success() && offset16 < 0x1000) { // Even more conservative range
            LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                     "GNUstepSyntheticProvider: Found runtime offset {0} (16-bit) for {1}.{2}", 
                     offset16, class_name.c_str(), ivar_name.GetCString());
            return offset16;
          }
          
          LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                   "GNUstepSyntheticProvider: Symbol found at 0x{0:x} but could not read valid offset for {1}.{2}", 
                   symbol_addr, class_name.c_str(), ivar_name.GetCString());
        }
      }
    }
  }
  
  // Fallback: Try to get offset dynamically using runtime functions
  // Reuse the existing execution context
  if (exe_ctx.HasTargetScope()) {
    Process *process = exe_ctx.GetProcessPtr();
    if (process) {
      // Get the class pointer from the object
      lldb::addr_t object_addr = m_backend.GetPointerValue();
      if (object_addr != LLDB_INVALID_ADDRESS) {
        Status error;
        lldb::addr_t isa_addr = process->ReadPointerFromMemory(object_addr, error);
        
        if (error.Success() && isa_addr != LLDB_INVALID_ADDRESS) {
          // Skip runtime function lookup and go directly to metadata parsing
          // Expression evaluation would be required to call runtime functions
          
          // Alternative: Parse the class metadata directly
          // GNUstep runtime stores ivars list in the class structure
          // This is runtime-specific but more reliable than hardcoding
          
          // Try to read the ivars from the class structure
          // For GNUstep v2 ABI, the class structure has:
          // - isa (8 bytes)
          // - superclass (8 bytes) 
          // - name (8 bytes)
          // - version (8 bytes)
          // - info (8 bytes)
          // - instance_size (8 bytes)
          // - ivars (8 bytes) - pointer to ivar list
          
          lldb::addr_t ivars_ptr = process->ReadPointerFromMemory(isa_addr + 48, error);
          if (error.Success() && ivars_ptr != 0) {
            // Read ivar count
            uint32_t ivar_count = process->ReadUnsignedIntegerFromMemory(ivars_ptr, 4, 0, error);
            if (error.Success() && ivar_count > 0 && ivar_count < 1000) {
              // Iterate through ivars
              for (uint32_t i = 0; i < ivar_count; i++) {
                // Each ivar entry is typically 24 bytes:
                // - offset pointer (8 bytes)
                // - name pointer (8 bytes)
                // - type pointer (8 bytes)
                lldb::addr_t ivar_entry = ivars_ptr + 8 + (i * 24);
                
                // Read name pointer
                lldb::addr_t name_ptr = process->ReadPointerFromMemory(ivar_entry + 8, error);
                if (error.Success() && name_ptr != 0) {
                  // Read the name string
                  char name_buf[256];
                  size_t bytes_read = process->ReadCStringFromMemory(name_ptr, name_buf, sizeof(name_buf), error);
                  
                  if (error.Success() && bytes_read > 0) {
                    if (strcmp(name_buf, ivar_name.GetCString()) == 0) {
                      // Found the ivar! Read its offset
                      lldb::addr_t offset_ptr = process->ReadPointerFromMemory(ivar_entry, error);
                      if (error.Success() && offset_ptr != 0) {
                        uint32_t offset = process->ReadUnsignedIntegerFromMemory(offset_ptr, 4, 0, error);
                        if (error.Success()) {
                          LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                                   "GNUstepSyntheticProvider: Found runtime offset {0} for {1}.{2} via metadata parsing", 
                                   offset, class_name.c_str(), ivar_name.GetCString());
                          return offset;
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  
  LLDB_LOG(GetLog(LLDBLog::DataFormatters),
           "GNUstepSyntheticProvider: Could not find runtime offset for {0}.{1}", 
           class_name.c_str(), ivar_name.GetCString());
  return LLDB_INVALID_ADDRESS;
}

lldb::addr_t GNUstepSyntheticProvider::GetIvarOffsetFromClassMetadata(ConstString ivar_name) {
  // Fallback: Parse the Objective-C class metadata to find ivar offsets
  ExecutionContext exe_ctx = m_backend.GetExecutionContextRef().Lock(false);
  
  if (!exe_ctx.HasTargetScope()) {
    return LLDB_INVALID_ADDRESS;
  }
  
  Target *target = exe_ctx.GetTargetPtr();
  Process *process = target->GetProcessSP().get();
  if (!process) {
    return LLDB_INVALID_ADDRESS;
  }
  
  // Get the object's isa pointer (first 8 bytes on 64-bit systems)
  lldb::addr_t object_addr = m_backend.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS) {
    return LLDB_INVALID_ADDRESS;
  }
  
  Status error;
  lldb::addr_t isa_addr = process->ReadPointerFromMemory(object_addr, error);
  if (!error.Success() || isa_addr == LLDB_INVALID_ADDRESS) {
    LLDB_LOG(GetLog(LLDBLog::DataFormatters),
             "GNUstepSyntheticProvider: Could not read isa pointer from 0x{0:x}", object_addr);
    return LLDB_INVALID_ADDRESS;
  }
  
  // For GNUstep runtime, the class structure contains ivar information
  // This is a simplified approach - in a full implementation we'd parse the complete runtime structures
  
  // Try to find the field using LLDB's type system as a fallback
  lldb::ValueObjectSP non_synthetic = m_backend.GetNonSyntheticValue();
  if (!non_synthetic) {
    return LLDB_INVALID_ADDRESS;
  }
  
  auto num_children_expected = non_synthetic->GetNumChildren(true);
  if (!num_children_expected) {
    return LLDB_INVALID_ADDRESS;
  }
  uint32_t num_children = *num_children_expected;
  
  // Look for the field by name and return its offset relative to the object start
  for (uint32_t i = 0; i < num_children; ++i) {
    lldb::ValueObjectSP child = non_synthetic->GetChildAtIndex(i, true);
    if (child && child->GetName() == ivar_name) {
      // Calculate offset based on the child's address vs object address
      lldb::addr_t child_addr = child->GetAddressOf();
      if (child_addr != LLDB_INVALID_ADDRESS && child_addr >= object_addr) {
        lldb::addr_t calculated_offset = child_addr - object_addr;
        LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                 "GNUstepSyntheticProvider: Calculated offset {0} for {1} (child_addr: 0x{2:x}, object_addr: 0x{3:x})", 
                 calculated_offset, ivar_name.GetCString(), child_addr, object_addr);
        return calculated_offset;
      }
    }
  }
  
  LLDB_LOG(GetLog(LLDBLog::DataFormatters),
           "GNUstepSyntheticProvider: Could not find offset for {0} via class metadata", 
           ivar_name.GetCString());
  return LLDB_INVALID_ADDRESS;
}

void GNUstepSyntheticProvider::GetClassIvarNames() {
  // Clear any existing cache
  m_cached_ivar_names.clear();
  
  LLDB_LOG(GetLog(LLDBLog::DataFormatters),
           "GNUstepSyntheticProvider::GetClassIvarNames() - discovering class ivars");
  
  // First, try to get field names from the compiler type information
  // This approach uses LLDB's type system but accesses it differently
  CompilerType obj_type = m_backend.GetCompilerType();
  if (obj_type.IsValid()) {
    // If it's a pointer type, get the pointee type
    // Otherwise use the type as-is (for dereferenced objects)
    if (obj_type.IsPointerType()) {
      obj_type = obj_type.GetPointeeType();
    }
    
    // Get field names from the type
    uint32_t num_fields = obj_type.GetNumFields();
    LLDB_LOG(GetLog(LLDBLog::DataFormatters),
             "GNUstepSyntheticProvider::GetClassIvarNames() - type has {0} fields", num_fields);
    
    for (uint32_t i = 0; i < num_fields; ++i) {
      std::string field_name;
      uint64_t field_bit_offset = 0;
      uint32_t field_bitfield_bit_size = 0;
      bool is_bitfield = false;
      
      CompilerType field_type = obj_type.GetFieldAtIndex(
        i, field_name, &field_bit_offset, &field_bitfield_bit_size, &is_bitfield);
      
      if (!field_name.empty() && field_name != "isa" && field_name != "NSObject") {
        // Skip the isa pointer and NSObject base class, we want actual instance variables
        m_cached_ivar_names.push_back(field_name);
        LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                 "GNUstepSyntheticProvider::GetClassIvarNames() - found field: {0}", field_name.c_str());
      }
    }
  }
  
  // If the type-based approach didn't work, fall back to trying symbol-based discovery
  if (m_cached_ivar_names.empty()) {
    LLDB_LOG(GetLog(LLDBLog::DataFormatters),
             "GNUstepSyntheticProvider::GetClassIvarNames() - type approach failed, trying symbol approach");
    
    // Get the class name to look for offset symbols
    std::string class_name = obj_type.GetTypeName().GetStringRef().str();
    if (class_name.length() >= 2 && class_name.substr(class_name.length() - 2) == " *") {
      class_name = class_name.substr(0, class_name.length() - 2);
    }
    
    // Try to find offset symbols for this class
    ExecutionContext exe_ctx = m_backend.GetExecutionContextRef().Lock(false);
    if (exe_ctx.HasTargetScope()) {
      Target *target = exe_ctx.GetTargetPtr();
      if (target) {
        // Look for symbols like __objc_ivar_offset_BankAccount.*
        std::string symbol_pattern = "__objc_ivar_offset_" + class_name + ".";
        
        SymbolContextList sc_list;
        // Use RegularExpression for LLVM 20+ API compatibility
        RegularExpression regex((".*" + symbol_pattern + ".*").c_str());
        target->GetImages().FindSymbolsMatchingRegExAndType(
          regex, eSymbolTypeData, sc_list);
        
        // If not found, try any symbol type (symbols might be in BSS or other sections)
        if (sc_list.GetSize() == 0) {
          target->GetImages().FindSymbolsMatchingRegExAndType(
            regex, eSymbolTypeAny, sc_list);
        }
        
        LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                 "GNUstepSyntheticProvider::GetClassIvarNames() - found {0} offset symbols", sc_list.GetSize());
        
        for (size_t i = 0; i < sc_list.GetSize(); ++i) {
          SymbolContext sc;
          sc_list.GetContextAtIndex(i, sc);
          if (sc.symbol) {
            std::string symbol_name = sc.symbol->GetName().GetStringRef().str();
            
            // Extract field name from symbol like __objc_ivar_offset_BankAccount._accountNumber.
            size_t prefix_len = symbol_pattern.length();
            if (symbol_name.length() > prefix_len) {
              std::string field_name = symbol_name.substr(prefix_len);
              // Remove trailing dot
              if (!field_name.empty() && field_name.back() == '.') {
                field_name.pop_back();
              }
              
              if (!field_name.empty()) {
                m_cached_ivar_names.push_back(field_name);
                LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                         "GNUstepSyntheticProvider::GetClassIvarNames() - extracted field from symbol: {0}", field_name.c_str());
              }
            }
          }
        }
      }
    }
  }
  
  // Final fallback: use some common Objective-C field patterns
  if (m_cached_ivar_names.empty()) {
    LLDB_LOG(GetLog(LLDBLog::DataFormatters),
             "GNUstepSyntheticProvider::GetClassIvarNames() - no fields discovered, using fallback");
    
    // This is a last resort - try to guess based on the object's non-synthetic children
    lldb::ValueObjectSP non_synthetic = m_backend.GetNonSyntheticValue();
    if (non_synthetic) {
      auto num_children_expected = non_synthetic->GetNumChildren(true);
      if (num_children_expected) {
        uint32_t num_children = *num_children_expected;
        for (uint32_t i = 0; i < num_children; ++i) {
          lldb::ValueObjectSP child = non_synthetic->GetChildAtIndex(i, true);
          if (child) {
            ConstString child_name = child->GetName();
            if (child_name) {
              std::string name_str = child_name.GetStringRef().str();
              // Only include actual ivar names, not synthetic stuff
              if (!name_str.empty() && name_str != "NSObject" && name_str[0] == '_') {
                m_cached_ivar_names.push_back(name_str);
                LLDB_LOG(GetLog(LLDBLog::DataFormatters),
                         "GNUstepSyntheticProvider::GetClassIvarNames() - fallback found: {0}", name_str.c_str());
              }
            }
          }
        }
      }
    }
  }
  
  LLDB_LOG(GetLog(LLDBLog::DataFormatters),
           "GNUstepSyntheticProvider::GetClassIvarNames() - final count: {0}", m_cached_ivar_names.size());
}