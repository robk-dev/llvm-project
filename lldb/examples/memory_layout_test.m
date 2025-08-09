#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("Testing memory layouts for Priority 1 formatters\n");
        
        // Test NSIndexSet (single index)
        NSIndexSet *singleIndex = [NSIndexSet indexSetWithIndex:42];
        printf("singleIndex = %p\n", singleIndex);
        
        // Test NSIndexSet (range)
        NSIndexSet *rangeIndex = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(10, 5)];
        printf("rangeIndex = %p\n", rangeIndex);
        
        // Test NSDecimalNumber
        NSDecimalNumber *decimal = [NSDecimalNumber decimalNumberWithString:@"123.45"];
        printf("decimal = %p\n", decimal);
        
        // Test NSDecimalNumber (integer)
        NSDecimalNumber *intDecimal = [NSDecimalNumber decimalNumberWithMantissa:999 exponent:0 isNegative:NO];
        printf("intDecimal = %p\n", intDecimal);
        
        // Test NSCharacterSet (predefined)
        NSCharacterSet *letters = [NSCharacterSet letterCharacterSet];
        printf("letters = %p\n", letters);
        
        // Test NSCharacterSet (custom)
        NSCharacterSet *custom = [NSCharacterSet characterSetWithCharactersInString:@"abc123"];
        printf("custom = %p\n", custom);
        
        // Breakpoint here for memory inspection
        printf("Ready for memory inspection - set breakpoint here\n");
        
        return 0;
    }
}