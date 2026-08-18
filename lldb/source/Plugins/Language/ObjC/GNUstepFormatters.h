//===-- GNUstepFormatters.h -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Data formatters for the concrete Foundation classes of gnustep-base, the
// Foundation implementation used with the GNUstep libobjc2 runtime.
//
// Like the Apple formatters these never run code in the inferior. Unlike them
// they do not hardcode ivar offsets: libobjc2 packs instance sizes and the
// widths of `long`-typed ivars differ between LP64 and LLP64, so ivars are
// looked up by name - through the debug info attached to the value's dynamic
// type where there is any, and through libobjc2's own metadata otherwise.
// Small objects (libobjc2's tagged pointers) are decoded from the pointer
// bits alone.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGE_OBJC_GNUSTEPFORMATTERS_H
#define LLDB_SOURCE_PLUGINS_LANGUAGE_OBJC_GNUSTEPFORMATTERS_H

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h"
#include "lldb/DataFormatters/TypeCategory.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/DataFormatters/TypeSynthetic.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/lldb-forward.h"

#include <optional>
#include <string>

namespace lldb_private {
namespace formatters {

/// True if the process debugging \p valobj uses the GNUstep libobjc2 runtime.
bool IsGNUstepObjCRuntime(ValueObject &valobj);

/// Registers all GNUstep formatters into the shared "objc" category, keyed
/// by gnustep-base's concrete class names so they are picked up through the
/// runtime-reported class name of a value.
void LoadGNUstepFormatters(lldb::TypeCategoryImplSP objc_category_sp);

/// Returns the ivar named \p name on \p valobj, or an empty pointer if it
/// cannot be reached, which the callers treat as "cannot format".
///
/// Debug info is preferred, because it is the only source that describes
/// members *inside* a struct-typed ivar. Where it has nothing - gnustep-base
/// hides several classes' ivars behind GS_EXPOSE, and a stripped build has
/// none at all - libobjc2's own metadata is used instead.
lldb::ValueObjectSP GNUstepGetIvar(ValueObject &valobj, llvm::StringRef name);

/// Finds an ivar using only libobjc2's runtime metadata, for classes whose
/// ivars debug info does not describe - gnustep-base hides several behind
/// GS_EXPOSE, and a stripped build has none. Only reaches an ivar the class
/// or one of its superclasses declares directly: the runtime's encodings
/// expand struct members positionally, with no field names, so a member
/// *inside* a struct-typed ivar is not addressable this way.
lldb::ValueObjectSP GNUstepGetIvarFromRuntime(ValueObject &valobj,
                                              llvm::StringRef name);

/// The value of a floating-point ValueObject as a double, or nullopt if it
/// cannot be read as one.
std::optional<double> GNUstepGetFloatValue(ValueObject &valobj);

// Small-object (tagged pointer) decoding. The tag is the low three bits of
// the pointer; the payload layouts come from libobjc2 and gnustep-base:
//   1 NSSmallInt, 2 NSSmallExtendedDouble, 3 NSSmallRepeatingDouble,
//   4 GSTinyString, 5 NSSmallFloat, 6 GSSmallDate.
constexpr uint64_t g_gnustep_small_object_mask = 7;

/// GSTinyString packs up to eight 7-bit characters and a 5-bit length into
/// the pointer (gnustep-base Source/GSString.m, clang CGObjCGNU.cpp).
std::optional<std::string> GNUstepDecodeTinyString(uint64_t ptr);
/// NSSmallInt stores an arithmetically shifted integer (Source/NSNumber.m).
int64_t GNUstepDecodeSmallInt(uint64_t ptr);
/// NSSmallExtendedDouble / NSSmallRepeatingDouble / NSSmallFloat store a
/// double whose low mantissa bits were displaced by the tag
/// (unboxSmallExtendedDouble / unboxSmallRepeatingDouble in Source/NSNumber.m).
double GNUstepDecodeSmallExtendedDouble(uint64_t ptr);
double GNUstepDecodeSmallRepeatingDouble(uint64_t ptr);
/// GSSmallDate stores a compressed NSTimeInterval since the 2001 reference
/// date (decompressTimeInterval in Source/NSDate.m).
double GNUstepDecodeSmallDate(uint64_t ptr);

bool GNUstepNSStringSummaryProvider(ValueObject &valobj, Stream &stream,
                                    const TypeSummaryOptions &options);
bool GNUstepNSNumberSummaryProvider(ValueObject &valobj, Stream &stream,
                                    const TypeSummaryOptions &options);
bool GNUstepNSDateSummaryProvider(ValueObject &valobj, Stream &stream,
                                  const TypeSummaryOptions &options);
bool GNUstepNSArraySummaryProvider(ValueObject &valobj, Stream &stream,
                                   const TypeSummaryOptions &options);
bool GNUstepNSDictionarySummaryProvider(ValueObject &valobj, Stream &stream,
                                        const TypeSummaryOptions &options);
bool GNUstepNSSetSummaryProvider(ValueObject &valobj, Stream &stream,
                                 const TypeSummaryOptions &options);
bool GNUstepNSDataSummaryProvider(ValueObject &valobj, Stream &stream,
                                  const TypeSummaryOptions &options);
bool GNUstepNSURLSummaryProvider(ValueObject &valobj, Stream &stream,
                                 const TypeSummaryOptions &options);

bool GNUstepNSNullSummaryProvider(ValueObject &valobj, Stream &stream,
                                  const TypeSummaryOptions &options);

SyntheticChildrenFrontEnd *
GNUstepNSArraySyntheticFrontEndCreator(CXXSyntheticChildren *,
                                       lldb::ValueObjectSP);
SyntheticChildrenFrontEnd *
GNUstepNSDictionarySyntheticFrontEndCreator(CXXSyntheticChildren *,
                                            lldb::ValueObjectSP);
SyntheticChildrenFrontEnd *
GNUstepNSSetSyntheticFrontEndCreator(CXXSyntheticChildren *,
                                     lldb::ValueObjectSP);

} // namespace formatters
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGE_OBJC_GNUSTEPFORMATTERS_H
