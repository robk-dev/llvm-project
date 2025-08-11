//===-- GNUstepRuntimeV2API.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepRuntimeV2API.h"

#include "lldb/Core/Module.h"
#include "lldb/Core/Section.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolFile.h"
#include "lldb/Symbol/Symtab.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Target/ABI.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Platform.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/StreamString.h"
#include "lldb/ValueObject/ValueObject.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <dlfcn.h>
#include <memory>
#include <string>
#include <vector>

#define LLDB_LOG_TAG "gnustep-runtime-v2"

using namespace lldb;
using namespace lldb_private;

namespace {

// Helper to create error messages
llvm::Error CreateError(const char *format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  return llvm::createStringError(llvm::inconvertibleErrorCode(), buffer);
}

} // anonymous namespace

GNUstepRuntimeV2API::GNUstepRuntimeV2API(Process *process)
    : m_process(process), m_valid(false) {
  if (!m_process) {
    return;
  }
  
  m_valid = InitializeRuntimeFunctions();
  
  if (m_valid) {
    Log *log = GetLog(LLDBLog::Language);
    LLDB_LOG(log, "[{0}] GNUstepRuntimeV2API initialized successfully", 
             LLDB_LOG_TAG);
  }
}

llvm::Expected<std::unique_ptr<GNUstepRuntimeV2API>>
GNUstepRuntimeV2API::Create(Process *process) {
  if (!process) {
    return CreateError("Invalid process");
  }
  
  auto api = std::unique_ptr<GNUstepRuntimeV2API>(
      new GNUstepRuntimeV2API(process));
  
  if (!api->IsValid()) {
    return CreateError("Failed to initialize runtime functions");
  }
  
  // Don't pre-cache Foundation classes - they may not be loaded yet
  // We'll register them on-demand when they're actually used
  LLDB_LOG(GetLog(LLDBLog::Language), "GNUstepRuntimeV2API created successfully - Foundation classes will be registered on-demand");
  
  return std::move(api);
}

bool GNUstepRuntimeV2API::InitializeRuntimeFunctions() {
  Log *log = GetLog(LLDBLog::Language);
  
  // Find libobjc2 module
  const ModuleList &modules = m_process->GetTarget().GetImages();
  for (size_t i = 0; i < modules.GetSize(); ++i) {
    ModuleSP module_sp = modules.GetModuleAtIndex(i);
    if (!module_sp)
      continue;
    
    const FileSpec &file_spec = module_sp->GetFileSpec();
    llvm::StringRef filename = file_spec.GetFilename().GetStringRef();
    
    if (filename.contains("libobjc.so") || filename.contains("libobjc2")) {
      m_objc_module = module_sp;
      LLDB_LOG(log, "[{0}] Found libobjc2 module: {1}", 
               LLDB_LOG_TAG, file_spec.GetPath());
    } else if (filename.contains("libgnustep-base")) {
      m_foundation_module = module_sp;
      LLDB_LOG(log, "[{0}] Found Foundation module: {1}",
               LLDB_LOG_TAG, file_spec.GetPath());
    }
  }
  
  if (!m_objc_module) {
    LLDB_LOG(log, "[{0}] libobjc2 module not found", LLDB_LOG_TAG);
    return false;
  }
  
  // Resolve runtime function pointers
  m_runtime.objc_getClass = (Class (*)(const char *))
      ResolveRuntimeSymbol("objc_getClass");
  m_runtime.objc_lookUpClass = (Class (*)(const char *))
      ResolveRuntimeSymbol("objc_lookUpClass");
  m_runtime.objc_copyClassList = (Class *(*)(unsigned int *))
      ResolveRuntimeSymbol("objc_copyClassList");
  
  m_runtime.class_getName = (const char *(*)(Class))
      ResolveRuntimeSymbol("class_getName");
  m_runtime.class_getSuperclass = (Class (*)(Class))
      ResolveRuntimeSymbol("class_getSuperclass");
  m_runtime.class_getInstanceSize = (size_t (*)(Class))
      ResolveRuntimeSymbol("class_getInstanceSize");
  m_runtime.class_isMetaClass = (bool (*)(Class))
      ResolveRuntimeSymbol("class_isMetaClass");
  
  m_runtime.class_copyIvarList = (Ivar *(*)(Class, unsigned int *))
      ResolveRuntimeSymbol("class_copyIvarList");
  m_runtime.ivar_getName = (const char *(*)(Ivar))
      ResolveRuntimeSymbol("ivar_getName");
  m_runtime.ivar_getTypeEncoding = (const char *(*)(Ivar))
      ResolveRuntimeSymbol("ivar_getTypeEncoding");
  m_runtime.ivar_getOffset = (ptrdiff_t (*)(Ivar))
      ResolveRuntimeSymbol("ivar_getOffset");
  
  m_runtime.class_copyMethodList = (Method *(*)(Class, unsigned int *))
      ResolveRuntimeSymbol("class_copyMethodList");
  m_runtime.method_getName = (SEL (*)(Method))
      ResolveRuntimeSymbol("method_getName");
  m_runtime.method_getTypeEncoding = (const char *(*)(Method))
      ResolveRuntimeSymbol("method_getTypeEncoding");
  m_runtime.method_getImplementation = (void *(*)(Method))
      ResolveRuntimeSymbol("method_getImplementation");
  m_runtime.sel_getName = (const char *(*)(SEL))
      ResolveRuntimeSymbol("sel_getName");
  
  m_runtime.class_copyPropertyList = (Property *(*)(Class, unsigned int *))
      ResolveRuntimeSymbol("class_copyPropertyList");
  m_runtime.property_getName = (const char *(*)(Property))
      ResolveRuntimeSymbol("property_getName");
  m_runtime.property_getAttributes = (const char *(*)(Property))
      ResolveRuntimeSymbol("property_getAttributes");
  
  m_runtime.object_getClass = (Class (*)(void *))
      ResolveRuntimeSymbol("object_getClass");
  m_runtime.object_getClassName = (const char *(*)(void *))
      ResolveRuntimeSymbol("object_getClassName");
  
  m_runtime.free = (void (*)(void *))
      ResolveRuntimeSymbol("free");
  
  // Check critical functions are resolved
  bool success = m_runtime.objc_copyClassList && 
                 m_runtime.class_getName &&
                 m_runtime.class_getSuperclass &&
                 m_runtime.class_copyIvarList;
  
  if (success) {
    LLDB_LOG(log, "[{0}] All critical runtime functions resolved", 
             LLDB_LOG_TAG);
  } else {
    LLDB_LOG(log, "[{0}] Failed to resolve critical runtime functions", 
             LLDB_LOG_TAG);
  }
  
  return success;
}

