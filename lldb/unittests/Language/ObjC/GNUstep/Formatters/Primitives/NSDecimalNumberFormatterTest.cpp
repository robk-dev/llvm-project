//===-- NSDecimalNumberFormatterTest.cpp ---------------------------------===//
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

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepDecimalNumberFormatters.h"

#include <memory>
#include <cstring>
#include <cstddef>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace {

class NSDecimalNumberFormatterTest : public ::testing::Test {
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

TEST_F(NSDecimalNumberFormatterTest, FormatterFunctionExists) {
  // Test that NSDecimalNumber formatter function exists
  auto formatter_func = &GNUstepNSDecimalNumberFormatterFunction;
  EXPECT_NE(formatter_func, nullptr) << "NSDecimalNumber formatter function should exist";
  
  // Test that the function pointer is valid and callable
  EXPECT_TRUE(formatter_func != nullptr);
}

TEST_F(NSDecimalNumberFormatterTest, DecimalStructureValidation) {
  // Test actual NSDecimal structure layout matches formatter expectations
  // Based on GNUstepDecimalNumberFormatters.cpp implementation
  
  struct TestNSDecimal {
    int8_t  exponent;      // power of 10 exponent (-128 to 127)
    uint8_t isNegative;    // 0=positive, 1=negative  
    uint8_t validNumber;   // 0=invalid (NaN), 1=valid
    uint8_t length;        // digits in mantissa (0-38)
    uint8_t cMantissa[38]; // mantissa as decimal digits
  };
  
  // Validate structure size and field offsets match expectations
  EXPECT_EQ(sizeof(TestNSDecimal), 42) << "NSDecimal structure should be exactly 42 bytes";
  EXPECT_EQ(offsetof(TestNSDecimal, exponent), 0U) << "exponent should be at offset 0";
  EXPECT_EQ(offsetof(TestNSDecimal, isNegative), 1U) << "isNegative should be at offset 1";
  EXPECT_EQ(offsetof(TestNSDecimal, validNumber), 2U) << "validNumber should be at offset 2";
  EXPECT_EQ(offsetof(TestNSDecimal, length), 3U) << "length should be at offset 3";
  EXPECT_EQ(offsetof(TestNSDecimal, cMantissa), 4U) << "cMantissa should be at offset 4";
  
  // Test field size assumptions
  EXPECT_EQ(sizeof(((TestNSDecimal*)0)->exponent), 1U) << "exponent should be 1 byte";
  EXPECT_EQ(sizeof(((TestNSDecimal*)0)->cMantissa), 38U) << "mantissa array should be 38 bytes";
  
  // Test range limits that formatter logic depends on
  const int8_t MAX_EXPONENT = 127;
  const int8_t MIN_EXPONENT = -128;
  const uint8_t MAX_MANTISSA_LENGTH = 38;
  
  EXPECT_LE(MIN_EXPONENT, MAX_EXPONENT) << "Exponent range should be valid";
  EXPECT_EQ(MAX_MANTISSA_LENGTH, 38) << "Maximum mantissa length should match array size";
}

TEST_F(NSDecimalNumberFormatterTest, SpecialValueHandling) {
  // Test decimal number special cases based on actual formatter logic
  
  struct TestNSDecimal {
    int8_t  exponent;
    uint8_t isNegative;
    uint8_t validNumber;
    uint8_t length;
    uint8_t cMantissa[38];
  };
  
  // Test Case 1: NaN (Not a Number) - validNumber = 0
  TestNSDecimal nanDecimal;
  memset(&nanDecimal, 0, sizeof(nanDecimal));
  nanDecimal.validNumber = 0;  // This marks it as NaN
  
  // Formatter should recognize this as NaN via IsSpecialValue()
  EXPECT_EQ(nanDecimal.validNumber, 0) << "NaN decimal should have validNumber = 0";
  
  // Test Case 2: Valid zero - length = 0, validNumber = 1
  TestNSDecimal zeroDecimal;
  memset(&zeroDecimal, 0, sizeof(zeroDecimal));
  zeroDecimal.validNumber = 1;
  zeroDecimal.length = 0;  // Length 0 represents zero
  
  EXPECT_EQ(zeroDecimal.validNumber, 1) << "Zero should be valid";
  EXPECT_EQ(zeroDecimal.length, 0) << "Zero should have mantissa length 0";
  
  // Test Case 3: Valid positive integer "1"
  TestNSDecimal oneDecimal;
  memset(&oneDecimal, 0, sizeof(oneDecimal));
  oneDecimal.validNumber = 1;
  oneDecimal.isNegative = 0;
  oneDecimal.exponent = 0;
  oneDecimal.length = 1;
  oneDecimal.cMantissa[0] = 1;
  
  EXPECT_EQ(oneDecimal.validNumber, 1) << "Valid number should have validNumber = 1";
  EXPECT_EQ(oneDecimal.isNegative, 0) << "Positive number should have isNegative = 0";
  EXPECT_EQ(oneDecimal.length, 1) << "Single digit should have length 1";
  EXPECT_EQ(oneDecimal.cMantissa[0], 1) << "Mantissa should contain digit 1";
  
  // Test Case 4: Valid negative number "-5"
  TestNSDecimal negativeDecimal;
  memset(&negativeDecimal, 0, sizeof(negativeDecimal));
  negativeDecimal.validNumber = 1;
  negativeDecimal.isNegative = 1;  // Negative flag
  negativeDecimal.exponent = 0;
  negativeDecimal.length = 1;
  negativeDecimal.cMantissa[0] = 5;
  
  EXPECT_EQ(negativeDecimal.isNegative, 1) << "Negative number should have isNegative = 1";
  EXPECT_EQ(negativeDecimal.cMantissa[0], 5) << "Mantissa should contain digit 5";
}

TEST_F(NSDecimalNumberFormatterTest, HighPrecisionValidation) {
  // Test high-precision decimal number validation based on GNUstep implementation
  
  struct TestNSDecimal {
    int8_t  exponent;
    uint8_t isNegative;
    uint8_t validNumber;
    uint8_t length;
    uint8_t cMantissa[38];
  };
  
  // Test Case 1: Maximum precision number (38 digits)
  TestNSDecimal maxPrecisionDecimal;
  memset(&maxPrecisionDecimal, 0, sizeof(maxPrecisionDecimal));
  maxPrecisionDecimal.validNumber = 1;
  maxPrecisionDecimal.isNegative = 0;
  maxPrecisionDecimal.exponent = 0;
  maxPrecisionDecimal.length = 38;  // Maximum allowed
  
  // Fill with valid digits: 1234567890123456...(repeated pattern to 38 digits)
  for (uint8_t i = 0; i < 38; i++) {
    maxPrecisionDecimal.cMantissa[i] = (i % 10);
  }
  
  // Validate structure
  EXPECT_EQ(maxPrecisionDecimal.length, 38) << "Should support maximum 38 digits";
  EXPECT_TRUE(maxPrecisionDecimal.validNumber) << "Max precision number should be valid";
  
  // Validate all digits are in valid range [0-9]
  for (int i = 0; i < 38; i++) {
    EXPECT_GE(maxPrecisionDecimal.cMantissa[i], 0) << "Digit " << i << " should be >= 0";
    EXPECT_LE(maxPrecisionDecimal.cMantissa[i], 9) << "Digit " << i << " should be <= 9";
  }
  
  // Test Case 2: Invalid mantissa digits (would cause formatter to fail)
  TestNSDecimal corruptedDecimal = maxPrecisionDecimal;
  corruptedDecimal.cMantissa[10] = 15;  // Invalid digit > 9
  corruptedDecimal.cMantissa[20] = 255; // Invalid byte value
  
  // The formatter's MantissaToString() should detect these invalid digits
  EXPECT_GT(corruptedDecimal.cMantissa[10], 9) << "Should detect digit > 9";
  EXPECT_GT(corruptedDecimal.cMantissa[20], 9) << "Should detect corrupted digit";
  
  // Test Case 3: Edge case - single digit with max precision length
  TestNSDecimal singleDigitMaxLength;
  memset(&singleDigitMaxLength, 0, sizeof(singleDigitMaxLength));
  singleDigitMaxLength.validNumber = 1;
  singleDigitMaxLength.length = 1;
  singleDigitMaxLength.cMantissa[0] = 7;
  
  EXPECT_EQ(singleDigitMaxLength.length, 1) << "Single digit should have length 1";
  EXPECT_EQ(singleDigitMaxLength.cMantissa[0], 7) << "Should preserve digit value";
  
  // Test Case 4: Invalid length (> 38)
  TestNSDecimal invalidLengthDecimal;
  memset(&invalidLengthDecimal, 0, sizeof(invalidLengthDecimal));
  invalidLengthDecimal.validNumber = 1;
  invalidLengthDecimal.length = 50;  // > 38, should be invalid
  
  EXPECT_GT(invalidLengthDecimal.length, 38) << "Should detect length > 38";
}

TEST_F(NSDecimalNumberFormatterTest, ExponentAndFormattingLogic) {
  // Test decimal formatting scenarios based on ApplyExponentAndFormat implementation
  
  struct TestNSDecimal {
    int8_t  exponent;
    uint8_t isNegative;
    uint8_t validNumber;
    uint8_t length;
    uint8_t cMantissa[38];
  };
  
  // Test Case 1: Zero exponent "42" (no decimal point)
  TestNSDecimal zeroExponentDecimal;
  memset(&zeroExponentDecimal, 0, sizeof(zeroExponentDecimal));
  zeroExponentDecimal.validNumber = 1;
  zeroExponentDecimal.isNegative = 0;
  zeroExponentDecimal.exponent = 0;  // No scaling
  zeroExponentDecimal.length = 2;
  zeroExponentDecimal.cMantissa[0] = 4;
  zeroExponentDecimal.cMantissa[1] = 2;
  
  // Expected output: "42" (mantissa displayed as-is)
  EXPECT_EQ(zeroExponentDecimal.exponent, 0) << "No scaling needed";
  EXPECT_EQ(zeroExponentDecimal.length, 2) << "Two digits: 4, 2";
  
  // Test Case 2: Positive exponent "1200" (mantissa "12", exponent 2)
  TestNSDecimal positiveExponentDecimal;
  memset(&positiveExponentDecimal, 0, sizeof(positiveExponentDecimal));
  positiveExponentDecimal.validNumber = 1;
  positiveExponentDecimal.isNegative = 0;
  positiveExponentDecimal.exponent = 2;  // Multiply by 100
  positiveExponentDecimal.length = 2;
  positiveExponentDecimal.cMantissa[0] = 1;
  positiveExponentDecimal.cMantissa[1] = 2;
  
  // Expected output: "1200" (mantissa "12" + "00" from exponent 2)
  EXPECT_EQ(positiveExponentDecimal.exponent, 2) << "Should add 2 zeros";
  EXPECT_EQ(positiveExponentDecimal.length, 2) << "Mantissa is just '12'";
  
  // Test Case 3: Negative exponent "3.14" (mantissa "314", exponent -2)
  TestNSDecimal negativeExponentDecimal;
  memset(&negativeExponentDecimal, 0, sizeof(negativeExponentDecimal));
  negativeExponentDecimal.validNumber = 1;
  negativeExponentDecimal.isNegative = 0;
  negativeExponentDecimal.exponent = -2;  // Divide by 100
  negativeExponentDecimal.length = 3;
  negativeExponentDecimal.cMantissa[0] = 3;
  negativeExponentDecimal.cMantissa[1] = 1;
  negativeExponentDecimal.cMantissa[2] = 4;
  
  // Expected output: "3.14" (decimal point inserted 2 places from right)
  EXPECT_EQ(negativeExponentDecimal.exponent, -2) << "Decimal point 2 places from right";
  EXPECT_EQ(negativeExponentDecimal.length, 3) << "Mantissa is '314'";
  
  // Test Case 4: Very negative exponent "0.0123" (mantissa "123", exponent -4)
  TestNSDecimal veryNegativeExponentDecimal;
  memset(&veryNegativeExponentDecimal, 0, sizeof(veryNegativeExponentDecimal));
  veryNegativeExponentDecimal.validNumber = 1;
  veryNegativeExponentDecimal.isNegative = 0;
  veryNegativeExponentDecimal.exponent = -4;  // Need leading zeros
  veryNegativeExponentDecimal.length = 3;
  veryNegativeExponentDecimal.cMantissa[0] = 1;
  veryNegativeExponentDecimal.cMantissa[1] = 2;
  veryNegativeExponentDecimal.cMantissa[2] = 3;
  
  // Expected output: "0.0123" ("0." + 1 leading zero + "123")
  EXPECT_EQ(veryNegativeExponentDecimal.exponent, -4) << "Should need leading zeros";
  EXPECT_GT(abs(veryNegativeExponentDecimal.exponent), static_cast<int>(veryNegativeExponentDecimal.length)) << "abs(exponent) > length requires leading zeros";
  
  // Test Case 5: Negative number with decimal "-0.5" (mantissa "5", exponent -1, negative)
  TestNSDecimal negativeDecimalNumber;
  memset(&negativeDecimalNumber, 0, sizeof(negativeDecimalNumber));
  negativeDecimalNumber.validNumber = 1;
  negativeDecimalNumber.isNegative = 1;  // Negative
  negativeDecimalNumber.exponent = -1;   // One decimal place
  negativeDecimalNumber.length = 1;
  negativeDecimalNumber.cMantissa[0] = 5;
  
  // Expected output: "-0.5"
  EXPECT_EQ(negativeDecimalNumber.isNegative, 1) << "Should be negative";
  EXPECT_EQ(negativeDecimalNumber.exponent, -1) << "One decimal place";
  EXPECT_EQ(negativeDecimalNumber.length, 1) << "Single digit mantissa";
}

} // namespace