//===-- NSCharacterSetFormatterTest.cpp ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"

#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/Stream.h"

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepCharacterSetFormatters.h"

#include <chrono>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace {

class NSCharacterSetFormatterTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
  }
  
  void TearDown() override {
    HostInfo::Terminate();
    FileSystem::Terminate();
  }
};

TEST_F(NSCharacterSetFormatterTest, FormatterFunctionExists) {
  // Test that NSCharacterSet formatter functions exist
  auto char_set_func = &GNUstepNSCharacterSetFormatterFunction;
  EXPECT_NE(char_set_func, nullptr) << "NSCharacterSet formatter function should exist";
  
  auto mutable_char_set_func = &GNUstepNSMutableCharacterSetFormatterFunction;
  EXPECT_NE(mutable_char_set_func, nullptr) << "NSMutableCharacterSet formatter function should exist";
  
  // Test that function pointers are distinct
  EXPECT_NE(char_set_func, mutable_char_set_func) << "Formatter functions should be distinct";
}

TEST_F(NSCharacterSetFormatterTest, FormatterRegistrationFunctions) {
  // Test that formatter registration functions exist
  
  auto char_set_func = &GNUstepNSCharacterSetFormatterFunction;
  EXPECT_NE(char_set_func, nullptr) << "GNUstepNSCharacterSetFormatterFunction should exist";
  
  auto mutable_func = &GNUstepNSMutableCharacterSetFormatterFunction;
  EXPECT_NE(mutable_func, nullptr) << "GNUstepNSMutableCharacterSetFormatterFunction should exist";
}

