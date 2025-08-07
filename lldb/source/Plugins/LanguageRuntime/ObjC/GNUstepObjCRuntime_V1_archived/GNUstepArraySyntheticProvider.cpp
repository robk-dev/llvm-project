//===-- GNUstepArraySyntheticProvider.cpp ----------------------*- C++ -*-===//
//
// Synthetic children provider for GNUstep NSArray objects
// Follows Apple NSArray pattern exactly for compatibility
// LLVM 20.1.8 - No dynamic introspection, pure address calculation
//
//===----------------------------------------------------------------------===//

#include "GNUstepArraySyntheticProvider.h"
#include "GNUstepObjCRuntime.h"
#include "GNUstepUtilities.h"
#include "RuntimeIntrospector.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"

#include "lldb/ValueObject/ValueObject.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

#include <set>

using namespace lldb;
using namespace lldb_private;

GNUstepArraySyntheticProvider::GNUstepArraySyntheticProvider(ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp),
      m_exe_ctx_ref(), m_id_type(),
      m_array_ptr(LLDB_INVALID_ADDRESS),
      m_count(0),
      m_data_address(LLDB_INVALID_ADDRESS),
      m_ptr_size(8),
      m_runtime_introspector(nullptr),
      m_contents_array_offset(-1),
      m_count_offset(-1),
      m_offsets_discovered(false) {
  
  // Initialize execution context (Apple pattern)
  if (valobj_sp) {
    m_exe_ctx_ref = valobj_sp->GetExecutionContextRef();
    
    // Get runtime introspector from the runtime
    if (ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP()) {
      if (auto *runtime = llvm::dyn_cast_or_null<GNUstepObjCRuntime>(
            process_sp->GetLanguageRuntime(lldb::eLanguageTypeObjC))) {
        m_runtime_introspector = runtime->GetRuntimeIntrospector();
      }
    }
    
    // Initialize id type (Apple pattern)
    if (Target *target = m_exe_ctx_ref.GetTargetSP().get()) {
      if (auto scratch_ts_sp = target->GetScratchTypeSystemForLanguage(eLanguageTypeObjC)) {
        if (auto *clang_ts = llvm::dyn_cast_or_null<TypeSystemClang>(scratch_ts_sp->get())) {
          m_id_type = clang_ts->GetType(clang_ts->getASTContext().ObjCBuiltinIdTy);
        }
      }
    }
  }
  
  Update();
}

lldb::ChildCacheState GNUstepArraySyntheticProvider::Update() {
  // RECURSION GUARD: Use static set to prevent infinite recursion
  static std::set<addr_t> processing_addresses;
  
  m_array_ptr = LLDB_INVALID_ADDRESS;
  m_count = 0;
  m_data_address = LLDB_INVALID_ADDRESS;
  
  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (!valobj_sp)
    return lldb::ChildCacheState::eRefetch;
    
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp)
    return lldb::ChildCacheState::eRefetch;
  
  m_ptr_size = process_sp->GetAddressByteSize();
  m_array_ptr = valobj_sp->GetPointerValue();
  
  if (m_array_ptr == 0 || m_array_ptr == LLDB_INVALID_ADDRESS)
    return lldb::ChildCacheState::eRefetch;
  
  // Check for recursion
  if (processing_addresses.count(m_array_ptr) > 0) {
    return lldb::ChildCacheState::eRefetch;
  }
  
  // Add to processing set
  processing_addresses.insert(m_array_ptr);
  
  if (!ExtractArrayInfo()) {
    processing_addresses.erase(m_array_ptr);
    return lldb::ChildCacheState::eRefetch;
  }
  
  // Remove from processing set before returning
  processing_addresses.erase(m_array_ptr);
  
  return lldb::ChildCacheState::eRefetch;
}

bool GNUstepArraySyntheticProvider::DiscoverIvarOffsets() {
  if (m_offsets_discovered)
    return true;
    
  if (!m_runtime_introspector || m_class_name.empty())
    return false;
    
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  // Get offset for _contents_array ivar
  m_contents_array_offset = m_runtime_introspector->GetIvarOffset(m_class_name, "_contents_array");
  if (m_contents_array_offset < 0) {
    // GSInlineArray might not have _contents_array as a stored ivar
    // Try its superclass GSArray
    m_contents_array_offset = m_runtime_introspector->GetIvarOffset("GSArray", "_contents_array");
  }
  
  // Get offset for _count ivar
  m_count_offset = m_runtime_introspector->GetIvarOffset(m_class_name, "_count");
  if (m_count_offset < 0) {
    // Try superclass
    m_count_offset = m_runtime_introspector->GetIvarOffset("GSArray", "_count");
  }
  
  if (log) {
    log->Printf("[GNUstep] Runtime introspection for class %s:", m_class_name.c_str());
    log->Printf("[GNUstep]   _contents_array offset: %ld", m_contents_array_offset);
    log->Printf("[GNUstep]   _count offset: %ld", m_count_offset);
  }
  
  // If we still don't have offsets, fall back to defaults for GSArray
  if (m_contents_array_offset < 0) {
    m_contents_array_offset = 8;  // Default for GSArray
    if (log) {
      log->Printf("[GNUstep] Using default _contents_array offset: 8");
    }
  }
  
  if (m_count_offset < 0) {
    m_count_offset = 16;  // Default for GSArray  
    if (log) {
      log->Printf("[GNUstep] Using default _count offset: 16");
    }
  }
  
  m_offsets_discovered = true;
  return true;
}

