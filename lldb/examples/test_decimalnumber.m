/*
 * Comprehensive NSDecimalNumber Formatter Test
 * 
 * This test program creates various NSDecimalNumber instances to validate
 * the formatter requirements:
 * - Precise decimal display with locale support
 * - Handle integers, decimals, scientific notation
 * - Handle very large/small numbers, NaN, infinity
 * - Show precision that NSNumber cannot provide
 */

#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        NSLog(@"=== NSDecimalNumber Formatter Test Starting ===");
        
        // Test Case 1: Basic integers
        NSDecimalNumber *zero = [NSDecimalNumber decimalNumberWithString:@"0"];
        NSDecimalNumber *positiveInt = [NSDecimalNumber decimalNumberWithString:@"42"];
        NSDecimalNumber *negativeInt = [NSDecimalNumber decimalNumberWithString:@"-123"];
        NSLog(@"Basic integers: 0, 42, -123");
        
        // Test Case 2: Small decimals
        NSDecimalNumber *smallDecimal = [NSDecimalNumber decimalNumberWithString:@"3.14159"];
        NSDecimalNumber *negativeDecimal = [NSDecimalNumber decimalNumberWithString:@"-2.71828"];
        NSDecimalNumber *trailingZeros = [NSDecimalNumber decimalNumberWithString:@"1.23000"];
        NSLog(@"Small decimals: 3.14159, -2.71828, 1.23000");
        
        // Test Case 3: High precision decimals
        NSDecimalNumber *highPrecision = [NSDecimalNumber decimalNumberWithString:@"123.456789012345678901234567890"];
        NSDecimalNumber *financialPrecision = [NSDecimalNumber decimalNumberWithString:@"999999.99"];
        NSLog(@"High precision: 30+ digits, financial precision");
        
        // Test Case 4: Very large numbers
        NSDecimalNumber *largeNumber = [NSDecimalNumber decimalNumberWithString:@"123456789012345678901234567890"];
        NSDecimalNumber *veryLargeNumber = [NSDecimalNumber decimalNumberWithString:@"9999999999999999999999999999999999999999"];
        NSLog(@"Large numbers: beyond double precision");
        
        // Test Case 5: Very small numbers
        NSDecimalNumber *smallNumber = [NSDecimalNumber decimalNumberWithString:@"0.000000000000000000000000000001"];
        NSDecimalNumber *tinyNumber = [NSDecimalNumber decimalNumberWithString:@"1E-38"];
        NSLog(@"Small numbers: tiny fractions and scientific notation");
        
        // Test Case 6: Scientific notation input
        NSDecimalNumber *scientificLarge = [NSDecimalNumber decimalNumberWithString:@"1.23E+10"];
        NSDecimalNumber *scientificSmall = [NSDecimalNumber decimalNumberWithString:@"4.56E-15"];
        NSDecimalNumber *scientificNegative = [NSDecimalNumber decimalNumberWithString:@"-7.89E+5"];
        NSLog(@"Scientific notation: 1.23E+10, 4.56E-15, -7.89E+5");
        
        // Test Case 7: Special values - NaN
        NSDecimalNumber *notANumber = [NSDecimalNumber notANumber];
        NSLog(@"Special value: NaN");
        
        // Test Case 8: Mathematical operations resulting in precise values
        NSDecimalNumber *operand1 = [NSDecimalNumber decimalNumberWithString:@"0.1"];
        NSDecimalNumber *operand2 = [NSDecimalNumber decimalNumberWithString:@"0.2"];
        NSDecimalNumber *preciseSum = [operand1 decimalNumberByAdding:operand2];
        NSLog(@"Precise arithmetic: 0.1 + 0.2 (should be exactly 0.3)");
        
        NSDecimalNumber *divisionResult = [[NSDecimalNumber decimalNumberWithString:@"1"] 
                                         decimalNumberByDividingBy:[NSDecimalNumber decimalNumberWithString:@"3"]];
        NSLog(@"Division: 1/3 with decimal precision");
        
        // Test Case 9: Currency and financial calculations
        NSDecimalNumber *price = [NSDecimalNumber decimalNumberWithString:@"19.99"];
        NSDecimalNumber *taxRate = [NSDecimalNumber decimalNumberWithString:@"0.08375"];
        NSDecimalNumber *tax = [price decimalNumberByMultiplyingBy:taxRate];
        NSDecimalNumber *total = [price decimalNumberByAdding:tax];
        NSLog(@"Financial: $19.99 with 8.375%% tax");
        
        // Test Case 10: Rounding behavior
        NSDecimalNumber *beforeRounding = [NSDecimalNumber decimalNumberWithString:@"123.456789"];
        NSDecimalNumberHandler *roundingHandler = [NSDecimalNumberHandler 
                                                  decimalNumberHandlerWithRoundingMode:NSRoundPlain
                                                  scale:2
                                                  raiseOnExactness:NO
                                                  raiseOnOverflow:NO
                                                  raiseOnUnderflow:NO
                                                  raiseOnDivideByZero:NO];
        NSDecimalNumber *rounded = [beforeRounding decimalNumberByRoundingAccordingToBehavior:roundingHandler];
        NSLog(@"Rounding: 123.456789 rounded to 2 decimal places");
        
        // Test Case 11: Comparison edge cases
        NSDecimalNumber *almostEqual1 = [NSDecimalNumber decimalNumberWithString:@"1.0000000000000000000000000001"];
        NSDecimalNumber *almostEqual2 = [NSDecimalNumber decimalNumberWithString:@"1.0000000000000000000000000002"];
        NSLog(@"Precision comparison: very close but distinct values");
        
        // Test Case 12: Zero variations
        NSDecimalNumber *positiveZero = [NSDecimalNumber decimalNumberWithString:@"+0.00"];
        NSDecimalNumber *negativeZero = [NSDecimalNumber decimalNumberWithString:@"-0.00"];
        NSDecimalNumber *plainZero = [NSDecimalNumber zero];
        NSLog(@"Zero variations: +0.00, -0.00, plain zero");
        
        // Test Case 13: Maximum/minimum values
        NSString *maxString = @"99999999999999999999999999999999999999";
        NSDecimalNumber *maxValue = [NSDecimalNumber decimalNumberWithString:maxString];
        NSString *minString = [NSString stringWithFormat:@"-%@", maxString];
        NSDecimalNumber *minValue = [NSDecimalNumber decimalNumberWithString:minString];
        NSLog(@"Extreme values: maximum and minimum representable numbers");
        
        // Test Case 14: Nil handling
        NSDecimalNumber *nilNumber = nil;
        NSLog(@"Nil number for error handling test");
        
        NSLog(@"=== Breakpoint location for LLDB testing ===");
        // LLDB test commands:
        // (lldb) b test_decimalnumber.m:105
        // (lldb) run
        // (lldb) po zero                # Expected: "0"
        // (lldb) po positiveInt         # Expected: "42"
        // (lldb) po negativeInt         # Expected: "-123"
        // (lldb) po smallDecimal        # Expected: "3.14159"
        // (lldb) po highPrecision       # Expected: "123.456789012345678901234567890"
        // (lldb) po largeNumber         # Expected: full precision display
        // (lldb) po smallNumber         # Expected: "0.000000000000000000000000000001"
        // (lldb) po scientificLarge     # Expected: "12300000000" or "1.23E+10"
        // (lldb) po scientificSmall     # Expected: "0.00000000000000456" or "4.56E-15"
        // (lldb) po notANumber          # Expected: "NaN"
        // (lldb) po preciseSum          # Expected: "0.3" (exactly, not 0.30000000000000004)
        // (lldb) po divisionResult      # Expected: precise 1/3 representation
        // (lldb) po total               # Expected: precise financial calculation
        // (lldb) po rounded             # Expected: "123.46"
        // (lldb) po almostEqual1        # Expected: full precision with distinction from almostEqual2
        // (lldb) po maxValue            # Expected: full large number display
        // (lldb) po nilNumber           # Expected: "(null)" or safe error handling
        
        return 0;
    }
}