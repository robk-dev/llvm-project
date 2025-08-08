//===-- GNUstepObjCRuntime.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCRuntime.h"
#include "GNUstepClassDescriptor.h"
#include "formatters/GNUstepFormattersRegistry.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/DataFormatters/DataVisualization.h"
#include "lldb/DataFormatters/TypeCategory.h"
#include "lldb/Expression/UtilityFunction.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/Stream.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "llvm/Support/Error.h"
#include "GNUstepObjCRuntimeIntrospector.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/StreamString.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

void GNUstepObjCRuntime::Initialize() {
  PluginManager::RegisterPlugin(
      "gnu-objc-v2", "GNUstep Objective-C V2 Runtime",
      CreateInstance, nullptr);
  
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::Initialize() called\n");
  
  // Register our formatters with LLDB
  // Get or create the GNUstep type category
  TypeCategoryImplSP category_sp;
  if (DataVisualization::Categories::GetCategory(ConstString("gnustep"), category_sp)) {
    if (category_sp) {
      GNUstepFormattersRegistry::RegisterFormatters(*category_sp);
      DataVisualization::Categories::Enable(category_sp, TypeCategoryMap::Default);
      LLDB_LOG(log, "GNUstepObjCRuntime: Formatters registered and enabled");
    } else {
      LLDB_LOG(log, "GNUstepObjCRuntime: Got null category pointer");
    }
  } else {
    // Create the category
    ConstString category_name("gnustep/libobjc2");
    DataVisualization::Categories::Add(category_name);
    
    // Get the newly created category
    if (DataVisualization::Categories::GetCategory(category_name, category_sp)) {
      GNUstepFormattersRegistry::RegisterFormatters(*category_sp);
      DataVisualization::Categories::Enable(category_name, TypeCategoryMap::Default);
      LLDB_LOG(log, "GNUstepObjCRuntime: Created new category and registered formatters");
    } else {
      LLDB_LOG(log, "GNUstepObjCRuntime: Failed to create category");
    }
  }
}

void GNUstepObjCRuntime::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}

LanguageRuntime *
GNUstepObjCRuntime::CreateInstance(Process *process,
                                     lldb::LanguageType language) {
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::CreateInstance() called for language {0}", language);
  
  if (language != eLanguageTypeObjC && language != eLanguageTypeObjC_plus_plus) {
    return nullptr;
  }
  
  if (!process) {
    return nullptr;
  }

  // First check if GNUstep runtime libraries are already loaded
  // This happens when attaching to a running process
  Target &target = process->GetTarget();
  ModuleList &modules = target.GetImages();
  
  bool found_gnustep_runtime = false;
  
  // Check for GNUstep runtime libraries
  for (size_t i = 0; i < modules.GetSize(); ++i) {
    ModuleSP module_sp = modules.GetModuleAtIndex(i);
    if (module_sp) {
      const char *module_name = module_sp->GetFileSpec().GetFilename().GetCString();
      if (module_name && 
          (strstr(module_name, "libobjc.so") || 
           strstr(module_name, "libgnustep-base.so") ||
           strstr(module_name, "libobjc2"))) {
        found_gnustep_runtime = true;
        LLDB_LOG(log, "GNUstepObjCRuntime: Found GNUstep runtime module: {0}", module_name);
        break;
      }
    }
  }
  
  if (found_gnustep_runtime) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Creating instance for GNUstep runtime (libraries already loaded)");
    return new GNUstepObjCRuntime(process);
  }
  
  // If libraries aren't loaded yet, check if the main executable has Objective-C code
  // This is needed when launching a process (libraries haven't loaded yet)
  ModuleSP exe_module_sp = target.GetExecutableModule();
  if (exe_module_sp) {
    // Check for Objective-C symbols in the executable
    // GNUstep compiled code will have .objc_ symbols
    SymbolFile *sym_file = exe_module_sp->GetSymbolFile();
    if (sym_file) {
      // Simple heuristic: check for .objc_ symbols which indicate GNUstep ObjC code
      Symtab *symtab = exe_module_sp->GetSymtab();
      if (symtab) {
        const size_t num_symbols = symtab->GetNumSymbols();
        for (size_t i = 0; i < num_symbols && i < 100; ++i) { // Check first 100 symbols for efficiency
          Symbol *symbol = symtab->SymbolAtIndex(i);
          if (symbol) {
            const char *name = symbol->GetName().GetCString();
            if (name && strstr(name, ".objc_")) {
              LLDB_LOG(log, "GNUstepObjCRuntime: Found .objc_ symbol '{0}' in executable, assuming GNUstep", name);
              return new GNUstepObjCRuntime(process);
            }
          }
        }
      }
    }
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: No GNUstep runtime or Objective-C symbols found");
  return nullptr;
}

