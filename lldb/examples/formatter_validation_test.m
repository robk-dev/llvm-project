#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== Priority 1 Formatter Validation Test ===\n");
        
        // Test 1: NSIndexSet - Known working
        printf("\n1. NSIndexSet Tests:\n");
        NSIndexSet *singleIndex = [NSIndexSet indexSetWithIndex:42];
        NSIndexSet *rangeIndex = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(10, 5)]; 
        NSMutableIndexSet *emptyIndex = [NSMutableIndexSet new];
        
        printf("singleIndex = %p (should show: 1 index: 42)\n", singleIndex);
        printf("rangeIndex = %p (should show: 5 indexes in [10-14])\n", rangeIndex);
        printf("emptyIndex = %p (should show: 0 indexes)\n", emptyIndex);
        
        // Test 2: NSDecimalNumber - Check if construction works 
        printf("\n2. NSDecimalNumber Tests:\n");
        NSDecimalNumber *zero = [NSDecimalNumber zero];
        NSDecimalNumber *one = [NSDecimalNumber one];
        
        printf("zero = %p (should show: 0)\n", zero);
        printf("one = %p (should show: 1)\n", one);
        
        // Try to construct a known decimal
        NSDecimal decimal;
        NSDecimalFromComponents(&decimal, 12345, 0, NO);
        NSDecimalNumber *simple = [[NSDecimalNumber alloc] initWithDecimal:decimal];
        
        printf("simple = %p (should show: 12345)\n", simple);
        
        // Test 3: NSCharacterSet - Known working
        printf("\n3. NSCharacterSet Tests:\n");
        NSCharacterSet *letters = [NSCharacterSet letterCharacterSet];
        NSCharacterSet *digits = [NSCharacterSet decimalDigitCharacterSet];
        NSCharacterSet *custom = [NSCharacterSet characterSetWithCharactersInString:@"abc123"];
        
        printf("letters = %p\n", letters);
        printf("digits = %p\n", digits);
        printf("custom = %p\n", custom);
        
        printf("\n=== Ready for LLDB inspection ===\n");
        // Breakpoint here
        
        return 0;
    }
}