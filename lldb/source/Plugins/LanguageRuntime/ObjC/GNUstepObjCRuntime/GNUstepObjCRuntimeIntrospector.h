//===-- GNUstepObjCRuntimeIntrospector.h ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEINTROSPECTOR_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEINTROSPECTOR_H

#include "GNUstepObjCRuntimeUtilities.h"
#include "lldb/Expression/FunctionCaller.h"
#include "lldb/Target/Process.h"
#include "lldb/lldb-private.h"
#include "llvm/Support/Error.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace lldb_private {

// Forward declarations
class ExecutionContext;
class CompilerType;
class ValueList;
class Status;

// Import the utilities namespace for convenience
using namespace gnustep_objc_runtime_utilities;

class GNUstepObjCRuntimeIntrospector {
public:
  GNUstepObjCRuntimeIntrospector(Process *process);
  ~GNUstepObjCRuntimeIntrospector() = default;

  // Extract ISA from a ValueObject
  lldb::addr_t GetISAFromObject(ValueObject &valobj);

  // Given an isa pointer, return the class name.
  std::string GetClassName(lldb::addr_t isa_addr);

  // Get class name from ISA with caching support
  lldb_private::ConstString GetClassNameFromISA(lldb::addr_t isa_addr);

  // Get class name directly from a ValueObject
  std::string GetClassNameFromObject(ValueObject &valobj);

  // Find a class by name in the runtime
  lldb::addr_t FindClass(const std::string &class_name);

  // Check if this looks like a valid GNUstep runtime
  bool IsValidGNUstepRuntime();

  // Load runtime function symbols for direct calling
  bool LoadRuntimeSymbols();

  // CRITICAL: Direct method introspection using runtime.h functions
  // These avoid expression evaluation during interface declaration to break
  // recursion
  struct MethodInfo {
    std::string selector_name;
    std::string type_encoding;
    lldb::addr_t implementation;
  };

  // Get all instance methods from a class pointer (direct memory access)
  std::vector<MethodInfo> GetInstanceMethods(lldb::addr_t class_ptr);

  // Get all class methods from a class pointer via metaclass (direct memory
  // access)
  std::vector<MethodInfo> GetClassMethods(lldb::addr_t class_ptr);

  // Get class pointer by name using objc_getClass (direct runtime call)
  lldb::addr_t GetClassPointer(const std::string &class_name);

  // Get metaclass pointer by name using objc_getMetaClass (direct runtime call)
  lldb::addr_t GetMetaClassPointer(const std::string &class_name);

  // Ensure runtime symbols are loaded (call this before using symbol addresses)
  void EnsureRuntimeSymbolsLoaded();

  // Get runtime function address by name with caching
  lldb::addr_t GetRuntimeFunctionAddress(const char *function_name);

  // Check if an object is a tagged pointer
  bool IsTaggedPointer(lldb::addr_t obj_addr);

  // Decode tagged pointer data for strings
  std::string DecodeTaggedString(lldb::addr_t obj_addr);

  // Get the class pointer for a tagged pointer using runtime functions
  lldb::addr_t GetTaggedPointerClass(lldb::addr_t obj_addr);

  // Get the class name for a tagged pointer
  std::string GetTaggedPointerClassName(lldb::addr_t obj_addr);

  // Check if an address represents a valid object
  bool IsValidObjectPointer(lldb::addr_t obj_addr);

  // Helper to call functions in the target process (public interface for
  // DeclVendor)
  lldb::addr_t CallRuntimeFunction(const std::string &function_name,
                                   const std::vector<lldb::addr_t> &args);

  // Runtime class enumeration methods for dynamic formatter registration
  
  // Get all class names currently registered with the runtime
  std::vector<std::string> GetAllClassNames();
  
  // Get all classes that are subclasses of a given base class (e.g., "NSString")
  std::vector<std::string> GetSubclassesOf(const std::string &base_class_name);
  
  // Check if a class name exists in the runtime
  bool ClassExists(const std::string &class_name);
  
  // Get all Foundation class names (classes starting with NS, GS, etc.)
  std::vector<std::string> GetFoundationClassNames();