lldb::addr_t GNUstepRuntimeV2API::ResolveRuntimeSymbol(const char *name) {
  if (!m_objc_module || !name)
    return LLDB_INVALID_ADDRESS;
  
  ConstString symbol_name(name);
  const Symbol *symbol = m_objc_module->FindFirstSymbolWithNameAndType(
      symbol_name, eSymbolTypeCode);
  
  if (!symbol) {
    // Try in Foundation module as fallback
    if (m_foundation_module) {
      symbol = m_foundation_module->FindFirstSymbolWithNameAndType(
          symbol_name, eSymbolTypeCode);
    }
  }
  
  if (symbol) {
    addr_t addr = symbol->GetAddress().GetLoadAddress(&m_process->GetTarget());
    if (addr != LLDB_INVALID_ADDRESS) {
      Log *log = GetLog(LLDBLog::Language);
      LLDB_LOG(log, "[{0}] Resolved {1} to 0x{2:x}", 
               LLDB_LOG_TAG, name, addr);
      return addr;
    }
  }
  
  return LLDB_INVALID_ADDRESS;
}

llvm::Expected<std::string> 
GNUstepRuntimeV2API::ReadCStringFromTarget(lldb::addr_t addr) {
  if (addr == 0 || addr == LLDB_INVALID_ADDRESS) {
    return CreateError("Invalid address");
  }
  
  char buffer[1024];
  Status error;
  m_process->ReadCStringFromMemory(addr, buffer, sizeof(buffer), error);
  
  if (error.Fail()) {
    return CreateError("Failed to read string: %s", error.AsCString());
  }
  
  return std::string(buffer);
}

llvm::Expected<std::vector<uint8_t>>
GNUstepRuntimeV2API::ReadMemory(lldb::addr_t addr, size_t size) {
  if (addr == 0 || addr == LLDB_INVALID_ADDRESS) {
    return CreateError("Invalid address");
  }
  
  std::vector<uint8_t> buffer(size);
  Status error;
  size_t bytes_read = m_process->ReadMemory(addr, buffer.data(), size, error);
  
  if (error.Fail() || bytes_read != size) {
    return CreateError("Failed to read memory: %s", error.AsCString());
  }
  
  return buffer;
}

// === Class Enumeration Implementation ===

llvm::Expected<std::vector<GNUstepRuntimeV2API::Class>>
GNUstepRuntimeV2API::GetAllClasses() {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  
  if (!m_runtime.objc_copyClassList) {
    return CreateError("objc_copyClassList not available");
  }
  
  // Call objc_copyClassList in target process
  ExecutionContext exe_ctx(m_process);
  ThreadSP thread_sp = exe_ctx.GetThreadSP();
  if (!thread_sp) {
    return CreateError("No thread available for function call");
  }
  
  // Prepare function call
  EvaluateExpressionOptions options;
  options.SetUnwindOnError(true);
  options.SetIgnoreBreakpoints(true);
  options.SetTryAllThreads(false);
  options.SetTimeout(std::chrono::seconds(5));
  
  // Execute: Class *objc_copyClassList(unsigned int *outCount)
  // Make a SINGLE call to get both count and class list pointer
  const char *expr = R"(
    unsigned int count = 0;
    void **classes = (void **)objc_copyClassList(&count);
    struct { void *ptr; unsigned int cnt; } result = { classes, count };
    result;
  )";
  
  ValueObjectSP result_sp;
  ExpressionResults expr_result = m_process->GetTarget().EvaluateExpression(
      expr, exe_ctx.GetFrameSP().get(), result_sp, options);
  
  if (expr_result != eExpressionCompleted || !result_sp) {
    return CreateError("Failed to enumerate classes");
  }
  
  // Extract both pointer and count from the result structure
  ValueObjectSP ptr_child = result_sp->GetChildAtIndex(0);
  ValueObjectSP count_child = result_sp->GetChildAtIndex(1);
  
  if (!ptr_child || !count_child) {
    return CreateError("Failed to extract class list result");
  }
  
  addr_t class_list_addr = ptr_child->GetValueAsUnsigned(0);
  unsigned int count = count_child->GetValueAsUnsigned(0);
  
  if (count == 0 || class_list_addr == 0) {
    // If no classes or null pointer, still need to free if we got a non-null pointer
    if (class_list_addr != 0) {
      const char *free_expr = R"(
        free((void *)0x%llx);
      )";
      char free_cmd[256];
      snprintf(free_cmd, sizeof(free_cmd), free_expr, (unsigned long long)class_list_addr);
      m_process->GetTarget().EvaluateExpression(
          free_cmd, exe_ctx.GetFrameSP().get(), result_sp, options);
    }
    return std::vector<Class>();
  }
  
  // Read the class pointers
  std::vector<Class> classes;
  size_t ptr_size = m_process->GetAddressByteSize();
  
  for (unsigned int i = 0; i < count; ++i) {
    addr_t class_ptr_addr = class_list_addr + (i * ptr_size);
    
    Status read_error;
    addr_t class_addr = m_process->ReadPointerFromMemory(class_ptr_addr, read_error);
    
    if (read_error.Success() && class_addr != 0) {
      classes.push_back(reinterpret_cast<Class>(class_addr));
    }
  }
  
  // Free the allocated list using the specific address we already have
  char free_cmd[256];
  snprintf(free_cmd, sizeof(free_cmd), "free((void *)0x%llx);", (unsigned long long)class_list_addr);
  
  m_process->GetTarget().EvaluateExpression(
      free_cmd, exe_ctx.GetFrameSP().get(), result_sp, options);
  
  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(log, "[{0}] Found {1} classes", LLDB_LOG_TAG, classes.size());
  
  return classes;
}

