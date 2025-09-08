//===-- GNUstepSimpleUniversalFormatter.cpp ------------------------------===//
//
// Minimal universal formatter for GNUstep - just bridges to LLDB's machinery
//
//===----------------------------------------------------------------------===//

#include "GNUstepUniversalFormatter.h"
#include "GNUstepObjCRuntime.h"
#include "GNUstepObjCRuntimeIntrospector.h"
#include "GNUstepObjCRuntimeUtilities.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Stream.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

// Helper function to format child names consistently
static void FormatChildName(char *buffer, size_t buffer_size,
                            const char *format, unsigned idx) {
  int written = snprintf(buffer, buffer_size, format, idx);
  if (written < 0 || written >= (int)buffer_size) {
    // Ensure null termination on error or truncation
    buffer[buffer_size - 1] = '\0';
  }
}

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
    // Check if this might be a boolean (0 or 1)
    if (value == 0 || value == 1) {
      // Try to determine if it's a boolean by checking the context
      // For now, just show both formats for 0 and 1
      if (value == 0)
        stream.Printf("NO");
      else
        stream.Printf("YES");
    } else {
      stream.Printf("%lld", (long long)value);
    }
    return true;
  }

  // Get runtime and function caller
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;

  ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
  if (!runtime)
    return false;

  GNUstepObjCRuntime *gnustep_runtime =
      static_cast<GNUstepObjCRuntime *>(runtime);
  auto *function_caller = gnustep_runtime->GetRuntimeFunctionCaller();

  // Try to get object description via [obj description] -> UTF8String
  // This is what po/expr -o uses
  if (function_caller) {
    // Call [obj description] to get NSString*
    lldb::addr_t description_result =
        function_caller->CallObjCMethod(obj_addr, "description");
    if (description_result != LLDB_INVALID_ADDRESS && description_result != 0) {
      // Call [description_nsstring UTF8String] to get C string
      lldb::addr_t utf8_result =
          function_caller->CallObjCMethod(description_result, "UTF8String");
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

  // Validate that this is a real object before trying to get its class
  // Check if the address looks valid (not too low, not uninitialized stack)
  if (obj_addr < 0x1000 ||
      (obj_addr & 0xFFFF000000000000) == 0xFFFF000000000000) {
    // Likely uninitialized or invalid pointer
    stream.Printf("0x%llx", (unsigned long long)obj_addr);
    return true;
  }

  // Fallback: Get class name and show basic info
  ObjCLanguageRuntime::ClassDescriptorSP descriptor =
      runtime->GetClassDescriptor(valobj);
  if (!descriptor) {
    // Try to at least show the address for custom classes
    stream.Printf("0x%llx", (unsigned long long)obj_addr);
    return true;
  }

  ConstString class_name_cs = descriptor->GetClassName();
  if (!class_name_cs)
    return false;

  const char *class_name = class_name_cs.GetCString();

  // For collections, try to show count at least
  if (strstr(class_name, "Array") || strstr(class_name, "Set") ||
      strstr(class_name, "Dictionary")) {
    if (function_caller) {
      lldb::addr_t count_result =
          function_caller->CallObjCMethod(obj_addr, "count");
      if (count_result != LLDB_INVALID_ADDRESS && count_result < 1000000) {
        if (strstr(class_name, "Dictionary"))
          stream.Printf("%s[%llu entries]", class_name,
                        (unsigned long long)count_result);
        else
          stream.Printf("%s[%llu]", class_name,
                        (unsigned long long)count_result);
        return true;
      }
    }
  }

  // For custom classes, try calling description again with a different approach
  // Sometimes the first attempt fails due to timing
  if (function_caller && !strstr(class_name, "NS") &&
      !strstr(class_name, "GS")) {
    // This looks like a custom class, try description one more time
    lldb::addr_t desc_result =
        function_caller->CallObjCMethod(obj_addr, "description");
    if (desc_result != LLDB_INVALID_ADDRESS && desc_result != 0) {
      lldb::addr_t utf8_result =
          function_caller->CallObjCMethod(desc_result, "UTF8String");
      if (utf8_result != LLDB_INVALID_ADDRESS && utf8_result != 0) {
        Status error;
        std::string desc_str;
        process_sp->ReadCStringFromMemory(utf8_result, desc_str, error);
        if (error.Success() && !desc_str.empty()) {
          stream.Printf("%s", desc_str.c_str());
          return true;
        }
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
      m_exe_ctx(valobj.GetExecutionContextRef()), m_obj_addr(0), m_count(0),
      m_type(Unknown), m_class_descriptor(nullptr) {

  m_obj_addr = valobj.GetValueAsUnsigned(0);

  if (m_obj_addr == 0) {
    m_obj_addr = valobj.GetLoadAddress();
  }

  if (m_obj_addr == 0) {
    return;
  }

  m_type = DetectObjectType();
}

GNUstepUniversalSyntheticProvider::ObjectType
GNUstepUniversalSyntheticProvider::DetectObjectType() {

  if (m_obj_addr == 0) {
    return Other;
  }

  if (m_obj_addr & 0x1) {
    return TaggedPointer;
  }

  // Get class name
  ProcessSP process_sp = m_backend.GetProcessSP();
  if (!process_sp) {
    return Other;
  }

  ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
  if (!runtime) {
    return Other;
  }

  ObjCLanguageRuntime::ClassDescriptorSP descriptor =
      runtime->GetClassDescriptor(m_backend);
  if (!descriptor) {
    return Other;
  }

  ConstString class_name_cs = descriptor->GetClassName();
  if (!class_name_cs) {
    return Other;
  }

  std::string class_name = class_name_cs.GetCString();

  m_class_descriptor = descriptor;

  // Check for collection types - be more specific to avoid false positives
  // NSArray, NSMutableArray, GSArray, etc.
  if (class_name.find("Array") != std::string::npos &&
      class_name.find("CharSet") == std::string::npos) {
    return Array;
  }
  // NSDictionary, NSMutableDictionary, GSDictionary, etc.
  if (class_name.find("Dictionary") != std::string::npos) {
    return Dictionary;
  }
  // NSSet, NSMutableSet, GSSet, etc. - but NOT CharacterSet classes
  if (class_name.find("Set") != std::string::npos &&
      class_name.find("CharSet") == std::string::npos &&
      class_name.find("CharacterSet") == std::string::npos &&
      class_name.find("IndexSet") == std::string::npos) {
    return Set;
  }

  // Check if this is a custom class (not NS or GS prefix)
  if (class_name.find("NS") != 0 && class_name.find("GS") != 0 &&
      class_name.find("__NS") != 0 && class_name.find("_NS") != 0) {
    return CustomClass;
  }

  return Other;
}

llvm::Expected<uint32_t>
GNUstepUniversalSyntheticProvider::CalculateNumChildren() {
  Process *process = m_exe_ctx.GetProcessPtr();
  if (!process)
    return 0;

  // Check process state before accessing memory
  if (process->GetState() != lldb::eStateStopped) {
    return 0;
  }

  if (m_type == Unknown)
    m_type = DetectObjectType();

  if (m_type == TaggedPointer || m_type == Other)
    return 0;

  if (m_type == CustomClass) {
    // Get number of ivars using the class descriptor
    ProcessSP process_sp = m_backend.GetProcessSP();
    if (!process_sp)
      return 0;

    ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
    if (!runtime)
      return 0;

    GNUstepObjCRuntime *gnustep_runtime =
        static_cast<GNUstepObjCRuntime *>(runtime);
    if (!gnustep_runtime)
      return 0;

    GNUstepObjCRuntimeIntrospector *introspector =
        gnustep_runtime->GetRuntimeIntrospector();
    if (!introspector)
      return 0;

    // Get or create class descriptor
    if (!m_class_descriptor) {
      m_class_descriptor = runtime->GetClassDescriptor(m_backend);
      if (!m_class_descriptor)
        return 0;
    }

    lldb::addr_t isa = m_class_descriptor->GetISA();
    if (isa == 0)
      return 0;

    // Get all ivars including inherited ones
    auto ivars_result =
        introspector->GetAllIvarsIncludingInherited((void *)isa);
    if (!ivars_result)
      return 0;

    return ivars_result->size();
  }

  if (m_type != Array && m_type != Dictionary && m_type != Set) {
    return 0;
  }

  ProcessSP process_sp = m_backend.GetProcessSP();
  if (!process_sp) {
    return 0;
  }

  ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
  GNUstepObjCRuntime *gnustep_runtime =
      static_cast<GNUstepObjCRuntime *>(runtime);
  if (!gnustep_runtime) {
    return 0;
  }

  auto *function_caller = gnustep_runtime->GetRuntimeFunctionCaller();
  if (!function_caller) {
    return 0;
  }

  lldb::addr_t count_result =
      function_caller->CallObjCMethod(m_obj_addr, "count");
  if (count_result == LLDB_INVALID_ADDRESS || count_result > 1000000) {
    return 0;
  }

  m_count = (uint32_t)count_result;

  uint32_t final_count = (m_type == Dictionary) ? m_count * 2 : m_count;
  return final_count;
}

lldb::ValueObjectSP
GNUstepUniversalSyntheticProvider::GetChildAtIndex(uint32_t idx) {

  ProcessSP process_sp = m_backend.GetProcessSP();
  if (!process_sp)
    return nullptr;

  // Check process state before accessing memory
  if (process_sp->GetState() != lldb::eStateStopped) {
    return nullptr;
  }

  ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
  GNUstepObjCRuntime *gnustep_runtime =
      static_cast<GNUstepObjCRuntime *>(runtime);
  if (!gnustep_runtime)
    return nullptr;

  auto *function_caller = gnustep_runtime->GetRuntimeFunctionCaller();
  if (!function_caller)
    return nullptr;

  // Get target and type system
  TargetSP target_sp = m_backend.GetTargetSP();
  if (!target_sp)
    return nullptr;

  TypeSystemClangSP scratch_ts_sp =
      ScratchTypeSystemClang::GetForTarget(*target_sp);
  if (!scratch_ts_sp)
    return nullptr;

  lldb::addr_t element_addr = LLDB_INVALID_ADDRESS;
  char name_buf[64];
  ValueObjectSP child_vo;

  switch (m_type) {
  case CustomClass: {
    // Get instance variables for custom classes using the introspector
    if (!m_class_descriptor)
      return nullptr;

    // Get the GNUstep runtime and introspector
    GNUstepObjCRuntime *gnustep_runtime =
        static_cast<GNUstepObjCRuntime *>(runtime);
    if (!gnustep_runtime)
      return nullptr;

    GNUstepObjCRuntimeIntrospector *introspector =
        gnustep_runtime->GetRuntimeIntrospector();
    if (!introspector)
      return nullptr;

    // Get ISA from the object's class descriptor
    if (!m_class_descriptor) {
      m_class_descriptor = runtime->GetClassDescriptor(m_backend);
      if (!m_class_descriptor)
        return nullptr;
    }

    lldb::addr_t isa = m_class_descriptor->GetISA();
    if (isa == 0)
      return nullptr;

    // Get all ivars including inherited ones
    auto ivars_result =
        introspector->GetAllIvarsIncludingInherited((void *)isa);
    if (!ivars_result) {
      return nullptr;
    }

    const std::vector<GNUstepObjCRuntimeIntrospector::IvarInfo> &ivars =
        *ivars_result;
    if (idx >= ivars.size())
      return nullptr;

    const auto &ivar = ivars[idx];
    // Direct copy of ivar name
    strncpy(name_buf, ivar.name.c_str(), sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';

    // Determine the type from the encoding
    CompilerType ivar_type;
    if (ivar.type_encoding[0] == '@') {
      // Object type - use id
      ivar_type = CompilerType(
          scratch_ts_sp->weak_from_this(),
          scratch_ts_sp->getASTContext().ObjCBuiltinIdTy.getAsOpaquePtr());
    } else if (ivar.type_encoding[0] == 'i') {
      // int
      ivar_type = scratch_ts_sp->GetBasicType(lldb::eBasicTypeInt);
    } else if (ivar.type_encoding[0] == 'f') {
      // float
      ivar_type = scratch_ts_sp->GetBasicType(lldb::eBasicTypeFloat);
    } else if (ivar.type_encoding[0] == 'd') {
      // double
      ivar_type = scratch_ts_sp->GetBasicType(lldb::eBasicTypeDouble);
    } else if (ivar.type_encoding[0] == 'q') {
      // long long
      ivar_type = scratch_ts_sp->GetBasicType(lldb::eBasicTypeLongLong);
    } else if (ivar.type_encoding[0] == 'Q') {
      // unsigned long long
      ivar_type = scratch_ts_sp->GetBasicType(lldb::eBasicTypeUnsignedLongLong);
    } else {
      // Default to void pointer for unknown types
      ivar_type =
          scratch_ts_sp->GetBasicType(lldb::eBasicTypeVoid).GetPointerType();
    }

    // Read the ivar value from memory
    lldb::addr_t ivar_addr = m_obj_addr + ivar.offset;
    Status error;

    // Allocate buffer based on type size
    size_t value_size = ivar_type.GetByteSize(nullptr).value_or(8);
    std::vector<uint8_t> buffer(value_size, 0);

    size_t bytes_read =
        process_sp->ReadMemory(ivar_addr, buffer.data(), value_size, error);

    if (error.Fail() || bytes_read == 0) {
      return nullptr;
    }

    // Create value object for the ivar
    DataExtractor data(buffer.data(), buffer.size(), m_exe_ctx.GetByteOrder(),
                       m_exe_ctx.GetAddressByteSize());

    child_vo = ValueObjectConstResult::Create(
        m_exe_ctx.GetBestExecutionContextScope(), ivar_type,
        ConstString(name_buf), data, LLDB_INVALID_ADDRESS);

    if (child_vo) {
      child_vo->SetFormat(lldb::eFormatDefault);
    }

    return child_vo;
  }

  case Array: {
    // Call objectAtIndex: directly
    lldb::addr_t selector =
        function_caller->GetSelectorForName("objectAtIndex:");
    std::vector<lldb::addr_t> args = {m_obj_addr, selector, idx};
    element_addr = function_caller->CallRuntimeFunction("objc_msgSend", args);

    if (element_addr == LLDB_INVALID_ADDRESS || element_addr == 0) {
      return nullptr;
    }

    FormatChildName(name_buf, sizeof(name_buf), "[%u]", idx);
    break;
  }

  case Dictionary: {
    // Get key/value pairs directly
    uint32_t pair_idx = idx / 2;
    bool is_key = (idx % 2 == 0);

    // Get allKeys array
    lldb::addr_t all_keys =
        function_caller->CallObjCMethod(m_obj_addr, "allKeys");
    if (all_keys == LLDB_INVALID_ADDRESS)
      return nullptr;

    // Get key at index
    std::vector<lldb::addr_t> args = {
        all_keys, function_caller->GetSelectorForName("objectAtIndex:"),
        pair_idx};
    lldb::addr_t key_addr =
        function_caller->CallRuntimeFunction("objc_msgSend", args);

    if (is_key) {
      element_addr = key_addr;
      FormatChildName(name_buf, sizeof(name_buf), "[%u].key", pair_idx);
    } else {
      // Get value for key
      std::vector<lldb::addr_t> value_args = {
          m_obj_addr, function_caller->GetSelectorForName("objectForKey:"),
          key_addr};
      element_addr =
          function_caller->CallRuntimeFunction("objc_msgSend", value_args);
      FormatChildName(name_buf, sizeof(name_buf), "[%u].value", pair_idx);
    }
    break;
  }

  case Set: {
    // Get allObjects array and fetch element directly
    lldb::addr_t all_objects =
        function_caller->CallObjCMethod(m_obj_addr, "allObjects");
    if (all_objects == LLDB_INVALID_ADDRESS)
      return nullptr;

    // Get object at index
    std::vector<lldb::addr_t> args = {
        all_objects, function_caller->GetSelectorForName("objectAtIndex:"),
        idx};
    element_addr = function_caller->CallRuntimeFunction("objc_msgSend", args);

    if (element_addr == LLDB_INVALID_ADDRESS || element_addr == 0) {
      return nullptr;
    }

    FormatChildName(name_buf, sizeof(name_buf), "[%u]", idx);
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

  if (is_tagged) {
    // For tagged pointers, create ValueObjectConstResult
    uint8_t buffer[8];
    memcpy(buffer, &element_addr, sizeof(element_addr));

    DataExtractor data(buffer, sizeof(buffer), m_exe_ctx.GetByteOrder(),
                       m_exe_ctx.GetAddressByteSize());

    child_vo = ValueObjectConstResult::Create(
        m_exe_ctx.GetBestExecutionContextScope(), child_type,
        ConstString(name_buf), data, element_addr);

    if (child_vo) {
      child_vo->SetFormat(lldb::eFormatDefault);
    }
  } else {
    // For regular objects, try to get the actual class first
    ValueObjectSP temp_vo = CreateValueObjectFromAddress("temp", element_addr,
                                                         m_exe_ctx, child_type);

    if (temp_vo) {
      ObjCLanguageRuntime::ClassDescriptorSP class_descriptor =
          runtime->GetClassDescriptor(*temp_vo);
      if (class_descriptor) {
        const char *class_name = class_descriptor->GetClassName().AsCString();

        if (class_name) {
        }
      }
    }

    // Create a data buffer containing the pointer value
    uint8_t ptr_buffer[8];
    memcpy(ptr_buffer, &element_addr, sizeof(element_addr));

    DataExtractor data(ptr_buffer, sizeof(ptr_buffer), m_exe_ctx.GetByteOrder(),
                       m_exe_ctx.GetAddressByteSize());

    child_vo = ValueObjectConstResult::Create(
        m_exe_ctx.GetBestExecutionContextScope(), child_type,
        ConstString(name_buf), data,
        LLDB_INVALID_ADDRESS); // No address, the value IS the pointer

    if (child_vo) {
      child_vo->SetFormat(lldb::eFormatDefault);

      ValueObjectSP dynamic_child =
          child_vo->GetDynamicValue(lldb::eDynamicCanRunTarget);
      if (dynamic_child) {
        dynamic_child->SetFormat(lldb::eFormatDefault);

        lldb::addr_t dynamic_addr = dynamic_child->GetValueAsUnsigned(0);
        if (dynamic_addr != element_addr) {
          return child_vo;
        }

        CompilerType dynamic_type = dynamic_child->GetCompilerType();

        return dynamic_child;
      }
    }
  }

  // Return the child ValueObject as-is

  return child_vo;
}

lldb::ChildCacheState GNUstepUniversalSyntheticProvider::Update() {
  return lldb::ChildCacheState::eReuse;
}

bool GNUstepUniversalSyntheticProvider::MightHaveChildren() {
  // Always return true for Unknown type to allow LLDB to check
  // This is important for recursive expansion
  if (m_type == Unknown) {
    m_type = DetectObjectType();
  }
  return (m_type == Array || m_type == Dictionary || m_type == Set ||
          m_type == CustomClass);
}

size_t
GNUstepUniversalSyntheticProvider::GetIndexOfChildWithName(ConstString name) {
  return UINT32_MAX;
}

// Creator function
SyntheticChildrenFrontEnd *
lldb_private::formatters::GNUstepUniversalSyntheticProviderCreator(
    CXXSyntheticChildren *synth, lldb::ValueObjectSP valobj_sp) {
  return (valobj_sp ? new GNUstepUniversalSyntheticProvider(*valobj_sp)
                    : nullptr);
}