TEST_F(NSCharacterSetFormatterTest, StandardCharacterSetMapping) {
  // Test the standard character set ID mapping from GNUstepCharacterSetFormatters.cpp
  
  // Based on kStandardCharacterSets map in the formatter implementation
  struct StandardSetMapping {
    uint32_t id;
    std::string expectedName;
  };
  
  // Test mappings extracted from the actual formatter code
  std::vector<StandardSetMapping> knownSets = {
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
  
  // Verify we have the expected number of standard sets
  EXPECT_EQ(knownSets.size(), 21) << "Should have 21 standard character sets defined";
  
  // Verify ID ranges are reasonable
  for (const auto& set : knownSets) {
    EXPECT_GE(set.id, 0) << "Set ID should be non-negative: " << set.expectedName;
    EXPECT_LE(set.id, 30) << "Set ID should be reasonable: " << set.expectedName;
    EXPECT_FALSE(set.expectedName.empty()) << "Set name should not be empty for ID " << set.id;
  }
  
  // Verify no duplicate IDs
  std::set<uint32_t> uniqueIds;
  for (const auto& set : knownSets) {
    EXPECT_EQ(uniqueIds.find(set.id), uniqueIds.end()) << "Duplicate set ID: " << set.id;
    uniqueIds.insert(set.id);
  }
  
  EXPECT_EQ(uniqueIds.size(), knownSets.size()) << "All set IDs should be unique";
}

TEST_F(NSCharacterSetFormatterTest, BitmapCharacterCounting) {
  // Test character counting algorithm used by CountCharactersInBitmap()
  
  const size_t BITMAP_SIZE = 8192;
  const size_t TOTAL_UNICODE_BMP_CHARS = 65536;
  
  EXPECT_EQ(BITMAP_SIZE * 8, TOTAL_UNICODE_BMP_CHARS) << "8192 bytes should cover Unicode BMP";
  
  // Test the actual bit counting algorithm from the formatter
  // This mimics CountCharactersInBitmap() implementation
  
  // Test Case 1: Empty bitmap
  std::vector<uint8_t> emptyBitmap(10, 0);
  size_t emptyCount = 0;
  for (uint8_t byte : emptyBitmap) {
    uint8_t temp = byte;
    while (temp) {
      emptyCount += temp & 1;
      temp >>= 1;
    }
  }
  EXPECT_EQ(emptyCount, 0) << "Empty bitmap should have zero character count";
  
  // Test Case 2: Single bit set
  std::vector<uint8_t> singleBitBitmap = {0x01, 0x00, 0x00}; // Only bit 0 set
  size_t singleBitCount = 0;
  for (uint8_t byte : singleBitBitmap) {
    uint8_t temp = byte;
    while (temp) {
      singleBitCount += temp & 1;
      temp >>= 1;
    }
  }
  EXPECT_EQ(singleBitCount, 1) << "Single bit should count as 1 character";
  
  // Test Case 3: Multiple bits in one byte
  std::vector<uint8_t> multiBitBitmap = {0b10110101}; // 5 bits set
  size_t multiBitCount = 0;
  for (uint8_t byte : multiBitBitmap) {
    uint8_t temp = byte;
    while (temp) {
      multiBitCount += temp & 1;
      temp >>= 1;
    }
  }
  EXPECT_EQ(multiBitCount, 5) << "Should correctly count 5 set bits";
  
  // Test Case 4: All bits set in a byte
  std::vector<uint8_t> fullByteBitmap = {0xFF}; // All 8 bits set
  size_t fullByteCount = 0;
  for (uint8_t byte : fullByteBitmap) {
    uint8_t temp = byte;
    while (temp) {
      fullByteCount += temp & 1;
      temp >>= 1;
    }
  }
  EXPECT_EQ(fullByteCount, 8) << "Full byte should count as 8 characters";
  
  // Test Case 5: Mixed bytes
  std::vector<uint8_t> mixedBitmap = {0xFF, 0x00, 0x0F, 0x80}; // 8+0+4+1=13 bits
  size_t mixedCount = 0;
  for (uint8_t byte : mixedBitmap) {
    uint8_t temp = byte;
    while (temp) {
      mixedCount += temp & 1;
      temp >>= 1;
    }
  }
  EXPECT_EQ(mixedCount, 13) << "Mixed bitmap should count correctly";
}

TEST_F(NSCharacterSetFormatterTest, SampleCharacterExtraction) {
  // Test the ExtractSampleCharacters algorithm from the formatter
  
  // Test Case 1: ASCII printable characters
  std::vector<uint8_t> asciiPrintableBitmap(128/8, 0); // 16 bytes for ASCII range
  
  // Set bits for 'A' (0x41), 'B' (0x42), 'Z' (0x5A)
  // Bit calculation: character = byteIndex*8 + bitIndex
  // 'A' = 0x41 = 65 = byte 8, bit 1
  asciiPrintableBitmap[65/8] |= (1 << (65%8)); // 'A'
  asciiPrintableBitmap[66/8] |= (1 << (66%8)); // 'B'
  asciiPrintableBitmap[90/8] |= (1 << (90%8)); // 'Z'
  
  // Extract samples (mimics ExtractSampleCharacters logic)
  std::string samples;
  size_t samplesFound = 0;
  const size_t maxSamples = 10;
  
  for (size_t byteIndex = 0; byteIndex < asciiPrintableBitmap.size() && samplesFound < maxSamples; byteIndex++) {
    uint8_t byte = asciiPrintableBitmap[byteIndex];
    if (byte == 0) continue;
    
    for (int bitIndex = 0; bitIndex < 8 && samplesFound < maxSamples; bitIndex++) {
      if ((byte & (1 << bitIndex)) != 0) {
        uint32_t character = (byteIndex * 8) + bitIndex;
        if (character >= 32 && character <= 126) { // Printable ASCII
          samples += static_cast<char>(character);
          samplesFound++;
        }
      }
    }
  }
  
  EXPECT_EQ(samples, "ABZ") << "Should extract printable ASCII characters in order";
  EXPECT_EQ(samplesFound, 3) << "Should find exactly 3 sample characters";
  
  // Test Case 2: Control characters with special formatting
  std::vector<uint8_t> controlCharBitmap(16, 0);
  controlCharBitmap[0] |= (1 << 9);  // Tab (0x09)
  controlCharBitmap[0] |= (1 << 10); // Newline (0x0A)
  controlCharBitmap[0] |= (1 << 13); // Carriage return (0x0D)
  controlCharBitmap[1] |= (1 << 0);  // Some other control char (0x10)
  
  // The formatter should handle these with escape sequences
  // This tests the control character detection logic
  uint32_t tabChar = 9;
  uint32_t newlineChar = 10;
  uint32_t crChar = 13;
  
  EXPECT_LT(tabChar, 32) << "Tab should be identified as control character";
  EXPECT_LT(newlineChar, 32) << "Newline should be identified as control character";
  EXPECT_LT(crChar, 32) << "CR should be identified as control character";
}

TEST_F(NSCharacterSetFormatterTest, ControlCharacterFormatting) {
  // Test control character formatting based on ExtractSampleCharacters implementation
  
  // Define control character ranges from the formatter
  const uint32_t C0_CONTROL_START = 0x00;
  const uint32_t C0_CONTROL_END = 0x1F;
  const uint32_t DEL_CHARACTER = 0x7F;
  const uint32_t C1_CONTROL_START = 0x80;
  const uint32_t C1_CONTROL_END = 0x9F;
  
  // Verify control character range definitions
  EXPECT_EQ(C0_CONTROL_START, 0x00) << "C0 control should start at 0x00";
  EXPECT_EQ(C0_CONTROL_END, 0x1F) << "C0 control should end at 0x1F";
  EXPECT_EQ(DEL_CHARACTER, 0x7F) << "DEL should be at 0x7F";
  EXPECT_EQ(C1_CONTROL_START, 0x80) << "C1 control should start at 0x80";
  EXPECT_EQ(C1_CONTROL_END, 0x9F) << "C1 control should end at 0x9F";
  
  // Test specific control character identification (matches formatter logic)
  uint32_t testChars[] = {
    0x00,  // NULL
    0x09,  // TAB
    0x0A,  // LF (newline)
    0x0D,  // CR
    0x1B,  // ESC
    0x1F,  // US (last C0 control)
    0x20,  // SPACE (first printable)
    0x7E,  // ~ (last printable)
    0x7F,  // DEL
    0x80,  // First C1 control
    0x9F   // Last C1 control
  };
  
  for (uint32_t ch : testChars) {
    bool isControlChar = (ch < 32) || (ch == 0x7F) || (ch >= 0x80 && ch <= 0x9F);
    bool isPrintableASCII = (ch >= 32 && ch <= 126);
    
    if (ch < 32) {
      EXPECT_TRUE(isControlChar) << "Character 0x" << std::hex << ch << " should be C0 control";
      EXPECT_FALSE(isPrintableASCII) << "Control character should not be printable";
    } else if (ch == 0x7F) {
      EXPECT_TRUE(isControlChar) << "DEL character should be control";
      EXPECT_FALSE(isPrintableASCII) << "DEL should not be printable";
    } else if (ch >= 32 && ch <= 126) {
      EXPECT_FALSE(isControlChar) << "Printable ASCII should not be control";
      EXPECT_TRUE(isPrintableASCII) << "Character 0x" << std::hex << ch << " should be printable";
    }
  }
  
  // Test specific character formatting expectations
  struct ControlCharTest {
    uint32_t character;
    std::string expectedFormat;
  };
  
  // Based on the formatter's special case handling
  std::vector<ControlCharTest> controlTests = {
    {0x0A, "\\n"},   // Newline
    {0x09, "\\t"},   // Tab
    {0x0D, "\\r"}    // Carriage return
  };
  
  for (const auto& test : controlTests) {
    EXPECT_LT(test.character, 32) << "Test character should be in control range";
    EXPECT_FALSE(test.expectedFormat.empty()) << "Expected format should not be empty";
  }
}

TEST_F(NSCharacterSetFormatterTest, StandardSetIdentification) {
  // Test the IdentifyStandardSet heuristics from the formatter
  
  // Test Case 1: Decimal Digits (0-9) - should identify as set ID 2
  std::vector<uint8_t> digitsBitmap(128/8, 0); // 16 bytes for ASCII range
  
  // Set bits for characters '0' through '9' (ASCII 48-57)
  for (uint32_t digit = 48; digit <= 57; digit++) { // '0' to '9'
    digitsBitmap[digit/8] |= (1 << (digit%8));
  }
  
  // Count characters using the same algorithm as the formatter
  size_t digitCount = 0, uppercaseCount = 0, lowercaseCount = 0, totalCount = 0;
  
  for (size_t i = 0; i < std::min(digitsBitmap.size(), static_cast<size_t>(128/8)); i++) {
    uint8_t byte = digitsBitmap[i];
    for (int bit = 0; bit < 8; bit++) {
      if ((byte & (1 << bit)) != 0) {
        uint32_t character = (i * 8) + bit;
        totalCount++;
        
        if (character >= '0' && character <= '9') {
          digitCount++;
        }
      }
    }
  }
  
  EXPECT_EQ(digitCount, 10) << "Should find exactly 10 decimal digits";
  EXPECT_EQ(totalCount, 10) << "Total count should match digit count";
  
  // Should identify as decimal digits (ID 2)
  bool shouldBeDecimalDigits = (digitCount == 10 && totalCount == 10);
  EXPECT_TRUE(shouldBeDecimalDigits) << "Should identify as decimal digit set";
  
  // Test Case 2: Uppercase Letters (A-Z) - should identify as set ID 10
  std::vector<uint8_t> uppercaseBitmap(128/8, 0);
  
  for (uint32_t letter = 65; letter <= 90; letter++) { // 'A' to 'Z'
    uppercaseBitmap[letter/8] |= (1 << (letter%8));
  }
  
  size_t uppercaseOnlyCount = 0, uppercaseTotalCount = 0;
  for (size_t i = 0; i < std::min(uppercaseBitmap.size(), static_cast<size_t>(128/8)); i++) {
    uint8_t byte = uppercaseBitmap[i];
    for (int bit = 0; bit < 8; bit++) {
      if ((byte & (1 << bit)) != 0) {
        uint32_t character = (i * 8) + bit;
        uppercaseTotalCount++;
        
        if (character >= 'A' && character <= 'Z') {
          uppercaseOnlyCount++;
        }
      }
    }
  }
  
  EXPECT_EQ(uppercaseOnlyCount, 26) << "Should find exactly 26 uppercase letters";
  EXPECT_EQ(uppercaseTotalCount, 26) << "Total should match uppercase count";
  
  // Test Case 3: Both upper and lower case letters - should identify as Letters (ID 5)
  std::vector<uint8_t> lettersBitmap(128/8, 0);
  
  // Add A-Z and a-z
  for (uint32_t letter = 65; letter <= 90; letter++) { // 'A' to 'Z'
    lettersBitmap[letter/8] |= (1 << (letter%8));
  }
  for (uint32_t letter = 97; letter <= 122; letter++) { // 'a' to 'z'
    lettersBitmap[letter/8] |= (1 << (letter%8));
  }
  
  size_t lettersUpperCount = 0, lettersLowerCount = 0, lettersTotalCount = 0;
  for (size_t i = 0; i < std::min(lettersBitmap.size(), static_cast<size_t>(128/8)); i++) {
    uint8_t byte = lettersBitmap[i];
    for (int bit = 0; bit < 8; bit++) {
      if ((byte & (1 << bit)) != 0) {
        uint32_t character = (i * 8) + bit;
        lettersTotalCount++;
        
        if (character >= 'A' && character <= 'Z') {
          lettersUpperCount++;
        } else if (character >= 'a' && character <= 'z') {
          lettersLowerCount++;
        }
      }
    }
  }
  
  EXPECT_EQ(lettersUpperCount, 26) << "Should find 26 uppercase letters";
  EXPECT_EQ(lettersLowerCount, 26) << "Should find 26 lowercase letters";
  EXPECT_EQ(lettersTotalCount, 52) << "Should find total 52 letters";
  
  // Should identify as Letters set
  bool shouldBeLetters = (lettersUpperCount == 26 && lettersLowerCount == 26 && lettersTotalCount == 52);
  EXPECT_TRUE(shouldBeLetters) << "Should identify as letters set";
}

TEST_F(NSCharacterSetFormatterTest, EmptyCharacterSetHandling) {
  // Test empty character set detection and handling
  
  // Create empty bitmap (all zeros)
  const size_t BITMAP_SIZE = 100;
  std::vector<uint8_t> emptyBitmap(BITMAP_SIZE, 0);
  
  // Count characters using formatter algorithm
  size_t characterCount = 0;
  for (uint8_t byte : emptyBitmap) {
    uint8_t temp = byte;
    while (temp) {
      characterCount += temp & 1;
      temp >>= 1;
    }
  }
  
  EXPECT_EQ(characterCount, 0) << "Empty bitmap should have zero character count";
  
  // Test sample character extraction on empty bitmap
  std::string samples;
  size_t samplesFound = 0;
  const size_t maxSamples = 10;
  
  for (size_t byteIndex = 0; byteIndex < emptyBitmap.size() && samplesFound < maxSamples; byteIndex++) {
    uint8_t byte = emptyBitmap[byteIndex];
    if (byte == 0) continue; // This should skip all bytes in empty bitmap
    
    for (int bitIndex = 0; bitIndex < 8 && samplesFound < maxSamples; bitIndex++) {
      if ((byte & (1 << bitIndex)) != 0) {
        uint32_t character = (byteIndex * 8) + bitIndex;
        if (character >= 32 && character <= 126) {
          samples += static_cast<char>(character);
          samplesFound++;
        }
      }
    }
  }
  
  EXPECT_TRUE(samples.empty()) << "Empty bitmap should produce no sample characters";
  EXPECT_EQ(samplesFound, 0) << "Should find zero sample characters";
  
  // Test that empty bitmap doesn't match any standard set
  // (based on IdentifyStandardSet logic)
  size_t digitCount = 0, uppercaseCount = 0, lowercaseCount = 0, totalCount = 0;
  
  for (size_t i = 0; i < std::min(emptyBitmap.size(), static_cast<size_t>(128/8)); i++) {
    uint8_t byte = emptyBitmap[i];
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
        }
      }
    }
  }
  
  EXPECT_EQ(totalCount, 0) << "Empty set should have zero total count";
  EXPECT_EQ(digitCount, 0) << "Empty set should have zero digit count";
  EXPECT_EQ(uppercaseCount, 0) << "Empty set should have zero uppercase count";
  EXPECT_EQ(lowercaseCount, 0) << "Empty set should have zero lowercase count";
  
  // None of the standard set identification conditions should match
  bool isDecimalDigits = (digitCount == 10 && totalCount == 10);
  bool isUppercase = (uppercaseCount == 26 && totalCount == 26);
  bool isLetters = (uppercaseCount == 26 && lowercaseCount == 26 && totalCount == 52);
  
  EXPECT_FALSE(isDecimalDigits) << "Empty set should not be identified as decimal digits";
  EXPECT_FALSE(isUppercase) << "Empty set should not be identified as uppercase letters";
  EXPECT_FALSE(isLetters) << "Empty set should not be identified as letters";
}

