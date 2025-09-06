//===-- GNUstepSimpleUniversalFormatter.cpp ------------------------------===//
//
// Minimal universal formatter for GNUstep - just bridges to LLDB's machinery
//
//===----------------------------------------------------------------------===//

#include "GNUstepUniversalFormatter.h"
#include "../GNUstepObjCRuntime.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Target/Process.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Utility/DataExtractor.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

// Universal summary - get object description via runtime calls
bool lldb_private::formatters::GNUstepUniversalSummaryProvider(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  lldb::addr_t obj_addr = valobj.GetValueAsUnsigned(0);
  if (obj_addr == 0) {
    stream.Printf("nil");
    return true;
  }
  
  // Handle small integers (tagged pointer pattern: xxx...xx1 with tag 0)
  if ((obj_addr & 0x1) && ((obj_addr >> 1) & 0x7) == 0) {
    int64_t value = ((int64_t)obj_addr) >> 3;
    stream.Printf("%lld", (long long)value);
    return true;
  }
  
  // Get runtime and function caller
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;
    
  ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
  if (!runtime)
    return false;
    
  GNUstepObjCRuntime *gnustep_runtime = static_cast<GNUstepObjCRuntime*>(runtime);
  auto *function_caller = gnustep_runtime->GetRuntimeFunctionCaller();
  
  // Try to get object description via [obj description] -> UTF8String
  // This is what po/expr -o uses
  if (function_caller) {
    // Call [obj description] to get NSString*
    lldb::addr_t description_result = function_caller->CallObjCMethod(obj_addr, "description");
    if (description_result != LLDB_INVALID_ADDRESS && description_result != 0) {
      // Call [description_nsstring UTF8String] to get C string
      lldb::addr_t utf8_result = function_caller->CallObjCMethod(description_result, "UTF8String");
      if (utf8_result != LLDB_INVALID_ADDRESS && utf8_result != 0) {
        // Read the C string from memory
        Status error;
        std::string description_str;
        process_sp->ReadCStringFromMemory(utf8_result, description_str, error);
        if (error.Success() && !description_str.empty()) {
          stream.Printf("%s", description_str.c_str());
          return true;
        }
      }
    }
  }
  
  // Fallback: Get class name and show basic info
  ObjCLanguageRuntime::ClassDescriptorSP descriptor = runtime->GetClassDescriptor(valobj);
  if (!descriptor)
    return false;
    
  ConstString class_name_cs = descriptor->GetClassName();
  if (!class_name_cs)
    return false;
    
  const char *class_name = class_name_cs.GetCString();
  
  // For collections, try to show count at least
  if (strstr(class_name, "Array") || strstr(class_name, "Set") || strstr(class_name, "Dictionary")) {
    if (function_caller) {
      lldb::addr_t count_result = function_caller->CallObjCMethod(obj_addr, "count");
      if (count_result != LLDB_INVALID_ADDRESS && count_result < 1000000) {
        if (strstr(class_name, "Dictionary"))
          stream.Printf("%s[%llu entries]", class_name, (unsigned long long)count_result);
        else
          stream.Printf("%s[%llu]", class_name, (unsigned long long)count_result);
        return true;
      }
    }
  }
  
  // Default: show class and address
  stream.Printf("<%s: 0x%llx>", class_name, (unsigned long long)obj_addr);
  return true;
}

// Minimal synthetic provider
GNUstepUniversalSyntheticProvider::GNUstepUniversalSyntheticProvider(
    ValueObject &valobj)
    : SyntheticChildrenFrontEnd(valobj),
      m_exe_ctx(valobj.GetExecutionContextRef()),
      m_obj_addr(0),
      m_count(0),
      m_type(Unknown) {
  
  // Get the actual pointer value - this is critical!
  // For child value objects returned by GetChildAtIndex, we need the pointer value
  m_obj_addr = valobj.GetValueAsUnsigned(0);
  
  // If that's 0, try getting the load address
  if (m_obj_addr == 0) {
    m_obj_addr = valobj.GetLoadAddress();
  }
  
  fprintf(stderr, "[DEBUG] GNUstepUniversalSyntheticProvider constructor: obj_addr=0x%llx from %s\n", 
          (unsigned long long)m_obj_addr, valobj.GetName().AsCString());
  
  if (m_obj_addr == 0) {
    fprintf(stderr, "[DEBUG] Constructor: obj_addr is 0, cannot proceed\n");
    return;
  }
  
  // Pre-detect type in constructor
  m_type = DetectObjectType();
  fprintf(stderr, "[DEBUG] Constructor: detected type=%d\n", (int)m_type);
}

// Destructor is default in header

