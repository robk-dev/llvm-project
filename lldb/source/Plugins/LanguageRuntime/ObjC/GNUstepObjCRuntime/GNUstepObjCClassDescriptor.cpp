//===-- GNUstepObjCClassDescriptor.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCClassDescriptor.h"

#include "lldb/Core/Module.h"
#include "lldb/Core/ModuleList.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Symbol/Variable.h"
#include "lldb/Symbol/VariableList.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/ArchSpec.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"
#include "lldb/ValueObject/ValueObjectVariable.h"

#include "llvm/Support/MathExtras.h"

using namespace lldb;
using namespace lldb_private;

// Flags from libobjc2's `enum objc_class_flags` (class.h).
static constexpr uint64_t g_class_flag_meta = 1ULL << 0;
static constexpr uint64_t g_class_flag_resolved = 1ULL << 9;
static constexpr uint64_t g_class_flag_hidden = 1ULL << 12;

// Bounds on what a well-formed ivar list can say about itself. These are not
// tuning knobs: the values come from inferior memory, so a corrupted or
// misidentified structure must not be able to drive an unbounded read.
static constexpr uint32_t g_max_ivars = 4096;
static constexpr size_t g_max_type_encoding_length = 1024;

// Element strides are read from the metadata rather than assumed, because
// that is how the runtime walks these arrays - but a stride this large is
// not something a compiler emitted.
static constexpr uint64_t g_max_element_stride = 1024;

// An upper bound for plausible class names. A string that does not terminate
// within this many bytes is not a class name, and stopping there keeps a
// stray pointer from dragging in arbitrary amounts of inferior memory.
static constexpr size_t g_max_class_name_length = 256;

namespace {
/// Offsets of the `struct objc_class` fields this descriptor reads. The first
/// three fields are pointers; the rest are `long`, which is not always the
/// same width (see the class documentation).
struct ClassLayout {
  uint32_t pointer_size;
  uint32_t long_size;
  uint64_t superclass_offset;
  uint64_t name_offset;
  uint64_t info_offset;
  uint64_t instance_size_offset;
  uint64_t ivars_offset;
  uint64_t methods_offset;
};

ClassLayout GetClassLayout(Process &process) {
  ClassLayout layout;
  layout.pointer_size = process.GetAddressByteSize();
  // Windows is LLP64, so `long` stays 32 bits there while pointers are 64 -
  // except under Cygwin, which is LP64.
  const llvm::Triple &triple =
      process.GetTarget().GetArchitecture().GetTriple();
  layout.long_size =
      (triple.isOSWindows() && !triple.isWindowsCygwinEnvironment() &&
       layout.pointer_size == 8)
          ? 4
          : layout.pointer_size;
  layout.superclass_offset = layout.pointer_size;
  layout.name_offset = 2 * layout.pointer_size;
  layout.info_offset = 3 * layout.pointer_size + layout.long_size;
  layout.instance_size_offset = 3 * layout.pointer_size + 2 * layout.long_size;
  // `ivars` is the first field after the three `long`s, so it picks up
  // whatever tail padding the target's alignment requires. Cross-check: two
  // pointers further on is `dtable`, which libobjc2 pins per data model in
  // asmconstants.h and asserts in dtable.c.
  layout.ivars_offset = llvm::alignTo(
      3 * layout.pointer_size + 3 * layout.long_size, layout.pointer_size);
  layout.methods_offset = layout.ivars_offset + layout.pointer_size;
  return layout;
}
} // namespace

GNUstepObjCClassDescriptor::GNUstepObjCClassDescriptor(
    ProcessSP process_sp, ObjCLanguageRuntime::ObjCISA isa)
    : m_process_wp(process_sp), m_isa(isa) {
  Read();
}

