#import <Foundation/Foundation.h>
#import <stdio.h>
#import <math.h>
#import <limits.h>
#import <float.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        printf("=== Comprehensive NSNumber Formatter Test ===\n");
        
        // ========================================
        // Test 1: Integer Types
        // ========================================
        
        // char/BOOL
        NSNumber *charMin = [NSNumber numberWithChar:CHAR_MIN];
        NSNumber *charMax = [NSNumber numberWithChar:CHAR_MAX];
        NSNumber *boolTrue = [NSNumber numberWithBool:YES];
        NSNumber *boolFalse = [NSNumber numberWithBool:NO];
        
        // short
        NSNumber *shortMin = [NSNumber numberWithShort:SHRT_MIN];
        NSNumber *shortMax = [NSNumber numberWithShort:SHRT_MAX];
        NSNumber *shortZero = [NSNumber numberWithShort:0];
        
        // int
        NSNumber *intMin = [NSNumber numberWithInt:INT_MIN];
        NSNumber *intMax = [NSNumber numberWithInt:INT_MAX];
        NSNumber *intNeg = [NSNumber numberWithInt:-42];
        NSNumber *intPos = [NSNumber numberWithInt:42];
        NSNumber *intZero = [NSNumber numberWithInt:0];
        
        // long
        NSNumber *longMin = [NSNumber numberWithLong:LONG_MIN];
        NSNumber *longMax = [NSNumber numberWithLong:LONG_MAX];
        NSNumber *longValue = [NSNumber numberWithLong:1234567890L];
        
        // long long
        NSNumber *longlongMin = [NSNumber numberWithLongLong:LLONG_MIN];
        NSNumber *longlongMax = [NSNumber numberWithLongLong:LLONG_MAX];
        NSNumber *longlongValue = [NSNumber numberWithLongLong:9876543210LL];
        
        // ========================================
        // Test 2: Unsigned Integer Types
        // ========================================
        
        // unsigned char
        NSNumber *ucharMax = [NSNumber numberWithUnsignedChar:UCHAR_MAX];
        NSNumber *ucharZero = [NSNumber numberWithUnsignedChar:0];
        
        // unsigned short
        NSNumber *ushortMax = [NSNumber numberWithUnsignedShort:USHRT_MAX];
        NSNumber *ushortMid = [NSNumber numberWithUnsignedShort:32768];
        
        // unsigned int
        NSNumber *uintMax = [NSNumber numberWithUnsignedInt:UINT_MAX];
        NSNumber *uintValue = [NSNumber numberWithUnsignedInt:3000000000U];
        
        // unsigned long
        NSNumber *ulongMax = [NSNumber numberWithUnsignedLong:ULONG_MAX];
        NSNumber *ulongValue = [NSNumber numberWithUnsignedLong:4000000000UL];
        
        // unsigned long long
        NSNumber *ulonglongMax = [NSNumber numberWithUnsignedLongLong:ULLONG_MAX];
        NSNumber *ulonglongBig = [NSNumber numberWithUnsignedLongLong:10000000000000000000ULL];
        
        // ========================================
        // Test 3: Floating Point Types
        // ========================================
        
        // float
        NSNumber *floatMin = [NSNumber numberWithFloat:FLT_MIN];
        NSNumber *floatMax = [NSNumber numberWithFloat:FLT_MAX];
        NSNumber *floatPi = [NSNumber numberWithFloat:3.14159265f];
        NSNumber *floatE = [NSNumber numberWithFloat:2.71828183f];
        NSNumber *floatNeg = [NSNumber numberWithFloat:-123.456f];
        NSNumber *floatSmall = [NSNumber numberWithFloat:0.0000001f];
        NSNumber *floatZero = [NSNumber numberWithFloat:0.0f];
        NSNumber *floatNegZero = [NSNumber numberWithFloat:-0.0f];
        
        // double
        NSNumber *doubleMin = [NSNumber numberWithDouble:DBL_MIN];
        NSNumber *doubleMax = [NSNumber numberWithDouble:DBL_MAX];
        NSNumber *doublePi = [NSNumber numberWithDouble:3.141592653589793];
        NSNumber *doubleE = [NSNumber numberWithDouble:2.718281828459045];
        NSNumber *doubleNeg = [NSNumber numberWithDouble:-9876.54321];
        NSNumber *doubleTiny = [NSNumber numberWithDouble:1e-308];
        NSNumber *doubleHuge = [NSNumber numberWithDouble:1e308];
        
        // ========================================
        // Test 4: Special Float/Double Values
        // ========================================
        
        NSNumber *floatNaN = [NSNumber numberWithFloat:NAN];
        NSNumber *floatInf = [NSNumber numberWithFloat:INFINITY];
        NSNumber *floatNegInf = [NSNumber numberWithFloat:-INFINITY];
        
        NSNumber *doubleNaN = [NSNumber numberWithDouble:NAN];
        NSNumber *doubleInf = [NSNumber numberWithDouble:INFINITY];
        NSNumber *doubleNegInf = [NSNumber numberWithDouble:-INFINITY];
        
        // Subnormal numbers
        NSNumber *floatSubnormal = [NSNumber numberWithFloat:FLT_MIN / 2.0f];
        NSNumber *doubleSubnormal = [NSNumber numberWithDouble:DBL_MIN / 2.0];
        
        // ========================================
        // Test 5: Literal Syntax (if supported)
        // ========================================
        
        NSNumber *literalInt = @42;
        NSNumber *literalNegInt = @(-100);
        NSNumber *literalFloat = @3.14f;
        NSNumber *literalDouble = @2.71828;
        NSNumber *literalBool = @YES;
        NSNumber *literalChar = @'A';
        NSNumber *literalLong = @123456789L;
        NSNumber *literalUnsigned = @42U;
        
        // ========================================
        // Test 6: Common Patterns
        // ========================================
        
        // Small integers (often tagged pointers)
        NSNumber *small0 = [NSNumber numberWithInt:0];
        NSNumber *small1 = [NSNumber numberWithInt:1];
        NSNumber *small2 = [NSNumber numberWithInt:2];
        NSNumber *small10 = [NSNumber numberWithInt:10];
        NSNumber *smallNeg1 = [NSNumber numberWithInt:-1];
        
        // Common fractions
        NSNumber *half = [NSNumber numberWithDouble:0.5];
        NSNumber *quarter = [NSNumber numberWithDouble:0.25];
        NSNumber *tenth = [NSNumber numberWithDouble:0.1];
        
        // Powers of 2
        NSNumber *pow2_8 = [NSNumber numberWithInt:256];
        NSNumber *pow2_16 = [NSNumber numberWithInt:65536];
        NSNumber *pow2_32 = [NSNumber numberWithLongLong:4294967296LL];
        
        // ========================================
        // Test 7: nil NSNumber
        // ========================================
        
        NSNumber *nilNumber = nil;
        
        // ========================================
        // BREAKPOINT - Set breakpoint here for testing
        // ========================================
        
        printf("All NSNumber objects created. Set breakpoint here.\n"); // Line for breakpoint
        
        // Verification prints
        printf("\nVerifying some values:\n");
        printf("intPos = %d\n", [intPos intValue]);
        printf("boolTrue = %d\n", [boolTrue boolValue]);
        printf("floatPi = %f\n", [floatPi floatValue]);
        printf("doublePi = %f\n", [doublePi doubleValue]);
        printf("longlongMax = %lld\n", [longlongMax longLongValue]);
        
        // Check special values
        if (isnan([doubleNaN doubleValue])) {
            printf("doubleNaN is correctly NaN\n");
        }
        if (isinf([doubleInf doubleValue])) {
            printf("doubleInf is correctly Infinity\n");
        }
        
        // ========================================
        // Test 8: NSNumber in Collections
        // ========================================
        
        NSArray *numberArray = @[
            intPos,
            floatPi,
            boolTrue,
            doubleE,
            @100,
            @(-50),
            @3.14159
        ];
        
        NSDictionary *numberDict = @{
            @"integer": intPos,
            @"float": floatPi,
            @"boolean": boolTrue,
            @"double": doubleE
        };
        
        printf("\nNumber array created with %lu elements\n", (unsigned long)[numberArray count]);
        printf("Number dictionary created with %lu entries\n", (unsigned long)[numberDict count]);
        
        // Another breakpoint location for collection testing
        printf("Collections ready for inspection.\n"); // Alternative breakpoint
        
        printf("\n=== Test Complete ===\n");
        
        return 0;
    }
}