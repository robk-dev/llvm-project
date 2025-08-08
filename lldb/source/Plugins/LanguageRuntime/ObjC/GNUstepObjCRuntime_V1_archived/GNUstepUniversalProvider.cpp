//===-- GNUstepUniversalProvider.cpp -----------------------*- C++ -*-===//
//
// Universal Synthetic Children Provider for GNUstep Objects
// Uses LLDB's type system to discover field offsets dynamically
// Revolutionary approach: NO HARDCODED OFFSETS!
//
//===----------------------------------------------------------------------===//

#include "GNUstepUniversalProvider.h"
#include "GNUstepObjCRuntime.h"
#include "GNUstepUtilities.h"

#include "lldb/ValueObject/ValueObject.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

using namespace lldb;
using namespace lldb_private;

GNUstepUniversalProvider::GNUstepUniversalProvider(ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp),
      m_exe_ctx_ref(), m_id_type(),
      m_ptr_size(8),
      m_object_ptr(LLDB_INVALID_ADDRESS),
      m_actual_class_name(),
      m_is_collection(false), 
      m_count(0), 
      m_elements_address(LLDB_INVALID_ADDRESS) {
  
  // Initialize Apple pattern execution context
  if (valobj_sp) {
    m_exe_ctx_ref = valobj_sp->GetExecutionContextRef();
    
    // Initialize id type for CreateValueObjectFromAddress
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

lldb::ChildCacheState GNUstepUniversalProvider::Update() {
  // Reset state
  m_object_ptr = LLDB_INVALID_ADDRESS;
  m_actual_class_name.clear();
  m_is_collection = false;
  m_count = 0;
  m_elements_address = LLDB_INVALID_ADDRESS;
  m_discovered_fields.clear();
  
  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (!valobj_sp)
    return lldb::ChildCacheState::eRefetch;
    
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp)
    return lldb::ChildCacheState::eRefetch;
  
  m_ptr_size = process_sp->GetAddressByteSize();
  m_object_ptr = valobj_sp->GetPointerValue();
  
  if (m_object_ptr == 0 || m_object_ptr == LLDB_INVALID_ADDRESS)
    return lldb::ChildCacheState::eRefetch;
  
  // REVOLUTIONARY APPROACH: Let LLDB's type system tell us everything!
  if (!AnalyzeObjectType())
    return lldb::ChildCacheState::eRefetch;
    
  if (!DiscoverFieldsFromTypeSystem())
    return lldb::ChildCacheState::eRefetch;
    
  if (!DetectCollectionPattern())
    return lldb::ChildCacheState::eRefetch;
  
  return lldb::ChildCacheState::eRefetch;
}

bool GNUstepUniversalProvider::AnalyzeObjectType() {
  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (!valobj_sp)
    return false;
  
  // Get the runtime class name if available
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (process_sp) {
    if (ObjCLanguageRuntime *objc_runtime = ObjCLanguageRuntime::Get(*process_sp)) {
      if (GNUstepObjCRuntime *runtime = static_cast<GNUstepObjCRuntime*>(objc_runtime)) {
        m_actual_class_name = runtime->GetClassNameFromObject(m_object_ptr);
      }
    }
  }
  
  // Fallback: use static type information
  if (m_actual_class_name.empty()) {
    CompilerType valobj_type = valobj_sp->GetCompilerType();
    if (valobj_type.IsValid()) {
      m_actual_class_name = valobj_type.GetTypeName().AsCString("");
    }
  }
  
  return !m_actual_class_name.empty();
}

bool GNUstepUniversalProvider::DiscoverFieldsFromTypeSystem() {
  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (!valobj_sp)
    return false;
    
  Target *target = m_exe_ctx_ref.GetTargetSP().get();
  if (!target)
    return false;
  
  // SIMPLIFIED APPROACH: Get the compiler type from the object itself
  // This avoids the complex GetTypeForIdentifier API issues
  CompilerType class_type = valobj_sp->GetCompilerType();
  
  // If the compiler type is not useful, try to get it from the static type
  if (!class_type.IsValid()) {
    CompilerType static_type = valobj_sp->GetStaticValue()->GetCompilerType();
    if (static_type.IsValid()) {
      class_type = static_type;
    }
  }
  
  // If we found a type, iterate through its fields
  if (class_type.IsValid()) {
    uint32_t num_fields = class_type.GetNumDirectBaseClasses() + class_type.GetNumFields();
    
    for (uint32_t i = 0; i < num_fields; ++i) {
      std::string field_name;
      uint64_t bit_offset;
      uint32_t bitfield_bit_size;
      bool is_bitfield;
      
      // Get field information from type system (LLVM 20.1.8 API)
      CompilerType field_type = class_type.GetFieldAtIndex(i, field_name, &bit_offset, 
                                                          &bitfield_bit_size, &is_bitfield);
      
      if (field_type.IsValid()) {
        FieldInfo field_info;
        field_info.name = field_name;
        field_info.offset = bit_offset / 8;  // Convert bits to bytes
        field_info.size = field_type.GetByteSize(nullptr).value_or(0);
        field_info.type = field_type;
        
        m_discovered_fields.push_back(field_info);
      }
    }
  }
  
  return !m_discovered_fields.empty();
}

bool GNUstepUniversalProvider::DetectCollectionPattern() {
  // USE EXISTING GNUSTEP INFRASTRUCTURE instead of reinventing!
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp)
    return false;
  
  // Use GNUstepCollectionHandler to detect collection type
  GNUstepCollectionHandler::CollectionType coll_type = 
      GNUstepCollectionHandler::DetectCollectionType(m_actual_class_name);
  
  m_is_collection = (coll_type != GNUstepCollectionHandler::kNotCollection);
  
  // DEBUG: Add logging to understand what's happening
  Log *log = GetLog(LLDBLog::DataFormatters);
  if (log) {
    log->Printf("GNUstepUniversalProvider::DetectCollectionPattern - class: %s, is_collection: %s", 
                m_actual_class_name.c_str(), m_is_collection ? "YES" : "NO");
  }
  
  if (!m_is_collection) {
    return true;  // Not a collection, that's fine
  }
  
  // Use existing infrastructure to get collection info
  GNUstepMemoryReader reader(process_sp.get());
  
  if (coll_type == GNUstepCollectionHandler::kArray) {
    auto array_info = GNUstepCollectionHandler::GetArrayInfo(
        m_object_ptr, m_actual_class_name, reader);
    
    m_count = array_info.count;
    m_elements_address = array_info.elements_ptr;
    
    // DEBUG: Log array info
    if (log) {
      log->Printf("GNUstepUniversalProvider::DetectCollectionPattern - Array info: count=%zu, elements_ptr=0x%llx", 
                  m_count, (unsigned long long)m_elements_address);
    }
    
    // Validate the information
    if (m_count > 0 && m_elements_address != LLDB_INVALID_ADDRESS) {
      return true;
    }
  } else if (coll_type == GNUstepCollectionHandler::kDictionary) {
    auto dict_info = GNUstepCollectionHandler::GetDictionaryInfo(
        m_object_ptr, m_actual_class_name, reader);
    
    m_count = dict_info.count;
    // For dictionaries, we'd need to implement key-value pair iteration
    // For now, just report the count
    m_elements_address = LLDB_INVALID_ADDRESS;  // Not applicable for dicts
    
    return true;
  } else if (coll_type == GNUstepCollectionHandler::kString) {
    // Strings are not indexed collections for synthetic children
    m_is_collection = false;
    return true;
  }
  
  return true;
}

