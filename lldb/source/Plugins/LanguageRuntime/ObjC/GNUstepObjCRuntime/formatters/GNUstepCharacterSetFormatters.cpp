//===-- GNUstepCharacterSetFormatters.cpp --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepCharacterSetFormatters.h"
#include "GNUstepFormattersBase.h"
#include "../GNUstepObjCRuntimeIntrospector.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"
#include <unordered_map>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace {

/// Helper class to format NSCharacterSet objects
class GNUstepNSCharacterSetSummaryProvider : public GNUstepSummaryProvider {
public:
  bool FormatObject(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) override;

private:
  /// Map of standard character set identifiers to human-readable names
  static const std::unordered_map<uint32_t, std::string> kStandardCharacterSets;
  
  /// Extract character set information
  struct CharacterSetInfo {
    bool isStandardSet;
    bool isInverted;
    uint32_t standardSetId;
    std::vector<uint8_t> bitmap;
    size_t characterCount;
    std::string sampleCharacters;
  };
  
  /// Extract character set information from the object
  bool ExtractCharacterSetInfo(ValueObject &valobj, CharacterSetInfo &info);
  
  /// Get the name of a standard character set
  std::string GetStandardSetName(uint32_t setId, bool isInverted);
  
  /// Count characters in bitmap
  size_t CountCharactersInBitmap(const std::vector<uint8_t> &bitmap);
  
  /// Extract sample characters from bitmap
  std::string ExtractSampleCharacters(const std::vector<uint8_t> &bitmap, size_t maxSamples = 10);
  
  /// Check if character set appears to be a standard set by comparing bitmaps
  bool IdentifyStandardSet(const std::vector<uint8_t> &bitmap, uint32_t &setId);
};

// Standard character set names mapping based on GNUstep NSCharacterSet.m cache indices
const std::unordered_map<uint32_t, std::string> 
GNUstepNSCharacterSetSummaryProvider::kStandardCharacterSets = {
  {0, "Alphanumerics"},
  {1, "Control Characters"},
  {2, "Decimal Digits"},
  {3, "Decomposables"},
  {4, "Illegal Characters"},
  {5, "Letters"},
  {6, "Lowercase Letters"},
  {7, "Non-Base Characters"},
  {8, "Punctuation"},
  {9, "Symbols"},
  {10, "Uppercase Letters"},
  {11, "Whitespace and Newlines"},
  {12, "Whitespace"},
  {13, "Capitalized Letters"},
  {14, "Newline Characters"},
  {15, "URL Fragment Allowed"},
  {16, "URL Password Allowed"},
  {17, "URL Path Allowed"},
  {18, "URL Query Allowed"},
  {19, "URL User Allowed"},
  {20, "URL Host Allowed"}
};

bool GNUstepNSCharacterSetSummaryProvider::FormatObject(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  
  if (!GNUstepRuntimeHelper::IsValidGNUstepObject(valobj)) {
    WriteErrorSummary(stream, "invalid NSCharacterSet");
    return false;
  }

  addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    stream.Printf("(null)");
    return true;
  }

  // Extract character set information
  CharacterSetInfo info;
  if (!ExtractCharacterSetInfo(valobj, info)) {
    WriteErrorSummary(stream, "could not extract character set data");
    return false;
  }

  // Handle empty character set
  if (info.characterCount == 0) {
    stream.Printf("Empty character set");
    return true;
  }

  // Handle standard character sets
  if (info.isStandardSet) {
    std::string name = GetStandardSetName(info.standardSetId, info.isInverted);
    stream.Printf("%s", name.c_str());
    return true;
  }

  // Handle custom character sets
  if (info.characterCount == 1) {
    stream.Printf("1 character: %s", info.sampleCharacters.c_str());
  } else if (info.characterCount <= 10) {
    stream.Printf("%zu characters: %s", info.characterCount, info.sampleCharacters.c_str());
  } else if (info.characterCount <= 50 && !info.sampleCharacters.empty()) {
    stream.Printf("%zu characters: %s...", info.characterCount, info.sampleCharacters.c_str());
  } else {
    stream.Printf("%zu characters", info.characterCount);
  }

  if (info.isInverted && !info.isStandardSet) {
    stream.Printf(" (inverted)");
  }

  return true;
}