GNUstepUniversalSyntheticProvider::ObjectType 
GNUstepUniversalSyntheticProvider::DetectObjectType() {
  fprintf(stderr, "[DEBUG] DetectObjectType: obj_addr=0x%llx\n", (unsigned long long)m_obj_addr);
  
  if (m_obj_addr == 0) {
    fprintf(stderr, "[DEBUG] DetectObjectType: obj_addr is 0, returning Unknown\n");
    return Other;
  }
    
  if (m_obj_addr & 0x1) {
    fprintf(stderr, "[DEBUG] DetectObjectType: detected tagged pointer\n");
    return TaggedPointer;
  }
    
  // Get class name
  ProcessSP process_sp = m_backend.GetProcessSP();
  if (!process_sp) {
    fprintf(stderr, "[DEBUG] DetectObjectType: no process, returning Unknown\n");
    return Other;
  }
    
  ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
  if (!runtime) {
    fprintf(stderr, "[DEBUG] DetectObjectType: no runtime, returning Unknown\n");
    return Other;
  }
    
  ObjCLanguageRuntime::ClassDescriptorSP descriptor = runtime->GetClassDescriptor(m_backend);
  if (!descriptor) {
    fprintf(stderr, "[DEBUG] DetectObjectType: no class descriptor, returning Unknown\n");
    return Other;
  }
    
  ConstString class_name_cs = descriptor->GetClassName();
  if (!class_name_cs) {
    fprintf(stderr, "[DEBUG] DetectObjectType: no class name, returning Unknown\n");
    return Other;
  }
    
  std::string class_name = class_name_cs.GetCString();
  fprintf(stderr, "[DEBUG] DetectObjectType: class_name='%s'\n", class_name.c_str());
  
  if (class_name.find("Array") != std::string::npos) {
    fprintf(stderr, "[DEBUG] DetectObjectType: detected Array type\n");
    return Array;
  }
  if (class_name.find("Dictionary") != std::string::npos) {
    fprintf(stderr, "[DEBUG] DetectObjectType: detected Dictionary type\n");
    return Dictionary;
  }
  if (class_name.find("Set") != std::string::npos) {
    fprintf(stderr, "[DEBUG] DetectObjectType: detected Set type\n");
    return Set;
  }
    
  fprintf(stderr, "[DEBUG] DetectObjectType: detected Other type\n");
  return Other;
}

llvm::Expected<uint32_t> GNUstepUniversalSyntheticProvider::CalculateNumChildren() {
  fprintf(stderr, "[DEBUG] CalculateNumChildren: m_obj_addr=0x%llx, m_type=%d\n", 
          (unsigned long long)m_obj_addr, (int)m_type);
  
  if (m_type == Unknown)
    m_type = DetectObjectType();
    
  if (m_type == TaggedPointer || m_type == Other)
    return 0;
    
  // Get count via runtime
  ProcessSP process_sp = m_backend.GetProcessSP();
  if (!process_sp) {
    fprintf(stderr, "[DEBUG] CalculateNumChildren: no process\n");
    return 0;
  }
    
  ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
  GNUstepObjCRuntime *gnustep_runtime = static_cast<GNUstepObjCRuntime*>(runtime);
  if (!gnustep_runtime) {
    fprintf(stderr, "[DEBUG] CalculateNumChildren: no gnustep runtime\n");
    return 0;
  }
    
  auto *function_caller = gnustep_runtime->GetRuntimeFunctionCaller();
  if (!function_caller) {
    fprintf(stderr, "[DEBUG] CalculateNumChildren: no function caller\n");
    return 0;
  }
  
  fprintf(stderr, "[DEBUG] CalculateNumChildren: calling count on 0x%llx\n", 
          (unsigned long long)m_obj_addr);
    
  lldb::addr_t count_result = function_caller->CallObjCMethod(m_obj_addr, "count");
  if (count_result == LLDB_INVALID_ADDRESS || count_result > 1000000) {
    fprintf(stderr, "[DEBUG] CalculateNumChildren: count failed, result=0x%llx\n", 
            (unsigned long long)count_result);
    return 0;
  }
    
  m_count = (uint32_t)count_result;
  fprintf(stderr, "[DEBUG] CalculateNumChildren: count=%u\n", m_count);
  
  // Dictionary shows key/value pairs
  if (m_type == Dictionary)
    return m_count * 2;
    
  return m_count;
}

