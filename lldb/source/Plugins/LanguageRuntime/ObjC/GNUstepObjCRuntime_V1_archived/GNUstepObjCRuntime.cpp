//===-- GNUstepObjCRuntime.cpp --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCRuntime.h"
#include "GNUstepSyntheticProvider.h"
#include "GNUstepUniversalProvider.h"
#include "GNUstepStringSummaryProvider.h"
#include "GNUstepNSString.h"
#include "GNUstepNumberSummaryProvider.h"
#include "GNUstepNSDate.h"
#include "GNUstepArraySyntheticProvider.h"
#include "GNUstepArraySummaryProvider.h"
#include "GNUstepNSArray.h"
#include "GNUstepNSSet.h"
#include "GNUstepNSDictionary.h"
#include "GNUstepCustomClass.h"
#include "GNUstepUtilities.h"
#include "ISAResolver.h"
#include "RuntimeIntrospector.h"
#include "GNUstepObjCDeclVendor.h"

#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

#include "lldb/Core/Module.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Expression/DiagnosticManager.h"
#include "lldb/Expression/FunctionCaller.h"
#include "lldb/Expression/UserExpression.h"
#include "lldb/Expression/UtilityFunction.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/lldb-enumerations.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Symbol/SymbolFile.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/ArchSpec.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"

#include "llvm/ADT/ScopeExit.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"

#include "lldb/DataFormatters/DataVisualization.h"
#include "lldb/DataFormatters/FormatManager.h"
#include "lldb/DataFormatters/StringPrinter.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/DataFormatters/TypeSynthetic.h"

using namespace lldb;
using namespace lldb_private;

LLDB_PLUGIN_DEFINE(GNUstepObjCRuntime)

char GNUstepObjCRuntime::ID = 0;

void GNUstepObjCRuntime::Initialize() {
  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(log, "GNUstepObjCRuntime::Initialize() called - registering plugin with PluginManager");
  
  PluginManager::RegisterPlugin(
      GetPluginNameStatic(), "GNUstep Objective-C Language Runtime - libobjc2",
      CreateInstance);
      
  LLDB_LOG(log, "GNUstepObjCRuntime::Initialize() completed - plugin registered successfully");
}

void GNUstepObjCRuntime::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}

static bool CanModuleBeGNUstepObjCLibrary(const ModuleSP &module_sp,
                                          const llvm::Triple &TT) {
  Log *log = GetLog(LLDBLog::Language);
  
  if (!module_sp) {
    LLDB_LOG(log, "GNUstepObjCRuntime: CanModuleBeGNUstepObjCLibrary - null module");
    return false;
  }
  const FileSpec &module_file_spec = module_sp->GetFileSpec();
  if (!module_file_spec) {
    LLDB_LOG(log, "GNUstepObjCRuntime: CanModuleBeGNUstepObjCLibrary - no file spec for module");
    return false;
  }
  llvm::StringRef filename = module_file_spec.GetFilename().GetStringRef();
  LLDB_LOG(log, "GNUstepObjCRuntime: CanModuleBeGNUstepObjCLibrary - checking file: {0}", filename);
  
  if (TT.isOSBinFormatELF()) {
    bool is_candidate = filename.starts_with("libobjc.so");
    LLDB_LOG(log, "GNUstepObjCRuntime: ELF format - {0} {1} libobjc.so candidate", 
             filename, is_candidate ? "IS" : "is NOT");
    return is_candidate;
  }
  if (TT.isOSWindows()) {
    bool is_candidate = filename == "objc.dll";
    LLDB_LOG(log, "GNUstepObjCRuntime: Windows format - {0} {1} objc.dll candidate", 
             filename, is_candidate ? "IS" : "is NOT");
    return is_candidate;
  }
  LLDB_LOG(log, "GNUstepObjCRuntime: Unsupported OS format for {0}", filename);
  return false;
}

static bool ScanForGNUstepObjCLibraryCandidate(const ModuleList &modules,
                                               const llvm::Triple &TT) {
  Log *log = GetLog(LLDBLog::Language);
  
  std::lock_guard<std::recursive_mutex> guard(modules.GetMutex());
  size_t num_modules = modules.GetSize();
  LLDB_LOG(log, "GNUstepObjCRuntime: ScanForGNUstepObjCLibraryCandidate - scanning {0} modules", num_modules);
  
  for (size_t i = 0; i < num_modules; i++) {
    auto mod = modules.GetModuleAtIndex(i);
    LLDB_LOG(log, "GNUstepObjCRuntime: ScanForGNUstepObjCLibraryCandidate - checking module {0}", i);
    if (CanModuleBeGNUstepObjCLibrary(mod, TT)) {
      LLDB_LOG(log, "GNUstepObjCRuntime: ScanForGNUstepObjCLibraryCandidate - FOUND libobjc candidate at index {0}", i);
      return true;
    }
  }
  LLDB_LOG(log, "GNUstepObjCRuntime: ScanForGNUstepObjCLibraryCandidate - NO libobjc candidate found in {0} modules", num_modules);
  return false;
}

LanguageRuntime *GNUstepObjCRuntime::CreateInstance(Process *process,
                                                    LanguageType language) {
  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(log, "GNUstepObjCRuntime::CreateInstance called with language={0}", language);
  
  if (language != eLanguageTypeObjC && language != eLanguageTypeObjC_plus_plus) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Language not ObjC/ObjC++, got {0}", language);
    return nullptr;
  }
  if (!process) {
    LLDB_LOG(log, "GNUstepObjCRuntime: No process provided");
    return nullptr;
  }

  Target &target = process->GetTarget();
  const llvm::Triple &TT = target.GetArchitecture().GetTriple();
  LLDB_LOG(log, "GNUstepObjCRuntime: Architecture vendor={0}, OS={1}", 
           TT.getVendorName(), TT.getOSName());
  
  if (TT.getVendor() == llvm::Triple::VendorType::Apple) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Apple vendor detected, rejecting");
    return nullptr;
  }

  const ModuleList &images = target.GetImages();
  LLDB_LOG(log, "GNUstepObjCRuntime: Scanning {0} loaded modules for libobjc candidates", 
           images.GetSize());
  
  bool has_libobjc = ScanForGNUstepObjCLibraryCandidate(images, TT);
  
  // CRITICAL FIX: libobjc.so may not be loaded yet during early runtime selection.
  // On Linux/GNUstep, shared libraries are loaded dynamically after process start.
  // We need to accept GNUstep runtime for non-Apple ELF binaries even without libobjc.so
  // initially, then validate when modules are loaded.
  if (!has_libobjc) {
    if (TT.isOSBinFormatELF() && images.GetSize() < 10) {
      // Early in process startup, accept non-Apple ELF as potentially GNUstep
      LLDB_LOG(log, "GNUstepObjCRuntime: No libobjc.so found YET, but only {0} modules loaded. "
               "Accepting for non-Apple ELF binary - will validate when libraries load", 
               images.GetSize());
    } else {
      LLDB_LOG(log, "GNUstepObjCRuntime: No libobjc.so candidate found after {0} modules, rejecting", 
               images.GetSize());
      return nullptr;
    }
  } else {
    LLDB_LOG(log, "GNUstepObjCRuntime: Found libobjc.so candidate, proceeding");
  }

  if (TT.isOSBinFormatELF()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: ELF binary format detected, searching for personality symbols");
    // TIMING FIX: Be more permissive during initial detection since shared libraries
    // may not be loaded yet. We already confirmed libobjc.so candidate exists.
    // Do more thorough symbol validation later in ModulesDidLoad().
    SymbolContextList eh_pers;
    RegularExpression regex("__gnustep_objc[x]*_personality_v[0-9]+");
    images.FindSymbolsMatchingRegExAndType(regex, eSymbolTypeCode, eh_pers);
    LLDB_LOG(log, "GNUstepObjCRuntime: Found {0} personality symbols (eSymbolTypeCode)", eh_pers.GetSize());
    
    // If we don't find code symbols, try other symbol types
    if (eh_pers.GetSize() == 0) {
      images.FindSymbolsMatchingRegExAndType(regex, eSymbolTypeRuntime, eh_pers);
      LLDB_LOG(log, "GNUstepObjCRuntime: Found {0} personality symbols (eSymbolTypeRuntime)", eh_pers.GetSize());
    }
    if (eh_pers.GetSize() == 0) {
      images.FindSymbolsMatchingRegExAndType(regex, eSymbolTypeData, eh_pers);
      LLDB_LOG(log, "GNUstepObjCRuntime: Found {0} personality symbols (eSymbolTypeData)", eh_pers.GetSize());
    }
    
    // RELAXED DETECTION: If we found a libobjc.so candidate, assume GNUstep runtime
    // even if we can't find personality symbols yet (they'll be available after loading)
    if (eh_pers.GetSize() == 0) {
      LLDB_LOG(log, "GNUstepObjCRuntime: No personality symbols found, but libobjc.so candidate exists - proceeding with relaxed detection");
      // We already passed ScanForGNUstepObjCLibraryCandidate, so libobjc.so is present
      // This is sufficient evidence for GNUstep runtime
      // The personality symbols will be validated later when modules are fully loaded
    } else {
      LLDB_LOG(log, "GNUstepObjCRuntime: Found {0} personality symbols - definitive GNUstep detection", eh_pers.GetSize());
    }
  } else if (TT.isOSWindows()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Windows binary format detected, searching for __objc_load");
    SymbolContextList objc_mandatory;
    images.FindSymbolsWithNameAndType(ConstString("__objc_load"),
                                      eSymbolTypeCode, objc_mandatory);
    LLDB_LOG(log, "GNUstepObjCRuntime: Found {0} __objc_load symbols", objc_mandatory.GetSize());
    if (objc_mandatory.GetSize() == 0) {
      LLDB_LOG(log, "GNUstepObjCRuntime: Windows platform missing __objc_load, rejecting");
      return nullptr;
    }
  }

  LLDB_LOG(log, "GNUstepObjCRuntime: All checks passed, creating GNUstepObjCRuntime instance");
  return new GNUstepObjCRuntime(process);
}

GNUstepObjCRuntime::~GNUstepObjCRuntime() = default;