bool GNUstepNSCharacterSetSummaryProvider::ExtractCharacterSetInfo(
    ValueObject &valobj, CharacterSetInfo &info) {
  
  Process *process = GNUstepRuntimeHelper::GetProcessFromValueObject(valobj);
  if (!process) {
    return false;
  }

  addr_t obj_addr = valobj.GetPointerValue();
  if (obj_addr == 0 || obj_addr == LLDB_INVALID_ADDRESS) {
    return false;
  }

  uint32_t addr_size = GNUstepRuntimeHelper::GetAddressByteSize(process);
  Status error;

  // Initialize info structure
  info.isStandardSet = false;
  info.isInverted = false;
  info.standardSetId = 0;
  info.characterCount = 0;
  info.sampleCharacters.clear();

  // Get class name to identify GNUstep character set type
  std::string class_name = GNUstepRuntimeHelper::GetGNUstepClassName(valobj);
  
  // Check for _GSStaticCharSet (standard character sets)
  if (class_name.find("_GSStaticCharSet") != std::string::npos) {
    // _GSStaticCharSet has two possible layouts depending on GNUSTEP_INDEX_CHARSET:
    // 1. With GNUSTEP_INDEX_CHARSET: inherits from _GSIndexCharSet
    //    Layout: isa + indexes (NSMutableIndexSet*) + _index (int)
    // 2. Without GNUSTEP_INDEX_CHARSET: inherits from NSCharacterSet  
    //    Layout: isa + _data + _length + _obj + _known + _present + _index
    
    // Based on memory inspection, the _index field is at offset 0x28 (40 bytes)
    // This corresponds to: isa (8) + indexes pointer (8) + padding/other fields (24)
    addr_t index_addr = obj_addr + 0x28; // Offset 40 bytes from start
    
    int32_t index = -1;
    index = process->ReadSignedIntegerFromMemory(index_addr, 4, -1, error);
    
    if (error.Success() && index >= 0 && index <= 30) {
      info.standardSetId = static_cast<uint32_t>(index);
      
      if (kStandardCharacterSets.find(info.standardSetId) != kStandardCharacterSets.end()) {
        info.isStandardSet = true;
        info.characterCount = 1; // Placeholder for non-empty
        return true;
      }
    }
  }
  
  // Also check _GSIndexCharSet separately
  if (class_name.find("_GSIndexCharSet") != std::string::npos) {
    // _GSIndexCharSet has: isa + indexes (NSMutableIndexSet*)
    // We could potentially read the indexes and analyze them
    info.characterCount = 1; // Placeholder
    info.sampleCharacters = "<index-based set>";
    return true;
  }
  
  // Handle NSBitmapCharSet and NSMutableBitmapCharSet
  if (class_name.find("BitmapCharSet") != std::string::npos) {
    
    // Layout: isa + _data + _length + _obj + _known + _present
    addr_t data_ptr_addr = obj_addr + addr_size;  // _data field
    addr_t length_addr = obj_addr + addr_size + addr_size;  // _length field
    
    addr_t data_ptr = 0;
    uint32_t length = 0;
    
    // Read _data pointer and _length
    if (process->ReadUnsignedIntegerFromMemory(data_ptr_addr, addr_size, 0, error) && error.Success()) {
      data_ptr = process->ReadUnsignedIntegerFromMemory(data_ptr_addr, addr_size, 0, error);
    }
    
    if (process->ReadUnsignedIntegerFromMemory(length_addr, 4, 0, error) && error.Success()) {
      length = process->ReadUnsignedIntegerFromMemory(length_addr, 4, 0, error);
    }
    
    if (error.Fail() || data_ptr == 0 || data_ptr == LLDB_INVALID_ADDRESS) {
      info.characterCount = 0;
      info.sampleCharacters = "<invalid>";
      return true;
    }
    
    // Handle empty character set
    if (length == 0) {
      info.characterCount = 0;
      return true;
    }
    
    // Read a sample of the bitmap (limit to prevent excessive memory reads)
    size_t sample_size = std::min(static_cast<size_t>(length), static_cast<size_t>(1024));
    info.bitmap.resize(sample_size);
    
    size_t bytes_read = process->ReadMemory(data_ptr, info.bitmap.data(), sample_size, error);
    if (error.Fail() || bytes_read == 0) {
      info.characterCount = 0;
      info.sampleCharacters = "<read error>";
      return true;
    }
    
    // Count characters and extract samples from the bitmap
    info.characterCount = CountCharactersInBitmap(info.bitmap);
    info.sampleCharacters = ExtractSampleCharacters(info.bitmap);
    
    // Try to identify if this matches a standard set pattern
    if (IdentifyStandardSet(info.bitmap, info.standardSetId)) {
      info.isStandardSet = true;
    }
    
    return true;
  }
  
  // Handle inverted character sets
  if (class_name.find("Inverted") != std::string::npos) {
    info.isInverted = true;
    // For inverted sets, we'd need to check the original set
    info.characterCount = 1; // Placeholder
    info.sampleCharacters = "<inverted>";
    return true;
  }
  
  // Fallback for unknown types
  info.characterCount = 0;
  info.sampleCharacters = "<unknown type>";
  return true;
}

std::string GNUstepNSCharacterSetSummaryProvider::GetStandardSetName(uint32_t setId, bool isInverted) {
  auto it = kStandardCharacterSets.find(setId);
  if (it == kStandardCharacterSets.end()) {
    return isInverted ? "Inverted Unknown Standard Set" : "Unknown Standard Set";
  }
  
  if (isInverted) {
    return "Inverted " + it->second;
  } else {
    return it->second;
  }
}