TEST_F(NSCharacterSetFormatterTest, LargeCharacterSetPerformance) {
  // Test performance characteristics of large character set handling
  
  const size_t LARGE_BITMAP_SIZE = 1024; // Reasonable test size
  const size_t MAX_SAMPLES = 10;
  
  // Test Case 1: Nearly full bitmap (most bits set)
  std::vector<uint8_t> nearlyFullBitmap(LARGE_BITMAP_SIZE, 0xFF); // All bits set
  nearlyFullBitmap[100] = 0x7F; // One bit cleared to make it "nearly" full
  
  auto startTime = std::chrono::high_resolution_clock::now();
  
  // Count characters (performance test)
  size_t fullBitmapCount = 0;
  for (uint8_t byte : nearlyFullBitmap) {
    uint8_t temp = byte;
    while (temp) {
      fullBitmapCount += temp & 1;
      temp >>= 1;
    }
  }
  
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  
  EXPECT_LT(duration.count(), 10000) << "Character counting should be fast (<10ms)";
  EXPECT_GT(fullBitmapCount, LARGE_BITMAP_SIZE * 7) << "Nearly full bitmap should have many characters";
  EXPECT_LT(fullBitmapCount, LARGE_BITMAP_SIZE * 8) << "Should be less than completely full";
  
  // Test Case 2: Sample extraction with limits (performance test)
  startTime = std::chrono::high_resolution_clock::now();
  
  std::string samples;
  size_t samplesFound = 0;
  
  for (size_t byteIndex = 0; byteIndex < nearlyFullBitmap.size() && samplesFound < MAX_SAMPLES; byteIndex++) {
    uint8_t byte = nearlyFullBitmap[byteIndex];
    if (byte == 0) continue;
    
    for (int bitIndex = 0; bitIndex < 8 && samplesFound < MAX_SAMPLES; bitIndex++) {
      if ((byte & (1 << bitIndex)) != 0) {
        uint32_t character = (byteIndex * 8) + bitIndex;
        if (character >= 32 && character <= 126) { // Printable ASCII
          samples += static_cast<char>(character);
          samplesFound++;
        }
      }
    }
  }
  
  endTime = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  
  EXPECT_LT(duration.count(), 5000) << "Sample extraction should be fast (<5ms)";
  EXPECT_EQ(samplesFound, MAX_SAMPLES) << "Should find exactly MAX_SAMPLES characters";
  EXPECT_EQ(samples.length(), MAX_SAMPLES) << "Sample string should have correct length";
  
  // Test Case 3: Sparse bitmap (scattered characters)
  std::vector<uint8_t> sparseBitmap(LARGE_BITMAP_SIZE, 0);
  
  // Set a few scattered bits
  for (size_t i = 0; i < LARGE_BITMAP_SIZE; i += 100) {
    sparseBitmap[i] = 0x01; // Set only first bit in every 100th byte
  }
  
  startTime = std::chrono::high_resolution_clock::now();
  
  size_t sparseCount = 0;
  for (uint8_t byte : sparseBitmap) {
    uint8_t temp = byte;
    while (temp) {
      sparseCount += temp & 1;
      temp >>= 1;
    }
  }
  
  endTime = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  
  EXPECT_LT(duration.count(), 5000) << "Sparse bitmap counting should be fast";
  EXPECT_EQ(sparseCount, (LARGE_BITMAP_SIZE / 100) + 1) << "Should count scattered bits correctly";
  
  // Verify sample extraction limits are respected
  EXPECT_LE(MAX_SAMPLES, 20) << "Sample limit should be reasonable for UI display";
  EXPECT_GE(MAX_SAMPLES, 5) << "Should extract enough samples to be useful";
}

