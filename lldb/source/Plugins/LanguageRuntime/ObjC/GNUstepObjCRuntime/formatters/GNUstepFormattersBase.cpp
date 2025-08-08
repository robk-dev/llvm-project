//===-- GNUstepFormattersBase.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepFormattersBase.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/Symbol/CompilerType.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Thread.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

// ===== GNUstepRuntimeHelper Implementation =====

Process *GNUstepRuntimeHelper::GetProcessFromValueObject(ValueObject &valobj) {
  ExecutionContext exe_ctx(valobj.GetExecutionContextRef());
  return exe_ctx.GetProcessPtr();
}

lldb::addr_t GNUstepRuntimeHelper::ReadPointer(Process *process, lldb::addr_t addr, Status &error) {
  if (!process) {
    // Status doesn't have SetErrorString in current LLDB API
    return LLDB_INVALID_ADDRESS;
  }
  
  uint32_t addr_size = process->GetAddressByteSize();
  if (addr_size == 4) {
    uint32_t ptr;
    size_t bytes_read = process->ReadMemory(addr, &ptr, sizeof(ptr), error);
    if (bytes_read == sizeof(ptr) && error.Success()) {
      return ptr;
    }
  } else if (addr_size == 8) {
    uint64_t ptr;
    size_t bytes_read = process->ReadMemory(addr, &ptr, sizeof(ptr), error);
    if (bytes_read == sizeof(ptr) && error.Success()) {
      return ptr;
    }
  }
  
  return LLDB_INVALID_ADDRESS;
}

bool GNUstepRuntimeHelper::ReadMemory(Process *process, lldb::addr_t addr, void *buffer, size_t size) {
  if (!process || !buffer || size == 0) {
    return false;
  }
  
  Status error;
  size_t bytes_read = process->ReadMemory(addr, buffer, size, error);
  return bytes_read == size && error.Success();
}

uint32_t GNUstepRuntimeHelper::GetAddressByteSize(Process *process) {
  return process ? process->GetAddressByteSize() : 0;
}

bool GNUstepRuntimeHelper::IsValidGNUstepObject(ValueObject &valobj) {
  // Basic validation: check if we have a valid address and it's not null
  // Remove the ValueType check since we're dealing with pointers to objects
  
  lldb::addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  // TODO: Add more sophisticated validation once we understand the libobjc2 runtime better
  // For now, assume it's valid if it has a non-null pointer value
  return true;
}

std::string GNUstepRuntimeHelper::GetGNUstepClassName(ValueObject &valobj) {
  Process *process = GetProcessFromValueObject(valobj);
  if (!process) {
    return "<unknown>";
  }
  
  // Use runtime introspector to get the actual class name
  GNUstepObjCRuntimeIntrospector introspector(process);
  std::string runtime_class_name = introspector.GetClassNameFromObject(valobj);
  
  if (!runtime_class_name.empty()) {
    return runtime_class_name;
  }
  
  // Fallback to static type name
  CompilerType type = valobj.GetCompilerType();
  if (type.IsValid()) {
    return type.GetDisplayTypeName().GetCString();
  }
  return "<unknown>";
}

std::string GNUstepRuntimeHelper::ReadUTF8String(Process *process, lldb::addr_t addr, size_t max_length) {
  if (!process || addr == 0 || addr == LLDB_INVALID_ADDRESS) {
    return "";
  }
  
  std::vector<char> buffer(max_length + 1, 0);
  Status error;
  size_t bytes_read = process->ReadMemory(addr, buffer.data(), max_length, error);
  
  if (bytes_read > 0 && error.Success()) {
    buffer[bytes_read] = '\0'; // Ensure null termination
    return std::string(buffer.data());
  }
  
  return "";
}

// ===== GNUstepSummaryProvider Implementation =====

void GNUstepSummaryProvider::WriteQuotedString(Stream &stream, const std::string &str) {
  stream.Printf("\"%s\"", str.c_str());
}

void GNUstepSummaryProvider::WriteErrorSummary(Stream &stream, const std::string &error_msg) {
  stream.Printf("<error: %s>", error_msg.c_str());
}

// ===== GNUstepSyntheticProvider Implementation =====

GNUstepSyntheticProvider::GNUstepSyntheticProvider(lldb::ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp), m_process(nullptr), m_update_called(false) {
  m_process = GNUstepRuntimeHelper::GetProcessFromValueObject(m_backend);
}

lldb::ChildCacheState GNUstepSyntheticProvider::Update() {
  m_update_called = true;
  if (UpdateImpl()) {
    return lldb::ChildCacheState::eRefetch;
  }
  return lldb::ChildCacheState::eReuse;
}

bool GNUstepSyntheticProvider::MightHaveChildren() {
  return true; // Let derived classes determine this
}

size_t GNUstepSyntheticProvider::GetIndexOfChildWithName(ConstString name) {
  // Default implementation: no name-based lookup
  return UINT32_MAX;
}

lldb::ValueObjectSP GNUstepSyntheticProvider::CreateValueObjectFromAddress(
    const std::string &name, lldb::addr_t addr, CompilerType type) {
  if (!m_process || addr == 0 || addr == LLDB_INVALID_ADDRESS || !type.IsValid()) {
    return nullptr;
  }
  
  ExecutionContext exe_ctx(m_backend.GetExecutionContextRef());
  return ValueObject::CreateValueObjectFromAddress(name, addr, exe_ctx, type);
}

lldb::ValueObjectSP GNUstepSyntheticProvider::CreateValueObjectFromData(
    const std::string &name, const DataExtractor &data, CompilerType type) {
  if (!type.IsValid()) {
    return nullptr;
  }
  
  ExecutionContext exe_ctx(m_backend.GetExecutionContextRef());
  return ValueObject::CreateValueObjectFromData(name, data, exe_ctx, type);
}
