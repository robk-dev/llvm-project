//===-- ISAResolver.h --------------------------------------------------===//
//
// Enhanced ISA Pointer Resolution for GNUstep Runtime
// Addresses VS Code debugging issues with nested object expansion
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_ISARESOLVER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_ISARESOLVER_H

#include "lldb/lldb-private.h"
#include "lldb/lldb-types.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace lldb_private {

class Process;

/// Enhanced ISA pointer resolution for GNUstep runtime
/// Provides multiple strategies to resolve ISA pointers to class names
/// Addresses VS Code debugging issues with nested object expansion
class ISAResolver {
public:
  explicit ISAResolver(Process *process);
  
  /// Get class name from ISA pointer using multiple strategies
  std::string GetClassNameFromISA(lldb::addr_t isa_ptr);
  
  /// Get class name from object address (reads ISA then resolves)
  std::string GetClassNameFromObject(lldb::addr_t obj_addr);
  
  /// Validate that an ISA pointer looks reasonable
  bool IsValidISAPointer(lldb::addr_t isa_ptr);
  
  /// Clear any cached data
  void ClearCache();
  
private:
  Process *m_process;
  
  // Runtime function addresses
  lldb::addr_t m_object_getClassName_addr;
  lldb::addr_t m_class_getName_addr;
  
  // Caching
  std::mutex m_cache_mutex;
  std::unordered_map<lldb::addr_t, std::string> m_isa_to_class_cache;
  
  // Helper methods
  void LoadRuntimeSymbols();
  
  // Resolution strategies (in order of preference)
  std::string GetClassNameViaRuntime(lldb::addr_t isa_ptr);
  std::string GetClassNameViaMemory(lldb::addr_t isa_ptr);
  std::string GetClassNameViaSymbols(lldb::addr_t isa_ptr);
  
  // Utility methods
  std::string ReadCString(lldb::addr_t addr, size_t max_len);
  void CacheClassNameForISA(lldb::addr_t isa_ptr, const std::string &class_name);
  std::string GetCachedClassNameForISA(lldb::addr_t isa_ptr);
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_ISARESOLVER_H
