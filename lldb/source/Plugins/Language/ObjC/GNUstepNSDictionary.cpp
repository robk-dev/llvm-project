//===-- GNUstepNSDictionary.cpp -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// gnustep-base's GSDictionary, GSSet and GSCountedSet all embed a
// `GSIMapTable_t map` (Headers/GNUstepBase/GSIMap.h): `bucketCount` buckets,
// each `{ nodeCount, firstNode }`, chaining nodes `{ nextInBucket, key
// [, value] }` through nextInBucket. Sets instantiate the map without the
// value member, so the layout is taken from the debug-info types rather than
// assumed.
//
//===----------------------------------------------------------------------===//

#include "GNUstepFormatters.h"
#include "NSDictionary.h"

#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/DataFormatters/TypeSynthetic.h"
#include "lldb/Target/Language.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"
#include "lldb/ValueObject/ValueObject.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"

#include <vector>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace {

struct MapEntry {
  addr_t key = 0;
  addr_t value = 0;
};

struct Field {
  uint64_t offset;
  CompilerType type;
};

/// Field \p name of \p type, if it has one.
std::optional<Field> FindField(const CompilerType &type, llvm::StringRef name) {
  const uint32_t num_fields = type.GetNumFields();
  for (uint32_t i = 0; i < num_fields; ++i) {
    std::string field_name;
    uint64_t bit_offset = 0;
    CompilerType field_type =
        type.GetFieldAtIndex(i, field_name, &bit_offset, nullptr, nullptr);
    if (field_name == name)
      return Field{bit_offset / 8, field_type};
  }
  return std::nullopt;
}

/// Reads just the element count.
std::optional<uint64_t> ReadNodeCount(ValueObject &valobj) {
  ValueObjectSP map_sp = GNUstepGetIvar(valobj, "map");
  if (!map_sp)
    return std::nullopt;
  ValueObjectSP count_sp = map_sp->GetChildMemberWithName("nodeCount");
  if (!count_sp)
    return std::nullopt;
  return count_sp->GetValueAsUnsigned(0);
}

/// Walks the map's buckets on demand, in bucket order (which is hash order,
/// like -objectEnumerator), so that showing the first few children of a large
/// table reads only as much of it as those children need. The layout comes
/// from the table's debug-info types, because sets instantiate GSIMap without
/// the value member (GSSet.m sets GSI_MAP_HAS_VALUE to 0); the contents are
/// read as raw memory.
class MapWalker {
public:
  /// Reads the table header. Returns false if \p valobj has no readable map.
  bool Init(ValueObject &valobj, bool want_value) {
    *this = MapWalker();
    ValueObjectSP map_sp = GNUstepGetIvar(valobj, "map");
    if (!map_sp)
      return false;
    ValueObjectSP count_sp = map_sp->GetChildMemberWithName("nodeCount");
    ValueObjectSP bucket_count_sp =
        map_sp->GetChildMemberWithName("bucketCount");
    ValueObjectSP buckets_sp = map_sp->GetChildMemberWithName("buckets");
    if (!count_sp || !bucket_count_sp || !buckets_sp)
      return false;
    m_process_sp = valobj.GetProcessSP();
    if (!m_process_sp)
      return false;
    m_ptr_size = m_process_sp->GetAddressByteSize();
    m_node_count = count_sp->GetValueAsUnsigned(0);
    m_bucket_count = bucket_count_sp->GetValueAsUnsigned(0);
    m_buckets = buckets_sp->GetValueAsUnsigned(0);
    if (m_node_count == 0)
      return true;
    // The counts are inferior data, so a garbage header must not turn into
    // a walk over gigabytes. Four million buckets is a table far larger
    // than anything a debugger user expands.
    constexpr uint64_t g_max_buckets = 1ULL << 22;
    if (m_bucket_count == 0 || m_bucket_count > g_max_buckets ||
        m_node_count > g_max_buckets || m_buckets == 0)
      return false;

    // GSIBucket is { nodeCount, firstNode }; a node is { nextInBucket, key
    // [, value] }, where the key and value are unions of pointer-sized
    // members.
    const CompilerType bucket_type =
        buckets_sp->GetCompilerType().GetPointeeType();
    llvm::Expected<uint64_t> stride = bucket_type.GetByteSize(nullptr);
    if (!stride) {
      llvm::consumeError(stride.takeError());
      return false;
    }
    m_bucket_stride = *stride;
    std::optional<Field> first_node = FindField(bucket_type, "firstNode");
    if (!first_node || m_bucket_stride < first_node->offset + m_ptr_size)
      return false;
    const CompilerType node_type = first_node->type.GetPointeeType();
    std::optional<Field> next = FindField(node_type, "nextInBucket");
    std::optional<Field> key = FindField(node_type, "key");
    std::optional<Field> value =
        want_value ? FindField(node_type, "value") : std::nullopt;
    if (!next || !key || (want_value && !value))
      return false;
    m_first_node_offset = first_node->offset;
    m_next_offset = next->offset;
    m_key_offset = key->offset;
    m_value_offset = want_value ? value->offset : 0;
    m_want_value = want_value;
    return true;
  }

