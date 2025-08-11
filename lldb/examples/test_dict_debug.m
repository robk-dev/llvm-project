#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Test with simple constant string keys
        NSDictionary *dict = @{
            @"name": @"John",
            @"occupation": @"Developer",
            @"age": @42
        };
        
        NSLog(@"Dictionary created: %@", dict);
        
        // Set a breakpoint here to inspect the dictionary
        NSLog(@"Break here"); // Line 14
        
        return 0;
    }
}