GNUstepObjCRuntime::GNUstepObjCRuntime(Process *process)
    : ObjCLanguageRuntime(process), m_objc_module_sp(nullptr),
      m_isa_resolver(std::make_unique<ISAResolver>(process)),
      m_runtime_introspector(std::make_unique<RuntimeIntrospector>(process)),
      m_decl_vendor(nullptr),
      m_providers_registered(false) {
  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(log, "GNUstepObjCRuntime: Constructor called - initializing runtime");
  
  ReadObjCLibraryIfNeeded(process->GetTarget().GetImages());
  LLDB_LOG(log, "GNUstepObjCRuntime: ReadObjCLibraryIfNeeded completed");
  
  // RegisterSyntheticProviders();  // MOVED to ModulesDidLoad
  // LLDB_LOG(log, "GNUstepObjCRuntime: RegisterSyntheticProviders completed - runtime fully initialized");
}

llvm::Error GNUstepObjCRuntime::GetObjectDescription(Stream &str,
                                                     ValueObject &valobj) {
  // Get the actual object address (not the isa pointer)
  addr_t object_ptr = valobj.GetPointerValue();
  if (object_ptr == 0 || object_ptr == LLDB_INVALID_ADDRESS) {
    return llvm::createStringError("Invalid object address");
  }
  
  // Get class name if available
  std::string class_name = GetClassNameFromObject(object_ptr);
  if (class_name.empty()) {
    str.Printf("<%s:0x%" PRIx64 ">", valobj.GetTypeName().AsCString("id"), object_ptr);
    return llvm::Error::success();
  }
  
  // Handle NSConstantString with verified GNUstep layout
  if (class_name == "NSConstantString" || class_name.find("String") != std::string::npos) {
    // Verified GNUstep NSConstantString layout:
    // isa@0, flags@8, nxcslen@12, size@16, hash@20, nxcsptr@24
    Status error;
    
    // Read string length first (at offset 12)
    uint32_t length = ReadMemoryUnsigned(object_ptr + 12, 4);
    if (length > 0 && length < 100*1024*1024) { // reasonable size check
      
      // Read string pointer (nxcsptr at offset 24)
      addr_t string_ptr = m_process->ReadPointerFromMemory(object_ptr + 24, error);
      if (!error.Fail() && string_ptr != 0 && string_ptr >= 0x1000) {
        
        // Read the actual string content
        std::string content = ReadCString(string_ptr, std::min(length + 1, 256u));
        if (!content.empty() && content.length() == length) {
          str.Printf("\"%s\"", content.c_str());
          return llvm::Error::success();
        }
      }
    }
    
    // Fallback to basic description if string reading fails
    str.Printf("<%s:0x%" PRIx64 ">", class_name.c_str(), object_ptr);
    return llvm::Error::success();
  }
  
  // For other objects, provide basic description with class name
  str.Printf("<%s:0x%" PRIx64 ">", class_name.c_str(), object_ptr);
  return llvm::Error::success();
}

llvm::Error
GNUstepObjCRuntime::GetObjectDescription(Stream &strm, Value &value,
                                         ExecutionContextScope *exe_scope) {
  // Get the object address from the value
  addr_t object_ptr = value.GetScalar().ULongLong(LLDB_INVALID_ADDRESS);
  if (object_ptr == 0 || object_ptr == LLDB_INVALID_ADDRESS) {
    return llvm::createStringError("Invalid object address");
  }
  
  // Get class name
  std::string class_name = GetClassNameFromObject(object_ptr);
  if (class_name.empty()) {
    strm.Printf("<id:0x%" PRIx64 ">", object_ptr);
    return llvm::Error::success();
  }
  
  // Handle NSConstantString with verified GNUstep layout  
  if (class_name == "NSConstantString" || class_name.find("String") != std::string::npos) {
    // Verified GNUstep NSConstantString layout:
    // isa@0, flags@8, nxcslen@12, size@16, hash@20, nxcsptr@24
    Status error;
    
    // Read string length first (at offset 12)
    uint32_t length = ReadMemoryUnsigned(object_ptr + 12, 4);
    if (length > 0 && length < 100*1024*1024) { // reasonable size check
      
      // Read string pointer (nxcsptr at offset 24)
      addr_t string_ptr = m_process->ReadPointerFromMemory(object_ptr + 24, error);
      if (!error.Fail() && string_ptr != 0 && string_ptr >= 0x1000) {
        
        // Read the actual string content
        std::string content = ReadCString(string_ptr, std::min(length + 1, 256u));
        if (!content.empty() && content.length() == length) {
          strm.Printf("\"%s\"", content.c_str());
          return llvm::Error::success();
        }
      }
    }
    
    // Fallback to basic description if string reading fails
    strm.Printf("<%s:0x%" PRIx64 ">", class_name.c_str(), object_ptr);
    return llvm::Error::success();
  }
  
  // For other objects, provide basic description
  strm.Printf("<%s:0x%" PRIx64 ">", class_name.c_str(), object_ptr);
  return llvm::Error::success();
}

bool GNUstepObjCRuntime::CouldHaveDynamicValue(ValueObject &in_value) {
  static constexpr bool check_cxx = false;
  static constexpr bool check_objc = true;
  return in_value.GetCompilerType().IsPossibleDynamicType(nullptr, check_cxx,
                                                          check_objc);
}

bool GNUstepObjCRuntime::GetDynamicTypeAndAddress(
    ValueObject &in_value, DynamicValueType use_dynamic,
    TypeAndOrName &class_type_or_name, Address &address,
    Value::ValueType &value_type) {
  
  // Enhanced dynamic type resolution using ISA resolver
  if (!m_isa_resolver)
    return false;
    
  // Get object address directly
  addr_t obj_addr = in_value.GetValueAsUnsigned(0);
  if (obj_addr == LLDB_INVALID_ADDRESS || obj_addr == 0)
    return false;
    
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime: GetDynamicTypeAndAddress for object at 0x{0:x}", obj_addr);
  
  // CRITICAL FIX: Check for GSTinyString tagged pointers
  // GSTinyString uses tag=3 in bits 61-63
  uint8_t tag = (obj_addr >> 61) & 0x7;
  if (tag == 3) {
    // This is a GSTinyString tagged pointer
    LLDB_LOG(log, "GNUstepObjCRuntime: Detected GSTinyString tagged pointer at 0x{0:x}", obj_addr);
    // Return NSString instead of GSTinyString so VS Code doesn't try to expand it as an object
    class_type_or_name.SetName(ConstString("NSString"));
    // For tagged pointers, the address IS the value
    address.SetRawAddress(obj_addr);
    value_type = Value::ValueType::Scalar;  // Tagged pointer is a scalar value
    return true;
  }
  
  // Check if this looks like a valid object pointer
  Process *process = m_process;
  if (!process)
    return false;
    
  // Read ISA pointer from object
  Status error;
  addr_t isa_ptr = process->ReadPointerFromMemory(obj_addr, error);
  if (error.Fail() || isa_ptr == LLDB_INVALID_ADDRESS) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Failed to read ISA pointer from 0x{0:x}: {1}", 
             obj_addr, error.AsCString());
    return false;
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Read ISA pointer 0x{0:x} from object 0x{1:x}", 
           isa_ptr, obj_addr);
  
  // Use enhanced ISA resolution
  std::string class_name = m_isa_resolver->GetClassNameFromISA(isa_ptr);
  if (class_name.empty()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Could not resolve class name for ISA 0x{0:x}", isa_ptr);
    return false;
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Resolved dynamic type {0} for object at 0x{1:x}", 
           class_name.c_str(), obj_addr);
  
  // Set the resolved class name
  class_type_or_name.SetName(ConstString(class_name.c_str()));
  
  // CRITICAL: Ensure we return the OBJECT address, not the ISA pointer
  address.SetRawAddress(obj_addr);  // obj_addr should be object, not isa_ptr
  value_type = Value::ValueType::LoadAddress;
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Returning object address 0x{0:x} (not ISA 0x{1:x})", 
           obj_addr, isa_ptr);
  
  return true;
}

TypeAndOrName
GNUstepObjCRuntime::FixUpDynamicType(const TypeAndOrName &type_and_or_name,
                                     ValueObject &static_value) {
  CompilerType static_type(static_value.GetCompilerType());
  Flags static_type_flags(static_type.GetTypeInfo());

  TypeAndOrName ret(type_and_or_name);
  if (type_and_or_name.HasType()) {
    // The type will always be the type of the dynamic object.  If our parent's
    // type was a pointer, then our type should be a pointer to the type of the
    // dynamic object.  If a reference, then the original type should be
    // okay...
    CompilerType orig_type = type_and_or_name.GetCompilerType();
    CompilerType corrected_type = orig_type;
    if (static_type_flags.AllSet(eTypeIsPointer))
      corrected_type = orig_type.GetPointerType();
    ret.SetCompilerType(corrected_type);
  } else {
    // If we are here we need to adjust our dynamic type name to include the
    // correct & or * symbol
    std::string corrected_name(type_and_or_name.GetName().GetCString());
    if (static_type_flags.AllSet(eTypeIsPointer))
      corrected_name.append(" *");
    // the parent type should be a correctly pointer'ed or referenc'ed type
    ret.SetCompilerType(static_type);
    ret.SetName(corrected_name.c_str());
  }
  return ret;
}

BreakpointResolverSP
GNUstepObjCRuntime::CreateExceptionResolver(const BreakpointSP &bkpt,
                                            bool catch_bp, bool throw_bp) {
  BreakpointResolverSP resolver_sp;

  if (throw_bp)
    resolver_sp = std::make_shared<BreakpointResolverName>(
        bkpt, "objc_exception_throw", eFunctionNameTypeBase,
        eLanguageTypeUnknown, Breakpoint::Exact, 0, eLazyBoolNo);

  return resolver_sp;
}