void GNUstepObjCClassDescriptor::Read() {
  ProcessSP process_sp = m_process_wp.lock();
  if (!process_sp || m_isa == 0 || m_isa == LLDB_INVALID_ADDRESS)
    return;

  const ClassLayout layout = GetClassLayout(*process_sp);
  // Class objects are at least pointer-aligned.
  if (m_isa % layout.pointer_size != 0)
    return;

  auto read_pointer = [&](uint64_t offset) -> addr_t {
    return llvm::expectedToOptional(
               process_sp->ReadPointerFromMemory(m_isa + offset))
        .value_or(LLDB_INVALID_ADDRESS);
  };

  Status error;
  const addr_t metaclass = read_pointer(0);
  if (metaclass == 0 || metaclass == LLDB_INVALID_ADDRESS)
    return;
  const addr_t superclass = read_pointer(layout.superclass_offset);
  if (superclass == LLDB_INVALID_ADDRESS)
    return;
  const addr_t name_ptr = read_pointer(layout.name_offset);
  if (name_ptr == 0 || name_ptr == LLDB_INVALID_ADDRESS)
    return;

  char name_buffer[g_max_class_name_length];
  const size_t name_length = process_sp->ReadCStringFromMemory(
      name_ptr, name_buffer, sizeof(name_buffer), error);
  // A string that fills the buffer was truncated, so it is not a class name.
  if (error.Fail() || name_length == 0 ||
      name_length >= sizeof(name_buffer) - 1)
    return;

  const uint64_t info = process_sp->ReadUnsignedIntegerFromMemory(
      m_isa + layout.info_offset, layout.long_size, 0, error);
  if (error.Fail())
    return;

  // A class and its metaclass must disagree about the meta flag. Checking
  // both directions is what keeps an arbitrary readable address from being
  // accepted as a class.
  const bool is_meta = (info & g_class_flag_meta) != 0;
  m_is_hidden = (info & g_class_flag_hidden) != 0;
  if (!is_meta) {
    const uint64_t metaclass_info = process_sp->ReadUnsignedIntegerFromMemory(
        metaclass + layout.info_offset, layout.long_size, 0, error);
    if (error.Fail() || (metaclass_info & g_class_flag_meta) == 0)
      return;
  }

  // Only a resolved class has a real superclass pointer and instance size;
  // see the class documentation.
  if (info & g_class_flag_resolved) {
    const int64_t instance_size = process_sp->ReadSignedIntegerFromMemory(
        m_isa + layout.instance_size_offset, layout.long_size, 0, error);
    if (error.Fail())
      return;
    m_superclass_isa = superclass;
    // objc_resolve_class sets the flag before objc_compute_ivar_offsets runs,
    // and until that finishes instance_size still holds minus the size of
    // this class's own ivars (ivar.c). Neither the size nor the ivar offsets
    // mean anything yet, so treat the class as unresolved until it is
    // positive.
    if (instance_size > 0) {
      m_instance_size = static_cast<uint64_t>(instance_size);
      m_resolved = true;
    }
  }

  m_metaclass_isa = metaclass;
  m_is_meta = is_meta;
  m_name = ConstString(name_buffer);
  m_valid = true;
}

