//===-- RuntimeIntrospector.h -----------------------------------------===//
//
// Runtime introspection for discovering ivar layouts dynamically
// Works alongside ISAResolver for complete runtime introspection
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_RUNTIMEINTROSPECTOR_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_RUNTIMEINTROSPECTOR_H

#include "lldb/lldb-private.h"
#include "lldb/lldb-types.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <set>

namespace lldb_private {

class Process;

/// Runtime introspection for discovering ivar offsets dynamically
/// Complements ISAResolver by providing ivar layout discovery
class RuntimeIntrospector {
public:
  explicit RuntimeIntrospector(Process *process);
  
  /// Get offset of an instance variable in a class
  /// Returns -1 if not found or on error
  ptrdiff_t GetIvarOffset(const std::string &class_name, const std::string &ivar_name);
  
  /// Clear cached ivar offsets
  void ClearCache();
  
  /// Get a list of all ivars for a class
  std::vector<std::string> GetIvarNames(const std::string &class_name);
  
  /// Get the instance size of a class
  /// Returns 0 if not found or on error
  size_t GetInstanceSize(const std::string &class_name);
  
private:
  Process *m_process;
  
  // Cache for discovered offsets: class_name -> (ivar_name -> offset)
  std::mutex m_cache_mutex;
  std::unordered_map<std::string, std::unordered_map<std::string, ptrdiff_t>> m_ivar_cache;
  
  // Guard against recursion
  std::set<std::string> m_in_progress;
  
  // Helper to evaluate expressions with proper timeout and error handling
  bool EvaluateExpression(const std::string &expr, uint64_t &result);
  
  // Load runtime function addresses
  void LoadRuntimeSymbols();
  
  // Runtime function addresses
  lldb::addr_t m_class_getInstanceVariable_addr;
  lldb::addr_t m_ivar_getOffset_addr;
  lldb::addr_t m_objc_getClass_addr;
  lldb::addr_t m_class_getInstanceSize_addr;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_RUNTIMEINTROSPECTOR_H