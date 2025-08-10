//===-- GNUstepUserDefaultsFormatters.h ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_USERDEFAULTS_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_USERDEFAULTS_H

#include "GNUstepFormattersBase.h"

namespace lldb_private {
namespace formatters {

/// Summary provider for GNUstep NSUserDefaults objects
class GNUstepNSUserDefaultsSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  /// Extract user defaults information from a GNUstep NSUserDefaults object
  struct UserDefaultsInfo {
    uint32_t domain_count;
    uint32_t estimated_key_count;
    bool has_persistent_domains;
    bool has_temporary_domains;
    std::string database_path;
    bool valid;
    
    UserDefaultsInfo() : domain_count(0), estimated_key_count(0), 
                        has_persistent_domains(false), has_temporary_domains(false), valid(false) {}
  };
  
  /// Extract user defaults information from NSUserDefaults object
  UserDefaultsInfo ExtractUserDefaultsInfo(ValueObject &valobj);
  
  /// Extract domain count from _searchList GSMutableArray
  uint32_t ExtractSearchListCount(Process *process, lldb::addr_t search_list_addr);
  
  /// Extract domain count and estimated key count from _persDomains GSMutableDictionary
  std::pair<uint32_t, uint32_t> ExtractPersistentDomainsInfo(Process *process, lldb::addr_t pers_domains_addr);
  
  /// Extract domain count from _tempDomains GSMutableDictionary
  uint32_t ExtractTemporaryDomainsCount(Process *process, lldb::addr_t temp_domains_addr);
  
  /// Extract database path from _defaultsDatabase string
  std::string ExtractDatabasePath(Process *process, lldb::addr_t database_addr);
  
  /// Read GSMutableArray count from GNUstep array structure
  uint32_t ReadGSArrayCount(Process *process, lldb::addr_t array_addr);
  
  /// Read GSMutableDictionary count from GNUstep dictionary structure
  uint32_t ReadGSDictionaryCount(Process *process, lldb::addr_t dict_addr);
  
  /// Estimate total key count from dictionary structure (approximate)
  uint32_t EstimateKeyCount(Process *process, lldb::addr_t dict_addr);
};

/// Function wrapper for LLDB registration
bool GNUstepNSUserDefaultsFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_USERDEFAULTS_H