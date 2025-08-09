//===-- GNUstepIndexSetFormatters.cpp -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepIndexSetFormatters.h"
#include "GNUstepFormattersBase.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"
#include <algorithm>
#include <cstring>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace {

/// Helper class to format NSIndexSet objects
class GNUstepNSIndexSetSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  struct IndexRange {
    uint64_t location;
    uint64_t length;
  };
  
  /// Extract the index ranges from the NSIndexSet
  std::vector<IndexRange> ExtractIndexRanges(ValueObject &valobj);
  
  /// Check if ranges are contiguous
  bool AreRangesContiguous(const std::vector<IndexRange> &ranges, uint64_t &totalCount);
  
  /// Format contiguous ranges
  void FormatContiguousRanges(Stream &stream, const std::vector<IndexRange> &ranges, uint64_t totalCount);
  
  /// Format scattered indexes
  void FormatScatteredIndexes(Stream &stream, uint64_t totalCount);
};

bool GNUstepNSIndexSetSummaryProvider::FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid NSIndexSet");
    return false;
  }

  addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    stream.Printf("(null)");
    return true;
  }

  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    WriteErrorSummary(stream, "no process");
    return false;
  }

  // Extract the index ranges from the NSIndexSet
  std::vector<IndexRange> ranges = ExtractIndexRanges(valobj);
  
  // Calculate total count
  uint64_t totalCount = 0;
  for (const auto &range : ranges) {
    totalCount += range.length;
  }

  // Handle empty set
  if (totalCount == 0) {
    stream.Printf("0 indexes");
    return true;
  }

  // Handle single index
  if (totalCount == 1 && ranges.size() == 1 && ranges[0].length == 1) {
    stream.Printf("1 index: %llu", (unsigned long long)ranges[0].location);
    return true;
  }

  // Check if ranges form a single contiguous block
  uint64_t contiguousTotal = 0;
  if (ranges.size() == 1) {
    // Single range - always contiguous
    const auto &range = ranges[0];
    stream.Printf("%llu indexes in [%llu-%llu]", 
                  (unsigned long long)range.length,
                  (unsigned long long)range.location,
                  (unsigned long long)(range.location + range.length - 1));
    return true;
  } else if (AreRangesContiguous(ranges, contiguousTotal) && contiguousTotal == totalCount) {
    // Multiple ranges but they form one contiguous block
    FormatContiguousRanges(stream, ranges, totalCount);
    return true;
  } else {
    // Scattered indexes
    FormatScatteredIndexes(stream, totalCount);
    return true;
  }
}

std::vector<GNUstepNSIndexSetSummaryProvider::IndexRange> 
GNUstepNSIndexSetSummaryProvider::ExtractIndexRanges(ValueObject &valobj) {
  std::vector<IndexRange> ranges;
  
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return ranges;
  }

  addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return ranges;
  }

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  Status error;

  // GNUstep NSIndexSet structure:
  // struct NSIndexSet {
  //   void *isa;          // offset 0 (8 bytes)
  //   void *_data;        // offset 8 (8 bytes) -> points to GSIArray_t
  // }
  //
  // struct GSIArray_t {   // This is what _data points to
  //   GSIArrayItem *ptr;  // offset 0 (8 bytes) -> points to NSRange array
  //   unsigned count;     // offset 8 (4 bytes) -> number of ranges
  //   unsigned cap;       // offset 12 (4 bytes) -> capacity
  //   unsigned old;       // offset 16 (4 bytes) -> old value
  //   NSZone *zone;       // offset 20/24 (8 bytes)
  // }
  
  // Read the _data pointer (GSIArray*) at offset 8
  addr_t data_ptr_addr = obj_addr + addr_size;
  addr_t gsi_array_addr = GNUstepRuntimeHelper::ReadPointer(process, data_ptr_addr, error);
  if (error.Fail() || gsi_array_addr == 0 || gsi_array_addr == LLDB_INVALID_ADDRESS) {
    return ranges; // Empty set or read error
  }

  // Read the GSIArray_t structure
  // ptr field at offset 0
  addr_t nsrange_array_addr = GNUstepRuntimeHelper::ReadPointer(process, gsi_array_addr, error);
  if (error.Fail() || nsrange_array_addr == 0 || nsrange_array_addr == LLDB_INVALID_ADDRESS) {
    return ranges;
  }

  // count field at offset 8 (unsigned, 4 bytes)
  uint32_t range_count = process->ReadUnsignedIntegerFromMemory(gsi_array_addr + 8, 4, 0, error);
  if (error.Fail()) {
    return ranges;
  }

  // Limit to reasonable number of ranges to avoid memory issues
  if (range_count > 1000) {
    return ranges;
  }

  // Read each NSRange from the array
  for (uint32_t i = 0; i < range_count; i++) {
    addr_t range_addr = nsrange_array_addr + (i * 16); // Each NSRange is 16 bytes
    
    // Read NSRange.location (8 bytes)
    uint64_t location = process->ReadUnsignedIntegerFromMemory(range_addr, 8, 0, error);
    if (error.Fail()) {
      break;
    }
    
    // Read NSRange.length (8 bytes)
    uint64_t length = process->ReadUnsignedIntegerFromMemory(range_addr + 8, 8, 0, error);
    if (error.Fail()) {
      break;
    }
    
    IndexRange range;
    range.location = location;
    range.length = length;
    ranges.push_back(range);
  }

  return ranges;
}

bool GNUstepNSIndexSetSummaryProvider::AreRangesContiguous(
    const std::vector<IndexRange> &ranges, uint64_t &totalCount) {
  if (ranges.empty()) {
    totalCount = 0;
    return true;
  }

  // Sort ranges by location for contiguity check
  std::vector<IndexRange> sortedRanges = ranges;
  std::sort(sortedRanges.begin(), sortedRanges.end(),
            [](const IndexRange &a, const IndexRange &b) {
              return a.location < b.location;
            });

  totalCount = 0;
  uint64_t expectedNext = sortedRanges[0].location;
  
  for (const auto &range : sortedRanges) {
    if (range.location != expectedNext) {
      return false; // Gap found
    }
    totalCount += range.length;
    expectedNext = range.location + range.length;
  }

  return true;
}

void GNUstepNSIndexSetSummaryProvider::FormatContiguousRanges(
    Stream &stream, const std::vector<IndexRange> &ranges, uint64_t totalCount) {
  // Find the overall range
  uint64_t minLocation = UINT64_MAX;
  uint64_t maxEnd = 0;
  
  for (const auto &range : ranges) {
    minLocation = std::min(minLocation, range.location);
    maxEnd = std::max(maxEnd, range.location + range.length);
  }

  stream.Printf("%llu indexes in [%llu-%llu]",
                (unsigned long long)totalCount,
                (unsigned long long)minLocation,
                (unsigned long long)(maxEnd - 1));
}

void GNUstepNSIndexSetSummaryProvider::FormatScatteredIndexes(Stream &stream, uint64_t totalCount) {
  stream.Printf("%llu indexes", (unsigned long long)totalCount);
}

} // anonymous namespace

// Public API implementations  
namespace lldb_private {
namespace formatters {

bool GNUstepNSIndexSetFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  GNUstepNSIndexSetSummaryProvider provider;
  return provider.FormatObject(valobj, stream, options);
}

bool GNUstepNSMutableIndexSetFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  // NSMutableIndexSet uses the same internal structure as NSIndexSet
  return GNUstepNSIndexSetFormatterFunction(valobj, stream, options);
}

} // namespace formatters
} // namespace lldb_private