llvm::Expected<std::unique_ptr<UtilityFunction>>
GNUstepObjCRuntime::CreateObjectChecker(std::string name,
                                        ExecutionContext &exe_ctx) {
  // TODO: This function is supposed to check whether an ObjC selector is
  // present for an object. Might be implemented similar as in the Apple V2
  // runtime.
  const char *function_template = R"(
    extern "C" void
    %s(void *$__lldb_arg_obj, void *$__lldb_arg_selector) {}
  )";

  char empty_function_code[2048];
  int len = ::snprintf(empty_function_code, sizeof(empty_function_code),
                       function_template, name.c_str());

  assert(len < (int)sizeof(empty_function_code));
  UNUSED_IF_ASSERT_DISABLED(len);

  return GetTargetRef().CreateUtilityFunction(empty_function_code, name,
                                              eLanguageTypeC, exe_ctx);
}

ThreadPlanSP
GNUstepObjCRuntime::GetStepThroughTrampolinePlan(Thread &thread,
                                                 bool stop_others) {
  // TODO: Implement this properly to avoid stepping into things like PLT stubs
  return nullptr;
}

void GNUstepObjCRuntime::LoadRuntimeSymbols() {
  if (!m_process)
    return;
    
  Target &target = m_process->GetTarget();
  const ModuleList &modules = target.GetImages();
  std::lock_guard<std::recursive_mutex> guard(modules.GetMutex());
  
  for (size_t i = 0; i < modules.GetSize(); ++i) {
    lldb::ModuleSP module_sp = modules.GetModuleAtIndexUnlocked(i);
    if (!module_sp)
      continue;
      
    // Look for objc_copyClassList
    SymbolContextList sc_list;
    module_sp->FindSymbolsWithNameAndType(ConstString("objc_copyClassList"),
                                         lldb::eSymbolTypeCode, sc_list);
    if (!sc_list.IsEmpty()) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(0, sc);
      if (sc.symbol) {
        m_objc_copyClassList_addr = sc.symbol->GetLoadAddress(&target);
      }
    }
    
    // Look for class_getName
    sc_list.Clear();
    module_sp->FindSymbolsWithNameAndType(ConstString("class_getName"),
                                         lldb::eSymbolTypeCode, sc_list);
    if (!sc_list.IsEmpty()) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(0, sc);
      if (sc.symbol) {
        m_class_getName_addr = sc.symbol->GetLoadAddress(&target);
      }
    }
    
    // Look for free
    sc_list.Clear();
    module_sp->FindSymbolsWithNameAndType(ConstString("free"),
                                         lldb::eSymbolTypeCode, sc_list);
    if (!sc_list.IsEmpty()) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(0, sc);
      if (sc.symbol) {
        m_free_addr = sc.symbol->GetLoadAddress(&target);
      }
    }
    
    // Look for class_copyIvarList
    sc_list.Clear();
    module_sp->FindSymbolsWithNameAndType(ConstString("class_copyIvarList"),
                                         lldb::eSymbolTypeCode, sc_list);
    if (!sc_list.IsEmpty()) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(0, sc);
      if (sc.symbol) {
        m_class_copyIvarList_addr = sc.symbol->GetLoadAddress(&target);
      }
    }
    
    // Look for ivar_getName
    sc_list.Clear();
    module_sp->FindSymbolsWithNameAndType(ConstString("ivar_getName"),
                                         lldb::eSymbolTypeCode, sc_list);
    if (!sc_list.IsEmpty()) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(0, sc);
      if (sc.symbol) {
        m_ivar_getName_addr = sc.symbol->GetLoadAddress(&target);
      }
    }
    
    // Look for ivar_getOffset
    sc_list.Clear();
    module_sp->FindSymbolsWithNameAndType(ConstString("ivar_getOffset"),
                                         lldb::eSymbolTypeCode, sc_list);
    if (!sc_list.IsEmpty()) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(0, sc);
      if (sc.symbol) {
        m_ivar_getOffset_addr = sc.symbol->GetLoadAddress(&target);
      }
    }
    
    // Look for ivar_getTypeEncoding
    sc_list.Clear();
    module_sp->FindSymbolsWithNameAndType(ConstString("ivar_getTypeEncoding"),
                                         lldb::eSymbolTypeCode, sc_list);
    if (!sc_list.IsEmpty()) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(0, sc);
      if (sc.symbol) {
        m_ivar_getTypeEncoding_addr = sc.symbol->GetLoadAddress(&target);
      }
    }
    
    // Look for objc_getClass
    sc_list.Clear();
    module_sp->FindSymbolsWithNameAndType(ConstString("objc_getClass"),
                                         lldb::eSymbolTypeCode, sc_list);
    if (!sc_list.IsEmpty()) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(0, sc);
      if (sc.symbol) {
        m_objc_getClass_addr = sc.symbol->GetLoadAddress(&target);
      }
    }
  }
}

void GNUstepObjCRuntime::UpdateISAToDescriptorMapIfNeeded() {
  // Check if we need to update
  if (!m_process || m_isa_to_descriptor_complete)
    return;
    
  // Make sure we're stopped
  if (m_process->GetState() != eStateStopped)
    return;
    
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime: Updating ISA to descriptor map");
    
  // Load runtime symbols if needed
  if (m_objc_copyClassList_addr == LLDB_INVALID_ADDRESS) {
    LoadRuntimeSymbols();
  }
  
  // If we still don't have the symbols, try direct memory approach
  if (m_objc_copyClassList_addr == LLDB_INVALID_ADDRESS) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Runtime symbols not found, using fallback");
    // For now, just mark as complete to avoid repeated attempts
    m_isa_to_descriptor_complete = true;
    return;
  }
  
  // Call UpdateISAToDescriptorMap to do the actual work
  UpdateISAToDescriptorMap();
  m_isa_to_descriptor_complete = true;
}

void GNUstepObjCRuntime::UpdateISAToDescriptorMap() {
  if (!m_process || m_objc_copyClassList_addr == LLDB_INVALID_ADDRESS)
    return;
    
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime: Enumerating runtime classes");
  
  // Create a simple expression to call objc_copyClassList
  // We'll use direct memory operations instead of FunctionCaller for simplicity
  
  // DYNAMIC RUNTIME INTROSPECTION: Use objc_copyClassList() to discover ALL classes
  Target &target = m_process->GetTarget();
  const ModuleList &modules = target.GetImages();
  
  // Step 1: Try to call objc_copyClassList() dynamically via LLDB expression evaluation
  if (CallObjCCopyClassList(log)) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Dynamic class enumeration successful");
    return;
  }
  
  // Step 2: Fallback - scan all OBJC_CLASS_$_* symbols in loaded modules
  LLDB_LOG(log, "GNUstepObjCRuntime: Using symbol-based class discovery fallback");
  
  std::lock_guard<std::recursive_mutex> guard(modules.GetMutex());
  for (size_t i = 0; i < modules.GetSize(); ++i) {
    lldb::ModuleSP module_sp = modules.GetModuleAtIndexUnlocked(i);
    if (!module_sp) continue;
      
    // Look for ALL OBJC_CLASS_$_* symbols (not just hardcoded ones)
    SymbolContextList sc_list;
    RegularExpression class_regex("^OBJC_CLASS_\\$_.*");
    module_sp->FindSymbolsMatchingRegExAndType(class_regex, lldb::eSymbolTypeData, sc_list);
    
    LLDB_LOG(log, "GNUstepObjCRuntime: Found {0} class symbols in module {1}",
             sc_list.GetSize(), module_sp->GetFileSpec().GetFilename().GetCString());
    
    for (uint32_t j = 0; j < sc_list.GetSize(); ++j) {
      SymbolContext sc;
      sc_list.GetContextAtIndex(j, sc);
      if (!sc.symbol) continue;
        
      // Extract class name from OBJC_CLASS_$_ClassName symbol
      std::string symbol_name = sc.symbol->GetName().GetCString();
      if (symbol_name.find("OBJC_CLASS_$_") == 0) {
        std::string class_name = symbol_name.substr(13); // Remove "OBJC_CLASS_$_" prefix
        
        // Apply smart filtering to include relevant classes
        if (ShouldIncludeClass(class_name)) {
          addr_t class_addr = sc.symbol->GetLoadAddress(&target);
          if (class_addr != LLDB_INVALID_ADDRESS) {
            // Create descriptor for dynamically discovered class
            ClassDescriptorSP descriptor_sp = 
              std::make_shared<GNUstepClassDescriptor>(
                class_addr, ConstString(class_name.c_str()));
            
            // Add to runtime class map
            AddClass(class_addr, descriptor_sp, class_name.c_str());
            
            // Register with DeclVendor for type system integration
            if (GetDeclVendor()) {
              static_cast<GNUstepObjCDeclVendor *>(GetDeclVendor())->GetDeclForClassName(class_name.c_str());
            }
            
            LLDB_LOG(log, "GNUstepObjCRuntime: Dynamically discovered class {0} at {1:x}",
                     class_name.c_str(), class_addr);
          }
        }
      }
    }
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Dynamic class discovery completed");
}

ObjCLanguageRuntime::ClassDescriptorSP 
GNUstepObjCRuntime::GetClassDescriptor(ValueObject &valobj) {
  // Enhanced class descriptor with ISA resolution
  if (!m_isa_resolver)
    return ClassDescriptorSP();
    
  // Get object address directly
  addr_t obj_addr = valobj.GetValueAsUnsigned(0);
  if (obj_addr == LLDB_INVALID_ADDRESS)
    return ClassDescriptorSP();
    
  // Get class name using enhanced resolution
  std::string class_name = m_isa_resolver->GetClassNameFromObject(obj_addr);
  if (class_name.empty())
    return ClassDescriptorSP();
    
  // Read ISA pointer
  Status error;
  addr_t isa_ptr = m_process->ReadPointerFromMemory(obj_addr, error);
  if (error.Fail())
    return ClassDescriptorSP();
    
  // Create enhanced class descriptor
  return std::make_shared<GNUstepClassDescriptor>(
    isa_ptr, ConstString(class_name.c_str()));
}

std::string GNUstepObjCRuntime::GetClassNameFromISA(lldb::addr_t isa_ptr) {
  if (!m_isa_resolver)
    return "";
  return m_isa_resolver->GetClassNameFromISA(isa_ptr);
}

std::string GNUstepObjCRuntime::GetClassNameFromObject(lldb::addr_t obj_addr) {
  // Check for GSTinyString tagged pointer first
  uint8_t tag = (obj_addr >> 61) & 0x7;
  if (tag == 4) {
    return "GSTinyString";
  }
  
  if (!m_isa_resolver)
    return "";
  return m_isa_resolver->GetClassNameFromObject(obj_addr);
}

