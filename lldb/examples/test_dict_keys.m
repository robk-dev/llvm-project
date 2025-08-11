#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Test dictionary with string literal keys
        NSDictionary *dict = @{
            @"name": @"John",
            @"occupation": @"Developer",
            @"city": @"NYC"
        };
        
        NSLog(@"Dictionary: %@", dict);
        NSLog(@"Breaking here to test key display...");
        
        // Set breakpoint on next line
        return 0;  // BREAKPOINT HERE
    }
}