// === Class Hierarchy Implementation ===

llvm::Expected<std::vector<GNUstepRuntimeV2API::Class>>
GNUstepRuntimeV2API::GetClassHierarchy(Class cls) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  
  if (!cls) {
    return CreateError("Invalid class pointer");
  }
  
  std::vector<Class> hierarchy;
  Class current = cls;
  
  // Walk up the superclass chain
  while (current) {
    hierarchy.push_back(current);
    
    // Get superclass using expression evaluation
    ExecutionContext exe_ctx(m_process);
    EvaluateExpressionOptions options;
    options.SetUnwindOnError(true);
    options.SetIgnoreBreakpoints(true);
    
    char expr[256];
    snprintf(expr, sizeof(expr), 
             "(void *)class_getSuperclass((void *)0x%" PRIx64 ")",
             reinterpret_cast<uint64_t>(current));
    
    ValueObjectSP result_sp;
    ExpressionResults expr_result = m_process->GetTarget().EvaluateExpression(
        expr, exe_ctx.GetFrameSP().get(), result_sp, options);
    
    if (expr_result != eExpressionCompleted || !result_sp) {
      break;
    }
    
    addr_t superclass_addr = result_sp->GetValueAsUnsigned(0);
    if (superclass_addr == 0) {
      break;  // Reached root class
    }
    
    current = reinterpret_cast<Class>(superclass_addr);
    
    // Safety check to prevent infinite loops
    if (hierarchy.size() > 100) {
      return CreateError("Class hierarchy too deep or circular");
    }
  }
  
  return hierarchy;
}

llvm::Expected<std::vector<GNUstepRuntimeV2API::IvarInfo>>
GNUstepRuntimeV2API::GetAllIvarsIncludingInherited(Class cls) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  
  // Get class hierarchy first
  auto hierarchy_or_error = GetClassHierarchy(cls);
  if (!hierarchy_or_error) {
    return hierarchy_or_error.takeError();
  }
  
  std::vector<IvarInfo> all_ivars;
  
  // For each class in hierarchy (starting from root)
  for (auto it = hierarchy_or_error->rbegin(); 
       it != hierarchy_or_error->rend(); ++it) {
    Class current_class = *it;
    
    // Get class name for this level
    ExecutionContext exe_ctx(m_process);
    EvaluateExpressionOptions options;
    options.SetUnwindOnError(true);
    options.SetIgnoreBreakpoints(true);
    
    char name_expr[256];
    snprintf(name_expr, sizeof(name_expr),
             "(const char *)class_getName((void *)0x%" PRIx64 ")",
             reinterpret_cast<uint64_t>(current_class));
    
    ValueObjectSP name_result;
    ExpressionResults expr_result = m_process->GetTarget().EvaluateExpression(
        name_expr, exe_ctx.GetFrameSP().get(), name_result, options);
    
    std::string class_name = "Unknown";
    if (expr_result == eExpressionCompleted && name_result) {
      addr_t name_addr = name_result->GetValueAsUnsigned(0);
      if (name_addr != 0) {
        auto name_or_error = ReadCStringFromTarget(name_addr);
        if (name_or_error) {
          class_name = *name_or_error;
        }
      }
    }
    
    // Get ivars for this class
    char count_expr[256];
    snprintf(count_expr, sizeof(count_expr),
             "unsigned int count = 0; "
             "class_copyIvarList((void *)0x%" PRIx64 ", &count); count;",
             reinterpret_cast<uint64_t>(current_class));
    
    ValueObjectSP count_result;
    expr_result = m_process->GetTarget().EvaluateExpression(
        count_expr, exe_ctx.GetFrameSP().get(), count_result, options);
    
    if (expr_result != eExpressionCompleted || !count_result) {
      continue;
    }
    
    unsigned int ivar_count = count_result->GetValueAsUnsigned(0);
    if (ivar_count == 0) {
      continue;
    }
    
    // Get ivar list pointer
    char list_expr[256];
    snprintf(list_expr, sizeof(list_expr),
             "unsigned int count = 0; "
             "(void **)class_copyIvarList((void *)0x%" PRIx64 ", &count);",
             reinterpret_cast<uint64_t>(current_class));
    
    ValueObjectSP list_result;
    expr_result = m_process->GetTarget().EvaluateExpression(
        list_expr, exe_ctx.GetFrameSP().get(), list_result, options);
    
    if (expr_result != eExpressionCompleted || !list_result) {
      continue;
    }
    
    addr_t ivar_list_addr = list_result->GetValueAsUnsigned(0);
    if (ivar_list_addr == 0) {
      continue;
    }
    
    // Read each ivar
    size_t ptr_size = m_process->GetAddressByteSize();
    for (unsigned int i = 0; i < ivar_count; ++i) {
      addr_t ivar_ptr_addr = ivar_list_addr + (i * ptr_size);
      
      Status read_error;
      addr_t ivar_addr = m_process->ReadPointerFromMemory(ivar_ptr_addr, read_error);
      
      if (read_error.Fail() || ivar_addr == 0) {
        continue;
      }
      
      IvarInfo info;
      info.defining_class = current_class;
      info.defining_class_name = class_name;
      
      // Get ivar name
      char ivar_name_expr[256];
      snprintf(ivar_name_expr, sizeof(ivar_name_expr),
               "(const char *)ivar_getName((void *)0x%" PRIx64 ")",
               ivar_addr);
      
      ValueObjectSP ivar_name_result;
      expr_result = m_process->GetTarget().EvaluateExpression(
          ivar_name_expr, exe_ctx.GetFrameSP().get(), 
          ivar_name_result, options);
      
      if (expr_result == eExpressionCompleted && ivar_name_result) {
        addr_t name_addr = ivar_name_result->GetValueAsUnsigned(0);
        if (name_addr != 0) {
          auto name_or_error = ReadCStringFromTarget(name_addr);
          if (name_or_error) {
            info.name = *name_or_error;
          }
        }
      }
      
      // Get ivar type encoding
      char ivar_type_expr[256];
      snprintf(ivar_type_expr, sizeof(ivar_type_expr),
               "(const char *)ivar_getTypeEncoding((void *)0x%" PRIx64 ")",
               ivar_addr);
      
      ValueObjectSP ivar_type_result;
      expr_result = m_process->GetTarget().EvaluateExpression(
          ivar_type_expr, exe_ctx.GetFrameSP().get(), 
          ivar_type_result, options);
      
      if (expr_result == eExpressionCompleted && ivar_type_result) {
        addr_t type_addr = ivar_type_result->GetValueAsUnsigned(0);
        if (type_addr != 0) {
          auto type_or_error = ReadCStringFromTarget(type_addr);
          if (type_or_error) {
            info.type_encoding = *type_or_error;
          }
        }
      }
      
      // Get ivar offset
      char ivar_offset_expr[256];
      snprintf(ivar_offset_expr, sizeof(ivar_offset_expr),
               "(long)ivar_getOffset((void *)0x%" PRIx64 ")",
               ivar_addr);
      
      ValueObjectSP ivar_offset_result;
      expr_result = m_process->GetTarget().EvaluateExpression(
          ivar_offset_expr, exe_ctx.GetFrameSP().get(), 
          ivar_offset_result, options);
      
      if (expr_result == eExpressionCompleted && ivar_offset_result) {
        info.offset = ivar_offset_result->GetValueAsSigned(0);
      }
      
      all_ivars.push_back(info);
    }
    
    // Free the ivar list
    char free_expr[256];
    snprintf(free_expr, sizeof(free_expr),
             "unsigned int count = 0; "
             "void **ivars = (void **)class_copyIvarList((void *)0x%" PRIx64 ", &count); "
             "if (ivars) free(ivars);",
             reinterpret_cast<uint64_t>(current_class));
    
    m_process->GetTarget().EvaluateExpression(
        free_expr, exe_ctx.GetFrameSP().get(), list_result, options);
  }
  
  return all_ivars;
}

