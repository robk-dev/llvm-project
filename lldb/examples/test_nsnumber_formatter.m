#import <Foundation/Foundation.h>
#import <stdio.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test integer numbers
        NSNumber *intNumber = [NSNumber numberWithInt:42];
        NSNumber *negativeInt = [NSNumber numberWithInt:-100];
        NSNumber *zeroInt = [NSNumber numberWithInt:0];
        
        // Test long long numbers
        NSNumber *longLongNumber = [NSNumber numberWithLongLong:9223372036854775807LL];
        NSNumber *negativeLongLong = [NSNumber numberWithLongLong:-9223372036854775807LL];
        
        // Test unsigned long long
        NSNumber *unsignedLongLong = [NSNumber numberWithUnsignedLongLong:18446744073709551615ULL];
        
        // Test boolean values
        NSNumber *boolYes = [NSNumber numberWithBool:YES];
        NSNumber *boolNo = [NSNumber numberWithBool:NO];
        
        // Test floating point numbers
        NSNumber *floatNumber = [NSNumber numberWithFloat:3.14159f];
        NSNumber *negativeFloat = [NSNumber numberWithFloat:-2.71828f];
        
        // Test double precision numbers
        NSNumber *doubleNumber = [NSNumber numberWithDouble:2.718281828459045];
        NSNumber *negativeDouble = [NSNumber numberWithDouble:-3.141592653589793];
        NSNumber *hugeDouble = [NSNumber numberWithDouble:1.7976931348623157e+308];
        NSNumber *tinyDouble = [NSNumber numberWithDouble:2.2250738585072014e-308];
        
        // Test special float/double values
        NSNumber *nanDouble = [NSNumber numberWithDouble:NAN];
        NSNumber *infDouble = [NSNumber numberWithDouble:INFINITY];
        NSNumber *negInfDouble = [NSNumber numberWithDouble:-INFINITY];
        
        // Test small integers (likely to be tagged pointers)
        NSNumber *smallInt1 = [NSNumber numberWithInt:1];
        NSNumber *smallInt2 = [NSNumber numberWithInt:2];
        NSNumber *smallInt12 = [NSNumber numberWithInt:12];
        NSNumber *smallIntNeg1 = [NSNumber numberWithInt:-1];
        
        // Print some values to verify they work
        printf("Integer: %d\n", [intNumber intValue]);
        printf("Long Long: %lld\n", [longLongNumber longLongValue]);
        printf("Unsigned Long Long: %llu\n", [unsignedLongLong unsignedLongLongValue]);
        printf("Bool YES: %d\n", [boolYes boolValue]);
        printf("Bool NO: %d\n", [boolNo boolValue]);
        printf("Float: %f\n", [floatNumber floatValue]);
        printf("Double: %f\n", [doubleNumber doubleValue]);
        
        // Set a breakpoint here to inspect all the NSNumber objects
        printf("Break here to inspect NSNumber objects\n");  // Line 49
        
        // Test with @() literal syntax (if supported)
        NSNumber *literalInt = @(123);
        NSNumber *literalFloat = @(456.789f);
        NSNumber *literalDouble = @(987.654321);
        NSNumber *literalBool = @(YES);
        
        printf("Literal int: %d\n", [literalInt intValue]);
        printf("Literal float: %f\n", [literalFloat floatValue]);
        printf("Literal double: %f\n", [literalDouble doubleValue]);
        printf("Literal bool: %d\n", [literalBool boolValue]);
        
        // Another breakpoint for literal numbers
        printf("Break here for literal NSNumber objects\n");  // Line 62
        
        return 0;
    }
}