lldb::ValueObjectSP 
GNUstepUniversalSyntheticProvider::GetChildAtIndex(uint32_t idx) {
  fprintf(stderr, "[DEBUG] GetChildAtIndex(%u): m_obj_addr=0x%llx, m_type=%d\n", 
          idx, (unsigned long long)m_obj_addr, (int)m_type);
  
  ProcessSP process_sp = m_backend.GetProcessSP();
  if (!process_sp)
    return nullptr;
    
  ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
  GNUstepObjCRuntime *gnustep_runtime = static_cast<GNUstepObjCRuntime*>(runtime);
  if (!gnustep_runtime)
    return nullptr;
    
  auto *function_caller = gnustep_runtime->GetRuntimeFunctionCaller();
  if (!function_caller)
    return nullptr;
  
  // Get target and type system
  TargetSP target_sp = m_backend.GetTargetSP();
  if (!target_sp)
    return nullptr;
    
  TypeSystemClangSP scratch_ts_sp = ScratchTypeSystemClang::GetForTarget(*target_sp);
  if (!scratch_ts_sp)
    return nullptr;
  
  lldb::addr_t element_addr = LLDB_INVALID_ADDRESS;
  char name_buf[64];
  
  switch (m_type) {
    case Array: {
      fprintf(stderr, "[DEBUG] GetChildAtIndex: Calling objectAtIndex:%u on 0x%llx\n", 
              idx, (unsigned long long)m_obj_addr);
      
      // Call objectAtIndex: using objc_msgSend
      lldb::addr_t selector = function_caller->GetSelectorForName("objectAtIndex:");
      fprintf(stderr, "[DEBUG] GetChildAtIndex: selector = 0x%llx\n", (unsigned long long)selector);
      
      std::vector<lldb::addr_t> args = {m_obj_addr, selector, idx};
      element_addr = function_caller->CallRuntimeFunction("objc_msgSend", args);
      
      snprintf(name_buf, sizeof(name_buf), "[%u]", idx);
      fprintf(stderr, "[DEBUG] GetChildAtIndex: Array[%u] element_addr = 0x%llx\n", idx, (unsigned long long)element_addr);
      
      // Verify this is a valid object
      if (element_addr == LLDB_INVALID_ADDRESS || element_addr == 0) {
        fprintf(stderr, "[DEBUG] GetChildAtIndex: Invalid element address\n");
        return nullptr;
      }
      break;
    }
    
    case Dictionary: {
      // Get key/value pairs
      uint32_t pair_idx = idx / 2;
      bool is_key = (idx % 2 == 0);
      
      // Get allKeys array
      lldb::addr_t all_keys = function_caller->CallObjCMethod(m_obj_addr, "allKeys");
      if (all_keys == LLDB_INVALID_ADDRESS)
        return nullptr;
      
      // Get key at index
      std::vector<lldb::addr_t> args = {all_keys,
                                        function_caller->GetSelectorForName("objectAtIndex:"),
                                        pair_idx};
      lldb::addr_t key_addr = function_caller->CallRuntimeFunction("objc_msgSend", args);
      
      if (is_key) {
        element_addr = key_addr;
        snprintf(name_buf, sizeof(name_buf), "[%u].key", pair_idx);
      } else {
        // Get value for key
        std::vector<lldb::addr_t> value_args = {m_obj_addr,
                                                function_caller->GetSelectorForName("objectForKey:"),
                                                key_addr};
        element_addr = function_caller->CallRuntimeFunction("objc_msgSend", value_args);
        snprintf(name_buf, sizeof(name_buf), "[%u].value", pair_idx);
      }
      break;
    }
    
    case Set: {
      // Get allObjects array
      lldb::addr_t all_objects = function_caller->CallObjCMethod(m_obj_addr, "allObjects");
      if (all_objects == LLDB_INVALID_ADDRESS)
        return nullptr;
      
      // Get object at index
      std::vector<lldb::addr_t> args = {all_objects,
                                        function_caller->GetSelectorForName("objectAtIndex:"),
                                        idx};
      element_addr = function_caller->CallRuntimeFunction("objc_msgSend", args);
      
      snprintf(name_buf, sizeof(name_buf), "[%u]", idx);
      break;
    }
    
    default:
      return nullptr;
  }
  
  if (element_addr == LLDB_INVALID_ADDRESS)
    return nullptr;
  
  // Check if this is a tagged pointer
  bool is_tagged = (element_addr & 0x1) != 0;
  
  // Start with generic id type
  CompilerType child_type = CompilerType(
      scratch_ts_sp->weak_from_this(),
      scratch_ts_sp->getASTContext().ObjCBuiltinIdTy.getAsOpaquePtr());
  
  ValueObjectSP child_vo;
  
  if (is_tagged) {
    // For tagged pointers, create ValueObjectConstResult
    uint8_t buffer[8];
    memcpy(buffer, &element_addr, sizeof(element_addr));
    
    DataExtractor data(buffer, sizeof(buffer), 
                      m_exe_ctx.GetByteOrder(), 
                      m_exe_ctx.GetAddressByteSize());
    
    child_vo = ValueObjectConstResult::Create(
      m_exe_ctx.GetBestExecutionContextScope(),
      child_type,
      ConstString(name_buf),
      data,
      element_addr);
      
    if (child_vo) {
      child_vo->SetFormat(lldb::eFormatDefault);
    }
  } else {
    // For regular objects, try to get the actual class first
    // Create a temporary value object to inspect the class
    ValueObjectSP temp_vo = CreateValueObjectFromAddress(
        "temp", element_addr, m_exe_ctx, child_type);
    
    if (temp_vo) {
      ObjCLanguageRuntime::ClassDescriptorSP class_descriptor = runtime->GetClassDescriptor(*temp_vo);
      if (class_descriptor) {
        const char* class_name = class_descriptor->GetClassName().AsCString();
        fprintf(stderr, "[DEBUG] GetChildAtIndex: element at %s has class: %s\n", 
                name_buf, class_name ? class_name : "unknown");
        
        // Try to create a typed pointer for this class
        if (class_name) {
          // Check if we can find or create the proper type
          // For GNUstep classes like GSInlineArray, we may not have full type info
          // but we can still use id type and it should work with our formatters
          fprintf(stderr, "[DEBUG] GetChildAtIndex: keeping id type for %s (class %s)\n", 
                  name_buf, class_name);
        }
      }
    }
    
    // Create the child value object with proper address
    // The issue is that CreateValueObjectFromAddress creates a pointer TO the address,
    // but for ObjC objects, the value IS the address (it's already a pointer)
    
    // Create a data buffer containing the pointer value
    uint8_t ptr_buffer[8];
    memcpy(ptr_buffer, &element_addr, sizeof(element_addr));
    
    DataExtractor data(ptr_buffer, sizeof(ptr_buffer),
                      m_exe_ctx.GetByteOrder(),
                      m_exe_ctx.GetAddressByteSize());
    
    child_vo = ValueObjectConstResult::Create(
      m_exe_ctx.GetBestExecutionContextScope(),
      child_type,
      ConstString(name_buf),
      data,
      LLDB_INVALID_ADDRESS);  // No address, the value IS the pointer
    
    if (child_vo) {
      child_vo->SetFormat(lldb::eFormatDefault);
      
      fprintf(stderr, "[DEBUG] GetChildAtIndex: Created child %s with pointer value 0x%llx, actual value: 0x%llx\n",
              name_buf, (unsigned long long)element_addr, 
              (unsigned long long)child_vo->GetValueAsUnsigned(0));
      
      // Try dynamic type resolution as well
      ValueObjectSP dynamic_child = child_vo->GetDynamicValue(lldb::eDynamicCanRunTarget);
      if (dynamic_child) {
        dynamic_child->SetFormat(lldb::eFormatDefault);
        
        CompilerType dynamic_type = dynamic_child->GetCompilerType();
        if (dynamic_type.IsValid()) {
          const char* type_name = dynamic_type.GetTypeName().AsCString();
          fprintf(stderr, "[DEBUG] GetChildAtIndex: final dynamic type for %s = %s, addr=0x%llx\n", 
                  name_buf, type_name ? type_name : "unknown",
                  (unsigned long long)dynamic_child->GetValueAsUnsigned(0));
        }
        
        return dynamic_child;
      }
    }
  }
  
  // Debug: Print the final type
  if (child_vo) {
    CompilerType final_type = child_vo->GetCompilerType();
    if (final_type.IsValid()) {
      const char* type_name = final_type.GetTypeName().AsCString();
      fprintf(stderr, "[DEBUG] GetChildAtIndex: returning %s with type = %s\n", 
              name_buf, type_name ? type_name : "unknown");
    }
  }
  
  return child_vo;
}

lldb::ChildCacheState GNUstepUniversalSyntheticProvider::Update() {
  // Detect type once and cache it
  if (m_type == Unknown) {
    m_type = DetectObjectType();
  }
  // Return eReuse to avoid constant recalculation
  return lldb::ChildCacheState::eReuse;
}

bool GNUstepUniversalSyntheticProvider::MightHaveChildren() {
  // Always return true for Unknown type to allow LLDB to check
  // This is important for recursive expansion
  if (m_type == Unknown) {
    m_type = DetectObjectType();
  }
  return (m_type == Array || m_type == Dictionary || m_type == Set);
}

size_t GNUstepUniversalSyntheticProvider::GetIndexOfChildWithName(ConstString name) {
  return UINT32_MAX;
}

// Creator function
SyntheticChildrenFrontEnd *
lldb_private::formatters::GNUstepUniversalSyntheticProviderCreator(
    CXXSyntheticChildren *synth, lldb::ValueObjectSP valobj_sp) {
  return (valobj_sp ? new GNUstepUniversalSyntheticProvider(*valobj_sp) : nullptr);
}