bool GNUstepArraySyntheticProvider::ExtractArrayInfo() {
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp)
    return false;
    
  Status error;
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  // Get the actual runtime class name
  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (valobj_sp && m_array_ptr != LLDB_INVALID_ADDRESS) {
    // First try to get the runtime class name from the dynamic type
    ValueObjectSP dynamic_valobj_sp = valobj_sp->GetDynamicValue(eDynamicDontRunTarget);
    if (dynamic_valobj_sp) {
      CompilerType dynamic_type = dynamic_valobj_sp->GetCompilerType();
      if (dynamic_type.IsValid()) {
        m_class_name = dynamic_type.GetTypeName().AsCString("");
        if (log) {
          log->Printf("[GNUstep] Got dynamic type: %s", m_class_name.c_str());
        }
      }
    }
    
    // If dynamic type is still generic NSArray, try to get from ISA
    if (m_class_name.empty() || m_class_name == "NSArray" || m_class_name == "NSArray *") {
      // Try using the runtime to get the actual class name
      if (auto *runtime = llvm::dyn_cast_or_null<GNUstepObjCRuntime>(
            process_sp->GetLanguageRuntime(lldb::eLanguageTypeObjC))) {
        std::string runtime_class = runtime->GetClassNameFromObject(m_array_ptr);
        if (!runtime_class.empty()) {
          m_class_name = runtime_class;
          if (log) {
            log->Printf("[GNUstep] Got runtime class name: %s", m_class_name.c_str());
          }
        }
      }
    }
    
    // Fall back to compile-time type if needed
    if (m_class_name.empty()) {
      CompilerType valobj_type = valobj_sp->GetCompilerType();
      if (valobj_type.IsValid()) {
        m_class_name = valobj_type.GetTypeName().AsCString("");
        if (log) {
          log->Printf("[GNUstep] Using compile-time type: %s", m_class_name.c_str());
        }
      }
    }
    
    // Remove pointer indicator if present
    size_t star_pos = m_class_name.find(" *");
    if (star_pos != std::string::npos) {
      m_class_name = m_class_name.substr(0, star_pos);
    }
    star_pos = m_class_name.find("*");
    if (star_pos != std::string::npos) {
      m_class_name = m_class_name.substr(0, star_pos);
    }
  }
  
  if (log) {
    log->Printf("[GNUstep] Extracting array info for class: %s", m_class_name.c_str());
  }
  
  // Handle special array types
  if (m_class_name == "GSArray0" || m_class_name == "__NSArray0") {
    // Empty array - no elements
    m_count = 0;
    m_data_address = LLDB_INVALID_ADDRESS;
    if (log) {
      log->Printf("[GNUstep] GSArray0/empty array detected: count=0");
    }
    return true;
  }
  
  if (m_class_name == "GSArray1") {
    // Single element array - element likely stored inline
    m_count = 1;
    // The single element is typically stored right after the standard ivars
    // Try offset 24 (ISA + _contents_array + _count + padding)
    m_data_address = m_array_ptr + 24;
    if (log) {
      log->Printf("[GNUstep] GSArray1 single element array: count=1, data at 0x%llx",
                  (unsigned long long)m_data_address);
    }
    return true;
  }
  
  // For GSInlineArray, GSArray, GSMutableArray
  // First try to discover offsets dynamically
  if (!DiscoverIvarOffsets()) {
    if (log) {
      log->Printf("[GNUstep] Failed to discover ivar offsets for class %s", m_class_name.c_str());
    }
  }
  
  // For GSInlineArray, we need to find the count
  // The count could be at various offsets depending on the exact implementation
  uint32_t count = 0;
  ptrdiff_t actual_count_offset = -1;
  
  // Scan memory to find a reasonable count value
  // We know the array has 4 elements, so look for that value
  // Try offsets from 8 to 64 in 4-byte increments
  for (ptrdiff_t offset = 8; offset <= 64; offset += 4) {
    uint32_t potential_count = process_sp->ReadUnsignedIntegerFromMemory(
        m_array_ptr + offset, 4, 0, error);
    
    if (!error.Fail() && potential_count > 0 && potential_count <= 100) {
      // This could be a valid count
      if (log) {
        log->Printf("[GNUstep] Found potential count %u at offset %ld", 
                    potential_count, offset);
      }
      
      // For GSInlineArray, check if we can find valid object pointers after the count
      // The elements should be right after the object structure
      bool looks_valid = true;
      if (m_class_name == "GSInlineArray" || m_class_name.find("GSInlineArray") != std::string::npos) {
        // Try to validate by checking if the memory after the instance looks like object pointers
        size_t instance_size = 24; // Default GSInlineArray size
        if (m_runtime_introspector) {
          size_t runtime_size = m_runtime_introspector->GetInstanceSize("GSInlineArray");
          if (runtime_size > 0) {
            instance_size = runtime_size;
          }
        }
        
        // Check if the potential element addresses look valid
        for (uint32_t i = 0; i < potential_count && i < 2; i++) {
          addr_t element_addr = process_sp->ReadPointerFromMemory(
              m_array_ptr + instance_size + (i * m_ptr_size), error);
          if (error.Fail() || element_addr == 0 || element_addr == LLDB_INVALID_ADDRESS) {
            looks_valid = false;
            break;
          }
          // Check if it looks like a valid pointer (within reasonable range)
          if (element_addr < 0x1000 || element_addr > 0x7FFFFFFFFFFF) {
            looks_valid = false;
            break;
          }
        }
      }
      
      if (looks_valid) {
        count = potential_count;
        actual_count_offset = offset;
        break;
      }
    }
  }
  
  // If we didn't find a valid count, try the default offsets
  if (count == 0 || count > 100) {
    // Try the standard offset
    count = process_sp->ReadUnsignedIntegerFromMemory(
        m_array_ptr + 16, 4, 0, error);
    
    if (error.Fail() || count > 1000000) {
      if (log) {
        log->Printf("[GNUstep] Failed to find valid count");
        // Dump memory for debugging
        uint8_t buffer[64];
        size_t bytes_read = process_sp->ReadMemory(
            m_array_ptr, buffer, sizeof(buffer), error);
        if (!error.Fail() && bytes_read > 0) {
          log->Printf("[GNUstep] Memory dump at 0x%llx:", (unsigned long long)m_array_ptr);
          for (size_t i = 0; i < bytes_read; i += 8) {
            log->Printf("[GNUstep]   +%zu: 0x%02x%02x%02x%02x %02x%02x%02x%02x",
                        i, buffer[i+3], buffer[i+2], buffer[i+1], buffer[i],
                        buffer[i+7], buffer[i+6], buffer[i+5], buffer[i+4]);
          }
        }
      }
      m_count = 0;
      m_data_address = LLDB_INVALID_ADDRESS;
      return false;
    }
  }
  
  if (log) {
    log->Printf("[GNUstep] Using count=%u from offset %ld", count, actual_count_offset);
  }
  
  // Sanity check the count
  if (count > 1000000) {
    if (log) {
      log->Printf("[GNUstep] Count too large (%u), likely incorrect offset", count);
    }
    m_count = 0;
    m_data_address = LLDB_INVALID_ADDRESS;
    return false;
  }
  
  m_count = count;
  
  // Handle different array types
  if (m_class_name == "GSInlineArray" || m_class_name.find("GSInlineArray") != std::string::npos) {
    // GSInlineArray: elements are stored INLINE after the object instance
    // According to GSArray.m line 438:
    // _contents_array = (id*)(((void*)self) + class_getInstanceSize([self class]));
    
    // Get the instance size for proper offset calculation
    size_t instance_size = 0;
    if (m_runtime_introspector) {
      instance_size = m_runtime_introspector->GetInstanceSize("GSInlineArray");
      if (log) {
        log->Printf("[GNUstep] GSInlineArray instance size: %zu", instance_size);
      }
    }
    
    // If we couldn't get instance size, use default
    if (instance_size == 0) {
      // GSInlineArray inherits from GSArray which has:
      // ISA (8) + _contents_array (8) + _count (4) + padding (4) = 24 bytes
      instance_size = 24;
      if (log) {
        log->Printf("[GNUstep] Using default GSInlineArray instance size: 24");
      }
    }
    
    m_data_address = m_array_ptr + instance_size;
    
    if (log) {
      log->Printf("[GNUstep] GSInlineArray: count=%u, inline data at 0x%llx (object + %zu)", 
                  count, (unsigned long long)m_data_address, instance_size);
    }
  } else if (m_class_name == "GSArray" || m_class_name == "GSMutableArray" ||
             m_class_name.find("GSArray") != std::string::npos ||
             m_class_name.find("GSMutableArray") != std::string::npos) {
    // Regular GSArray/GSMutableArray: _contents_array is a pointer at offset 8
    addr_t contents_ptr = process_sp->ReadPointerFromMemory(
        m_array_ptr + 8, error);
    
    if (error.Fail() || contents_ptr == LLDB_INVALID_ADDRESS || contents_ptr == 0) {
      if (log) {
        log->Printf("[GNUstep] Failed to read contents pointer at offset 8: %s", 
                    error.AsCString());
      }
      m_data_address = LLDB_INVALID_ADDRESS;
      return false;
    }
    
    m_data_address = contents_ptr;
    
    if (log) {
      log->Printf("[GNUstep] %s: count=%u, data pointer at 0x%llx", 
                  m_class_name.c_str(), count, (unsigned long long)m_data_address);
    }
  } else {
    // Unknown array type - try default GSArray layout
    if (log) {
      log->Printf("[GNUstep] Unknown array type %s, trying GSArray layout", m_class_name.c_str());
    }
    
    addr_t contents_ptr = process_sp->ReadPointerFromMemory(
        m_array_ptr + 8, error);
    
    if (!error.Fail() && contents_ptr != 0 && contents_ptr != LLDB_INVALID_ADDRESS) {
      m_data_address = contents_ptr;
    } else {
      // Try inline layout as fallback
      m_data_address = m_array_ptr + 24;
    }
    
    if (log) {
      log->Printf("[GNUstep] Using fallback for %s: count=%u, data at 0x%llx",
                  m_class_name.c_str(), count, (unsigned long long)m_data_address);
    }
  }
  
  return true;
}

