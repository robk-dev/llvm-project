#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== NSArray Element Corruption Fix Test ===\n");
        
        // Test 1: Simple string array (the main corruption case)
        NSArray *fruits = @[@"apple", @"banana", @"cherry", @"date"];
        printf("Test 1 - String array: %s\n", [[fruits description] UTF8String]);
        
        // Test 2: Mixed array with strings and numbers
        NSArray *mixed = @[@"text", @42, @3.14, @YES];
        printf("Test 2 - Mixed array: %s\n", [[mixed description] UTF8String]);
        
        // Test 3: Nested arrays
        NSArray *nested = @[@[@"a", @"b"], @[@"c", @"d"], @[@"e"]];
        printf("Test 3 - Nested arrays: %s\n", [[nested description] UTF8String]);
        
        // Test 4: Custom objects in array
        NSDictionary *dict1 = @{@"key1": @"value1"};
        NSDictionary *dict2 = @{@"key2": @"value2"};
        NSArray *objects = @[dict1, dict2];
        printf("Test 4 - Object array: %s\n", [[objects description] UTF8String]);
        
        // Test 5: Empty array
        NSArray *empty = @[];
        printf("Test 5 - Empty array: %s\n", [[empty description] UTF8String]);
        
        // Test 6: Large array (test performance and corruption with many elements)
        NSMutableArray *large = [NSMutableArray array];
        for (int i = 0; i < 10; i++) {
            [large addObject:[NSString stringWithFormat:@"item_%d", i]];
        }
        printf("Test 6 - Large array (%lu items): %s\n", 
               (unsigned long)[large count], 
               [[[large subarrayWithRange:NSMakeRange(0, 3)] description] UTF8String]);
        
        // Test 7: NSConstantString vs runtime strings
        NSString *constant = @"constant_string";
        NSString *runtime = [NSString stringWithFormat:@"runtime_%d", 123];
        NSArray *stringTypes = @[constant, runtime];
        printf("Test 7 - String types: %s\n", [[stringTypes description] UTF8String]);
        
        printf("=== Test Arrays Created - Set Breakpoint Here ===\n");
        return 0;
    }
}

// VALIDATION EXPECTED RESULTS:
// - fruits should show: @["apple", "banana", "cherry", "date"] NOT comma, 4, 4
// - mixed should show proper types: @["text", 42, 3.14, 1] 
// - nested should show proper nesting without corruption
// - objects should show dictionary representations
// - large array should show item_0, item_1, item_2 without corruption
// - Individual elements [0], [1], etc. should display correct values