GNUstepObjCRuntime::GNUstepObjCRuntime(Process *process)
    : ObjCLanguageRuntime(process), m_formatters_registered(false), m_gnustep_library_loaded(false) {
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime constructor called");
  if (process) {
    m_introspector_up = std::make_unique<GNUstepObjCRuntimeIntrospector>(process);
    // Runtime API will be initialized when libraries are loaded
  }
  
  // Don't register formatters here - wait until we confirm GNUstep libraries are loaded
  // This will happen in ModulesDidLoad
}

GNUstepObjCRuntime::~GNUstepObjCRuntime() {
  // Note: We don't unregister formatters here as they may be used by other GNUstep processes
}

llvm::Error GNUstepObjCRuntime::GetObjectDescription(Stream &str,
                                                ValueObject &object) {
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::GetObjectDescription called");
  
  if (!m_introspector_up)
    return llvm::createStringError(llvm::inconvertibleErrorCode(), "No introspector available");

  // Get the ISA
  lldb::addr_t isa_addr = object.GetPointerValue();
  if (isa_addr == LLDB_INVALID_ADDRESS)
    return llvm::createStringError(llvm::inconvertibleErrorCode(), "Invalid object address");

  // Get the class name from the introspector
  std::string class_name = m_introspector_up->GetClassName(isa_addr);

  if (class_name.empty())
    return llvm::createStringError(llvm::inconvertibleErrorCode(), "Could not determine class name");

  str.Printf("(%s *) 0x%" PRIx64, class_name.c_str(), object.GetPointerValue());
  LLDB_LOG(log, "GNUstepObjCRuntime: Formatted object as ({0} *) 0x{1:x}", 
           class_name.c_str(), object.GetPointerValue());
  return llvm::Error::success();
}

llvm::Error GNUstepObjCRuntime::GetObjectDescription(
    Stream &str, Value &value, ExecutionContextScope *exe_scope) {
  // This version is less critical for now, just return a simple implementation
  if (value.GetValueType() != Value::ValueType::Scalar)
    return llvm::createStringError(llvm::inconvertibleErrorCode(), "Value is not scalar");

  // For now, just print the raw value since creating ValueObject is complex
  str.Printf("GNUstep object at 0x%" PRIx64, (uint64_t)value.GetScalar().ULongLong());
  return llvm::Error::success();
}

bool GNUstepObjCRuntime::GetDynamicTypeAndAddress(ValueObject &in_value,
                                                  lldb::DynamicValueType use_dynamic,
                                                  TypeAndOrName &class_type_or_name,
                                                  Address &address,
                                                  Value::ValueType &value_type) {
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::GetDynamicTypeAndAddress called");
  
  if (!m_introspector_up) {
    LLDB_LOG(log, "No introspector available");
    return false;
  }
  
  // Get the runtime class name
  std::string class_name = m_introspector_up->GetClassNameFromObject(in_value);
  if (class_name.empty()) {
    LLDB_LOG(log, "Could not get class name from object");
    return false;
  }
  
  LLDB_LOG(log, "Got dynamic class name: {0}", class_name);
  
  // Set the class name in the result
  class_type_or_name.SetName(ConstString(class_name));
  
  // Get the object address
  lldb::addr_t object_addr = in_value.GetPointerValue();
  if (object_addr == LLDB_INVALID_ADDRESS || object_addr == 0) {
    LLDB_LOG(log, "Invalid object address");
    return false;
  }
  
  // Set the address
  address.SetRawAddress(object_addr);
  value_type = Value::ValueType::LoadAddress;
  
  LLDB_LOG(log, "Dynamic type resolved: {0} at 0x{1:x}", class_name, object_addr);
  return true;
}

TypeAndOrName GNUstepObjCRuntime::FixUpDynamicType(const TypeAndOrName &type_and_or_name,
                                                   ValueObject &static_value) {
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::FixUpDynamicType called");
  
  if (!m_introspector_up) {
    LLDB_LOG(log, "No introspector available for FixUpDynamicType");
    return type_and_or_name;
  }
  
  // If we already have a class name from dynamic typing, use it
  if (type_and_or_name.HasName()) {
    return type_and_or_name;
  }
  
  // Try to get the dynamic class name
  std::string class_name = m_introspector_up->GetClassNameFromObject(static_value);
  if (!class_name.empty()) {
    LLDB_LOG(log, "FixUpDynamicType resolved to: {0}", class_name);
    TypeAndOrName result(type_and_or_name);
    result.SetName(ConstString(class_name));
    return result;
  }
  
  return type_and_or_name;
}