// === Class Information Implementation ===

llvm::Expected<GNUstepRuntimeV2API::ClassInfo>
GNUstepRuntimeV2API::GetClassInfo(const std::string &class_name) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  
  // Check cache first
  auto cache_result = GetCachedClassInfo(class_name);
  if (cache_result) {
    return cache_result;
  }
  // Consume the error - this is expected for cache misses
  llvm::consumeError(cache_result.takeError());
  
  // Find the class
  auto class_or_error = FindClass(class_name);
  if (!class_or_error) {
    return class_or_error.takeError();
  }
  
  return GetClassInfoFromPointer(*class_or_error);
}

llvm::Expected<GNUstepRuntimeV2API::ClassInfo>
GNUstepRuntimeV2API::GetClassInfoFromPointer(Class cls) {
  if (!cls) {
    return CreateError("Invalid class pointer");
  }
  
  ClassInfo info;
  info.class_ptr = cls;
  
  ExecutionContext exe_ctx(m_process);
  EvaluateExpressionOptions options;
  options.SetUnwindOnError(true);
  options.SetIgnoreBreakpoints(true);
  
  // Get class name
  char name_expr[256];
  snprintf(name_expr, sizeof(name_expr),
           "(const char *)class_getName((void *)0x%" PRIx64 ")",
           reinterpret_cast<uint64_t>(cls));
  
  ValueObjectSP name_result;
  ExpressionResults expr_result = m_process->GetTarget().EvaluateExpression(
      name_expr, exe_ctx.GetFrameSP().get(), name_result, options);
  
  if (expr_result == eExpressionCompleted && name_result) {
    addr_t name_addr = name_result->GetValueAsUnsigned(0);
    if (name_addr != 0) {
      auto name_or_error = ReadCStringFromTarget(name_addr);
      if (name_or_error) {
        info.name = *name_or_error;
      }
    }
  }
  
  // Get superclass
  char super_expr[256];
  snprintf(super_expr, sizeof(super_expr),
           "(void *)class_getSuperclass((void *)0x%" PRIx64 ")",
           reinterpret_cast<uint64_t>(cls));
  
  ValueObjectSP super_result;
  expr_result = m_process->GetTarget().EvaluateExpression(
      super_expr, exe_ctx.GetFrameSP().get(), super_result, options);
  
  if (expr_result == eExpressionCompleted && super_result) {
    info.superclass_ptr = reinterpret_cast<Class>(
        super_result->GetValueAsUnsigned(0));
    
    if (info.superclass_ptr) {
      // Get superclass name
      char super_name_expr[256];
      snprintf(super_name_expr, sizeof(super_name_expr),
               "(const char *)class_getName((void *)0x%" PRIx64 ")",
               reinterpret_cast<uint64_t>(info.superclass_ptr));
      
      ValueObjectSP super_name_result;
      expr_result = m_process->GetTarget().EvaluateExpression(
          super_name_expr, exe_ctx.GetFrameSP().get(), 
          super_name_result, options);
      
      if (expr_result == eExpressionCompleted && super_name_result) {
        addr_t super_name_addr = super_name_result->GetValueAsUnsigned(0);
        if (super_name_addr != 0) {
          auto super_name_or_error = ReadCStringFromTarget(super_name_addr);
          if (super_name_or_error) {
            info.superclass_name = *super_name_or_error;
          }
        }
      }
    }
  }
  
  info.is_root_class = (info.superclass_ptr == nullptr);
  
  // Get instance size
  char size_expr[256];
  snprintf(size_expr, sizeof(size_expr),
           "(unsigned long)class_getInstanceSize((void *)0x%" PRIx64 ")",
           reinterpret_cast<uint64_t>(cls));
  
  ValueObjectSP size_result;
  expr_result = m_process->GetTarget().EvaluateExpression(
      size_expr, exe_ctx.GetFrameSP().get(), size_result, options);
  
  if (expr_result == eExpressionCompleted && size_result) {
    info.instance_size = size_result->GetValueAsUnsigned(0);
  }
  
  // Check if meta class
  if (m_runtime.class_isMetaClass) {
    char meta_expr[256];
    snprintf(meta_expr, sizeof(meta_expr),
             "(int)class_isMetaClass((void *)0x%" PRIx64 ")",
             reinterpret_cast<uint64_t>(cls));
    
    ValueObjectSP meta_result;
    expr_result = m_process->GetTarget().EvaluateExpression(
        meta_expr, exe_ctx.GetFrameSP().get(), meta_result, options);
    
    if (expr_result == eExpressionCompleted && meta_result) {
      info.is_meta_class = meta_result->GetValueAsUnsigned(0) != 0;
    }
  }
  
  // Get hierarchy
  auto hierarchy_or_error = GetClassHierarchy(cls);
  if (hierarchy_or_error) {
    info.hierarchy = *hierarchy_or_error;
    
    // Get hierarchy names
    for (Class hier_cls : info.hierarchy) {
      char hier_name_expr[256];
      snprintf(hier_name_expr, sizeof(hier_name_expr),
               "(const char *)class_getName((void *)0x%" PRIx64 ")",
               reinterpret_cast<uint64_t>(hier_cls));
      
      ValueObjectSP hier_name_result;
      expr_result = m_process->GetTarget().EvaluateExpression(
          hier_name_expr, exe_ctx.GetFrameSP().get(), 
          hier_name_result, options);
      
      std::string hier_name = "Unknown";
      if (expr_result == eExpressionCompleted && hier_name_result) {
        addr_t hier_name_addr = hier_name_result->GetValueAsUnsigned(0);
        if (hier_name_addr != 0) {
          auto hier_name_or_error = ReadCStringFromTarget(hier_name_addr);
          if (hier_name_or_error) {
            hier_name = *hier_name_or_error;
          }
        }
      }
      info.hierarchy_names.push_back(hier_name);
    }
  }
  
  // Get all ivars including inherited
  auto ivars_or_error = GetAllIvarsIncludingInherited(cls);
  if (ivars_or_error) {
    info.all_ivars = *ivars_or_error;
    
    // Separate declared vs inherited
    for (const auto &ivar : info.all_ivars) {
      if (ivar.defining_class == cls) {
        info.declared_ivars.push_back(ivar);
      }
    }
  }
  
  // Get all methods including inherited
  auto methods_or_error = GetAllMethodsIncludingInherited(cls);
  if (methods_or_error) {
    info.all_methods = *methods_or_error;
    
    // Separate declared vs inherited
    for (const auto &method : info.all_methods) {
      if (method.defining_class == cls) {
        info.declared_methods.push_back(method);
      }
    }
  }
  
  // Get all properties including inherited
  auto properties_or_error = GetAllPropertiesIncludingInherited(cls);
  if (properties_or_error) {
    info.all_properties = *properties_or_error;
    
    // Separate declared vs inherited
    for (const auto &property : info.all_properties) {
      if (property.defining_class == cls) {
        info.declared_properties.push_back(property);
      }
    }
  }
  
  // Cache the result
  CacheClassInfo(info);
  
  return info;
}