std::vector<GNUstepObjCClassDescriptor::RawIvar>
GNUstepObjCClassDescriptor::ReadIvarList() const {
  std::vector<RawIvar> ivars;
  ProcessSP process_sp = m_process_wp.lock();
  if (!process_sp || !m_valid || m_is_meta)
    return ivars;

  // See the header: the resolved flag is set before the offsets are computed,
  // and a positive instance_size is what says the computation finished.
  if (!m_resolved || m_instance_size == 0)
    return ivars;

  const ClassLayout layout = GetClassLayout(*process_sp);
  const uint32_t ptr_size = layout.pointer_size;

  const addr_t list_addr =
      llvm::expectedToOptional(
          process_sp->ReadPointerFromMemory(m_isa + layout.ivars_offset))
          .value_or(LLDB_INVALID_ADDRESS);
  if (list_addr == 0 || list_addr == LLDB_INVALID_ADDRESS ||
      list_addr % ptr_size != 0)
    return ivars;

  Status error;

  // struct objc_ivar_list { int count; size_t size; struct objc_ivar[]; }
  // (ivar.h). `size` is the element stride and is read rather than assumed,
  // because the runtime uses it to walk the array.
  const uint64_t count_offset = 0;
  const uint64_t size_offset = llvm::alignTo(sizeof(uint32_t), ptr_size);
  const uint64_t entries_offset = size_offset + ptr_size;

  const int64_t count = process_sp->ReadSignedIntegerFromMemory(
      list_addr + count_offset, sizeof(uint32_t), 0, error);
  if (error.Fail() || count <= 0 || count > g_max_ivars)
    return ivars;

  const uint64_t stride = process_sp->ReadUnsignedIntegerFromMemory(
      list_addr + size_offset, ptr_size, 0, error);
  // An element smaller than the struct this code knows how to read would make
  // every field after the first come from the wrong place.
  const uint64_t min_stride = 3 * ptr_size + 2 * sizeof(uint32_t);
  if (error.Fail() || stride < min_stride || stride > g_max_element_stride)
    return ivars;

  Log *log = GetLog(LLDBLog::Types);
  ivars.reserve(count);
  for (int64_t i = 0; i < count; ++i) {
    // struct objc_ivar { const char *name; const char *type; int *offset;
    //                    uint32_t size; uint32_t flags; }
    const addr_t entry = list_addr + entries_offset + i * stride;

    auto read_pointer = [&](addr_t addr) -> addr_t {
      return llvm::expectedToOptional(process_sp->ReadPointerFromMemory(addr))
          .value_or(LLDB_INVALID_ADDRESS);
    };
    const addr_t name_ptr = read_pointer(entry);
    if (name_ptr == 0 || name_ptr == LLDB_INVALID_ADDRESS)
      return {};
    const addr_t type_ptr = read_pointer(entry + ptr_size);
    if (type_ptr == 0 || type_ptr == LLDB_INVALID_ADDRESS)
      return {};
    const addr_t offset_ptr = read_pointer(entry + 2 * ptr_size);
    // libobjc2 stores a pointer to the offset, not the offset itself, so that
    // the runtime can rewrite it in place. Between class_addIvar and
    // objc_registerClassPair (runtime.c) the field holds the offset itself,
    // cast to a pointer; such a class is not yet resolved, which is what
    // keeps that from being read here.
    if (offset_ptr == 0 || offset_ptr == LLDB_INVALID_ADDRESS ||
        offset_ptr % sizeof(int32_t) != 0)
      return {};
    const uint32_t ivar_size = process_sp->ReadUnsignedIntegerFromMemory(
        entry + 3 * ptr_size, sizeof(uint32_t), 0, error);
    if (error.Fail())
      return {};

    // The offset is a 32-bit signed int wherever libobjc2 runs.
    const int64_t offset = process_sp->ReadSignedIntegerFromMemory(
        offset_ptr, sizeof(int32_t), 0, error);
    if (error.Fail())
      return {};

    char name_buffer[g_max_class_name_length];
    const size_t name_length = process_sp->ReadCStringFromMemory(
        name_ptr, name_buffer, sizeof(name_buffer), error);
    if (error.Fail() || name_length == 0 ||
        name_length >= sizeof(name_buffer) - 1)
      return {};

    char type_buffer[g_max_type_encoding_length];
    size_t type_length = process_sp->ReadCStringFromMemory(
        type_ptr, type_buffer, sizeof(type_buffer), error);
    if (error.Fail())
      return {};
    // A deeply nested struct can legitimately have a longer encoding. Keep
    // the ivar with no type rather than dropping it, so it still has a name,
    // an offset and a size.
    if (type_length >= sizeof(type_buffer) - 1)
      type_length = 0;

    // An ivar that does not fit inside the object is evidence that this is
    // not really an ivar list, so discard the whole thing rather than
    // reporting a plausible-looking subset.
    if (offset < 0 ||
        static_cast<uint64_t>(offset) + ivar_size > m_instance_size) {
      LLDB_LOG(log,
               "GNUstep ivar {0} of {1} lies outside the object "
               "(offset {2}, size {3}, instance size {4}); ignoring the list",
               name_buffer, m_name, offset, ivar_size, m_instance_size);
      return {};
    }

    RawIvar ivar;
    ivar.name = ConstString(name_buffer);
    ivar.type_encoding.assign(type_buffer, type_length);
    ivar.offset = static_cast<int32_t>(offset);
    ivar.size = ivar_size;
    ivars.push_back(std::move(ivar));
  }
  return ivars;
}