  uint64_t GetNodeCount() const { return m_node_count; }

  /// Returns entry \p idx, reading as many more buckets as that takes.
  std::optional<MapEntry> GetEntry(size_t idx) {
    while (m_entries.size() <= idx && !m_done)
      ReadNextChunk();
    if (idx < m_entries.size())
      return m_entries[idx];
    return std::nullopt;
  }

private:
  addr_t ReadPointer(addr_t addr) {
    return llvm::expectedToOptional(m_process_sp->ReadPointerFromMemory(addr))
        .value_or(0);
  }

  /// Reads the next run of buckets and follows every chain in it. Bounded by
  /// the bucket count, by nodeCount, and by never revisiting a node.
  void ReadNextChunk() {
    constexpr uint64_t g_buckets_per_read = 256;
    if (m_next_bucket >= m_bucket_count || m_entries.size() >= m_node_count) {
      m_done = true;
      return;
    }
    const uint64_t num_buckets =
        std::min(g_buckets_per_read, m_bucket_count - m_next_bucket);
    std::vector<uint8_t> buffer(num_buckets * m_bucket_stride);
    Status error;
    const addr_t base = m_buckets + m_next_bucket * m_bucket_stride;
    if (m_process_sp->ReadMemory(base, buffer.data(), buffer.size(), error) !=
        buffer.size()) {
      m_done = true;
      return;
    }
    m_next_bucket += num_buckets;
    DataExtractor data(buffer.data(), buffer.size(),
                       m_process_sp->GetByteOrder(), m_ptr_size);
    for (uint64_t b = 0; b < num_buckets; ++b) {
      lldb::offset_t offset = b * m_bucket_stride + m_first_node_offset;
      addr_t node = data.GetAddress(&offset);
      while (node != 0 && m_entries.size() < m_node_count) {
        if (!m_seen.insert(node).second) {
          m_done = true; // a cycle: the table is corrupt or being mutated
          return;
        }
        MapEntry entry;
        entry.key = ReadPointer(node + m_key_offset);
        if (m_want_value)
          entry.value = ReadPointer(node + m_value_offset);
        m_entries.push_back(entry);
        node = ReadPointer(node + m_next_offset);
      }
    }
  }

  ProcessSP m_process_sp;
  uint32_t m_ptr_size = 8;
  uint64_t m_node_count = 0;
  uint64_t m_bucket_count = 0;
  addr_t m_buckets = 0;
  uint64_t m_bucket_stride = 0;
  uint64_t m_first_node_offset = 0;
  uint64_t m_next_offset = 0;
  uint64_t m_key_offset = 0;
  uint64_t m_value_offset = 0;
  bool m_want_value = false;

  uint64_t m_next_bucket = 0;
  bool m_done = false;
  std::vector<MapEntry> m_entries;
  llvm::DenseSet<addr_t> m_seen;
};

/// Presents each entry as `[i] = { key, value }` (dictionaries) or as the
/// key object itself (sets).
class GNUstepMapSyntheticFrontEnd : public SyntheticChildrenFrontEnd {
public:
  GNUstepMapSyntheticFrontEnd(ValueObjectSP valobj_sp, bool is_dictionary)
      : SyntheticChildrenFrontEnd(*valobj_sp), m_is_dictionary(is_dictionary) {
    if (valobj_sp) {
      m_exe_ctx_ref = valobj_sp->GetExecutionContextRef();
      if (ProcessSP process_sp = valobj_sp->GetProcessSP()) {
        m_ptr_size = process_sp->GetAddressByteSize();
        m_order = process_sp->GetByteOrder();
      }
      if (TargetSP target_sp = valobj_sp->GetTargetSP()) {
        if (TypeSystemClangSP scratch_ts_sp =
                ScratchTypeSystemClang::GetForTarget(*target_sp))
          m_id_type = scratch_ts_sp->GetBasicType(eBasicTypeObjCID);
        if (m_is_dictionary)
          m_pair_type = GetLLDBNSPairType(target_sp);
      }
    }
  }