size_t GNUstepNSCharacterSetSummaryProvider::CountCharactersInBitmap(const std::vector<uint8_t> &bitmap) {
  size_t count = 0;
  for (uint8_t byte : bitmap) {
    // Count set bits in each byte
    uint8_t temp = byte;
    while (temp) {
      count += temp & 1;
      temp >>= 1;
    }
  }
  return count;
}

std::string GNUstepNSCharacterSetSummaryProvider::ExtractSampleCharacters(
    const std::vector<uint8_t> &bitmap, size_t maxSamples) {
  
  if (bitmap.empty()) {
    return "";
  }
  
  std::string samples;
  size_t samplesFound = 0;
  
  // First pass: collect printable ASCII characters
  for (size_t byteIndex = 0; byteIndex < bitmap.size() && samplesFound < maxSamples; byteIndex++) {
    uint8_t byte = bitmap[byteIndex];
    if (byte == 0) continue;
    
    for (int bitIndex = 0; bitIndex < 8 && samplesFound < maxSamples; bitIndex++) {
      if ((byte & (1 << bitIndex)) != 0) {
        uint32_t character = (byteIndex * 8) + bitIndex;
        
        // Prioritize readable characters
        if (character >= 32 && character <= 126) {
          samples += static_cast<char>(character);
          samplesFound++;
        }
      }
    }
  }
  
  // If we didn't find enough printable characters, add control chars with notation
  if (samplesFound < maxSamples && samplesFound < 3) {
    for (size_t byteIndex = 0; byteIndex < bitmap.size() && samplesFound < maxSamples; byteIndex++) {
      uint8_t byte = bitmap[byteIndex];
      if (byte == 0) continue;
      
      for (int bitIndex = 0; bitIndex < 8 && samplesFound < maxSamples; bitIndex++) {
        if ((byte & (1 << bitIndex)) != 0) {
          uint32_t character = (byteIndex * 8) + bitIndex;
          
          if (character < 32) {
            // Control character - show compact hex notation
            if (character == '\n') {
              samples += "\\n";
            } else if (character == '\t') {
              samples += "\\t";
            } else if (character == '\r') {
              samples += "\\r";
            } else {
              char hex[8];
              snprintf(hex, sizeof(hex), "\\x%02x", character);
              samples += hex;
            }
            samplesFound++;
          }
        }
      }
    }
  }
  
  return samples;
}

bool GNUstepNSCharacterSetSummaryProvider::IdentifyStandardSet(
    const std::vector<uint8_t> &bitmap, uint32_t &setId) {
  
  if (bitmap.empty()) {
    return false;
  }
  
  // Enhanced heuristics to identify common standard sets
  size_t digitCount = 0;
  size_t uppercaseCount = 0;
  size_t lowercaseCount = 0;
  size_t whitespaceCount = 0;
  size_t totalCount = 0;
  
  // Count total characters in ASCII range
  for (size_t i = 0; i < std::min(bitmap.size(), static_cast<size_t>(128/8)); i++) {
    uint8_t byte = bitmap[i];
    for (int bit = 0; bit < 8; bit++) {
      if ((byte & (1 << bit)) != 0) {
        uint32_t character = (i * 8) + bit;
        totalCount++;
        
        if (character >= '0' && character <= '9') {
          digitCount++;
        } else if (character >= 'A' && character <= 'Z') {
          uppercaseCount++;
        } else if (character >= 'a' && character <= 'z') {
          lowercaseCount++;
        } else if (character == ' ' || character == '\t' || 
                   character == '\n' || character == '\r' || character == '\f' || character == '\v') {
          whitespaceCount++;
        }
      }
    }
  }
  
  // More precise identification logic based on exact counts
  if (digitCount == 10 && totalCount == 10) {
    setId = 2; // Decimal Digits
    return true;
  } else if (uppercaseCount == 26 && totalCount == 26) {
    setId = 10; // Uppercase Letters
    return true;
  } else if (lowercaseCount == 26 && totalCount == 26) {
    setId = 6; // Lowercase Letters
    return true;
  } else if (uppercaseCount == 26 && lowercaseCount == 26 && totalCount == 52) {
    setId = 5; // Letters
    return true;
  } else if (uppercaseCount == 26 && lowercaseCount == 26 && digitCount == 10 && totalCount == 62) {
    setId = 0; // Alphanumerics
    return true;
  } else if (whitespaceCount > 0 && (uppercaseCount + lowercaseCount + digitCount) == 0 && totalCount <= 10) {
    setId = 12; // Whitespace
    return true;
  }
  
  return false;
}

} // anonymous namespace

// Public API implementations
namespace lldb_private {
namespace formatters {

bool GNUstepNSCharacterSetFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  // Delegate to the class-based formatter
  GNUstepNSCharacterSetSummaryProvider provider;
  return provider.FormatObject(valobj, stream, options);
}

bool GNUstepNSMutableCharacterSetFormatterFunction(ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  // NSMutableCharacterSet uses the same internal structure as NSCharacterSet
  return GNUstepNSCharacterSetFormatterFunction(valobj, stream, options);
}

} // namespace formatters
} // namespace lldb_private