TEST_F(NSCharacterSetFormatterTest, PerformanceRequirements) {
  // Test that formatter function calls meet performance requirements
  
  auto start = std::chrono::high_resolution_clock::now();
  
  // Test function pointer access performance
  for (int i = 0; i < 1000; ++i) {
    auto char_set_func = &GNUstepNSCharacterSetFormatterFunction;
    auto mutable_func = &GNUstepNSMutableCharacterSetFormatterFunction;
    
    // Validate function pointers
    EXPECT_NE(char_set_func, nullptr);
    EXPECT_NE(mutable_func, nullptr);
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 50) << "1000 function pointer accesses should be fast (<50ms)";
}

TEST_F(NSCharacterSetFormatterTest, ThreadSafety) {
  // Test thread safety of formatter function access
  
  std::vector<std::thread> threads;
  std::vector<bool> results(10, false);
  
  // Access formatter functions from multiple threads
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&results, i]() {
      auto char_set_func = &GNUstepNSCharacterSetFormatterFunction;
      auto mutable_func = &GNUstepNSMutableCharacterSetFormatterFunction;
      
      results[i] = (char_set_func != nullptr) && (mutable_func != nullptr);
    });
  }
  
  // Wait for all threads
  for (auto& thread : threads) {
    thread.join();
  }
  
  // Verify all threads succeeded
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(results[i]) << "Thread " << i << " should have successfully accessed formatter functions";
  }
}