  llvm::Expected<uint32_t> CalculateNumChildren() override {
    return m_num_children;
  }

  ValueObjectSP GetChildAtIndex(uint32_t idx) override {
    if (idx >= m_num_children)
      return {};
    if (auto it = m_children.find(idx); it != m_children.end())
      return it->second;
    std::optional<MapEntry> maybe_entry = m_walker.GetEntry(idx);
    if (!maybe_entry)
      return {};
    const MapEntry &entry = *maybe_entry;
    StreamString name;
    name.Printf("[%u]", idx);
    if (!m_is_dictionary) {
      // A set element is the key object itself.
      m_children[idx] =
          CreateChildFromWords(name.GetString(), {entry.key}, m_id_type);
      return m_children[idx];
    }
    if (!m_pair_type.IsValid())
      return {};
    m_children[idx] = CreateChildFromWords(
        name.GetString(), {entry.key, entry.value}, m_pair_type);
    return m_children[idx];
  }

  lldb::ChildCacheState Update() override {
    m_children.clear();
    m_num_children = 0;
    if (m_walker.Init(m_backend, m_is_dictionary))
      m_num_children = m_walker.GetNodeCount();
    return lldb::ChildCacheState::eRefetch;
  }

  llvm::Expected<size_t> GetIndexOfChildWithName(ConstString name) override {
    if (std::optional<size_t> idx = ExtractIndexFromString(name.GetCString()))
      if (*idx < m_num_children)
        return *idx;
    return llvm::createStringError("Type has no child named '%s'",
                                   name.AsCString(""));
  }

private:
  ValueObjectSP CreateChildFromWords(llvm::StringRef name,
                                     std::initializer_list<addr_t> words,
                                     CompilerType type) {
    WritableDataBufferSP buffer_sp(
        new DataBufferHeap(words.size() * m_ptr_size, 0));
    uint8_t *bytes = buffer_sp->GetBytes();
    for (addr_t word : words) {
      if (m_ptr_size == 8)
        memcpy(bytes, &word, 8);
      else {
        uint32_t narrow = static_cast<uint32_t>(word);
        memcpy(bytes, &narrow, 4);
      }
      bytes += m_ptr_size;
    }
    DataExtractor data(buffer_sp, m_order, m_ptr_size);
    return CreateChildValueObjectFromData(name, data, m_exe_ctx_ref, type);
  }

  bool m_is_dictionary;
  ExecutionContextRef m_exe_ctx_ref;
  uint8_t m_ptr_size = 8;
  lldb::ByteOrder m_order = lldb::eByteOrderLittle;
  CompilerType m_id_type;
  CompilerType m_pair_type;
  MapWalker m_walker;
  uint32_t m_num_children = 0;
  llvm::DenseMap<uint32_t, ValueObjectSP> m_children;
};

} // namespace

bool lldb_private::formatters::GNUstepNSDictionarySummaryProvider(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  if (!IsGNUstepObjCRuntime(valobj))
    return false;
  std::optional<uint64_t> count = ReadNodeCount(valobj);
  if (!count)
    return false;
  stream.Printf("%" PRIu64 " key/value pair%s", *count, *count == 1 ? "" : "s");
  return true;
}

bool lldb_private::formatters::GNUstepNSSetSummaryProvider(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  if (!IsGNUstepObjCRuntime(valobj))
    return false;
  std::optional<uint64_t> count = ReadNodeCount(valobj);
  if (!count)
    return false;
  stream.Printf("%" PRIu64 " element%s", *count, *count == 1 ? "" : "s");
  return true;
}

SyntheticChildrenFrontEnd *
lldb_private::formatters::GNUstepNSDictionarySyntheticFrontEndCreator(
    CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp) {
  if (!valobj_sp || !IsGNUstepObjCRuntime(*valobj_sp))
    return nullptr;
  return new GNUstepMapSyntheticFrontEnd(valobj_sp, /*is_dictionary=*/true);
}

SyntheticChildrenFrontEnd *
lldb_private::formatters::GNUstepNSSetSyntheticFrontEndCreator(
    CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp) {
  if (!valobj_sp || !IsGNUstepObjCRuntime(*valobj_sp))
    return nullptr;
  return new GNUstepMapSyntheticFrontEnd(valobj_sp, /*is_dictionary=*/false);
}