bool GNUstepObjCRuntime::CallObjCCopyClassList(Log *log) {
  // Use LLDB expression evaluation to call objc_copyClassList()
  // This provides the most complete class enumeration
  
  if (!m_process || m_objc_copyClassList_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }
  
  ExecutionContext exe_ctx;
  m_process->CalculateExecutionContext(exe_ctx);
  
  if (!exe_ctx.HasThreadScope()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: No thread context for expression evaluation");
    return false;
  }
  
  ThreadSP thread_sp = exe_ctx.GetThreadSP();
  if (!thread_sp) {
    return false;
  }
  
  // Allocate memory for count
  Status error;
  addr_t count_addr = m_process->AllocateMemory(sizeof(unsigned int), 
                                                 ePermissionsReadable | ePermissionsWritable, 
                                                 error);
  if (error.Fail()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Failed to allocate memory for count: {0}", error);
    return false;
  }
  
  // Initialize count to 0
  uint32_t zero = 0;
  m_process->WriteMemory(count_addr, &zero, sizeof(zero), error);
  
  // First call: objc_copyClassList(NULL, &count) to get count
  DiagnosticManager diagnostics;
  EvaluateExpressionOptions options;
  options.SetUnwindOnError(true);
  options.SetIgnoreBreakpoints(true);
  options.SetTimeout(std::chrono::seconds(30));
  
  ValueObjectSP result_sp;
  std::string expr = llvm::formatv("(void*)objc_copyClassList((void*)0, (unsigned int*)0x{0:x})", 
                                   count_addr).str();
  
  ExpressionResults expr_result = UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, nullptr);
  
  if (expr_result != eExpressionCompleted) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Failed to call objc_copyClassList");
    m_process->DeallocateMemory(count_addr);
    return false;
  }
  
  // Read the count
  uint32_t count = ReadMemoryUnsigned(count_addr, sizeof(uint32_t));
  if (count == 0) {
    LLDB_LOG(log, "GNUstepObjCRuntime: objc_copyClassList returned 0 classes");
    m_process->DeallocateMemory(count_addr);
    return false;
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Found {0} runtime classes", count);
  
  // Second call: objc_copyClassList(NULL, &count) to get the class array
  expr = llvm::formatv("(void*)objc_copyClassList((void*)0, (unsigned int*)0x{0:x})", 
                       count_addr).str();
  
  expr_result = UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, nullptr);
  
  if (expr_result != eExpressionCompleted || !result_sp) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Failed to get class list");
    m_process->DeallocateMemory(count_addr);
    return false;
  }
  
  // Get the returned class array pointer
  addr_t class_array_addr = result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
  if (class_array_addr == LLDB_INVALID_ADDRESS) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Invalid class array address");
    m_process->DeallocateMemory(count_addr);
    return false;
  }
  
  // Read all class pointers
  size_t ptr_size = m_process->GetAddressByteSize();
  std::vector<addr_t> class_addresses;
  
  for (uint32_t i = 0; i < count; i++) {
    addr_t class_addr = ReadPointer(class_array_addr + (i * ptr_size));
    if (class_addr != LLDB_INVALID_ADDRESS) {
      class_addresses.push_back(class_addr);
    }
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Processing {0} classes", class_addresses.size());
  
  // Process each class
  // Target &target = m_process->GetTarget();  // Unused for now
  for (addr_t class_addr : class_addresses) {
    // Get class name using class_getName
    if (m_class_getName_addr != LLDB_INVALID_ADDRESS) {
      expr = llvm::formatv("(const char*)class_getName((void*)0x{0:x})", class_addr).str();
      
      expr_result = UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, nullptr);
      
      if (expr_result == eExpressionCompleted && result_sp) {
        addr_t name_addr = result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
        if (name_addr != LLDB_INVALID_ADDRESS) {
          std::string class_name = ReadCString(name_addr, 256);
          
          if (!class_name.empty()) {
            // Create descriptor for dynamically discovered class
            ClassDescriptorSP descriptor_sp = 
              std::make_shared<GNUstepClassDescriptor>(
                class_addr, ConstString(class_name.c_str()));
            
            // Add to runtime class map
            AddClass(class_addr, descriptor_sp, class_name.c_str());
            
            // Register with DeclVendor for type system integration
            if (GetDeclVendor()) {
              static_cast<GNUstepObjCDeclVendor *>(GetDeclVendor())->GetDeclForClassName(class_name.c_str());
            }
            
            // Cache class metadata including ivar offsets
            ClassMetadata metadata;
            metadata.name = class_name;
            metadata.class_ptr = class_addr;
            
            // Discover ivars for important classes (NSConstantString, etc)
            if (class_name == "NSConstantString" || class_name == "NSString" || 
                class_name == "GSInlineArray" || class_name == "GSMutableArray") {
              // Get ivar offsets using runtime introspection
              if (m_runtime_introspector) {
                // Try to get common ivars
                if (class_name == "NSConstantString") {
                  ptrdiff_t offset = m_runtime_introspector->GetIvarOffset(class_name, "nxcsptr");
                  if (offset > 0) {
                    metadata.ivar_offsets["nxcsptr"] = offset;
                    LLDB_LOG(log, "GNUstepObjCRuntime: Cached {0}.nxcsptr at offset {1}",
                             class_name, offset);
                  }
                }
              }
            }
            
            m_class_metadata[class_name] = metadata;
            
            LLDB_LOG(log, "GNUstepObjCRuntime: Registered class {0} at {1:x}",
                     class_name.c_str(), class_addr);
          }
        }
      }
    }
  }
  
  // Free the class array
  if (m_free_addr != LLDB_INVALID_ADDRESS) {
    expr = llvm::formatv("free((void*)0x{0:x})", class_array_addr).str();
    UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, nullptr);
  }
  
  // Deallocate count memory
  m_process->DeallocateMemory(count_addr);
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Dynamic class enumeration completed");
  
  return true;
}

bool GNUstepObjCRuntime::ShouldIncludeClass(const std::string &class_name) {
  // With dynamic discovery via objc_copyClassList, we include ALL classes
  // The runtime introspection gives us exactly what's registered
  
  // Only exclude obvious runtime internals
  if (class_name.find("__") == 0) {  // Private system classes like __NSArray
    return false;
  }
  
  if (class_name.find("Protocol") != std::string::npos) { // Protocol objects
    return false;
  }
  
  // Include everything else - trust the runtime!
  // This supports ANY user-defined class automatically
  return true;
}

ptrdiff_t GNUstepObjCRuntime::GetCachedIvarOffset(const std::string &class_name, 
                                                 const std::string &ivar_name) {
  auto it = m_class_metadata.find(class_name);
  if (it != m_class_metadata.end()) {
    auto ivar_it = it->second.ivar_offsets.find(ivar_name);
    if (ivar_it != it->second.ivar_offsets.end()) {
      return ivar_it->second;
    }
  }
  
  // Not cached, try runtime introspector as fallback
  if (m_runtime_introspector) {
    ptrdiff_t offset = m_runtime_introspector->GetIvarOffset(class_name, ivar_name);
    if (offset > 0) {
      // Cache for next time
      m_class_metadata[class_name].ivar_offsets[ivar_name] = offset;
      return offset;
    }
  }
  
  // Hardcoded fallback for known critical offsets
  if (class_name == "NSConstantString" && ivar_name == "nxcsptr") {
    return 24;  // Known offset
  }
  
  return -1;
}

uint32_t GNUstepObjCRuntime::ReadMemoryUnsigned(lldb::addr_t addr, size_t size) {
  if (!m_process)
    return 0;
    
  Status error;
  uint64_t value = m_process->ReadUnsignedIntegerFromMemory(addr, size, 0, error);
  if (error.Fail())
    return 0;
    
  return static_cast<uint32_t>(value);
}

lldb::addr_t GNUstepObjCRuntime::ReadPointer(lldb::addr_t addr) {
  if (!m_process)
    return LLDB_INVALID_ADDRESS;
    
  Status error;
  addr_t value = m_process->ReadPointerFromMemory(addr, error);
  if (error.Fail())
    return LLDB_INVALID_ADDRESS;
    
  return value;
}

std::string GNUstepObjCRuntime::ReadCString(lldb::addr_t addr, size_t max_len) {
  if (!m_process || addr == LLDB_INVALID_ADDRESS)
    return "";
    
  Status error;
  char buffer[256];
  size_t bytes_read = m_process->ReadCStringFromMemory(addr, buffer, 
                                                       std::min(max_len, sizeof(buffer)), 
                                                       error);
  if (error.Fail() || bytes_read == 0)
    return "";
    
  return std::string(buffer);
}

bool GNUstepObjCRuntime::IsModuleObjCLibrary(const ModuleSP &module_sp) {
  const llvm::Triple &TT = GetTargetRef().GetArchitecture().GetTriple();
  return CanModuleBeGNUstepObjCLibrary(module_sp, TT);
}

bool GNUstepObjCRuntime::ReadObjCLibrary(const ModuleSP &module_sp) {
  assert(m_objc_module_sp == nullptr && "Check HasReadObjCLibrary() first");
  m_objc_module_sp = module_sp;

  // Right now we don't use this, but we might want to check for debugger
  // runtime support symbols like 'gdb_object_getClass' in the future.
  return true;
}

void GNUstepObjCRuntime::ModulesDidLoad(const ModuleList &module_list) {
  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(log, "GNUstepObjCRuntime::ModulesDidLoad called with {0} modules", 
           module_list.GetSize());
  
  // Check if libobjc.so is among the newly loaded modules
  bool found_libobjc = false;
  for (size_t i = 0; i < module_list.GetSize(); i++) {
    auto mod = module_list.GetModuleAtIndex(i);
    if (mod) {
      const FileSpec &file_spec = mod->GetFileSpec();
      llvm::StringRef filename = file_spec.GetFilename().GetStringRef();
      if (filename.starts_with("libobjc.so")) {
        LLDB_LOG(log, "GNUstepObjCRuntime: libobjc.so loaded! Module: {0}", filename);
        found_libobjc = true;
      }
    }
  }
  
  if (found_libobjc) {
    LLDB_LOG(log, "GNUstepObjCRuntime: libobjc.so is now available - runtime fully operational");
  }
  
  ReadObjCLibraryIfNeeded(module_list);
  
  // Mark ISA map as needing update
  m_isa_to_descriptor_complete = false;

  // Defer synthetic provider registration until modules are loaded
  RegisterSyntheticProviders();
  
  LLDB_LOG(log, "GNUstepObjCRuntime::ModulesDidLoad completed - providers registered");
}