llvm::Expected<GNUstepRuntimeV2API::Class>
GNUstepRuntimeV2API::FindClass(const std::string &class_name) {
  if (class_name.empty()) {
    return CreateError("Empty class name");
  }
  
  ExecutionContext exe_ctx(m_process);
  EvaluateExpressionOptions options;
  options.SetUnwindOnError(true);
  options.SetIgnoreBreakpoints(true);
  
  char expr[256];
  snprintf(expr, sizeof(expr),
           "(void *)objc_lookUpClass(\"%s\")",
           class_name.c_str());
  
  ValueObjectSP result;
  ExpressionResults expr_result = m_process->GetTarget().EvaluateExpression(
      expr, exe_ctx.GetFrameSP().get(), result, options);
  
  if (expr_result != eExpressionCompleted || !result) {
    return CreateError("Failed to find class: %s", class_name.c_str());
  }
  
  addr_t class_addr = result->GetValueAsUnsigned(0);
  if (class_addr == 0) {
    return CreateError("Class not found: %s", class_name.c_str());
  }
  
  return reinterpret_cast<Class>(class_addr);
}

// === Foundation Class Registration ===

llvm::Expected<std::vector<GNUstepRuntimeV2API::ClassInfo>>
GNUstepRuntimeV2API::GetAllFoundationClasses() {
  auto classes_or_error = GetAllClasses();
  if (!classes_or_error) {
    return classes_or_error.takeError();
  }
  
  std::vector<ClassInfo> foundation_classes;
  
  for (Class cls : *classes_or_error) {
    auto info_or_error = GetClassInfoFromPointer(cls);
    if (!info_or_error) {
      continue;
    }
    
    if (IsFoundationClass(info_or_error->name)) {
      foundation_classes.push_back(*info_or_error);
    }
  }
  
  Log *log = GetLog(LLDBLog::Language);
  LLDB_LOG(log, "[{0}] Found {1} Foundation classes", 
           LLDB_LOG_TAG, foundation_classes.size());
  
  return foundation_classes;
}

