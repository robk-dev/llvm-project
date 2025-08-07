//===-- GNUstepArraySummaryProviderSimple.cpp ------------------*- C++ -*-===//
//
// Simple array summary using expression evaluation
//
//===----------------------------------------------------------------------===//

#include "GNUstepArraySummaryProvider.h"
#include "GNUstepUtilities.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"

using namespace lldb;
using namespace lldb_private;

namespace lldb_private {
namespace formatters {

bool GNUstepArraySummaryProvider(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  // Get object address directly
  addr_t array_ptr = valobj.GetValueAsUnsigned(0);
  if (array_ptr == 0 || array_ptr == LLDB_INVALID_ADDRESS) {
    stream.Printf("nil");
    return true;
  }
  
  ProcessSP process_sp = valobj.GetProcessSP();
  if (!process_sp)
    return false;
  
  // Try to read count at common offset 16 (discovered for GSArray)
  Status error;
  uint32_t count = process_sp->ReadUnsignedIntegerFromMemory(array_ptr + 16, 4, 0, error);
  
  if (!error.Fail() && count < 10000) {
    stream.Printf("@[%u %s]", count, count == 1 ? "object" : "objects");
  } else {
    stream.Printf("@[...]");
  }
  
  return true;
}

} // namespace formatters
} // namespace lldb_private