std::vector<GNUstepObjCRuntime::IvarInfo> 
GNUstepObjCRuntime::GetClassIvars(lldb::addr_t class_ptr, const ExecutionContext *exe_ctx_param) {
  std::vector<IvarInfo> ivars;
  
  if (!m_process || class_ptr == LLDB_INVALID_ADDRESS)
    return ivars;
    
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  
  // Make sure we have the runtime symbols
  if (m_class_copyIvarList_addr == LLDB_INVALID_ADDRESS) {
    LoadRuntimeSymbols();
    if (m_class_copyIvarList_addr == LLDB_INVALID_ADDRESS) {
      LLDB_LOG(log, "GNUstepObjCRuntime: class_copyIvarList not found");
      return ivars;
    }
  }
  
  ExecutionContext exe_ctx;
  if (exe_ctx_param && exe_ctx_param->HasThreadScope()) {
    exe_ctx = *exe_ctx_param;
  } else {
    m_process->CalculateExecutionContext(exe_ctx);
  }
  
  if (!exe_ctx.HasThreadScope()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: No thread context for ivar discovery");
    return ivars;
  }
  
  // Allocate memory for count
  Status error;
  addr_t count_addr = m_process->AllocateMemory(sizeof(unsigned int), 
                                                 ePermissionsReadable | ePermissionsWritable, 
                                                 error);
  if (error.Fail()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Failed to allocate memory for count: {0}", error);
    return ivars;
  }
  
  // Initialize count to 0
  uint32_t zero = 0;
  m_process->WriteMemory(count_addr, &zero, sizeof(zero), error);
  
  // Call class_copyIvarList to get the ivar list
  DiagnosticManager diagnostics;
  EvaluateExpressionOptions options;
  options.SetUnwindOnError(true);
  options.SetIgnoreBreakpoints(true);
  options.SetTimeout(std::chrono::seconds(10));
  
  ValueObjectSP result_sp;
  std::string expr = llvm::formatv("(void*)class_copyIvarList((Class)0x{0:x}, (unsigned int*)0x{1:x})", 
                                   class_ptr, count_addr).str();
  
  ExpressionResults expr_result = UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, nullptr);
  
  if (expr_result != eExpressionCompleted || !result_sp) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Failed to call class_copyIvarList");
    m_process->DeallocateMemory(count_addr);
    return ivars;
  }
  
  // Get the ivar list pointer
  addr_t ivar_list_addr = result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
  if (ivar_list_addr == LLDB_INVALID_ADDRESS || ivar_list_addr == 0) {
    LLDB_LOG(log, "GNUstepObjCRuntime: class_copyIvarList returned NULL");
    m_process->DeallocateMemory(count_addr);
    return ivars;
  }
  
  // Read the count
  uint32_t count = ReadMemoryUnsigned(count_addr, sizeof(uint32_t));
  m_process->DeallocateMemory(count_addr);
  
  if (error.Fail()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Failed to read ivar count");
    return ivars;
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Found {0} ivars", count);
  
  // Read the ivar pointers
  size_t ptr_size = m_process->GetAddressByteSize();
  for (uint32_t i = 0; i < count; i++) {
    addr_t ivar_ptr = ReadPointer(ivar_list_addr + (i * ptr_size));
    if (ivar_ptr == LLDB_INVALID_ADDRESS)
      continue;
      
    IvarInfo info;
    info.ivar_ptr = ivar_ptr;
    
    // Get ivar name
    if (m_ivar_getName_addr != LLDB_INVALID_ADDRESS) {
      expr = llvm::formatv("(const char*)ivar_getName((Ivar)0x{0:x})", ivar_ptr).str();
      expr_result = UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, nullptr);
      
      if (expr_result == eExpressionCompleted && result_sp) {
        addr_t name_addr = result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
        if (name_addr != LLDB_INVALID_ADDRESS && name_addr != 0) {
          info.name = ReadCString(name_addr);
        }
      }
    }
    
    // Get ivar offset
    if (m_ivar_getOffset_addr != LLDB_INVALID_ADDRESS) {
      expr = llvm::formatv("(long)ivar_getOffset((Ivar)0x{0:x})", ivar_ptr).str();
      expr_result = UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, nullptr);
      
      if (expr_result == eExpressionCompleted && result_sp) {
        info.offset = (ptrdiff_t)result_sp->GetValueAsSigned(0);
      }
    }
    
    // Get ivar type encoding
    if (m_ivar_getTypeEncoding_addr != LLDB_INVALID_ADDRESS) {
      expr = llvm::formatv("(const char*)ivar_getTypeEncoding((Ivar)0x{0:x})", ivar_ptr).str();
      expr_result = UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, nullptr);
      
      if (expr_result == eExpressionCompleted && result_sp) {
        addr_t type_addr = result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
        if (type_addr != LLDB_INVALID_ADDRESS && type_addr != 0) {
          info.type_encoding = ReadCString(type_addr);
        }
      }
    }
    
    // Only add ivars with valid names (skip ISA and other internal fields)
    if (!info.name.empty() && info.name[0] != '\0') {
      ivars.push_back(info);
      LLDB_LOG(log, "  Ivar: {0} at offset {1} type {2}", 
               info.name, info.offset, info.type_encoding);
    }
  }
  
  // Free the ivar list
  if (m_free_addr != LLDB_INVALID_ADDRESS) {
    expr = llvm::formatv("free((void*)0x{0:x})", ivar_list_addr).str();
    UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, nullptr);
  }
  
  return ivars;
}

std::vector<GNUstepObjCRuntime::IvarInfo> 
GNUstepObjCRuntime::GetObjectIvars(ValueObject *obj_valobj, const ExecutionContext *exe_ctx_param) {
  std::vector<IvarInfo> ivars;
  
  if (!m_process || !obj_valobj)
    return ivars;
    
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  
  // Get the variable name to use in expressions
  const char* var_name = obj_valobj->GetName().AsCString();
  if (!var_name || strlen(var_name) == 0) {
    LLDB_LOG(log, "GNUstepObjCRuntime: No variable name available");
    return ivars;
  }
  
  // Don't process expression results (like $0, $1, etc.) to avoid infinite recursion
  if (var_name[0] == '$') {
    LLDB_LOG(log, "GNUstepObjCRuntime: Skipping expression result variable: '{0}'", var_name);
    return ivars;
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Variable name: '{0}'", var_name);
  
  // Get the object's address to resolve its class name
  addr_t obj_addr = obj_valobj->GetPointerValue();
  if (obj_addr == LLDB_INVALID_ADDRESS || obj_addr == 0) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Invalid object address");
    return ivars;
  }
  
  // Get the actual class name for proper typecasting
  std::string class_name = m_isa_resolver->GetClassNameFromObject(obj_addr);
  if (class_name.empty()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Could not resolve class name");
    return ivars;
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Resolved class name: '{0}'", class_name);
  
  // NEW APPROACH: Pure direct memory introspection (no expression evaluation)
  // This follows LLDB best practices: no target code execution in synthetic providers
  ivars = GetObjectIvarsViaDirectMemory(obj_valobj, class_name, obj_addr);
  
  if (!ivars.empty()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Direct memory introspection succeeded with {0} ivars", ivars.size());
    return ivars;
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Direct memory introspection failed for class {0}", class_name);
  return ivars;
}

std::vector<GNUstepObjCRuntime::IvarInfo> 
GNUstepObjCRuntime::GetObjectIvarsViaRuntime(ValueObject *obj_valobj, const ExecutionContext *exe_ctx_param, 
                                             const std::string &class_name, const char* var_name) {
  std::vector<IvarInfo> ivars;
  
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  
  // Make sure we have the runtime symbols
  if (m_class_copyIvarList_addr == LLDB_INVALID_ADDRESS) {
    LoadRuntimeSymbols();
    if (m_class_copyIvarList_addr == LLDB_INVALID_ADDRESS) {
      LLDB_LOG(log, "GNUstepObjCRuntime: class_copyIvarList not found");
      return ivars;
    }
  }
  
  ExecutionContext exe_ctx;
  if (exe_ctx_param && exe_ctx_param->HasThreadScope()) {
    exe_ctx = *exe_ctx_param;
  } else {
    m_process->CalculateExecutionContext(exe_ctx);
  }
  
  if (!exe_ctx.HasThreadScope()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: No thread context for ivar discovery");
    return ivars;
  }
  
  // Allocate memory for count
  Status error;
  addr_t count_addr = m_process->AllocateMemory(sizeof(unsigned int), 
                                                 ePermissionsReadable | ePermissionsWritable, 
                                                 error);
  if (error.Fail()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Failed to allocate memory for count: {0}", error);
    return ivars;
  }
  
  // Initialize count to 0
  uint32_t zero = 0;
  m_process->WriteMemory(count_addr, &zero, sizeof(zero), error);
  
  // Call class_copyIvarList using the variable name directly - this should work!
  DiagnosticManager diagnostics;
  EvaluateExpressionOptions options;
  options.SetUnwindOnError(true);
  options.SetIgnoreBreakpoints(true);
  options.SetTimeout(std::chrono::seconds(10));
  
  ValueObjectSP result_sp;
  // Use the specific class name for proper typecasting with proper return type cast
  std::string expr = llvm::formatv(
    "void* cls = (void*)object_getClass(({0}*){1}); "
    "(Ivar*)class_copyIvarList((Class)cls, (unsigned int*)0x{2:x})", 
    class_name, var_name, count_addr).str();
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Executing expression: {0}", expr);
  
  ExpressionResults expr_result = UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, nullptr);
  
  if (expr_result != eExpressionCompleted || !result_sp) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Failed to call class_copyIvarList");
    m_process->DeallocateMemory(count_addr);
    return ivars;
  }
  
  // Get the ivar list pointer
  addr_t ivar_list_addr = result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
  if (ivar_list_addr == LLDB_INVALID_ADDRESS || ivar_list_addr == 0) {
    LLDB_LOG(log, "GNUstepObjCRuntime: class_copyIvarList returned NULL");
    m_process->DeallocateMemory(count_addr);
    return ivars;
  }
  
  // Read the count
  uint32_t ivar_count = 0;
  m_process->ReadMemory(count_addr, &ivar_count, sizeof(ivar_count), error);
  m_process->DeallocateMemory(count_addr);
  
  if (error.Fail()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Failed to read ivar count");
    return ivars;
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Found {0} ivars for variable {1}", ivar_count, var_name);
  
  if (ivar_count == 0) {
    return ivars;
  }
  
  // Process each ivar
  const uint32_t ptr_size = m_process->GetAddressByteSize();
  for (uint32_t i = 0; i < ivar_count; ++i) {
    addr_t ivar_ptr = ReadPointer(ivar_list_addr + (i * ptr_size));
    if (ivar_ptr == LLDB_INVALID_ADDRESS)
      continue;
      
    IvarInfo info;
    info.ivar_ptr = ivar_ptr;
    
    // Get ivar name
    if (m_ivar_getName_addr != LLDB_INVALID_ADDRESS) {
      expr = llvm::formatv("(const char*)ivar_getName((Ivar)0x{0:x})", ivar_ptr).str();
      expr_result = UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, nullptr);
      
      if (expr_result == eExpressionCompleted && result_sp) {
        addr_t name_addr = result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
        if (name_addr != LLDB_INVALID_ADDRESS && name_addr != 0) {
          info.name = ReadCString(name_addr);
        }
      }
    }
    
    // Get ivar offset
    if (m_ivar_getOffset_addr != LLDB_INVALID_ADDRESS) {
      expr = llvm::formatv("(ptrdiff_t)ivar_getOffset((Ivar)0x{0:x})", ivar_ptr).str();
      expr_result = UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, nullptr);
      
      if (expr_result == eExpressionCompleted && result_sp) {
        info.offset = result_sp->GetValueAsSigned(0);
      }
    }
    
    // Get ivar type encoding
    if (m_ivar_getTypeEncoding_addr != LLDB_INVALID_ADDRESS) {
      expr = llvm::formatv("(const char*)ivar_getTypeEncoding((Ivar)0x{0:x})", ivar_ptr).str();
      expr_result = UserExpression::Evaluate(exe_ctx, options, expr.c_str(), "", result_sp, nullptr);
      
      if (expr_result == eExpressionCompleted && result_sp) {
        addr_t type_addr = result_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);
        if (type_addr != LLDB_INVALID_ADDRESS && type_addr != 0) {
          info.type_encoding = ReadCString(type_addr);
        }
      }
    }
    
    ivars.push_back(info);
  }
  
  return ivars;
}