  // === Enhanced Runtime API (merged from GNUstepRuntimeV2API) ===

  // Runtime type aliases matching libobjc2
  using Class = void *;
  using Ivar = void *;
  using Method = void *;
  using Property = void *;
  using SEL = void *;

  // Information about an instance variable
  struct IvarInfo {
    std::string name;
    std::string type_encoding;
    ptrdiff_t offset;
    size_t size;
    Class defining_class; // Which class in hierarchy defines this
    std::string defining_class_name;
  };

  // Information about a property
  struct PropertyInfo {
    std::string name;
    std::string attributes;
    Class defining_class;
    std::string defining_class_name;
  };

  // Complete class information including hierarchy
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

  // === Core Enhanced Runtime Methods ===

  // Get all runtime classes as Class pointers
  llvm::Expected<std::vector<Class>> GetAllClasses();

  // Get all Foundation classes with full ClassInfo
  llvm::Expected<std::vector<ClassInfo>> GetAllFoundationClasses();

  // Get complete class hierarchy from class to root
  llvm::Expected<std::vector<Class>> GetClassHierarchy(Class cls);

  // Get complete class hierarchy with names
  llvm::Expected<std::vector<std::pair<Class, std::string>>>
  GetClassHierarchyWithNames(Class cls);

  // Get all ivars including inherited ones
  llvm::Expected<std::vector<IvarInfo>>
  GetAllIvarsIncludingInherited(Class cls);

  // Get all methods including inherited ones
  llvm::Expected<std::vector<MethodInfo>>
  GetAllMethodsIncludingInherited(Class cls);

  // Get all class methods from metaclass (for class method discovery)
  llvm::Expected<std::vector<MethodInfo>>
  GetAllClassMethods(const std::string &class_name);

  // Get all properties including inherited ones
  llvm::Expected<std::vector<PropertyInfo>>
  GetAllPropertiesIncludingInherited(Class cls);

  // Get comprehensive class information
  llvm::Expected<ClassInfo> GetObjCClassInfo(const std::string &class_name);

  // Get class information from pointer
  llvm::Expected<ClassInfo> GetClassInfoFromPointer(Class cls);

  // Find a class by name (enhanced version returning Class pointer)
  llvm::Expected<Class> FindClassPointer(const std::string &class_name);

  // Get class of an object (or metaclass of a class)
  llvm::Expected<Class> GetObjectClass(void *obj);

  // Get class name of an object
  llvm::Expected<std::string> GetObjectClassName(lldb::addr_t obj_addr);

  // Check if a class is a Foundation class
  bool IsFoundationClass(const std::string &class_name);

  // Check if a class responds to a selector
  bool ClassRespondsToSelector(const std::string &class_name,
                               const std::string &selector_name);

  // Get instance method for selector
  llvm::Expected<MethodInfo>
  GetInstanceMethod(const std::string &class_name,
                    const std::string &selector_name);

  // Get runtime version information
  std::string GetRuntimeVersion() const;

  // Free memory allocated by runtime functions
  bool CallFreeFunction(lldb::addr_t ptr);

private:
  Process *m_process;
  uint32_t m_address_size;
  lldb::ByteOrder m_byte_order;

  // Reentrancy protection
  mutable std::atomic<bool> m_in_function_call{false};

  // Runtime function addresses for direct calling (performance optimization)
  lldb::addr_t m_object_getClass_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_class_getSuperclass_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_class_getInstanceSize_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_class_getMethodImplementation_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_objc_msgSend_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_objc_copyClassList_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_class_getName_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_free_addr = LLDB_INVALID_ADDRESS;

  // Method introspection function addresses from runtime.h for dynamic method
  // discovery
  lldb::addr_t m_objc_getMetaClass_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_objc_getClass_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_class_copyMethodList_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_method_getName_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_method_getTypeEncoding_addr = LLDB_INVALID_ADDRESS;
  lldb::addr_t m_sel_getName_addr = LLDB_INVALID_ADDRESS;

  // === Enhanced Runtime Function Addresses (merged from GNUstepRuntimeV2API) ===
  
