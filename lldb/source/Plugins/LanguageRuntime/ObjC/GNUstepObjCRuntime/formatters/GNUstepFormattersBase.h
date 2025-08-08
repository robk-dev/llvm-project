//===-- GNUstepFormattersBase.h --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_BASE_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_BASE_H

#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/DataFormatters/TypeSynthetic.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"
#include "lldb/ValueObject/ValueObject.h"
#include <unordered_set>

namespace lldb_private {
namespace formatters {

// Constants to prevent infinite recursion and timeouts
static constexpr uint32_t MAX_FORMATTER_DEPTH = 8;
static constexpr uint32_t MAX_COLLECTION_ELEMENTS_INLINE = 5;
static constexpr uint32_t MAX_STRING_PREVIEW_LENGTH = 20;
static constexpr uint32_t MAX_LOOP_ITERATIONS = 1000;  // Prevent runaway loops

/// Context for tracking recursion depth and visited objects
struct FormatterContext {
  uint32_t depth;
  std::unordered_set<lldb::addr_t> visited_objects;
  
  FormatterContext() : depth(0) {}
  
  bool ShouldStopRecursion(lldb::addr_t addr) const {
    return depth >= MAX_FORMATTER_DEPTH || visited_objects.count(addr) > 0;
  }
  
  void EnterObject(lldb::addr_t addr) {
    depth++;
    visited_objects.insert(addr);
  }
  
  void ExitObject(lldb::addr_t addr) {
    if (depth > 0) depth--;
    visited_objects.erase(addr);
  }
};

/// Base utility class for GNUstep runtime introspection
class GNUstepRuntimeHelper {
public:
  /// Get the Process from a ValueObject
  static Process *GetProcessFromValueObject(ValueObject &valobj);
  
  /// Read a pointer value from the target process
  static lldb::addr_t ReadPointer(Process *process, lldb::addr_t addr, Status &error);
  
  /// Read raw bytes from the target process
  static bool ReadMemory(Process *process, lldb::addr_t addr, void *buffer, size_t size);
  
  /// Get the target's address byte size
  static uint32_t GetAddressByteSize(Process *process);
  
  /// Check if a ValueObject represents a valid GNUstep object
  static bool IsValidGNUstepObject(ValueObject &valobj);
  
  /// Get the class name of a GNUstep object
  static std::string GetGNUstepClassName(ValueObject &valobj);
  
  /// Read a UTF-8 string from target memory
  static std::string ReadUTF8String(Process *process, lldb::addr_t addr, size_t max_length = 1024);
};

/// Base class for GNUstep summary providers
class GNUstepSummaryProvider {
public:
  virtual ~GNUstepSummaryProvider() = default;
  virtual bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) = 0;
  
protected:
  /// Helper to write a quoted string to the stream
  static void WriteQuotedString(Stream &stream, const std::string &str);
  
  /// Helper to write an error message to the stream
  static void WriteErrorSummary(Stream &stream, const std::string &error_msg);
};

/// Base class for GNUstep synthetic children providers
class GNUstepSyntheticProvider : public SyntheticChildrenFrontEnd {
public:
  GNUstepSyntheticProvider(lldb::ValueObjectSP valobj_sp);
  ~GNUstepSyntheticProvider() override = default;

  lldb::ChildCacheState Update() override;
  bool MightHaveChildren() override;
  size_t GetIndexOfChildWithName(ConstString name) override;

protected:
  /// Derived classes should implement this to update their internal state
  virtual bool UpdateImpl() = 0;
  
  /// Helper to create a child ValueObject for a given address and type
  lldb::ValueObjectSP CreateValueObjectFromAddress(const std::string &name, 
                                                   lldb::addr_t addr, 
                                                   CompilerType type);
  
  /// Helper to create a child ValueObject for a given value
  lldb::ValueObjectSP CreateValueObjectFromData(const std::string &name,
                                                const DataExtractor &data,
                                                CompilerType type);

  // Cached process pointer for efficiency
  Process *m_process;
  bool m_update_called;
};

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_BASE_H
