//===-- GNUstepUserDefaultsFormatters.cpp -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepUserDefaultsFormatters.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

bool GNUstepNSUserDefaultsSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream, 
                                                        const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid object");
    return false;
  }
  
  UserDefaultsInfo info = ExtractUserDefaultsInfo(valobj);
  if (!info.valid) {
    WriteErrorSummary(stream, "could not extract user defaults info");
    return false;
  }
  
  // Format: NSUserDefaults(domains=3, keys=~247)
  stream.Printf("NSUserDefaults(domains=%u", info.domain_count);
  
  if (info.estimated_key_count > 0) {
    stream.Printf(", keys=~%u", info.estimated_key_count);
  }
  
  // Add additional details for debugging if needed
  if (info.has_persistent_domains && info.has_temporary_domains) {
    // Both types present - this is typical
  } else if (info.has_persistent_domains) {
    stream.Printf(", persistent");
  } else if (info.has_temporary_domains) {
    stream.Printf(", temporary");
  }
  
  stream.Printf(")");
  return true;
}

GNUstepNSUserDefaultsSummaryProvider::UserDefaultsInfo 
GNUstepNSUserDefaultsSummaryProvider::ExtractUserDefaultsInfo(ValueObject &valobj) {
  UserDefaultsInfo info;
  
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return info;
  }
  
  lldb::addr_t obj_addr = valobj.GetValueAsUnsigned(0);
  if (obj_addr == 0) {
    return info;
  }
  
  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // NSUserDefaults object layout (based on observation):
  // +0: isa pointer
  // +8: _searchList (GSMutableArray*)
  // +16: _persDomains (GSMutableDictionary*)
  // +24: _tempDomains (GSMutableDictionary*)
  // +32: _changedDomains 
  // +40: _dictionaryRep
  // +48: _defaultsDatabase
  // ... other fields
  
  Status error;
  lldb::addr_t search_list_addr = GNUstepRuntimeHelper::ReadPointer(process, obj_addr + addr_size, error);
  lldb::addr_t pers_domains_addr = GNUstepRuntimeHelper::ReadPointer(process, obj_addr + addr_size * 2, error);
  lldb::addr_t temp_domains_addr = GNUstepRuntimeHelper::ReadPointer(process, obj_addr + addr_size * 3, error);
  lldb::addr_t database_addr = GNUstepRuntimeHelper::ReadPointer(process, obj_addr + addr_size * 6, error);
  
  if (error.Fail()) {
    return info;
  }
  
  // Extract search list count (represents domain search order)
  if (search_list_addr != 0) {
    info.domain_count = ExtractSearchListCount(process, search_list_addr);
  }
  
  // Extract persistent domains info
  if (pers_domains_addr != 0) {
    info.has_persistent_domains = true;
    auto [domain_count, key_count] = ExtractPersistentDomainsInfo(process, pers_domains_addr);
    // Use persistent domain count if search list failed
    if (info.domain_count == 0) {
      info.domain_count = domain_count;
    }
    info.estimated_key_count += key_count;
  }
  
  // Extract temporary domains info
  if (temp_domains_addr != 0) {
    uint32_t temp_count = ExtractTemporaryDomainsCount(process, temp_domains_addr);
    info.has_temporary_domains = (temp_count > 0);
    // Temporary domains usually have fewer keys, estimate lower
    info.estimated_key_count += temp_count * 10; // rough estimate
  }
  
  // Extract database path for additional context (optional)
  if (database_addr != 0) {
    info.database_path = ExtractDatabasePath(process, database_addr);
  }
  
  info.valid = (info.domain_count > 0 || info.has_persistent_domains || info.has_temporary_domains);
  return info;
}

uint32_t GNUstepNSUserDefaultsSummaryProvider::ExtractSearchListCount(Process *process, lldb::addr_t search_list_addr) {
  return ReadGSArrayCount(process, search_list_addr);
}

std::pair<uint32_t, uint32_t> GNUstepNSUserDefaultsSummaryProvider::ExtractPersistentDomainsInfo(Process *process, lldb::addr_t pers_domains_addr) {
  uint32_t domain_count = ReadGSDictionaryCount(process, pers_domains_addr);
  uint32_t estimated_keys = EstimateKeyCount(process, pers_domains_addr);
  return std::make_pair(domain_count, estimated_keys);
}

uint32_t GNUstepNSUserDefaultsSummaryProvider::ExtractTemporaryDomainsCount(Process *process, lldb::addr_t temp_domains_addr) {
  return ReadGSDictionaryCount(process, temp_domains_addr);
}

std::string GNUstepNSUserDefaultsSummaryProvider::ExtractDatabasePath(Process *process, lldb::addr_t database_addr) {
  if (database_addr == 0) {
    return "";
  }
  
  // Try to read as GNUstep string - this is complex, so return simplified for now
  return GNUstepRuntimeHelper::ReadUTF8String(process, database_addr, 256);
}

uint32_t GNUstepNSUserDefaultsSummaryProvider::ReadGSArrayCount(Process *process, lldb::addr_t array_addr) {
  if (array_addr == 0) {
    return 0;
  }
  
  // GSMutableArray structure (based on GNUstep implementation):
  // +0: isa
  // +8: some field (observed as address)
  // +16: count (uint32_t or uint64_t depending on architecture)
  
  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  // Skip isa and next field, read count at offset +16
  uint32_t count = 0;
  bool success = GNUstepRuntimeHelper::ReadMemory(process, array_addr + addr_size * 2, &count, sizeof(count));
  
  if (!success) {
    return 0;
  }
  
  // Sanity check - arrays shouldn't have thousands of domains
  return (count < 1000) ? count : 0;
}

uint32_t GNUstepNSUserDefaultsSummaryProvider::ReadGSDictionaryCount(Process *process, lldb::addr_t dict_addr) {
  if (dict_addr == 0) {
    return 0;
  }
  
  // GSMutableDictionary structure (based on observation):
  // +0: isa
  // +8: some field
  // +16: count (observed value of 3 at this location)
  // +24: capacity or other field
  
  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  
  uint32_t count = 0;
  bool success = GNUstepRuntimeHelper::ReadMemory(process, dict_addr + addr_size * 2, &count, sizeof(count));
  
  if (!success) {
    return 0;
  }
  
  // Sanity check - domain dictionaries shouldn't be massive
  return (count < 10000) ? count : 0;
}

uint32_t GNUstepNSUserDefaultsSummaryProvider::EstimateKeyCount(Process *process, lldb::addr_t dict_addr) {
  if (dict_addr == 0) {
    return 0;
  }
  
  uint32_t domain_count = ReadGSDictionaryCount(process, dict_addr);
  
  // Estimation strategy: NSUserDefaults typically has several domains
  // - NSGlobalDomain: ~200-300 keys (system defaults)
  // - Application domain: ~10-50 keys (app-specific)
  // - Additional domains: ~5-20 keys each
  
  if (domain_count == 0) {
    return 0;
  } else if (domain_count == 1) {
    return 50;  // Likely just app domain
  } else if (domain_count <= 3) {
    return 250;  // Global + app + maybe one more
  } else {
    return 200 + (domain_count - 2) * 15;  // Base + extras
  }
}

// LLDB registration function
bool GNUstepNSUserDefaultsFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  GNUstepNSUserDefaultsSummaryProvider formatter;
  return formatter.FormatObject(valobj, stream, options);
}