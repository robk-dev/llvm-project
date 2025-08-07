//===-- GNUstepUniversalProvider.cpp -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepUniversalProvider.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace lldb_private {
namespace formatters {

// Universal summary provider for any Objective-C object
bool GNUstepUniversalSummaryProvider(ValueObject &valobj, Stream &stream,
                                     const TypeSummaryOptions &options) {
  
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "GNUstepUniversalSummaryProvider called for {0}", valobj.GetName());
  
  // Get object address
  lldb::addr_t obj_addr = valobj.GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }
  
  // Get process for runtime API access
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp) {
    stream.Printf("<invalid process>");
    return true;
  }
  
  // Initialize runtime API
  GNUstepRuntimeAPISP runtime_api = GNUstepRuntimeAPI::Create(process_sp.get());
  if (!runtime_api || !runtime_api->IsValid()) {
    // Fallback to basic information
    std::string type_name = valobj.GetTypeName().GetCString();
    if (!type_name.empty()) {
      stream.Printf("<%s: 0x%llx>", type_name.c_str(), (unsigned long long)obj_addr);
    } else {
      stream.Printf("<object: 0x%llx>", (unsigned long long)obj_addr);
    }
    return true;
  }
  
  // Get class information from runtime
  auto class_info_result = runtime_api->GetObjectClassInfo(obj_addr);
  if (!class_info_result) {
    // Fallback to type name
    std::string type_name = valobj.GetTypeName().GetCString();
    if (!type_name.empty()) {
      stream.Printf("<%s: 0x%llx>", type_name.c_str(), (unsigned long long)obj_addr);
    } else {
      stream.Printf("<object: 0x%llx>", (unsigned long long)obj_addr);
    }
    return true;
  }
  
  const auto &class_info = *class_info_result;
  
  // Show class name and address in standard format
  stream.Printf("<%s: 0x%llx>", class_info.name.c_str(), (unsigned long long)obj_addr);
  
  LLDB_LOG(log, "GNUstepUniversalSummaryProvider: Successfully formatted {0} at 0x{1:x}", 
           class_info.name, obj_addr);
  
  return true;
}

// Universal synthetic provider implementation
GNUstepUniversalSyntheticProvider::GNUstepUniversalSyntheticProvider(lldb::ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp), m_object_addr(LLDB_INVALID_ADDRESS) {
  
  if (valobj_sp) {
    Update();
  }
}

lldb::ChildCacheState GNUstepUniversalSyntheticProvider::Update() {
  // Clear previous state
  m_ivars.clear();
  m_object_addr = LLDB_INVALID_ADDRESS;
  m_class_name.clear();
  m_runtime_api.reset();
  
  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (!valobj_sp)
    return lldb::ChildCacheState::eRefetch;
  
  ProcessSP process_sp = valobj_sp->GetProcessSP();
  if (!process_sp)
    return lldb::ChildCacheState::eRefetch;
  
  // Get object address
  m_object_addr = valobj_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
  if (m_object_addr == 0 || m_object_addr == LLDB_INVALID_ADDRESS)
    return lldb::ChildCacheState::eRefetch;
  
  // Initialize runtime API
  m_runtime_api = GNUstepRuntimeAPI::Create(process_sp.get());
  if (!m_runtime_api || !m_runtime_api->IsValid()) {
    Log *log = GetLog(LLDBLog::DataFormatters);
    LLDB_LOG(log, "GNUstepUniversalSyntheticProvider: Failed to initialize runtime API");
    return lldb::ChildCacheState::eRefetch;
  }
  
  // Get class information from runtime
  auto class_info_result = m_runtime_api->GetObjectClassInfo(m_object_addr);
  if (!class_info_result) {
    Log *log = GetLog(LLDBLog::DataFormatters);
    LLDB_LOG(log, "GNUstepUniversalSyntheticProvider: Failed to get class info for object at 0x{0:x}", 
             m_object_addr);
    return lldb::ChildCacheState::eRefetch;
  }
  
  const auto &class_info = *class_info_result;
  m_class_name = class_info.name;
  m_ivars = class_info.ivars;
  
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "GNUstepUniversalSyntheticProvider: Updated for class {0} with {1} ivars", 
           m_class_name, m_ivars.size());
  
  return lldb::ChildCacheState::eRefetch;
}

llvm::Expected<uint32_t> GNUstepUniversalSyntheticProvider::CalculateNumChildren() {
  return static_cast<uint32_t>(m_ivars.size());
}

bool GNUstepUniversalSyntheticProvider::MightHaveChildren() {
  return !m_ivars.empty();
}