void GNUstepObjCClassDescriptor::GetIVarInformation() {
  if (m_ivars_filled)
    return;

  std::vector<RawIvar> raw = ReadIvarList();

  // A descriptor is kept in the runtime's ISA map for the life of the
  // process, so an answer latched here is permanent. Latch only a positive
  // one: every failure inside ReadIvarList - a class caught mid-resolution,
  // a list briefly dangling while another thread grows it - also yields an
  // empty vector, and caching that would report "this class has no ivars"
  // for the rest of the session. A class that genuinely declares none is
  // cheap to re-read.
  if (raw.empty())
    return;

  ProcessSP process_sp = m_process_wp.lock();
  ObjCLanguageRuntime::EncodingToTypeSP encoding_to_type_sp;
  if (process_sp) {
    if (ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp))
      encoding_to_type_sp = runtime->GetEncodingToType();
  }

  std::vector<iVarDescriptor> ivars;
  ivars.reserve(raw.size());
  for (const RawIvar &ivar : raw) {
    iVarDescriptor descriptor;
    descriptor.m_name = ivar.name;
    descriptor.m_size = ivar.size;
    descriptor.m_offset = ivar.offset;
    // The name, offset and size are useful on their own, so an encoding that
    // does not realize leaves the ivar in place with an empty type rather
    // than dropping it.
    if (encoding_to_type_sp)
      descriptor.m_type = encoding_to_type_sp->RealizeType(
          ivar.type_encoding.c_str(), /*for_expression=*/false);
    ivars.push_back(std::move(descriptor));
  }
  // Publish only once complete, so that nothing reached from the loop above
  // can observe a partial list.
  m_ivars = std::move(ivars);
  m_ivars_filled = true;
}

size_t GNUstepObjCClassDescriptor::GetNumIVars() {
  std::lock_guard<std::recursive_mutex> guard(m_ivars_mutex);
  GetIVarInformation();
  return m_ivars.size();
}

ObjCLanguageRuntime::ClassDescriptor::iVarDescriptor
GNUstepObjCClassDescriptor::GetIVarAtIndex(size_t idx) {
  std::lock_guard<std::recursive_mutex> guard(m_ivars_mutex);
  if (idx >= GetNumIVars())
    return iVarDescriptor();
  return m_ivars[idx];
}

ObjCLanguageRuntime::ClassDescriptorSP
GNUstepObjCClassDescriptor::GetSuperclass() {
  if (!m_valid || m_superclass_isa == 0)
    return ObjCLanguageRuntime::ClassDescriptorSP();
  // A class that is its own superclass would make any walk up the chain spin
  // forever. The chain comes from inferior memory, so refuse the edge here
  // rather than leaving every caller to remember. Longer cycles cannot be
  // seen from a single descriptor and still need the caller's depth bound.
  if (m_superclass_isa == m_isa)
    return ObjCLanguageRuntime::ClassDescriptorSP();
  ProcessSP process_sp = m_process_wp.lock();
  if (!process_sp)
    return ObjCLanguageRuntime::ClassDescriptorSP();
  // Through the runtime's map where there is one, so that walking the chain
  // reuses the cached descriptors - and the ivars they have already read -
  // rather than parsing every superclass again.
  if (ObjCLanguageRuntime *runtime = ObjCLanguageRuntime::Get(*process_sp))
    return runtime->GetClassDescriptorFromISA(m_superclass_isa);
  return std::make_shared<GNUstepObjCClassDescriptor>(process_sp,
                                                      m_superclass_isa);
}

std::unique_ptr<ObjCLanguageRuntime::ClassDescriptor>
GNUstepObjCClassDescriptor::GetMetaclass() const {
  if (!m_valid || m_metaclass_isa == 0)
    return nullptr;
  ProcessSP process_sp = m_process_wp.lock();
  if (!process_sp)
    return nullptr;
  return std::make_unique<GNUstepObjCClassDescriptor>(process_sp,
                                                      m_metaclass_isa);
}

bool GNUstepObjCTaggedPointerClassDescriptor::GetTaggedPointerInfo(
    uint64_t *info_bits, uint64_t *value_bits, uint64_t *payload) {
  if (info_bits)
    *info_bits = m_tag;
  if (value_bits)
    *value_bits = m_pointer_value >> m_payload_shift;
  if (payload)
    *payload = m_pointer_value;
  return true;
}