bool GNUstepRuntimeV2API::RegisterFoundationClasses() {
  // Pre-cache common Foundation class names
  m_foundation_classes = {
    "NSObject", "NSString", "NSMutableString", "NSArray", "NSMutableArray",
    "NSDictionary", "NSMutableDictionary", "NSSet", "NSMutableSet",
    "NSNumber", "NSValue", "NSDate", "NSCalendarDate", "NSURL",
    "NSData", "NSMutableData", "NSError", "NSException", "NSUUID",
    "NSNull", "NSProxy", "NSAutoreleasePool", "NSBundle", "NSCharacterSet",
    "NSCoder", "NSDecimalNumber", "NSExpression", "NSFileHandle",
    "NSFileManager", "NSFormatter", "NSIndexPath", "NSIndexSet",
    "NSInvocation", "NSJSONSerialization", "NSKeyedArchiver",
    "NSKeyedUnarchiver", "NSLocale", "NSLock", "NSMethodSignature",
    "NSNotification", "NSNotificationCenter", "NSOperation",
    "NSOperationQueue", "NSOrderedSet", "NSPersonNameComponents",
    "NSPredicate", "NSProcessInfo", "NSPropertyListSerialization",
    "NSRegularExpression", "NSRunLoop", "NSScanner", "NSStream",
    "NSTask", "NSThread", "NSTimer", "NSTimeZone", "NSUndoManager",
    "NSURLComponents", "NSURLRequest", "NSURLResponse", "NSURLSession",
    "NSUserDefaults", "NSXMLParser", "NSAttributedString",
    "NSMutableAttributedString", "NSCountedSet", "NSHashTable",
    "NSMapTable", "NSPointerArray", "NSPointerFunctions"
  };
  
  // Try to pre-cache info for common classes
  // Skip this for now - classes might not be loaded yet at plugin initialization
  // We'll register them on-demand when they're actually used
  LLDB_LOG(GetLog(LLDBLog::Language), "Skipping Foundation class pre-registration - will register on-demand");
  
  return true;
}

bool GNUstepRuntimeV2API::IsFoundationClass(const std::string &class_name) {
  // Quick check for NS prefix
  if (class_name.size() >= 2 && class_name[0] == 'N' && class_name[1] == 'S') {
    return true;
  }
  
  // Check against known Foundation classes
  return std::find(m_foundation_classes.begin(), m_foundation_classes.end(),
                   class_name) != m_foundation_classes.end();
}

// === Cache Management ===

void GNUstepRuntimeV2API::CacheClassInfo(const ClassInfo &info) {
  m_class_cache[info.name] = info;
  m_class_name_cache[info.class_ptr] = info.name;
}

llvm::Expected<GNUstepRuntimeV2API::ClassInfo>
GNUstepRuntimeV2API::GetCachedClassInfo(const std::string &name) {
  auto it = m_class_cache.find(name);
  if (it != m_class_cache.end()) {
    return it->second;
  }
  return CreateError("Not in cache");
}