bool GNUstepObjCRuntime::CouldHaveDynamicValue(ValueObject &in_value) {
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::CouldHaveDynamicValue called");
  
  // Check if this is a valid object pointer
  lldb::addr_t obj_addr = in_value.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    LLDB_LOG(log, "Invalid object address, no dynamic value possible");
    return false;
  }
  
  // Check the static type - should be an Objective-C object pointer
  CompilerType static_type = in_value.GetCompilerType();
  if (static_type.IsValid()) {
    // Check if it's a pointer type
    if (static_type.IsPointerType()) {
      CompilerType pointee_type = static_type.GetPointeeType();
      if (pointee_type.IsValid()) {
        // Check if it looks like an Objective-C class
        std::string type_name = pointee_type.GetTypeName().GetCString();
        if (type_name.find("NS") == 0 || type_name == "id" || type_name.find("objc_object") != std::string::npos) {
          LLDB_LOG(log, "Type {0} could have dynamic value", type_name);
          return true;
        }
      }
    }
  }
  
  // If we have an introspector, check if it's a valid object
  if (m_introspector_up) {
    bool is_valid = m_introspector_up->IsValidObjectPointer(obj_addr);
    LLDB_LOG(log, "Introspector validation: {0}", is_valid ? "valid" : "invalid");
    return is_valid;
  }
  
  // Conservative default: assume it could have dynamic value
  LLDB_LOG(log, "Assuming could have dynamic value (conservative default)");
  return true;
}

lldb::BreakpointResolverSP
GNUstepObjCRuntime::CreateExceptionResolver(const lldb::BreakpointSP &bkpt, 
                                           bool catch_bp, bool throw_bp) {
  // Stub implementation
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Breakpoints);
  LLDB_LOG(log, "GNUstepObjCRuntime::CreateExceptionResolver called");
  return nullptr;
}

lldb::ThreadPlanSP GNUstepObjCRuntime::GetStepThroughTrampolinePlan(Thread &thread,
                                                                    bool stop_others) {
  // Stub implementation
  Log *log = GetLog(LLDBLog::Step);
  LLDB_LOG(log, "GNUstepObjCRuntime::GetStepThroughTrampolinePlan called");
  return nullptr;
}

bool GNUstepObjCRuntime::IsModuleObjCLibrary(const lldb::ModuleSP &module_sp) {
  if (!module_sp)
    return false;
  
  const char *module_name = module_sp->GetFileSpec().GetFilename().GetCString();
  if (!module_name)
    return false;
  
  // Check for GNUstep runtime libraries - handle versioned library names
  // Examples: libobjc.so.4.6, libgnustep-base.so.1.31, libobjc2.so.4
  return (strstr(module_name, "libobjc.so") ||         // libobjc.so.4.6
          strstr(module_name, "libgnustep-base.so") || // libgnustep-base.so.1.31  
          strstr(module_name, "libobjc2.so") ||        // libobjc2.so.4
          strstr(module_name, "libobjc2") ||           // libobjc2 (unversioned)
          strstr(module_name, "libBlocksRuntime"));    // libBlocksRuntime for blocks support
}

bool GNUstepObjCRuntime::ReadObjCLibrary(const lldb::ModuleSP &module_sp) {
  if (!IsModuleObjCLibrary(module_sp))
    return false;
  
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::ReadObjCLibrary called for module: {0}",
           module_sp->GetFileSpec().GetFilename().GetCString());
  
  m_has_read_objc_library = true;
  return true;
}

bool GNUstepObjCRuntime::HasReadObjCLibrary() {
  return m_has_read_objc_library;
}