bool GNUstepUniversalProvider::CheckInheritanceForCollectionTypes() {
  // SIMPLIFIED: Just use the existing GNUstep collection detection
  // This avoids complex inheritance traversal that might not work correctly
  GNUstepCollectionHandler::CollectionType coll_type = 
      GNUstepCollectionHandler::DetectCollectionType(m_actual_class_name);
  
  return (coll_type != GNUstepCollectionHandler::kNotCollection);
}

bool GNUstepUniversalProvider::TraverseInheritanceHierarchy(
    CompilerType type, const std::vector<std::string>& target_classes, int depth) {
  
  // Simplified implementation - just return false for now
  // The main collection detection is handled by existing GNUstep infrastructure
  return false;
}

lldb::addr_t GNUstepUniversalProvider::GetDataAddressForCollection() {
  return m_elements_address;
}

llvm::Expected<uint32_t> GNUstepUniversalProvider::CalculateNumChildren() {
  if (m_is_collection) {
    return m_count;
  }
  // For custom objects, return the number of discovered fields (instance variables)
  return m_discovered_fields.size();
}

bool GNUstepUniversalProvider::MightHaveChildren() {
  if (m_is_collection) {
    return m_count > 0;
  }
  // For custom objects, check if we have discovered fields
  return !m_discovered_fields.empty();
}