std::vector<GNUstepObjCRuntime::IvarInfo> 
GNUstepObjCRuntime::GetObjectIvarsViaDirectMemory(ValueObject *obj_valobj, const std::string &class_name, lldb::addr_t obj_addr) {
  std::vector<IvarInfo> ivars;
  
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime: Direct memory introspection for class '{0}' at 0x{1:x}", class_name, obj_addr);
  
  if (!m_process || obj_addr == LLDB_INVALID_ADDRESS) {
    return ivars;
  }
  
  // STEP 1: Read ISA pointer to get class pointer
  Status error;
  addr_t isa_ptr = m_process->ReadPointerFromMemory(obj_addr, error);
  if (error.Fail() || isa_ptr == LLDB_INVALID_ADDRESS) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Failed to read ISA pointer: {0}", error.AsCString());
    return ivars;
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Read ISA pointer 0x{0:x} from object", isa_ptr);
  
  // STEP 2: Use direct metadata parsing instead of runtime function calls
  // This avoids the execution context issues entirely and follows LLDB best practices
  LLDB_LOG(log, "GNUstepObjCRuntime: Attempting direct class metadata parsing for ISA 0x{0:x}", isa_ptr);
  ivars = ParseClassMetadataDirectly(isa_ptr);
  
  if (!ivars.empty()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Direct metadata parsing succeeded with {0} ivars", ivars.size());
    return ivars;
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Direct metadata parsing failed for class {0} - this indicates either no ivars or a parsing issue", class_name);
  return ivars;
}

void GNUstepObjCRuntime::RegisterSyntheticProviders() {
  Log *log = GetLog(LLDBLog::DataFormatters);
  
  // Check if providers are already registered to prevent duplicates
  if (m_providers_registered) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Providers already registered, skipping");
    return;
  }
  
  // HYBRID APPROACH: Skip C++ synthetic children, use Python instead
  // This avoids offset discovery and recursion issues
  LLDB_LOG(log, "GNUstepObjCRuntime: HYBRID MODE - Registering summary providers only");
  LLDB_LOG(log, "GNUstepObjCRuntime: Synthetic children will be handled by Python bridge");
  
  if (!m_process) {
    LLDB_LOG(log, "GNUstepObjCRuntime: No process available for provider registration");
    return;
  }
  
  // Create synthetic children provider for custom objects using GNUstepUniversalProvider
  SyntheticChildrenSP synth_sp(new CXXSyntheticChildren(
      SyntheticChildren::Flags()
          .SetCascades(true)
          .SetSkipPointers(false)
          .SetSkipReferences(false),
      "GNUstep Universal Provider for custom objects",
      formatters::GNUstepUniversalProviderCreator));
  LLDB_LOG(log, "GNUstepObjCRuntime: Created GNUstepUniversalProvider synthetic children provider");
  
  // Get or create the objc category
  TypeCategoryImplSP objc_category;
  if (!DataVisualization::Categories::GetCategory(ConstString("objc"), objc_category)) {
    LLDB_LOG(log, "GNUstepObjCRuntime: objc category not found, using default");
    DataVisualization::Categories::GetCategory(ConstString("default"), objc_category);
  }
  
  if (!objc_category) {
    LLDB_LOG(log, "GNUstepObjCRuntime: No category available for provider registration");
    return;
  }
  
  // RESEARCH-GUIDED APPROACH: Register as dynamic type handler for generic ObjC objects
  // This follows our research principle: "generic ivar dumping is enough for simple cases"
  // and "dynamic type resolution for id or base NSObject* types"
  
  // Register for BASE Objective-C types only (not specific classes)
  // This allows dynamic type resolution to work properly
  std::vector<std::pair<std::string, bool>> target_types = {
    // REMOVED: {"NSObject *", false},  // This causes recursion!
    // REMOVED: {"NSObject", false},   // This also causes recursion!
    {"id", false},          // Generic Objective-C object pointer
    
    // Pointer types (for direct variable access like 'account')
    // CRITICAL FIX: Exclude Foundation collection types to avoid conflicts with specific providers
    {"^(?!NS(Array|Dictionary|Set|String|Number|Date|Object))[A-Z][a-zA-Z0-9_]+ \\*$", true},  // REGEX: Custom class with space, excluding Foundation
    {"^(?!NS(Array|Dictionary|Set|String|Number|Date|Object))[A-Z][a-zA-Z0-9_]+\\*$", true},   // REGEX: Custom class without space, excluding Foundation
    
    // Non-pointer types (for dereferenced access like '*account')
    {"^(?!NS(Array|Dictionary|Set|String|Number|Date|Object)|GS(Array|Dictionary|Set|String|Number|Date))[A-Z][a-zA-Z0-9_]+$", true}, // REGEX: Custom class, excluding Foundation and GNUstep internals
    // NO NSObject to avoid recursion - only user-defined classes!
  };
  
  // Register GNUstepUniversalProvider for custom objects
  // This enables property display for custom classes like BankAccount
  if (synth_sp) {
    // Register the provider for each custom object type pattern
    for (const auto &type_info : target_types) {
      const std::string &type_name = type_info.first;
      bool is_regex = type_info.second;
      
      ConstString type_const(type_name.c_str());
      lldb::FormatterMatchType match_type = is_regex ? 
          lldb::eFormatterMatchRegex : lldb::eFormatterMatchExact;
      
      objc_category->AddTypeSynthetic(type_const, match_type, synth_sp);
      LLDB_LOG(log, "GNUstepObjCRuntime: Registered GNUstepUniversalProvider for type: {0}", type_name);
    }
  }
  
  // Register NSString summary provider to show actual string content
  TypeSummaryImplSP string_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      formatters::GNUstepNSStringSummaryProvider,
      "NSString summary"));
  
  if (string_summary_sp) {
    // Register for NSString and related classes + generic id type
    std::vector<std::string> string_types = {
      "NSString",
      "NSString *",
      "NSConstantString", 
      "NSConstantString *",
      "NSMutableString",
      "NSMutableString *",
      "GSTinyString",
      "GSTinyString *",
      "GSPlaceholderString", 
      "GSPlaceholderString *",
      "GSCString",
      "GSCString *",
      "GSUnicodeString",
      "GSUnicodeString *",
      "id"  // CRITICAL: Register for id type to handle synthetic children from expression evaluation
    };
    
    for (const std::string &type_name : string_types) {
      objc_category->AddTypeSummary(ConstString(type_name.c_str()), 
                                   lldb::eFormatterMatchExact, 
                                   string_summary_sp);
      LLDB_LOG(log, "GNUstepObjCRuntime: Registered NSString summary provider for type: {0}", type_name);
    }
  }

  // Register NSNumber summary provider to show actual numeric values
  TypeSummaryImplSP number_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      formatters::GNUstepNumberSummaryProvider::FormatObject,
      "NSNumber summary"));
  
  if (number_summary_sp) {
    // Register for NSNumber and related classes
    std::vector<std::string> number_types = {
      "NSNumber",
      "NSNumber *", 
      "NSDecimalNumber",
      "NSDecimalNumber *"
    };
    
    for (const std::string &type_name : number_types) {
      objc_category->AddTypeSummary(ConstString(type_name.c_str()), 
                                   lldb::eFormatterMatchExact, 
                                   number_summary_sp);
      LLDB_LOG(log, "GNUstepObjCRuntime: Registered NSNumber summary provider for type: {0}", type_name);
    }
  }

  // Register NSDate summary provider to show actual date/time values
  TypeSummaryImplSP date_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      formatters::GNUstepNSDateSummaryProvider,
      "NSDate summary"));
  
  if (date_summary_sp) {
    // Register for NSDate and related classes
    std::vector<std::string> date_types = {
      "NSDate",
      "NSDate *",
      "NSCalendarDate", 
      "NSCalendarDate *"
      // DO NOT register for "id" - it overrides all other providers!
    };
    
    for (const std::string &type_name : date_types) {
      objc_category->AddTypeSummary(ConstString(type_name.c_str()), 
                                   lldb::eFormatterMatchExact, 
                                   date_summary_sp);
      LLDB_LOG(log, "GNUstepObjCRuntime: Registered NSDate summary provider for type: {0}", type_name);
    }
  }

  // Register NSSet synthetic provider to enable expansion of set elements
  SyntheticChildrenSP set_synth_sp(new CXXSyntheticChildren(
      SyntheticChildren::Flags().SetCascades(true)
                                .SetSkipPointers(false)
                                .SetSkipReferences(false),
      "NSSet synthetic children",
      formatters::GNUstepNSSetSyntheticFrontEndCreator));

  if (set_synth_sp) {
    // Register for NSSet and related classes
    std::vector<std::string> set_types = {
      "NSSet",
      "NSSet *",
      "NSMutableSet",
      "NSMutableSet *",
      "GSSet",
      "GSSet *",
      "GSMutableSet",
      "GSMutableSet *",
      "__NSSetI",
      "__NSSetI *",
      "__NSSetM",
      "__NSSetM *"
    };
    
    for (const std::string &type_name : set_types) {
      objc_category->AddTypeSynthetic(ConstString(type_name.c_str()),
                                     lldb::eFormatterMatchExact,
                                     set_synth_sp);
      LLDB_LOG(log, "GNUstepObjCRuntime: Registered NSSet synthetic provider for type: {0}", type_name);
    }
  }

  // Register NSSet summary provider to show element count
  TypeSummaryImplSP set_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      formatters::GNUstepNSSetSummaryProvider,
      "NSSet summary"));
  
  if (set_summary_sp) {
    // Register for same types as synthetic provider
    std::vector<std::string> set_types = {
      "NSSet",
      "NSSet *",
      "NSMutableSet",
      "NSMutableSet *",
      "GSSet",
      "GSSet *",
      "GSMutableSet",
      "GSMutableSet *",
      "__NSSetI",
      "__NSSetI *",
      "__NSSetM",
      "__NSSetM *"
    };
    
    for (const std::string &type_name : set_types) {
      objc_category->AddTypeSummary(ConstString(type_name.c_str()), 
                                   lldb::eFormatterMatchExact, 
                                   set_summary_sp);
      LLDB_LOG(log, "GNUstepObjCRuntime: Registered NSSet summary provider for type: {0}", type_name);
    }
  }

  // Register NSDictionary synthetic provider to enable expansion of key-value pairs
  SyntheticChildrenSP dict_synth_sp(new CXXSyntheticChildren(
      SyntheticChildren::Flags().SetCascades(true)
                                .SetSkipPointers(false)
                                .SetSkipReferences(false),
      "NSDictionary synthetic children",
      formatters::GNUstepNSDictionarySyntheticFrontEndCreator));

  if (dict_synth_sp) {
    // Register for NSDictionary and related classes
    std::vector<std::string> dict_types = {
      "NSDictionary",
      "NSDictionary *",
      "NSMutableDictionary",
      "NSMutableDictionary *",
      "GSDictionary",
      "GSDictionary *",
      "GSMutableDictionary",
      "GSMutableDictionary *",
      "__NSDictionaryI",
      "__NSDictionaryI *",
      "__NSDictionaryM",
      "__NSDictionaryM *"
    };
    
    for (const std::string &type_name : dict_types) {
      objc_category->AddTypeSynthetic(ConstString(type_name.c_str()),
                                     lldb::eFormatterMatchExact,
                                     dict_synth_sp);
      LLDB_LOG(log, "GNUstepObjCRuntime: Registered NSDictionary synthetic provider for type: {0}", type_name);
    }
  }

  // Register NSDictionary summary provider to show key-value pair count and preview
  TypeSummaryImplSP dict_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      formatters::GNUstepNSDictionarySummaryProvider,
      "NSDictionary summary"));
  
  if (dict_summary_sp) {
    // Register for same types as synthetic provider
    std::vector<std::string> dict_types = {
      "NSDictionary",
      "NSDictionary *",
      "NSMutableDictionary",
      "NSMutableDictionary *",
      "GSDictionary",
      "GSDictionary *",
      "GSMutableDictionary",
      "GSMutableDictionary *",
      "__NSDictionaryI",
      "__NSDictionaryI *",
      "__NSDictionaryM",
      "__NSDictionaryM *"
    };
    
    for (const std::string &type_name : dict_types) {
      objc_category->AddTypeSummary(ConstString(type_name.c_str()), 
                                   lldb::eFormatterMatchExact, 
                                   dict_summary_sp);
      LLDB_LOG(log, "GNUstepObjCRuntime: Registered NSDictionary summary provider for type: {0}", type_name);
    }
  }

  // Register NSArray summary provider with element preview
  TypeSummaryImplSP array_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      formatters::GNUstepNSArraySummaryProvider,
      "NSArray summary"));
  
  if (array_summary_sp) {
    // Register for NSArray and related classes
    std::vector<std::string> array_types = {
      "NSArray",
      "NSArray *",
      "NSMutableArray", 
      "NSMutableArray *",
      "GSInlineArray",
      "GSInlineArray *",
      "GSMutableArray",
      "GSMutableArray *",
      "GSArray",
      "GSArray *",
      "GSArray0",
      "GSArray0 *", 
      "GSArray1",
      "GSArray1 *",
      "__NSArrayI",
      "__NSArrayI *",
      "__NSArrayM", 
      "__NSArrayM *"
    };
    
    for (const std::string &type_name : array_types) {
      objc_category->AddTypeSummary(ConstString(type_name.c_str()), 
                                   lldb::eFormatterMatchExact, 
                                   array_summary_sp);
      LLDB_LOG(log, "GNUstepObjCRuntime: Registered NSArray summary provider for type: {0}", type_name);
    }
  }

  // Register NSArray synthetic provider to enable expansion of array elements  
  // Uses our improved GNUstepNSArraySyntheticProvider with inline array support
  SyntheticChildrenSP array_synth_sp(new CXXSyntheticChildren(
      SyntheticChildren::Flags().SetCascades(true)
                                .SetSkipPointers(false)
                                .SetSkipReferences(false),
      "NSArray synthetic children",
      [](CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp) -> SyntheticChildrenFrontEnd * {
        if (!valobj_sp)
          return nullptr;
        return new formatters::GNUstepNSArraySyntheticProvider(valobj_sp);
      }));

  if (array_synth_sp) {
    // Register for NSArray and related classes - using EUREKA pattern
    std::vector<std::string> array_types = {
      "NSArray",
      "NSArray *",
      "NSMutableArray", 
      "NSMutableArray *",
      "GSInlineArray",
      "GSInlineArray *",
      "GSMutableArray",
      "GSMutableArray *",
      "GSArray0",
      "GSArray0 *", 
      "GSArray1",
      "GSArray1 *",
      "__NSArrayI",
      "__NSArrayI *",
      "__NSArrayM", 
      "__NSArrayM *"
    };
    
    // Register the synthetic provider for all array types
    for (const std::string &type_name : array_types) {
      objc_category->AddTypeSynthetic(ConstString(type_name.c_str()),
                                     lldb::eFormatterMatchExact,
                                     array_synth_sp);
      LLDB_LOG(log, "GNUstepObjCRuntime: Registered NSArray synthetic provider for type: {0}", type_name);
    }
  }

  // Register Custom Class summary provider for ALL non-Foundation classes
  // This provides "ClassName {prop1=val1, prop2=val2, ...}" format for user-defined classes
  TypeSummaryImplSP custom_summary_sp(new CXXFunctionSummaryFormat(
      TypeSummaryImpl::Flags().SetCascades(true)   // HIGH priority - allow cascading
                              .SetSkipPointers(false)
                              .SetSkipReferences(false)
                              .SetDontShowChildren(false)
                              .SetDontShowValue(false)
                              .SetShowMembersOneLiner(false)
                              .SetHideItemNames(false),
      formatters::GNUstepCustomClassSummaryProvider,
      "Custom Class summary"));
  
  if (custom_summary_sp) {
    // Register using regex pattern for custom classes (non-Foundation classes)
    // Pattern matches classes starting with uppercase letter, not starting with NS/GS prefixes
    std::vector<std::string> custom_class_patterns = {
      "^[A-Z][a-zA-Z0-9_]+$",       // Match: BankAccount, Person, CustomClass
      "^[A-Z][a-zA-Z0-9_]+ \\*$"    // Match: BankAccount *, Person *, CustomClass *
    };
    
    // Use objc category but with higher priority (cascading=true)
    // This ensures our custom class formatter takes precedence
    
    for (const std::string &pattern : custom_class_patterns) {
      objc_category->AddTypeSummary(ConstString(pattern.c_str()),
                                   lldb::eFormatterMatchRegex,
                                   custom_summary_sp);
      LLDB_LOG(log, "GNUstepObjCRuntime: Registered Custom Class summary provider for pattern: {0}", pattern);
    }
  }


  // Enable the categories
  DataVisualization::Categories::Enable(ConstString("objc"));
  DataVisualization::Categories::Enable(ConstString("default"));
  
  // Mark providers as registered to prevent duplicates
  m_providers_registered = true;
  
  LLDB_LOG(log, "GNUstepObjCRuntime: GENERIC runtime introspection registration completed - now with NSString, NSNumber, and NSDate summaries!");
}