llvm::Expected<std::unique_ptr<UtilityFunction>>
GNUstepObjCRuntime::CreateObjectChecker(std::string name, ExecutionContext &exe_ctx) {
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::CreateObjectChecker called with name: {0}", name);
  
  // GNUstep object checker implementation
  // Unlike Apple, GNUstep doesn't have gdb_object_getClass, so we use a simpler approach
  // that doesn't require dynamic function resolution during expression evaluation
  
  char check_function_code[2048];
  
  // Use a simplified object checker that avoids calling runtime functions
  // that might not be available in the expression context
  int len = ::snprintf(check_function_code, sizeof(check_function_code), R"(
                     extern "C" int printf(const char *format, ...);
                     extern "C" void
                     %s(void *$__lldb_arg_obj, void *$__lldb_arg_selector) {
                       // nil objects are always acceptable
                       if ($__lldb_arg_obj == (void *)0)
                         return;
                       
                       // For GNUstep, we perform basic pointer validation
                       // Check if the pointer looks reasonable (not in low memory)
                       unsigned long addr = (unsigned long)$__lldb_arg_obj;
                       if (addr < 0x1000) {
                         // Very low addresses are likely invalid
                         *((volatile int *)0) = 'ocgc';
                         return;
                       }
                       
                       // Try to dereference the isa pointer safely
                       // In GNUstep, isa is the first field of any object
                       void **obj_as_ptr = (void **)$__lldb_arg_obj;
                       void *isa = *obj_as_ptr;
                       
                       // Basic sanity check on the isa pointer
                       if (isa == (void *)0 || (unsigned long)isa < 0x1000) {
                         // Invalid isa pointer
                         *((volatile int *)0) = 'ocgc';
                         return;
                       }
                       
                       // If we got here, the object passed basic validation
                       // For selector checking, we skip it to avoid runtime calls
                       // that might fail in expression evaluation context
                     })",
                     name.c_str());

  if (len >= (int)sizeof(check_function_code)) {
    LLDB_LOG(log, "GNUstepObjCRuntime::CreateObjectChecker: Generated code too long");
    return llvm::createStringError(llvm::inconvertibleErrorCode(), 
                                   "Object checker code generation failed - code too long");
  }

  LLDB_LOG(log, "GNUstepObjCRuntime::CreateObjectChecker: Generated function code:\n{0}", 
           check_function_code);

  // Create the utility function that LLDB can execute
  return GetTargetRef().CreateUtilityFunction(check_function_code, name,
                                              eLanguageTypeC, exe_ctx);
}

void GNUstepObjCRuntime::UpdateISAToDescriptorMapIfNeeded() {
  // Stub implementation
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::UpdateISAToDescriptorMapIfNeeded called");
}

ObjCLanguageRuntime::ClassDescriptorSP
GNUstepObjCRuntime::GetClassDescriptorFromISA(ObjCISA isa) {
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::GetClassDescriptorFromISA called with ISA {0:x}", isa);
  
  if (!isa)
    return ClassDescriptorSP();
  
  // First check the base class cache
  UpdateISAToDescriptorMap();
  ClassDescriptorSP descriptor_sp = ObjCLanguageRuntime::GetClassDescriptorFromISA(isa);
  if (descriptor_sp)
    return descriptor_sp;
  
  // If not in cache, create a new GNUstepClassDescriptor
  descriptor_sp = ClassDescriptorSP(new GNUstepClassDescriptor(*this, isa, nullptr));
  
  // Add to cache if valid
  if (descriptor_sp && descriptor_sp->IsValid()) {
    AddClass(isa, descriptor_sp);
    return descriptor_sp;
  }
  
  return ClassDescriptorSP();
}

ObjCLanguageRuntime::ClassDescriptorSP
GNUstepObjCRuntime::GetClassDescriptor(ValueObject &valobj) {
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::GetClassDescriptor called for ValueObject");
  
  // Handle base class case (like Apple's implementation)
  if (valobj.IsBaseClass()) {
    ValueObject *parent = valobj.GetParent();
    if (parent && parent != &valobj) {
      ClassDescriptorSP parent_descriptor_sp = GetClassDescriptor(*parent);
      if (parent_descriptor_sp)
        return parent_descriptor_sp->GetSuperclass();
    }
    return nullptr;
  }
  
  // Get the ISA pointer from the object
  addr_t isa_pointer = valobj.GetPointerValue();
  if (!isa_pointer)
    return ClassDescriptorSP();
  
  // Read the ISA from the object's first field
  ExecutionContext exe_ctx(valobj.GetExecutionContextRef());
  Process *process = exe_ctx.GetProcessPtr();
  if (!process)
    return ClassDescriptorSP();
  
  Status error;
  ObjCISA isa = process->ReadPointerFromMemory(isa_pointer, error);
  if (error.Fail())
    return ClassDescriptorSP();
  
  return GetClassDescriptorFromISA(isa);
}