llvm::Expected<uint32_t> GNUstepArraySyntheticProvider::CalculateNumChildren() {
  Log *log = GetLog(LLDBLog::DataFormatters);
  if (log) {
    log->Printf("[GNUstep] CalculateNumChildren returning count=%u for array at 0x%llx", 
                m_count, (unsigned long long)m_array_ptr);
  }
  return m_count;
}

bool GNUstepArraySyntheticProvider::MightHaveChildren() {
  return m_count > 0;
}

lldb::ValueObjectSP GNUstepArraySyntheticProvider::GetChildAtIndex(uint32_t idx) {
  if (idx >= m_count)
    return ValueObjectSP();
    
  // Validate data address
  if (m_data_address == LLDB_INVALID_ADDRESS || m_data_address == 0)
    return ValueObjectSP();
  
  // APPLE PATTERN: Direct address calculation like NSArray.cpp line 490-499
  // Each array element is a pointer (8 bytes on 64-bit systems)
  addr_t object_at_idx = m_data_address + (idx * m_ptr_size);
  
  Log *log = GetLog(LLDBLog::DataFormatters);
  if (log) {
    log->Printf("[GNUstep] Array[%u]: data_addr=0x%llx, element_addr=0x%llx", 
                idx, (unsigned long long)m_data_address, (unsigned long long)object_at_idx);
  }
  
  // Create name like Apple does
  char name_buffer[32];
  snprintf(name_buffer, sizeof(name_buffer), "[%u]", idx);
  
  // APPLE PATTERN: CreateValueObjectFromAddress(name, object_at_idx, m_exe_ctx_ref, m_id_type)
  return CreateValueObjectFromAddress(name_buffer, object_at_idx, m_exe_ctx_ref, m_id_type);
}

size_t GNUstepArraySyntheticProvider::GetIndexOfChildWithName(ConstString name) {
  // Handle array index notation [n] - Apple pattern
  std::string name_str = name.GetStringRef().str();
  if (name_str.size() >= 3 && name_str[0] == '[' && name_str[name_str.size()-1] == ']') {
    std::string index_str = name_str.substr(1, name_str.size()-2);
    size_t idx = 0;
    if (sscanf(index_str.c_str(), "%zu", &idx) == 1) {
      if (idx < m_count)
        return idx;
    }
  }
  return UINT32_MAX;
}

// Free function implementation for formatters namespace (Apple Pattern Integration)
namespace lldb_private {
namespace formatters {

SyntheticChildrenFrontEnd *
GNUstepArraySyntheticFrontEndCreator(CXXSyntheticChildren *, 
                                    lldb::ValueObjectSP valobj_sp) {
  return new GNUstepArraySyntheticProvider(valobj_sp);
}

} // namespace formatters
} // namespace lldb_private