//===-- GNUstepUUIDFormatters.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepUUIDFormatters.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include <iomanip>
#include <sstream>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSUUIDSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream,
                                               const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid NSUUID object");
    return false;
  }

  uint8_t uuid_bytes[16];
  if (!ExtractUUIDBytes(valobj, uuid_bytes)) {
    WriteErrorSummary(stream, "could not extract UUID bytes");
    return false;
  }
  
  std::string uuid_string = FormatUUIDString(uuid_bytes);
  WriteQuotedString(stream, uuid_string);
  return true;
}

bool GNUstepNSUUIDSummaryProvider::ExtractUUIDBytes(ValueObject &valobj, uint8_t uuid_bytes[16]) {
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process)
    return false;

  lldb::addr_t object_addr = valobj.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS)
    return false;

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // NSUUID layout (simplified for GNUstep):
  // [isa][uuid_bytes[16]]
  // The 16 UUID bytes are stored directly in the object after the isa pointer
  lldb::addr_t uuid_bytes_addr = object_addr + addr_size;
  
  Status error;
  size_t bytes_read = process->ReadMemory(uuid_bytes_addr, uuid_bytes, 16, error);
  
  if (bytes_read != 16 || error.Fail()) {
    return false;
  }
  
  return true;
}

std::string GNUstepNSUUIDSummaryProvider::FormatUUIDString(const uint8_t uuid_bytes[16]) {
  // Format as standard UUID string: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
  std::ostringstream oss;
  oss << std::hex << std::uppercase << std::setfill('0');
  
  // First group (8 hex digits)
  for (int i = 0; i < 4; ++i) {
    oss << std::setw(2) << static_cast<unsigned>(uuid_bytes[i]);
  }
  oss << "-";
  
  // Second group (4 hex digits)
  for (int i = 4; i < 6; ++i) {
    oss << std::setw(2) << static_cast<unsigned>(uuid_bytes[i]);
  }
  oss << "-";
  
  // Third group (4 hex digits)
  for (int i = 6; i < 8; ++i) {
    oss << std::setw(2) << static_cast<unsigned>(uuid_bytes[i]);
  }
  oss << "-";
  
  // Fourth group (4 hex digits)
  for (int i = 8; i < 10; ++i) {
    oss << std::setw(2) << static_cast<unsigned>(uuid_bytes[i]);
  }
  oss << "-";
  
  // Fifth group (12 hex digits)
  for (int i = 10; i < 16; ++i) {
    oss << std::setw(2) << static_cast<unsigned>(uuid_bytes[i]);
  }
  
  return oss.str();
}

bool GNUstepNSUUIDFormatterFunction(ValueObject &valobj, Stream &stream,
                                   const TypeSummaryOptions &options) {
  GNUstepNSUUIDSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}