  // Runtime function structure for comprehensive access
  struct RuntimeFunctions {
    // Core class functions
    Class (*objc_getClass)(const char *name);
    Class (*objc_lookUpClass)(const char *name);
    Class (*objc_getMetaClass)(const char *name);
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

    // Method lookup and selector checking
    SEL (*sel_getUid)(const char *str);
    bool (*class_respondsToSelector)(Class cls, SEL sel);
    Method (*class_getInstanceMethod)(Class cls, SEL sel);
    Method (*class_getClassMethod)(Class cls, SEL sel);

    // Property introspection
    Property *(*class_copyPropertyList)(Class cls, unsigned int *outCount);
    const char *(*property_getName)(Property prop);
    const char *(*property_getAttributes)(Property prop);

    // Object introspection
    Class (*object_getClass)(void *obj);
    const char *(*object_getClassName)(void *obj);

    // Memory management
    void (*free)(void *ptr);
  } m_runtime;

  // Thread safety for enhanced functionality
  mutable std::recursive_mutex m_mutex;

  // Flag to track if we've attempted to load runtime symbols
  bool m_runtime_symbols_loaded = false;

  // Cache for function callers to avoid repeated compilation
  struct FunctionCallerCache {
    std::unique_ptr<FunctionCaller> objc_lookup_class_caller;
    std::unique_ptr<FunctionCaller> class_getName_caller;
    std::unique_ptr<FunctionCaller> object_getClass_caller;
    std::unique_ptr<FunctionCaller> class_getSuperclass_caller;
    // Generic cache for other functions
    std::unordered_map<std::string, std::unique_ptr<FunctionCaller>>
        generic_callers;
  };

  mutable FunctionCallerCache m_function_cache;

  // Cache for ISA to class name mapping
  mutable std::unordered_map<lldb::addr_t, lldb_private::ConstString>
      m_isa_to_name_cache;

  // New implementation methods for function calling
  lldb::addr_t CallRuntimeFunctionImpl(const char *function_name,
                                       const CompilerType &return_type,
                                       const ValueList &args,
                                       ExecutionContext &exe_ctx,
                                       Status &error) const;

  std::unique_ptr<FunctionCaller> &
  GetOrCreateFunctionCaller(const char *function_name,
                            const CompilerType &return_type,
                            const ValueList &arg_types,
                            ExecutionContext &exe_ctx, Status &error) const;

  // Helper to get modules
  lldb::ModuleSP GetObjCModule() const;
  lldb::ModuleSP GetFoundationModule() const;

  // === Enhanced Private Methods (merged from GNUstepRuntimeV2API) ===

  // Initialize comprehensive runtime function pointers
  bool InitializeRuntimeFunctions();

  // Resolve a runtime function by name with module searching
  lldb::addr_t ResolveRuntimeSymbol(const char *name);

  // Call a runtime function in target process with comprehensive error handling
  template <typename ReturnType>
  llvm::Expected<ReturnType>
  CallRuntimeFunction(lldb::addr_t function_addr,
                      const std::vector<lldb::addr_t> &args);

  // Read a C string from target memory with error handling
  llvm::Expected<std::string> ReadCStringFromTarget(lldb::addr_t addr);

  // Read memory from target with comprehensive error handling
  llvm::Expected<std::vector<uint8_t>> ReadMemory(lldb::addr_t addr,
                                                  size_t size);

  // Cache for class information
  void CacheClassInfo(const ClassInfo &info);

  // Get cached class info
  llvm::Expected<ClassInfo> GetCachedClassInfo(const std::string &name);

  // Direct memory reading approach for class enumeration
  llvm::Expected<std::vector<Class>> GetAllClassesDirect();

  // Enhanced runtime modules
  lldb::ModuleSP m_objc_module;
  lldb::ModuleSP m_foundation_module;

  // Caches for performance
  std::unordered_map<std::string, ClassInfo> m_class_cache;
  std::unordered_map<Class, std::string> m_class_name_cache;
  std::vector<std::string> m_foundation_classes;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIMEINTROSPECTOR_H