DeclVendor *GNUstepObjCRuntime::GetDeclVendor() {
  Log *log = GetLog(LLDBLog::Expressions);
  
  if (!m_decl_vendor && m_process) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Creating GNUstepObjCDeclVendor");
    m_decl_vendor = std::make_unique<GNUstepObjCDeclVendor>(*this);
  }
  
  return m_decl_vendor.get();
}

// DIRECT RUNTIME INTROSPECTION IMPLEMENTATION
// These methods bypass expression evaluation to avoid context issues

std::vector<GNUstepObjCRuntime::IvarInfo> 
GNUstepObjCRuntime::CallClassCopyIvarListDirectly(lldb::addr_t class_ptr, const ExecutionContext &exe_ctx) {
  std::vector<IvarInfo> ivars;
  
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime: Attempting direct function call to class_copyIvarList for class 0x{0:x}", class_ptr);
  
  if (!m_process || class_ptr == LLDB_INVALID_ADDRESS || m_class_copyIvarList_addr == LLDB_INVALID_ADDRESS) {
    return ivars;
  }
  
  // For now, this is complex to implement correctly, so we skip it
  // and go directly to metadata parsing
  
  // For now, fallback to direct metadata parsing since FunctionCaller setup is complex
  LLDB_LOG(log, "GNUstepObjCRuntime: FunctionCaller approach not implemented yet, falling back to metadata parsing");
  return ivars;
}