void GNUstepObjCRuntime::ModulesDidLoad(const ModuleList &module_list) {
  // CRITICAL FIX: Add guards to prevent infinite recursion
  static thread_local int recursion_depth = 0;
  static thread_local bool in_modules_did_load = false;
  
  // Prevent infinite recursion
  if (in_modules_did_load || recursion_depth > 5) {
    Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
    LLDB_LOG(log, "GNUstepObjCRuntime::ModulesDidLoad - recursion detected, skipping (depth={0})", recursion_depth);
    return;
  }
  
  // Set guards
  in_modules_did_load = true;
  recursion_depth++;
  
  // Reduce debug spam - only log when we find GNUstep libraries
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::ModulesDidLoad called with {0} modules", 
           module_list.GetSize());
  
  // Call parent class method first (like Apple's implementation)
  ObjCLanguageRuntime::ModulesDidLoad(module_list);
  
  // Check if any of the newly loaded modules are GNUstep ObjC libraries
  bool found_gnustep = false;
  for (size_t i = 0; i < module_list.GetSize(); ++i) {
    ModuleSP module_sp = module_list.GetModuleAtIndex(i);
    if (IsModuleObjCLibrary(module_sp)) {
      found_gnustep = true;
      m_gnustep_library_loaded = true;
      ReadObjCLibrary(module_sp);
      LLDB_LOG(log, "GNUstepObjCRuntime: Found GNUstep library: {0}", 
               module_sp->GetFileSpec().GetFilename().GetCString());
    }
  }
  
  // Register formatters when we confirm GNUstep libraries are loaded
  if (found_gnustep) {
    if (!m_formatters_registered) {
      LLDB_LOG(log, "GNUstepObjCRuntime: GNUstep libraries detected, registering formatters");
      RegisterFormatters();
    }
    
    // Initialize the runtime API now that libraries are loaded
    if (!m_runtime_api_up) {
      InitializeRuntimeAPI();
    }
  }
  
  // Clear guards
  recursion_depth--;
  in_modules_did_load = false;
}

void GNUstepObjCRuntime::InitializeRuntimeAPI() {
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::InitializeRuntimeAPI - Initializing runtime API");
  
  if (!m_process) {
    LLDB_LOG(log, "No process available for runtime API");
    return;
  }
  
  auto api_or_error = GNUstepRuntimeV2API::Create(m_process);
  if (api_or_error) {
    m_runtime_api_up = std::move(*api_or_error);
    LLDB_LOG(log, "Runtime V2 API initialized successfully");
    
    // Log runtime version
    if (m_runtime_api_up) {
      std::string version = m_runtime_api_up->GetRuntimeVersion();
      LLDB_LOG(log, "Runtime version: {0}", version);
      
      // Enumerate and log Foundation classes
      auto foundation_classes = m_runtime_api_up->GetAllFoundationClasses();
      if (foundation_classes) {
        LLDB_LOG(log, "Found {0} Foundation classes", foundation_classes->size());
        
        // Log first few Foundation classes for debugging
        size_t count = 0;
        for (const auto &cls : *foundation_classes) {
          if (count++ < 10) {
            LLDB_LOG(log, "  Foundation class: {0} (superclass: {1})", 
                     cls.name, cls.superclass_name);
          }
        }
      } else {
        // Consume the error from GetAllFoundationClasses
        llvm::consumeError(foundation_classes.takeError());
        // This is not critical - formatters will still work through other mechanisms
        LLDB_LOG(log, "[GNUstepObjC] Note: Runtime class enumeration not available, using fallback mechanisms");
      }
    }
  } else {
    llvm::handleAllErrors(api_or_error.takeError(),
                          [&](const llvm::StringError &SE) {
                            LLDB_LOG(log, "Failed to initialize runtime API: {0}", 
                                     SE.getMessage());
                          });
  }
}

void GNUstepObjCRuntime::RegisterFormatters() {
  if (m_formatters_registered) {
    return;
  }
    
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime::RegisterFormatters - Registering GNUstep formatters");
  
  // Get or create the GNUstep type category
  TypeCategoryImplSP category_sp;
  if (!DataVisualization::Categories::GetCategory(ConstString("gnustep"), 
                                                  category_sp, true)) {
    LLDB_LOG(log, "Failed to get/create GNUstep category");
    return;
  }
  
  if (!category_sp) {
    LLDB_LOG(log, "Got null category pointer");
    return;
  }
  
  // Register our formatters
  GNUstepFormattersRegistry::RegisterFormatters(*category_sp);
  
  // Enable the category
  DataVisualization::Categories::Enable(ConstString("gnustep"), 
                                        TypeCategoryMap::Default);
  
  m_formatters_registered = true;
  LLDB_LOG(log, "GNUstep formatters registered and category enabled");
}

DeclVendor *GNUstepObjCRuntime::GetDeclVendor() {
  if (!m_decl_vendor_up) {
    m_decl_vendor_up = std::make_unique<GNUstepObjCDeclVendor>(*this);
  }
  
  return m_decl_vendor_up.get();
}

LLDB_PLUGIN_DEFINE(GNUstepObjCRuntime)