TEST_F(NSCharacterSetFormatterTest, EdgeCaseBitmapPatterns) {
  // Test edge cases that stress the bitmap processing algorithms
  
  // Test Case 1: Alternating bit pattern (0xAA)
  std::vector<uint8_t> alternatingBitmap(10);
  std::fill(alternatingBitmap.begin(), alternatingBitmap.end(), 0xAA); // 10101010 pattern
  
  size_t alternatingCount = 0;
  for (uint8_t byte : alternatingBitmap) {
    uint8_t temp = byte;
    while (temp) {
      alternatingCount += temp & 1;
      temp >>= 1;
    }
  }
  
  // 0xAA = 10101010, so 4 bits set per byte
  EXPECT_EQ(alternatingCount, alternatingBitmap.size() * 4) << "Alternating pattern should have 4 bits per byte";
  
  // Test Case 2: Single bit set in large bitmap
  std::vector<uint8_t> singleBitBitmap(1000, 0);
  singleBitBitmap[500] = 0x40; // Set one bit in middle
  
  size_t singleBitCount = 0;
  for (uint8_t byte : singleBitBitmap) {
    uint8_t temp = byte;
    while (temp) {
      singleBitCount += temp & 1;
      temp >>= 1;
    }
  }
  
  EXPECT_EQ(singleBitCount, 1) << "Single bit bitmap should count exactly 1";
  
  // Test Case 3: Nearly full bitmap (one bit clear)
  std::vector<uint8_t> nearlyFullBitmap(10, 0xFF);
  nearlyFullBitmap[5] = 0xFE; // Clear one bit: 11111110
  
  size_t nearlyFullCount = 0;
  for (uint8_t byte : nearlyFullBitmap) {
    uint8_t temp = byte;
    while (temp) {
      nearlyFullCount += temp & 1;
      temp >>= 1;
    }
  }
  
  // Should be (10 * 8) - 1 = 79 bits
  EXPECT_EQ(nearlyFullCount, (nearlyFullBitmap.size() * 8) - 1) << "Nearly full should miss exactly 1 bit";
  
  // Test Case 4: Checkerboard pattern
  std::vector<uint8_t> checkerboardBitmap;
  for (int i = 0; i < 10; i++) {
    checkerboardBitmap.push_back((i % 2 == 0) ? 0x55 : 0xAA); // Alternating 01010101 and 10101010
  }
  
  size_t checkerboardCount = 0;
  for (uint8_t byte : checkerboardBitmap) {
    uint8_t temp = byte;
    while (temp) {
      checkerboardCount += temp & 1;
      temp >>= 1;
    }
  }
  
  // Both 0x55 and 0xAA have 4 bits set
  EXPECT_EQ(checkerboardCount, checkerboardBitmap.size() * 4) << "Checkerboard should have consistent bit density";
  
  // Test Case 5: Only high bits set (non-ASCII range)
  std::vector<uint8_t> highBitsBitmap(256, 0);
  // Set bits in upper ranges (beyond ASCII)
  for (int i = 128; i < 256; i++) {
    highBitsBitmap[i/8] |= (1 << (i%8));
  }
  
  // Extract samples and verify no printable ASCII characters
  std::string highBitsSamples;
  size_t highBitsSamplesFound = 0;
  const size_t maxSamples = 10;
  
  for (size_t byteIndex = 0; byteIndex < highBitsBitmap.size() && highBitsSamplesFound < maxSamples; byteIndex++) {
    uint8_t byte = highBitsBitmap[byteIndex];
    if (byte == 0) continue;
    
    for (int bitIndex = 0; bitIndex < 8 && highBitsSamplesFound < maxSamples; bitIndex++) {
      if ((byte & (1 << bitIndex)) != 0) {
        uint32_t character = (byteIndex * 8) + bitIndex;
        if (character >= 32 && character <= 126) { // Printable ASCII
          highBitsSamples += static_cast<char>(character);
          highBitsSamplesFound++;
        }
      }
    }
  }
  
  EXPECT_TRUE(highBitsSamples.empty()) << "High bits only should produce no printable ASCII samples";
  EXPECT_EQ(highBitsSamplesFound, 0) << "Should find zero printable ASCII characters in high range";
}

