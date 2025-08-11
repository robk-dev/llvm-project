#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Test basic Foundation objects
        NSString *string = @"Hello, World!";
        NSNumber *number = @42;
        NSArray *array = @[@"Apple", @"Banana", @"Cherry"];
        NSDictionary *dict = @{@"name": @"John", @"age": @25};
        
        // Test manual description calls to see if they work
        NSString *string_desc = [string description];
        NSString *number_desc = [number description];
        NSString *array_desc = [array description];
        NSString *dict_desc = [dict description];
        
        printf("String description: %s\n", [string_desc UTF8String]);
        printf("Number description: %s\n", [number_desc UTF8String]);
        printf("Array description: %s\n", [array_desc UTF8String]);
        printf("Dict description: %s\n", [dict_desc UTF8String]);
        
        // Breakpoint location for LLDB testing
        printf("All objects created successfully - BREAKPOINT HERE\n"); // Line 21
        
        return 0;
    }
}