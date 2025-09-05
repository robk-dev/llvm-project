//===-- GNUstepUniversalFormatter.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepUniversalFormatter.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/Stream.h"
#include <cstdio>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

//===----------------------------------------------------------------------===//
// Universal Summary Provider - Handles ALL GNUstep/ObjC objects
//===----------------------------------------------------------------------===//

bool lldb_private::formatters::GNUstepUniversalSummaryProvider(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  lldb::addr_t obj_addr = valobj.GetValueAsUnsigned(0);
  
  // Handle nil
  if (obj_addr == 0) {
    stream.Printf("nil");
    return true;
  }
  
  // Handle tagged pointers (small integers, etc.)
  if (obj_addr & 0x1) {
    uint8_t tag = (obj_addr >> 1) & 0x7;
    
    // Small integer (tag 0)
    if (tag == 0) {
      int64_t value = ((int64_t)obj_addr) >> 3;
      stream.Printf("%lld", (long long)value);
      return true;
    }
    
    // Other tagged types - try description method
    // Fall through to expression evaluation
  }
  
  // For all objects, use expression evaluation to call description
  ExecutionContext exe_ctx(valobj.GetExecutionContextRef());
  Target *target = exe_ctx.GetTargetPtr();
  if (!target)
    return false;
  
  // Try to get description string
  char expr_buf[256];
  snprintf(expr_buf, sizeof(expr_buf), 
           "(id)[(id)0x%llx description]", 
           (unsigned long long)obj_addr);
  
  EvaluateExpressionOptions eval_options;
  eval_options.SetCoerceToId(false);
  eval_options.SetUnwindOnError(true);
  eval_options.SetIgnoreBreakpoints(true);
  eval_options.SetTimeout(std::chrono::milliseconds(500));
  eval_options.SetTryAllThreads(false);
  
  ValueObjectSP result_sp;
  target->EvaluateExpression(expr_buf, exe_ctx.GetFrameSP().get(), 
                             result_sp, eval_options);
  
  if (result_sp && result_sp->GetError().Success()) {
    lldb::addr_t desc_addr = result_sp->GetValueAsUnsigned(0);
    if (desc_addr != 0) {
      // Get the UTF8 string
      snprintf(expr_buf, sizeof(expr_buf), 
               "(const char *)[(id)0x%llx UTF8String]", 
               (unsigned long long)desc_addr);
      
      target->EvaluateExpression(expr_buf, exe_ctx.GetFrameSP().get(), 
                                result_sp, eval_options);
      
      if (result_sp && result_sp->GetError().Success()) {
        const char *str = result_sp->GetValueAsCString();
        if (str) {
          stream.Printf("%s", str);
          return true;
        }
      }
    }
  }
  
  // Fallback: try to get class name and show basic info
  ProcessSP process_sp = valobj.GetProcessSP();
  if (process_sp) {
    ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
    if (runtime) {
      ObjCLanguageRuntime::ClassDescriptorSP descriptor = 
          runtime->GetClassDescriptor(valobj);
      if (descriptor) {
        std::string class_name = descriptor->GetClassName().GetStringRef().str();
        
        // For some known types, provide better default display
        if (class_name.find("String") != std::string::npos) {
          stream.Printf("@\"...\"");
        } else if (class_name.find("Array") != std::string::npos) {
          stream.Printf("@[...]");
        } else if (class_name.find("Dictionary") != std::string::npos) {
          stream.Printf("@{...}");
        } else if (class_name.find("Set") != std::string::npos) {
          stream.Printf("{...}");
        } else {
          stream.Printf("<%s: 0x%llx>", class_name.c_str(), 
                       (unsigned long long)obj_addr);
        }
        return true;
      }
    }
  }
  
  stream.Printf("<object: 0x%llx>", (unsigned long long)obj_addr);
  return true;
}

//===----------------------------------------------------------------------===//
// Universal Synthetic Children Provider
//===----------------------------------------------------------------------===//

GNUstepUniversalSyntheticProvider::GNUstepUniversalSyntheticProvider(
    ValueObject &valobj)
    : SyntheticChildrenFrontEnd(valobj),
      m_exe_ctx(valobj.GetExecutionContextRef()),
      m_obj_addr(valobj.GetValueAsUnsigned(0)),
      m_count(0),
      m_type(Unknown) {
  
  if (m_obj_addr == 0)
    return;
    
  m_type = DetectObjectType();
}

GNUstepUniversalSyntheticProvider::ObjectType 
GNUstepUniversalSyntheticProvider::DetectObjectType() {
  if (m_obj_addr == 0)
    return Unknown;
    
  if (IsTaggedPointer())
    return TaggedPointer;
  
  // Get class name to determine type
  ProcessSP process_sp = m_backend.GetProcessSP();
  if (!process_sp)
    return Unknown;
    
  ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp);
  if (!runtime)
    return Unknown;
  
  ObjCLanguageRuntime::ClassDescriptorSP descriptor = 
      runtime->GetClassDescriptor(m_backend);
  if (!descriptor)
    return Unknown;
    
  m_class_name = descriptor->GetClassName().GetStringRef().str();
  
  // Detect collection types
  if (m_class_name.find("Array") != std::string::npos)
    return Array;
  if (m_class_name.find("Dictionary") != std::string::npos)
    return Dictionary;
  if (m_class_name.find("Set") != std::string::npos)
    return Set;
    
  return Other;
}