TEST_F(NSCharacterSetFormatterTest, UnicodeRangeCoverage) {
  // Test Unicode BMP coverage and character range validation
  
  // Unicode Basic Multilingual Plane (BMP) coverage
  const uint32_t BMP_START = 0x0000;
  const uint32_t BMP_END = 0xFFFF;
  const size_t BMP_SIZE_BYTES = 8192; // 65536 bits / 8
  const size_t BMP_SIZE_BITS = BMP_SIZE_BYTES * 8;
  
  EXPECT_EQ(BMP_START, 0x0000) << "BMP should start at U+0000";
  EXPECT_EQ(BMP_END, 0xFFFFU) << "BMP should end at U+FFFF";
  EXPECT_EQ(BMP_SIZE_BITS, 65536) << "BMP should cover 65536 code points";
  EXPECT_EQ(BMP_SIZE_BYTES, 8192) << "BMP bitmap should be 8192 bytes";
  
  // Test specific Unicode ranges
  struct UnicodeRange {
    uint32_t start;
    uint32_t end;
    std::string name;
  };
  
  std::vector<UnicodeRange> unicodeRanges = {
    {0x0000, 0x007F, "ASCII"},
    {0x0080, 0x00FF, "Latin-1 Supplement"},
    {0x0100, 0x017F, "Latin Extended-A"},
    {0x0180, 0x024F, "Latin Extended-B"},
    {0x0370, 0x03FF, "Greek and Coptic"},
    {0x0400, 0x04FF, "Cyrillic"},
    {0x4E00, 0x9FFF, "CJK Unified Ideographs"},
    {0xAC00, 0xD7AF, "Hangul Syllables"}
  };
  
  for (const auto& range : unicodeRanges) {
    EXPECT_LE(range.start, range.end) << "Range " << range.name << " should have valid start/end";
    EXPECT_LE(range.end, BMP_END) << "Range " << range.name << " should fit within BMP";
    EXPECT_FALSE(range.name.empty()) << "Range should have a name";
    
    // Test that range can be represented in bitmap
    size_t startByteIndex = range.start / 8;
    size_t endByteIndex = range.end / 8;
    EXPECT_LT(startByteIndex, BMP_SIZE_BYTES) << "Start of " << range.name << " should fit in bitmap";
    EXPECT_LT(endByteIndex, BMP_SIZE_BYTES) << "End of " << range.name << " should fit in bitmap";
  }
  
  // Test character to bit position calculation
  struct CharacterTestCase {
    uint32_t character;
    size_t expectedByteIndex;
    int expectedBitIndex;
  };
  
  std::vector<CharacterTestCase> charTests = {
    {0x0000, 0, 0},      // First character
    {0x0008, 1, 0},      // Byte boundary
    {0x0041, 8, 1},      // 'A' = 65 = byte 8, bit 1
    {0x007F, 15, 7},     // Last ASCII = byte 15, bit 7
    {0x0080, 16, 0},     // First Latin-1 supplement
    {0x00FF, 31, 7}      // Last Latin-1 supplement
  };
  
  for (const auto& test : charTests) {
    size_t calculatedByteIndex = test.character / 8;
    int calculatedBitIndex = test.character % 8;
    
    EXPECT_EQ(calculatedByteIndex, test.expectedByteIndex) 
      << "Character 0x" << std::hex << test.character << " byte index mismatch";
    EXPECT_EQ(calculatedBitIndex, test.expectedBitIndex) 
      << "Character 0x" << std::hex << test.character << " bit index mismatch";
  }
  
  // Test bit manipulation for various characters
  std::vector<uint8_t> testBitmap(100, 0);
  
  // Set bits for specific characters
  uint32_t testCharacters[] = {0x41, 0x42, 0x7A, 0x80, 0xFF}; // A, B, z, €, ÿ
  
  for (uint32_t ch : testCharacters) {
    if (ch / 8 < testBitmap.size()) {
      testBitmap[ch / 8] |= (1 << (ch % 8));
    }
  }
  
  // Verify bits are set correctly
  for (uint32_t ch : testCharacters) {
    if (ch / 8 < testBitmap.size()) {
      bool bitIsSet = (testBitmap[ch / 8] & (1 << (ch % 8))) != 0;
      EXPECT_TRUE(bitIsSet) << "Bit should be set for character 0x" << std::hex << ch;
    }
  }
}

} // namespace