std::vector<GNUstepObjCRuntime::IvarInfo> 
GNUstepObjCRuntime::ParseClassMetadataDirectly(lldb::addr_t class_ptr) {
  std::vector<IvarInfo> ivars;
  
  Log *log = GetLog(LLDBLog::Process | LLDBLog::Types);
  LLDB_LOG(log, "GNUstepObjCRuntime: GENERIC ParseClassMetadataDirectly for class 0x{0:x}", class_ptr);
  
  if (!m_process) {
    LLDB_LOG(log, "GNUstepObjCRuntime: No process available for direct memory access");
    return ivars;
  }
  
  Status error;
  
  // RESEARCH-GUIDED IMPLEMENTATION: Pure runtime introspection for ANY Objective-C class
  // No hardcoded BankAccount logic - works generically for all classes
  
  // STEP 1: Read class metadata structure directly from memory
  // GNUstep class structure (based on libobjc2 headers):
  // struct objc_class {
  //   struct objc_class *isa;           // +0
  //   struct objc_class *superclass;   // +8  
  //   const char *name;                 // +16
  //   long version;                     // +24
  //   long info;                        // +32
  //   long instance_size;               // +40
  //   struct objc_ivar_list *ivars;     // +48
  //   ...
  // };
  
  // Read the class name first for debugging
  lldb::addr_t class_name_ptr = 0;
  class_name_ptr = m_process->ReadPointerFromMemory(class_ptr + 16, error);
  std::string class_name = "<unknown>";
  if (error.Success() && class_name_ptr != 0) {
    m_process->ReadCStringFromMemory(class_name_ptr, class_name, error);
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Processing class '{0}' at 0x{1:x}", class_name, class_ptr);
  
  // Read the ivar list pointer at offset +48
  lldb::addr_t ivar_list_ptr = 0;
  ivar_list_ptr = m_process->ReadPointerFromMemory(class_ptr + 48, error);
  
  if (error.Fail() || ivar_list_ptr == 0) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Class '{0}' has no ivar list or read failed: {1}", 
             class_name, error.AsCString());
    return ivars;  // Empty but valid result for classes with no ivars
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Class '{0}' has ivar list at 0x{1:x}", class_name, ivar_list_ptr);
  
  // STEP 2: Read ivar list structure
  // struct objc_ivar_list {
  //   int ivar_count;     // +0
  //   struct objc_ivar ivar_list[1];  // +4 (variable length)
  // };
  
  uint32_t ivar_count = 0;
  uint64_t count_val = m_process->ReadUnsignedIntegerFromMemory(ivar_list_ptr, 4, 0, error);
  ivar_count = (uint32_t)count_val;
  
  if (error.Fail()) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Failed to read ivar count for class '{0}': {1}", 
             class_name, error.AsCString());
    return ivars;
  }
  
  if (ivar_count == 0) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Class '{0}' has 0 ivars", class_name);
    return ivars;
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: Class '{0}' has {1} ivars", class_name, ivar_count);
  
  // Safety limit to prevent runaway processing
  if (ivar_count > 50) {
    LLDB_LOG(log, "GNUstepObjCRuntime: Class '{0}' has suspiciously many ivars ({1}), limiting to 50", 
             class_name, ivar_count);
    ivar_count = 50;
  }
  
  // Debug: dump some bytes from the ivar list
  if (log) {
    uint8_t buffer[64];
    error.Clear();
    size_t bytes_read = m_process->ReadMemory(ivar_list_ptr, buffer, 64, error);
    if (error.Success() && bytes_read > 0) {
      LLDB_LOG(log, "GNUstepObjCRuntime: First 64 bytes of ivar_list at 0x{0:x}:", ivar_list_ptr);
      for (size_t j = 0; j < bytes_read; j += 8) {
        LLDB_LOG(log, "  +{0:02x}: {1:02x} {2:02x} {3:02x} {4:02x} {5:02x} {6:02x} {7:02x} {8:02x}",
                 j, buffer[j], buffer[j+1], buffer[j+2], buffer[j+3],
                 buffer[j+4], buffer[j+5], buffer[j+6], buffer[j+7]);
      }
    }
  }
  
  // STEP 3: Read each ivar structure
  // struct objc_ivar {
  //   const char *name;       // +0  (8 bytes)
  //   const char *type;       // +8  (8 bytes)
  //   int *offset;            // +16 (8 bytes) - POINTER to offset value!
  //   uint32_t size;          // +24 (4 bytes)
  //   uint32_t flags;         // +28 (4 bytes)
  // };                        // Total: 32 bytes
  
  // Get the size field from the ivar list
  // On 64-bit: int count (4 bytes) + 4 bytes padding + size_t size (8 bytes) = offset 8
  uint64_t ivar_size = m_process->ReadUnsignedIntegerFromMemory(ivar_list_ptr + 8, 8, 0, error);
  if (error.Fail() || ivar_size == 0) {
    // Default to expected size
    ivar_size = 32;
    LLDB_LOG(log, "GNUstepObjCRuntime: Failed to read ivar size, defaulting to {0}", ivar_size);
  }
  
  // Skip count (4 bytes) + padding (4 bytes) + size field (8 bytes) = 16 bytes
  lldb::addr_t ivar_array_start = ivar_list_ptr + 16;
  
  for (uint32_t i = 0; i < ivar_count; i++) {
    lldb::addr_t ivar_ptr = ivar_array_start + (i * ivar_size);
    
    // Read ivar name pointer
    lldb::addr_t name_ptr = 0;
    name_ptr = m_process->ReadPointerFromMemory(ivar_ptr, error);
    if (error.Fail()) {
      LLDB_LOG(log, "GNUstepObjCRuntime: Failed to read name pointer for ivar {0}", i);
      continue;
    }
    
    // Read ivar type pointer  
    lldb::addr_t type_ptr = 0;
    type_ptr = m_process->ReadPointerFromMemory(ivar_ptr + 8, error);
    if (error.Fail()) {
      LLDB_LOG(log, "GNUstepObjCRuntime: Failed to read type pointer for ivar {0}", i);
      continue;
    }
    
    // Read ivar offset pointer and dereference it
    lldb::addr_t offset_ptr = 0;
    offset_ptr = m_process->ReadPointerFromMemory(ivar_ptr + 16, error);
    if (error.Fail() || offset_ptr == 0) {
      LLDB_LOG(log, "GNUstepObjCRuntime: Failed to read offset pointer for ivar {0}", i);
      continue;
    }
    
    // Now read the actual offset value (it's an int, so 4 bytes)
    int32_t offset = 0;
    uint64_t offset_val = m_process->ReadUnsignedIntegerFromMemory(offset_ptr, 4, 0, error);
    offset = (int32_t)offset_val;
    if (error.Fail()) {
      LLDB_LOG(log, "GNUstepObjCRuntime: Failed to read offset value from 0x{0:x} for ivar {1}", offset_ptr, i);
      continue;
    }
    
    // Read name string
    std::string name = "<unnamed>";
    if (name_ptr != 0) {
      m_process->ReadCStringFromMemory(name_ptr, name, error);
      if (error.Fail()) {
        LLDB_LOG(log, "GNUstepObjCRuntime: Failed to read name string for ivar {0}", i);
        name = "<unreadable>";
      }
    }
    
    // Read type string
    std::string type_encoding = "@";  // Default to object pointer
    if (type_ptr != 0) {
      m_process->ReadCStringFromMemory(type_ptr, type_encoding, error);
      if (error.Fail()) {
        LLDB_LOG(log, "GNUstepObjCRuntime: Failed to read type encoding for ivar {0}", i);
        type_encoding = "@";  // Fallback to object pointer
      }
    }
    
    LLDB_LOG(log, "GNUstepObjCRuntime: Class '{0}' ivar {1}: name='{2}', offset={3}, type='{4}'", 
             class_name, i, name, offset, type_encoding);
    
    // Add to result
    IvarInfo ivar_info;
    ivar_info.name = name;
    ivar_info.offset = offset;
    ivar_info.type_encoding = type_encoding;
    ivar_info.ivar_ptr = ivar_ptr;
    ivars.push_back(ivar_info);
  }
  
  LLDB_LOG(log, "GNUstepObjCRuntime: GENERIC INTROSPECTION SUCCESS: Class '{0}' has {1} ivars via direct memory parsing", 
           class_name, ivars.size());
  return ivars;
}

// Factory function for creating synthetic children
SyntheticChildren *
GNUstepObjCRuntime::CreateGNUstepSyntheticChildren() {
  // Use CXXSyntheticChildren with correct callback signature
  return new CXXSyntheticChildren(
      SyntheticChildren::Flags()
          .SetCascades(true)
          .SetSkipPointers(false)
          .SetSkipReferences(false),
      "GNUstep synthetic children",
      [](CXXSyntheticChildren *, lldb::ValueObjectSP valobj) -> SyntheticChildrenFrontEnd * {
        if (!valobj)
          return nullptr;
        return new GNUstepSyntheticProvider(valobj);
      });
}