bool GNUstepObjCTaggedPointerClassDescriptor::GetTaggedPointerInfoSigned(
    uint64_t *info_bits, int64_t *value_bits, uint64_t *payload) {
  if (info_bits)
    *info_bits = m_tag;
  if (value_bits) {
    // Sign-extend from the target's pointer width before shifting, so that a
    // negative payload in a 32-bit pointer is not read as a large positive.
    // Done through SignExtend64 rather than by hand: shifting a signed value
    // into its own sign bit is undefined.
    const uint32_t pointer_bits = m_pointer_size * 8;
    *value_bits =
        llvm::SignExtend64(m_pointer_value, pointer_bits) >> m_payload_shift;
  }
  if (payload)
    *payload = m_pointer_value;
  return true;
}

bool GNUstepTaggedPointerVendor::IsPossibleTaggedPointer(lldb::addr_t ptr) {
  const uint64_t mask = m_process.GetAddressByteSize() == 8 ? 7 : 1;
  return (ptr & mask) != 0;
}

std::unique_ptr<ObjCLanguageRuntime::ClassDescriptor>
GNUstepTaggedPointerVendor::GetClassDescriptor(lldb::addr_t ptr) {
  const uint32_t pointer_size = m_process.GetAddressByteSize();
  const bool is_64_bit = pointer_size == 8;
  const uint64_t mask = is_64_bit ? 7 : 1;
  const uint32_t payload_shift = is_64_bit ? 3 : 1;
  const uint64_t tag = ptr & mask;
  if (tag == 0)
    return nullptr;

  // Mirror libobjc2's classForObject(): 32-bit targets have a single small
  // object class at index 0; 64-bit targets index the table by the tag.
  // `SmallObjectClasses` has 7 entries (class_table.c), so a tag of 7 has no
  // corresponding class.
  const uint64_t index = is_64_bit ? tag : 0;
  if (index > 6)
    return nullptr;

  if (!m_table_addr) {
    m_table_addr = LLDB_INVALID_ADDRESS;
    Target &target = m_process.GetTarget();
    SymbolContextList sc_list;
    target.GetImages().FindSymbolsWithNameAndType(
        ConstString("SmallObjectClasses"), eSymbolTypeAny, sc_list);
    for (const SymbolContext &sc : sc_list) {
      if (!sc.symbol)
        continue;
      const addr_t table = sc.symbol->GetAddress().GetLoadAddress(&target);
      if (table != LLDB_INVALID_ADDRESS) {
        m_table_addr = table;
        break;
      }
    }
    // The table has hidden visibility, so a linked image carries no symbol
    // for it unless a PDB or an unstripped symtab is around; the debug info
    // still describes it as a global, which is enough to find its address.
    if (*m_table_addr == LLDB_INVALID_ADDRESS) {
      VariableList variables;
      target.GetImages().FindGlobalVariables(ConstString("SmallObjectClasses"),
                                             1, variables);
      if (VariableSP variable_sp = variables.GetVariableAtIndex(0)) {
        ValueObjectSP valobj_sp =
            ValueObjectVariable::Create(&target, variable_sp);
        if (valobj_sp) {
          const addr_t table = valobj_sp->GetAddressOf(false).address;
          if (table != 0 && table != LLDB_INVALID_ADDRESS)
            m_table_addr = table;
        }
      }
    }
    if (*m_table_addr == LLDB_INVALID_ADDRESS)
      LLDB_LOG(GetLog(LLDBLog::Language),
               "GNUstepTaggedPointerVendor: SmallObjectClasses not found in "
               "any symbol table or debug info (stripped libobjc?); tagged "
               "pointer classes unknown");
  }
  if (*m_table_addr == LLDB_INVALID_ADDRESS)
    return nullptr;

  const addr_t isa =
      llvm::expectedToOptional(
          m_process.ReadPointerFromMemory(*m_table_addr + index * pointer_size))
          .value_or(LLDB_INVALID_ADDRESS);
  if (isa == 0 || isa == LLDB_INVALID_ADDRESS)
    return nullptr;

  auto descriptor_up =
      std::make_unique<GNUstepObjCTaggedPointerClassDescriptor>(
          m_process.shared_from_this(), isa, ptr, tag, payload_shift,
          pointer_size);
  if (!descriptor_up->IsValid())
    return nullptr;
  return descriptor_up;
}
