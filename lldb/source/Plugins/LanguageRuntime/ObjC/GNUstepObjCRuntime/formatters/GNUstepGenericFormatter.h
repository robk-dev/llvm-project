//===-- GNUstepGenericFormatter.h ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GENERICFORMATTER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GENERICFORMATTER_H

#include "GNUstepFormattersBase.h"
#include <vector>
#include <string>

namespace lldb_private {
namespace formatters {

/// Structure to hold information about an instance variable
struct IvarInfo {
  std::string name;
  std::string type_encoding;
  int32_t offset;
  uint32_t size;
  lldb::addr_t value_addr;  // Address where the ivar's value is stored
  
  IvarInfo() : offset(0), size(0), value_addr(LLDB_INVALID_ADDRESS) {}
};

/// Generic formatter that works for ANY Objective-C class
/// by introspecting runtime metadata and walking the superclass hierarchy
class GNUstepGenericFormatter : public GNUstepSummaryProvider {
public:
  /// Returns true for ANY ObjC object that doesn't have a specific formatter
  static bool WouldWork(ValueObject& valobj);
  
  /// Format the object by introspecting its ivars
  bool FormatObject(ValueObject &valobj, Stream &stream, 
                   const TypeSummaryOptions &options) override;
  
private:
  /// Get the class pointer from an object
  lldb::addr_t GetClassFromObject(Process *process, lldb::addr_t obj_addr);
  
  /// Get the superclass of a class
  lldb::addr_t GetSuperclass(Process *process, lldb::addr_t class_addr);
  
  /// Get the class name from a class pointer
  std::string GetClassName(Process *process, lldb::addr_t class_addr);
  
  /// Extract ivars from a specific class (not including superclass ivars)
  std::vector<IvarInfo> ExtractIvarsFromClass(Process *process, 
                                              lldb::addr_t class_addr,
                                              lldb::addr_t obj_addr);
  
public:  // Make this method public so the synthetic provider can use it
  /// Walk the superclass hierarchy and collect all ivars
  std::vector<IvarInfo> CollectAllIvars(Process *process, 
                                        lldb::addr_t obj_addr);
  
  /// Format a single ivar based on its type encoding
  std::string FormatIvar(Process *process, const IvarInfo &ivar);
  
  /// Format an object reference ivar
  std::string FormatObjectIvar(Process *process, lldb::addr_t obj_addr);
  
  /// Format a primitive type ivar
  std::string FormatPrimitiveIvar(Process *process, const IvarInfo &ivar);
  
  /// Format a C string ivar
  std::string FormatCStringIvar(Process *process, lldb::addr_t str_addr);
  
  /// Format a struct ivar (basic support)
  std::string FormatStructIvar(Process *process, const IvarInfo &ivar);
  
  /// Check if a class has a specific formatter registered
  bool HasSpecificFormatter(const std::string &class_name);
  
  /// Parse type encoding to determine basic type
  enum class BasicType {
    Object,      // @
    Integer,     // i, I, l, L, q, Q
    Float,       // f, d
    Boolean,     // c, C (when size is 1)
    CString,     // *
    Pointer,     // ^
    Struct,      // {
    Array,       // [
    Unknown
  };
  
  BasicType GetBasicType(const std::string &type_encoding);
  
  /// Helper to read various sized integers
  int64_t ReadSignedInteger(Process *process, lldb::addr_t addr, uint32_t size);
  uint64_t ReadUnsignedInteger(Process *process, lldb::addr_t addr, uint32_t size);
  
  /// Helper to read floating point values
  double ReadFloatingPoint(Process *process, lldb::addr_t addr, uint32_t size);
};

/// Formatter function for generic objects
bool GNUstepGenericFormatterFunction(ValueObject &valobj, Stream &stream,
                                     const TypeSummaryOptions &options);

/// Synthetic children provider for generic Objective-C objects
/// This provider filters out the isa pointer and shows only user-defined ivars
class GNUstepGenericObjectSyntheticProvider : public GNUstepSyntheticProvider {
public:
  GNUstepGenericObjectSyntheticProvider(lldb::ValueObjectSP valobj_sp);
  ~GNUstepGenericObjectSyntheticProvider() override = default;
  
  llvm::Expected<uint32_t> CalculateNumChildren() override;
  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;
  
protected:
  bool UpdateImpl() override;
  
private:
  std::vector<IvarInfo> m_ivars; // List of ivars excluding isa
  lldb::addr_t m_obj_addr;
  // Child cache to ensure unique ValueObject instances
  std::map<uint32_t, lldb::ValueObjectSP> m_children_cache;
};

/// Creator function for generic object synthetic provider
SyntheticChildrenFrontEnd *
GNUstepGenericObjectSyntheticFrontEndCreator(CXXSyntheticChildren *synth,
                                              lldb::ValueObjectSP valobj_sp);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_GENERICFORMATTER_H