llvm::Expected<std::vector<GNUstepRuntimeV2API::MethodInfo>>
GNUstepRuntimeV2API::GetAllMethodsIncludingInherited(Class cls) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  
  // Get class hierarchy first
  auto hierarchy_or_error = GetClassHierarchy(cls);
  if (!hierarchy_or_error) {
    return hierarchy_or_error.takeError();
  }
  
  std::vector<MethodInfo> all_methods;
  
  // For each class in hierarchy (starting from root)
  for (auto it = hierarchy_or_error->rbegin(); 
       it != hierarchy_or_error->rend(); ++it) {
    Class current_class = *it;
    
    // Get class name for this level
    ExecutionContext exe_ctx(m_process);
    EvaluateExpressionOptions options;
    options.SetUnwindOnError(true);
    options.SetIgnoreBreakpoints(true);
    
    char name_expr[256];
    snprintf(name_expr, sizeof(name_expr),
             "(const char *)class_getName((void *)0x%" PRIx64 ")",
             reinterpret_cast<uint64_t>(current_class));
    
    ValueObjectSP name_result;
    ExpressionResults expr_result = m_process->GetTarget().EvaluateExpression(
        name_expr, exe_ctx.GetFrameSP().get(), name_result, options);
    
    std::string class_name = "Unknown";
    if (expr_result == eExpressionCompleted && name_result) {
      addr_t name_addr = name_result->GetValueAsUnsigned(0);
      if (name_addr != 0) {
        auto name_or_error = ReadCStringFromTarget(name_addr);
        if (name_or_error) {
          class_name = *name_or_error;
        }
      }
    }
    
    // Get methods for this class
    char count_expr[256];
    snprintf(count_expr, sizeof(count_expr),
             "unsigned int count = 0; "
             "class_copyMethodList((void *)0x%" PRIx64 ", &count); count;",
             reinterpret_cast<uint64_t>(current_class));
    
    ValueObjectSP count_result;
    expr_result = m_process->GetTarget().EvaluateExpression(
        count_expr, exe_ctx.GetFrameSP().get(), count_result, options);
    
    if (expr_result != eExpressionCompleted || !count_result) {
      continue;
    }
    
    unsigned int method_count = count_result->GetValueAsUnsigned(0);
    if (method_count == 0) {
      continue;
    }
    
    // Get method list pointer
    char list_expr[256];
    snprintf(list_expr, sizeof(list_expr),
             "unsigned int count = 0; "
             "(void **)class_copyMethodList((void *)0x%" PRIx64 ", &count);",
             reinterpret_cast<uint64_t>(current_class));
    
    ValueObjectSP list_result;
    expr_result = m_process->GetTarget().EvaluateExpression(
        list_expr, exe_ctx.GetFrameSP().get(), list_result, options);
    
    if (expr_result != eExpressionCompleted || !list_result) {
      continue;
    }
    
    addr_t method_list_addr = list_result->GetValueAsUnsigned(0);
    if (method_list_addr == 0) {
      continue;
    }
    
    // Read each method
    size_t ptr_size = m_process->GetAddressByteSize();
    for (unsigned int i = 0; i < method_count; ++i) {
      addr_t method_ptr_addr = method_list_addr + (i * ptr_size);
      
      Status read_error;
      addr_t method_addr = m_process->ReadPointerFromMemory(method_ptr_addr, read_error);
      
      if (read_error.Fail() || method_addr == 0) {
        continue;
      }
      
      MethodInfo info;
      info.defining_class = current_class;
      info.defining_class_name = class_name;
      
      // Get method selector name
      char sel_expr[256];
      snprintf(sel_expr, sizeof(sel_expr),
               "(void *)method_getName((void *)0x%" PRIx64 ")",
               method_addr);
      
      ValueObjectSP sel_result;
      expr_result = m_process->GetTarget().EvaluateExpression(
          sel_expr, exe_ctx.GetFrameSP().get(), sel_result, options);
      
      if (expr_result == eExpressionCompleted && sel_result) {
        addr_t sel_addr = sel_result->GetValueAsUnsigned(0);
        if (sel_addr != 0) {
          // Get selector name
          char sel_name_expr[256];
          snprintf(sel_name_expr, sizeof(sel_name_expr),
                   "(const char *)sel_getName((void *)0x%" PRIx64 ")",
                   sel_addr);
          
          ValueObjectSP sel_name_result;
          expr_result = m_process->GetTarget().EvaluateExpression(
              sel_name_expr, exe_ctx.GetFrameSP().get(), 
              sel_name_result, options);
          
          if (expr_result == eExpressionCompleted && sel_name_result) {
            addr_t sel_name_addr = sel_name_result->GetValueAsUnsigned(0);
            if (sel_name_addr != 0) {
              auto name_or_error = ReadCStringFromTarget(sel_name_addr);
              if (name_or_error) {
                info.selector_name = *name_or_error;
              }
            }
          }
        }
      }
      
      // Get method type encoding
      char type_expr[256];
      snprintf(type_expr, sizeof(type_expr),
               "(const char *)method_getTypeEncoding((void *)0x%" PRIx64 ")",
               method_addr);
      
      ValueObjectSP type_result;
      expr_result = m_process->GetTarget().EvaluateExpression(
          type_expr, exe_ctx.GetFrameSP().get(), type_result, options);
      
      if (expr_result == eExpressionCompleted && type_result) {
        addr_t type_addr = type_result->GetValueAsUnsigned(0);
        if (type_addr != 0) {
          auto type_or_error = ReadCStringFromTarget(type_addr);
          if (type_or_error) {
            info.type_encoding = *type_or_error;
          }
        }
      }
      
      // Get method implementation address
      char impl_expr[256];
      snprintf(impl_expr, sizeof(impl_expr),
               "(void *)method_getImplementation((void *)0x%" PRIx64 ")",
               method_addr);
      
      ValueObjectSP impl_result;
      expr_result = m_process->GetTarget().EvaluateExpression(
          impl_expr, exe_ctx.GetFrameSP().get(), impl_result, options);
      
      if (expr_result == eExpressionCompleted && impl_result) {
        info.implementation = impl_result->GetValueAsUnsigned(0);
      }
      
      all_methods.push_back(info);
    }
    
    // Free the method list
    char free_expr[256];
    snprintf(free_expr, sizeof(free_expr),
             "unsigned int count = 0; "
             "void **methods = (void **)class_copyMethodList((void *)0x%" PRIx64 ", &count); "
             "if (methods) free(methods);",
             reinterpret_cast<uint64_t>(current_class));
    
    m_process->GetTarget().EvaluateExpression(
        free_expr, exe_ctx.GetFrameSP().get(), list_result, options);
  }
  
  return all_methods;
}

