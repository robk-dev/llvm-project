//===-- GNUstepDataFormatters.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepDataFormatters.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include <iomanip>
#include <sstream>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSDataSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream,
                                               const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid NSData object");
    return false;
  }

  size_t length = ExtractLength(valobj);
  
  std::string formatted_length = FormatLength(length);
  
  if (length > 0 && length <= 64) {
    // Show a preview for small data objects
    std::string preview = ExtractDataPreview(valobj);
    if (!preview.empty()) {
      std::ostringstream oss;
      oss << formatted_length << " [" << preview << "]";
      WriteQuotedString(stream, oss.str());
      return true;
    }
  }
  
  WriteQuotedString(stream, formatted_length);
  return true;
}

size_t GNUstepNSDataSummaryProvider::ExtractLength(ValueObject &valobj) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return 0;

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return 0;

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // NSData layout (simplified for GNUstep):
  // [isa][_length NSUInteger][_bytes void*]
  lldb::addr_t length_addr = object_addr + addr_size;
  
  Status error;
  size_t length = 0;
  
  if (addr_size == 8) {
    // 64-bit
    uint64_t length64 = 0;
    size_t bytes_read = process->ReadMemory(length_addr, &length64, sizeof(uint64_t), error);
    if (bytes_read == sizeof(uint64_t) && !error.Fail()) {
      length = static_cast<size_t>(length64);
    }
  } else {
    // 32-bit
    uint32_t length32 = 0;
    size_t bytes_read = process->ReadMemory(length_addr, &length32, sizeof(uint32_t), error);
    if (bytes_read == sizeof(uint32_t) && !error.Fail()) {
      length = static_cast<size_t>(length32);
    }
  }
  
  return length;
}

std::string GNUstepNSDataSummaryProvider::ExtractDataPreview(ValueObject &valobj, size_t max_bytes) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return "";

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return "";

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // Get the length first
  size_t length = ExtractLength(valobj);
  if (length == 0)
    return "";
  
  // Get the bytes pointer
  lldb::addr_t bytes_ptr_addr = object_addr + addr_size + addr_size; // After isa and length
  
  Status error;
  lldb::addr_t bytes_addr = GNUstepRuntimeHelper::ReadPointer(process, bytes_ptr_addr, error);
  
  if (bytes_addr == 0 || bytes_addr == LLDB_INVALID_ADDRESS || error.Fail()) {
    return "";
  }

  // Read a small preview of the data
  size_t preview_size = std::min(length, max_bytes);
  std::vector<uint8_t> buffer(preview_size);
  
  size_t bytes_read = process->ReadMemory(bytes_addr, buffer.data(), preview_size, error);
  if (bytes_read == 0 || error.Fail()) {
    return "";
  }
  
  // Format as hex bytes
  std::ostringstream oss;
  for (size_t i = 0; i < bytes_read; ++i) {
    if (i > 0) oss << " ";
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(buffer[i]);
  }
  
  if (length > max_bytes) {
    oss << "...";
  }
  
  return oss.str();
}

std::string GNUstepNSDataSummaryProvider::FormatLength(size_t length) {
  std::ostringstream oss;
  
  if (length == 0) {
    oss << "0 bytes";
  } else if (length == 1) {
    oss << "1 byte";
  } else if (length < 1024) {
    oss << length << " bytes";
  } else if (length < 1024 * 1024) {
    double kb = static_cast<double>(length) / 1024.0;
    oss << std::fixed << std::setprecision(1) << kb << " KB";
  } else if (length < 1024 * 1024 * 1024) {
    double mb = static_cast<double>(length) / (1024.0 * 1024.0);
    oss << std::fixed << std::setprecision(1) << mb << " MB";
  } else {
    double gb = static_cast<double>(length) / (1024.0 * 1024.0 * 1024.0);
    oss << std::fixed << std::setprecision(1) << gb << " GB";
  }
  
  return oss.str();
}

bool GNUstepNSDataFormatterFunction(ValueObject &valobj, Stream &stream,
                                   const TypeSummaryOptions &options) {
  GNUstepNSDataSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}