lldb::ValueObjectSP GNUstepUniversalSyntheticProvider::GetChildAtIndex(uint32_t idx) {
  if (idx >= m_ivars.size())
    return ValueObjectSP();
  
  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (!valobj_sp)
    return ValueObjectSP();
  
  const auto &ivar = m_ivars[idx];
  
  // Calculate ivar address
  lldb::addr_t ivar_addr = m_object_addr + ivar.offset;
  
  Log *log = GetLog(LLDBLog::DataFormatters);
  LLDB_LOG(log, "GNUstepUniversalSyntheticProvider::GetChildAtIndex({0}): Creating child for ivar '{1}' at offset {2} (addr 0x{3:x})", 
           idx, ivar.name, ivar.offset, ivar_addr);
  
  // Get the target for type lookup
  ProcessSP process_sp = valobj_sp->GetProcessSP();
  if (!process_sp)
    return ValueObjectSP();
  
  Target &target = process_sp->GetTarget();
  
  // Determine the appropriate type for this ivar
  CompilerType ivar_type;
  
  // Parse the Objective-C type encoding to determine the appropriate LLDB type
  std::string type_name;
  
  if (ivar.type_encoding == "@") {
    // Objective-C object pointer
    type_name = "id";
  } else if (ivar.type_encoding == "d") {
    // double
    type_name = "double";
  } else if (ivar.type_encoding == "f") {
    // float
    type_name = "float";
  } else if (ivar.type_encoding == "i") {
    // int
    type_name = "int";
  } else if (ivar.type_encoding == "l") {
    // long
    type_name = "long";
  } else if (ivar.type_encoding == "q") {
    // long long
    type_name = "long long";
  } else if (ivar.type_encoding == "c") {
    // char
    type_name = "char";
  } else if (ivar.type_encoding == "s") {
    // short
    type_name = "short";
  } else if (ivar.type_encoding == "*") {
    // char pointer (C string)
    type_name = "char *";
  } else {
    // Default to id for unknown types
    type_name = "id";
  }
  
  // Get type system to create types
  auto ts_sp = ScratchTypeSystemClang::GetForTarget(target);
  if (!ts_sp) {
    LLDB_LOG(log, "GNUstepUniversalSyntheticProvider: Failed to get type system");
    return ValueObjectSP();
  }
  
  // Create the type based on the encoding
  if (type_name == "id") {
    // Use ObjCBuiltinIdTy for object types
    ivar_type = CompilerType(
        ts_sp->weak_from_this(),
        ts_sp->getASTContext().ObjCBuiltinIdTy.getAsOpaquePtr());
  } else if (type_name == "int") {
    ivar_type = ts_sp->GetBasicType(eBasicTypeInt);
  } else if (type_name == "float") {
    ivar_type = ts_sp->GetBasicType(eBasicTypeFloat);
  } else if (type_name == "double") {
    ivar_type = ts_sp->GetBasicType(eBasicTypeDouble);
  } else if (type_name == "BOOL") {
    ivar_type = ts_sp->GetBasicType(eBasicTypeBool);
  } else if (type_name == "char") {
    ivar_type = ts_sp->GetBasicType(eBasicTypeChar);
  } else if (type_name == "short") {
    ivar_type = ts_sp->GetBasicType(eBasicTypeShort);
  } else if (type_name == "long") {
    ivar_type = ts_sp->GetBasicType(eBasicTypeLong);
  } else if (type_name == "char *") {
    ivar_type = ts_sp->GetBasicType(eBasicTypeChar).GetPointerType();
  } else if (type_name == "void *") {
    ivar_type = ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType();
  } else {
    // Default to void* for unknown types
    ivar_type = ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType();
  }
  
  if (!ivar_type.IsValid()) {
    LLDB_LOG(log, "GNUstepUniversalSyntheticProvider::GetChildAtIndex({0}): Failed to find type '{1}' for ivar '{2}'", 
             idx, type_name, ivar.name);
    return ValueObjectSP();
  }
  
  // Create the child ValueObject
  ValueObjectSP child_sp = CreateValueObjectFromAddress(
      ivar.name.c_str(), ivar_addr, valobj_sp->GetExecutionContextRef(), ivar_type);
  
  if (!child_sp) {
    LLDB_LOG(log, "GNUstepUniversalSyntheticProvider::GetChildAtIndex({0}): Failed to create ValueObject for ivar '{1}'", 
             idx, ivar.name);
    return ValueObjectSP();
  }
  
  LLDB_LOG(log, "GNUstepUniversalSyntheticProvider::GetChildAtIndex({0}): Successfully created child for ivar '{1}'", 
           idx, ivar.name);
  
  return child_sp;
}

size_t GNUstepUniversalSyntheticProvider::GetIndexOfChildWithName(ConstString name) {
  std::string name_str = name.GetStringRef().str();
  
  for (size_t i = 0; i < m_ivars.size(); ++i) {
    if (m_ivars[i].name == name_str) {
      return i;
    }
  }
  
  return UINT32_MAX;
}

// Creator function for LLDB registration
SyntheticChildrenFrontEnd *
GNUstepUniversalProviderCreator(CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp) {
  return new GNUstepUniversalSyntheticProvider(valobj_sp);
}

} // namespace formatters
} // namespace lldb_private