lldb::ValueObjectSP GNUstepUniversalProvider::GetChildAtIndex(uint32_t idx) {
  if (m_is_collection) {
    if (idx >= m_count)
      return ValueObjectSP();
      
    // APPLE PATTERN: Direct address calculation like NSArray.cpp
    lldb::addr_t element_address = m_elements_address + (idx * m_ptr_size);
    
    // Create name like Apple does
    char name_buffer[32];
    snprintf(name_buffer, sizeof(name_buffer), "[%u]", idx);
    
    // APPLE PATTERN: CreateValueObjectFromAddress
    return CreateValueObjectFromAddress(name_buffer, element_address, m_exe_ctx_ref, m_id_type);
  }
  
  // For custom objects, return the field at the given index
  if (idx >= m_discovered_fields.size())
    return ValueObjectSP();
    
  const FieldInfo &field = m_discovered_fields[idx];
  lldb::addr_t field_address = m_object_ptr + field.offset;
  
  // Create ValueObject for this field using the field's type
  return CreateValueObjectFromAddress(field.name.c_str(), field_address, m_exe_ctx_ref, field.type);
}

size_t GNUstepUniversalProvider::GetIndexOfChildWithName(ConstString name) {
  if (m_is_collection) {
    // Handle array index notation [n]
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
  
  // For custom objects, search for field by name
  std::string name_str = name.GetStringRef().str();
  for (size_t i = 0; i < m_discovered_fields.size(); ++i) {
    if (m_discovered_fields[i].name == name_str) {
      return i;
    }
  }
  return UINT32_MAX;
}

// Utility methods for field access
lldb::ValueObjectSP GNUstepUniversalProvider::ReadFieldByName(const std::string& field_name) {
  for (const auto& field : m_discovered_fields) {
    if (field.name == field_name) {
      ValueObjectSP valobj_sp = m_backend.GetSP();
      if (valobj_sp) {
        return valobj_sp->GetChildMemberWithName(ConstString(field_name));
      }
    }
  }
  return ValueObjectSP();
}

uint32_t GNUstepUniversalProvider::ReadUInt32Field(const std::string& field_name) {
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp)
    return 0;
    
  for (const auto& field : m_discovered_fields) {
    if (field.name == field_name) {
      Status error;
      uint32_t value = process_sp->ReadUnsignedIntegerFromMemory(
          m_object_ptr + field.offset, 4, 0, error);
      return error.Success() ? value : 0;
    }
  }
  return 0;
}

lldb::addr_t GNUstepUniversalProvider::ReadPointerField(const std::string& field_name) {
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp)
    return LLDB_INVALID_ADDRESS;
    
  for (const auto& field : m_discovered_fields) {
    if (field.name == field_name) {
      Status error;
      lldb::addr_t value = process_sp->ReadPointerFromMemory(
          m_object_ptr + field.offset, error);
      return error.Success() ? value : LLDB_INVALID_ADDRESS;
    }
  }
  return LLDB_INVALID_ADDRESS;
}

// Free function for formatter namespace
namespace lldb_private {
namespace formatters {

SyntheticChildrenFrontEnd *
GNUstepUniversalProviderCreator(CXXSyntheticChildren *, 
                               lldb::ValueObjectSP valobj_sp) {
  return new GNUstepUniversalProvider(valobj_sp);
}

} // namespace formatters
} // namespace lldb_private