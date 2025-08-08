//===-- GNUstepRuntimeV2API.h -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides a safe, thread-safe API wrapper for libobjc2 runtime
// functions, with complete class hierarchy support and Foundation class
// registration capabilities.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPRUNTIMEV2API_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPRUNTIMEV2API_H

#include "lldb/Target/Process.h"
#include "lldb/Utility/Log.h"
#include "lldb/lldb-forward.h"
#include "lldb/lldb-types.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace lldb_private {

/// GNUstepRuntimeV2API provides a comprehensive wrapper around libobjc2
/// runtime functions with full class hierarchy traversal support.
/// This class ensures thread-safe access to runtime introspection.
class GNUstepRuntimeV2API {
public:
  // Runtime type aliases matching libobjc2
  using Class = void *;
  using Ivar = void *;
  using Method = void *;
  using Property = void *;
  using SEL = void *;

  /// Information about an instance variable
  struct IvarInfo {
    std::string name;
    std::string type_encoding;
    ptrdiff_t offset;
    size_t size;
    Class defining_class;         // Which class in hierarchy defines this
    std::string defining_class_name;
  };

  /// Information about a method
  struct MethodInfo {
    std::string selector_name;
    std::string type_encoding;
    lldb::addr_t implementation;
    Class defining_class;
    std::string defining_class_name;
  };

  /// Information about a property
  struct PropertyInfo {
    std::string name;
    std::string attributes;
    Class defining_class;
    std::string defining_class_name;
  };

  /// Complete class information including hierarchy
  struct ClassInfo {
    std::string name;
    Class class_ptr;
    Class superclass_ptr;
    std::string superclass_name;
    size_t instance_size;
    
    // Complete hierarchy from this class to NSObject
    std::vector<Class> hierarchy;
    std::vector<std::string> hierarchy_names;
    
    // All ivars including inherited
    std::vector<IvarInfo> all_ivars;
    
    // All methods including inherited
    std::vector<MethodInfo> all_methods;
    
    // All properties including inherited
    std::vector<PropertyInfo> all_properties;
    
    // Ivars defined only in this class
    std::vector<IvarInfo> declared_ivars;
    
    // Methods defined only in this class
    std::vector<MethodInfo> declared_methods;
    
    // Properties defined only in this class
    std::vector<PropertyInfo> declared_properties;
    
    bool is_meta_class;
    bool is_root_class;
  };

  /// Runtime function pointers
  struct RuntimeFunctions {
    // Core class functions
    Class (*objc_getClass)(const char *name);
    Class (*objc_lookUpClass)(const char *name);
    Class *(*objc_copyClassList)(unsigned int *outCount);
    
    // Class introspection
    const char *(*class_getName)(Class cls);
    Class (*class_getSuperclass)(Class cls);
    size_t (*class_getInstanceSize)(Class cls);
    bool (*class_isMetaClass)(Class cls);
    
    // Ivar introspection
    Ivar *(*class_copyIvarList)(Class cls, unsigned int *outCount);
    const char *(*ivar_getName)(Ivar ivar);
    const char *(*ivar_getTypeEncoding)(Ivar ivar);
    ptrdiff_t (*ivar_getOffset)(Ivar ivar);
    
    // Method introspection
    Method *(*class_copyMethodList)(Class cls, unsigned int *outCount);
    SEL (*method_getName)(Method method);
    const char *(*method_getTypeEncoding)(Method method);
    void *(*method_getImplementation)(Method method);
    const char *(*sel_getName)(SEL sel);
    
    // Property introspection
    Property *(*class_copyPropertyList)(Class cls, unsigned int *outCount);
    const char *(*property_getName)(Property prop);
    const char *(*property_getAttributes)(Property prop);
    
    // Object introspection
    Class (*object_getClass)(void *obj);
    const char *(*object_getClassName)(void *obj);
    
    // Memory management
    void (*free)(void *ptr);
  };

  /// Create a new instance of the runtime API
  static llvm::Expected<std::unique_ptr<GNUstepRuntimeV2API>>
  Create(Process *process);