llvm::Expected<std::vector<GNUstepRuntimeV2API::PropertyInfo>>
GNUstepRuntimeV2API::GetAllPropertiesIncludingInherited(Class cls) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  
  // Get class hierarchy first
  auto hierarchy_or_error = GetClassHierarchy(cls);
  if (!hierarchy_or_error) {
    return hierarchy_or_error.takeError();
  }
  
  std::vector<PropertyInfo> all_properties;
  
  // For each class in hierarchy (starting from root)
  for (auto it = hierarchy_or_error->rbegin(); 
       it != hierarchy_or_error->rend(); ++it) {
    Class current_class = *it;
    
    // Get class name for this level
    ExecutionContext exe_ctx(m_process);
    EvaluateExpressionOptions options;
    options.SetUnwindOnError(true);
    options.SetIgnoreBreakpoints(true);
    
    char name_expr[256];
    snprintf(name_expr, sizeof(name_expr),
             "(const char *)class_getName((void *)0x%" PRIx64 ")",
             reinterpret_cast<uint64_t>(current_class));
    
    ValueObjectSP name_result;
    ExpressionResults expr_result = m_process->GetTarget().EvaluateExpression(
        name_expr, exe_ctx.GetFrameSP().get(), name_result, options);
    
    std::string class_name = "Unknown";
    if (expr_result == eExpressionCompleted && name_result) {
      addr_t name_addr = name_result->GetValueAsUnsigned(0);
      if (name_addr != 0) {
        auto name_or_error = ReadCStringFromTarget(name_addr);
        if (name_or_error) {
          class_name = *name_or_error;
        }
      }
    }
    
    // Get properties for this class
    if (!m_runtime.class_copyPropertyList) {
      continue;  // Property introspection not available
    }
    
    char count_expr[256];
    snprintf(count_expr, sizeof(count_expr),
             "unsigned int count = 0; "
             "class_copyPropertyList((void *)0x%" PRIx64 ", &count); count;",
             reinterpret_cast<uint64_t>(current_class));
    
    ValueObjectSP count_result;
    expr_result = m_process->GetTarget().EvaluateExpression(
        count_expr, exe_ctx.GetFrameSP().get(), count_result, options);
    
    if (expr_result != eExpressionCompleted || !count_result) {
      continue;
    }
    
    unsigned int property_count = count_result->GetValueAsUnsigned(0);
    if (property_count == 0) {
      continue;
    }
    
    // Get property list pointer
    char list_expr[256];
    snprintf(list_expr, sizeof(list_expr),
             "unsigned int count = 0; "
             "(void **)class_copyPropertyList((void *)0x%" PRIx64 ", &count);",
             reinterpret_cast<uint64_t>(current_class));
    
    ValueObjectSP list_result;
    expr_result = m_process->GetTarget().EvaluateExpression(
        list_expr, exe_ctx.GetFrameSP().get(), list_result, options);
    
    if (expr_result != eExpressionCompleted || !list_result) {
      continue;
    }
    
    addr_t property_list_addr = list_result->GetValueAsUnsigned(0);
    if (property_list_addr == 0) {
      continue;
    }
    
    // Read each property
    size_t ptr_size = m_process->GetAddressByteSize();
    for (unsigned int i = 0; i < property_count; ++i) {
      addr_t property_ptr_addr = property_list_addr + (i * ptr_size);
      
      Status read_error;
      addr_t property_addr = m_process->ReadPointerFromMemory(property_ptr_addr, read_error);
      
      if (read_error.Fail() || property_addr == 0) {
        continue;
      }
      
      PropertyInfo info;
      info.defining_class = current_class;
      info.defining_class_name = class_name;
      
      // Get property name
      if (m_runtime.property_getName) {
        char prop_name_expr[256];
        snprintf(prop_name_expr, sizeof(prop_name_expr),
                 "(const char *)property_getName((void *)0x%" PRIx64 ")",
                 property_addr);
        
        ValueObjectSP prop_name_result;
        expr_result = m_process->GetTarget().EvaluateExpression(
            prop_name_expr, exe_ctx.GetFrameSP().get(), 
            prop_name_result, options);
        
        if (expr_result == eExpressionCompleted && prop_name_result) {
          addr_t name_addr = prop_name_result->GetValueAsUnsigned(0);
          if (name_addr != 0) {
            auto name_or_error = ReadCStringFromTarget(name_addr);
            if (name_or_error) {
              info.name = *name_or_error;
            }
          }
        }
      }
      
      // Get property attributes
      if (m_runtime.property_getAttributes) {
        char prop_attr_expr[256];
        snprintf(prop_attr_expr, sizeof(prop_attr_expr),
                 "(const char *)property_getAttributes((void *)0x%" PRIx64 ")",
                 property_addr);
        
        ValueObjectSP prop_attr_result;
        expr_result = m_process->GetTarget().EvaluateExpression(
            prop_attr_expr, exe_ctx.GetFrameSP().get(), 
            prop_attr_result, options);
        
        if (expr_result == eExpressionCompleted && prop_attr_result) {
          addr_t attr_addr = prop_attr_result->GetValueAsUnsigned(0);
          if (attr_addr != 0) {
            auto attr_or_error = ReadCStringFromTarget(attr_addr);
            if (attr_or_error) {
              info.attributes = *attr_or_error;
            }
          }
        }
      }
      
      all_properties.push_back(info);
    }
    
    // Free the property list
    char free_expr[256];
    snprintf(free_expr, sizeof(free_expr),
             "unsigned int count = 0; "
             "void **props = (void **)class_copyPropertyList((void *)0x%" PRIx64 ", &count); "
             "if (props) free(props);",
             reinterpret_cast<uint64_t>(current_class));
    
    m_process->GetTarget().EvaluateExpression(
        free_expr, exe_ctx.GetFrameSP().get(), list_result, options);
  }
  
  return all_properties;
}

// === Runtime Version ===

std::string GNUstepRuntimeV2API::GetRuntimeVersion() const {
  // Try to call objc_getVersion if available
  ExecutionContext exe_ctx(m_process);
  EvaluateExpressionOptions options;
  options.SetUnwindOnError(true);
  options.SetIgnoreBreakpoints(true);
  
  const char *expr = "(int)objc_getVersion()";
  
  ValueObjectSP result;
  ExpressionResults expr_result = const_cast<Process *>(m_process)->GetTarget().EvaluateExpression(
      expr, exe_ctx.GetFrameSP().get(), result, options);
  
  if (expr_result == eExpressionCompleted && result) {
    int version = result->GetValueAsSigned(0);
    return llvm::formatv("GNUstep libobjc2 v{0}", version).str();
  }
  
  return "GNUstep libobjc2 (version unknown)";
}