lldb::ValueObjectSP 
GNUstepUniversalSyntheticProvider::EvaluateExpression(const std::string &expr) {
  Target *target = m_exe_ctx.GetTargetPtr();
  if (!target)
    return nullptr;
    
  EvaluateExpressionOptions eval_options;
  eval_options.SetCoerceToId(false);
  eval_options.SetUnwindOnError(true);
  eval_options.SetIgnoreBreakpoints(true);
  eval_options.SetTimeout(std::chrono::milliseconds(500));
  
  ValueObjectSP result_sp;
  target->EvaluateExpression(expr, m_exe_ctx.GetFrameSP().get(), 
                            result_sp, eval_options);
  return result_sp;
}

llvm::Expected<uint32_t> 
GNUstepUniversalSyntheticProvider::CalculateNumChildren() {
  if (m_obj_addr == 0 || m_type == TaggedPointer || m_type == Other)
    return 0;
  
  // For collections, get count
  if (m_type == Array || m_type == Dictionary || m_type == Set) {
    char expr_buf[256];
    snprintf(expr_buf, sizeof(expr_buf), 
             "(NSUInteger)[(id)0x%llx count]", 
             (unsigned long long)m_obj_addr);
    
    ValueObjectSP result_sp = EvaluateExpression(expr_buf);
    if (result_sp && result_sp->GetError().Success()) {
      m_count = result_sp->GetValueAsUnsigned(0);
      
      // For dictionaries, we show key-value pairs
      if (m_type == Dictionary) {
        // Pre-cache children for dictionaries
        m_children_cache.clear();
        for (uint32_t i = 0; i < m_count; i++) {
          // Get key
          snprintf(expr_buf, sizeof(expr_buf),
                   "(id)[[(id)0x%llx allKeys] objectAtIndex:%u]",
                   (unsigned long long)m_obj_addr, i);
          ValueObjectSP key_sp = EvaluateExpression(expr_buf);
          
          // Get value for key
          if (key_sp && key_sp->GetError().Success()) {
            lldb::addr_t key_addr = key_sp->GetValueAsUnsigned(0);
            snprintf(expr_buf, sizeof(expr_buf),
                     "(id)[(id)0x%llx objectForKey:(id)0x%llx]",
                     (unsigned long long)m_obj_addr,
                     (unsigned long long)key_addr);
            ValueObjectSP value_sp = EvaluateExpression(expr_buf);
            
            // Store both key and value
            if (value_sp && value_sp->GetError().Success()) {
              m_children_cache.push_back(key_sp);
              m_children_cache.push_back(value_sp);
            }
          }
        }
        return m_children_cache.size() / 2;  // Return number of pairs
      }
      
      return m_count;
    }
  }
  
  return 0;
}

lldb::ValueObjectSP 
GNUstepUniversalSyntheticProvider::GetChildAtIndex(uint32_t idx) {
  if (m_type == TaggedPointer || m_type == Other)
    return nullptr;
  
  char expr_buf[256];
  char name_buf[64];
  
  switch (m_type) {
    case Array: {
      if (idx >= m_count)
        return nullptr;
        
      snprintf(expr_buf, sizeof(expr_buf),
               "(id)[(id)0x%llx objectAtIndex:%u]",
               (unsigned long long)m_obj_addr, idx);
      snprintf(name_buf, sizeof(name_buf), "[%u]", idx);
      
      ValueObjectSP child_sp = EvaluateExpression(expr_buf);
      if (child_sp && child_sp->GetError().Success()) {
        child_sp->SetName(ConstString(name_buf));
        return child_sp;
      }
      break;
    }
    
    case Dictionary: {
      // Return cached children
      if (idx * 2 + 1 < m_children_cache.size()) {
        // Return key for even indices, value for odd
        if (idx % 2 == 0) {
          uint32_t pair_idx = idx / 2;
          snprintf(name_buf, sizeof(name_buf), "key[%u]", pair_idx);
          ValueObjectSP key_sp = m_children_cache[pair_idx * 2];
          if (key_sp) {
            key_sp->SetName(ConstString(name_buf));
            return key_sp;
          }
        } else {
          uint32_t pair_idx = idx / 2;
          snprintf(name_buf, sizeof(name_buf), "value[%u]", pair_idx);
          ValueObjectSP value_sp = m_children_cache[pair_idx * 2 + 1];
          if (value_sp) {
            value_sp->SetName(ConstString(name_buf));
            return value_sp;
          }
        }
      }
      break;
    }
    
    case Set: {
      if (idx >= m_count)
        return nullptr;
        
      snprintf(expr_buf, sizeof(expr_buf),
               "(id)[[(id)0x%llx allObjects] objectAtIndex:%u]",
               (unsigned long long)m_obj_addr, idx);
      snprintf(name_buf, sizeof(name_buf), "[%u]", idx);
      
      ValueObjectSP child_sp = EvaluateExpression(expr_buf);
      if (child_sp && child_sp->GetError().Success()) {
        child_sp->SetName(ConstString(name_buf));
        return child_sp;
      }
      break;
    }
    
    default:
      break;
  }
  
  return nullptr;
}

lldb::ChildCacheState GNUstepUniversalSyntheticProvider::Update() {
  m_children_cache.clear();
  m_type = DetectObjectType();
  return lldb::ChildCacheState::eRefetch;
}

bool GNUstepUniversalSyntheticProvider::MightHaveChildren() {
  return (m_type == Array || m_type == Dictionary || m_type == Set) && m_count > 0;
}

size_t GNUstepUniversalSyntheticProvider::GetIndexOfChildWithName(
    ConstString name) {
  return UINT32_MAX;
}

SyntheticChildrenFrontEnd *
lldb_private::formatters::GNUstepUniversalSyntheticProviderCreator(
    CXXSyntheticChildren *synth, lldb::ValueObjectSP valobj_sp) {
  return (valobj_sp ? new GNUstepUniversalSyntheticProvider(*valobj_sp) 
                    : nullptr);
}