  ~GNUstepRuntimeV2API() = default;

  // === Core Class Enumeration ===
  
  /// Get all runtime classes
  llvm::Expected<std::vector<Class>> GetAllClasses();
  
  /// Get all Foundation classes (NS* prefix)
  llvm::Expected<std::vector<ClassInfo>> GetAllFoundationClasses();
  
  // === Class Hierarchy Support ===
  
  /// Get complete class hierarchy from class to root
  llvm::Expected<std::vector<Class>> GetClassHierarchy(Class cls);
  
  /// Get complete class hierarchy with names
  llvm::Expected<std::vector<std::pair<Class, std::string>>> 
  GetClassHierarchyWithNames(Class cls);
  
  /// Get all ivars including inherited ones
  llvm::Expected<std::vector<IvarInfo>> 
  GetAllIvarsIncludingInherited(Class cls);
  
  /// Get all methods including inherited ones
  llvm::Expected<std::vector<MethodInfo>>
  GetAllMethodsIncludingInherited(Class cls);
  
  /// Get all properties including inherited ones
  llvm::Expected<std::vector<PropertyInfo>>
  GetAllPropertiesIncludingInherited(Class cls);
  
  // === Class Information ===
  
  /// Get comprehensive class information
  llvm::Expected<ClassInfo> GetClassInfo(const std::string &class_name);
  
  /// Get class information from pointer
  llvm::Expected<ClassInfo> GetClassInfoFromPointer(Class cls);
  
  /// Find a class by name
  llvm::Expected<Class> FindClass(const std::string &class_name);
  
  // === Object Introspection ===
  
  /// Get class of an object
  llvm::Expected<Class> GetObjectClass(lldb::addr_t obj_addr);
  
  /// Get class name of an object
  llvm::Expected<std::string> GetObjectClassName(lldb::addr_t obj_addr);
  
  // === Foundation Class Registration ===
  
  /// Register all Foundation class formatters
  bool RegisterFoundationClasses();
  
  /// Check if a class is a Foundation class
  bool IsFoundationClass(const std::string &class_name);
  
  // === Runtime Validation ===
  
  /// Check if runtime functions are available
  bool IsValid() const { return m_valid; }
  
  /// Get runtime version information
  std::string GetRuntimeVersion() const;
  
private:
  GNUstepRuntimeV2API(Process *process);
  
  /// Initialize runtime function pointers
  bool InitializeRuntimeFunctions();
  
  /// Resolve a runtime function by name
  lldb::addr_t ResolveRuntimeSymbol(const char *name);
  
  /// Call a runtime function in target process
  template <typename ReturnType>
  llvm::Expected<ReturnType> CallRuntimeFunction(
      lldb::addr_t function_addr,
      const std::vector<lldb::addr_t> &args);
  
  /// Read a C string from target memory
  llvm::Expected<std::string> ReadCStringFromTarget(lldb::addr_t addr);
  
  /// Read memory from target
  llvm::Expected<std::vector<uint8_t>> ReadMemory(lldb::addr_t addr, 
                                                    size_t size);
  
  /// Cache for class information
  void CacheClassInfo(const ClassInfo &info);
  
  /// Get cached class info
  llvm::Expected<ClassInfo> GetCachedClassInfo(const std::string &name);
  
  /// Direct memory reading approach for class enumeration
  llvm::Expected<std::vector<Class>> GetAllClassesDirect();
  
private:
  Process *m_process;
  RuntimeFunctions m_runtime;
  bool m_valid;
  
  // Thread safety
  mutable std::recursive_mutex m_mutex;
  
  // Caches for performance
  std::unordered_map<std::string, ClassInfo> m_class_cache;
  std::unordered_map<Class, std::string> m_class_name_cache;
  std::vector<std::string> m_foundation_classes;
  
  // Runtime module information
  lldb::ModuleSP m_objc_module;
  lldb::ModuleSP m_foundation_module;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_GNUSTEPRUNTIMEV2API_H