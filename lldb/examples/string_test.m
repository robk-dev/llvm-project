#import <Foundation/Foundation.h>

int main(void) {
    @autoreleasepool {
        // Test different string types
        NSString *constantString = @"Hello World";  // NSConstantString
        NSString *mutableString = [@"Mutable" mutableCopy];  // NSMutableString
        NSArray *fruits = @[@"apple", @"banana", @"cherry"];  // GSTinyString tagged pointers
        
        // This line is for setting a breakpoint
        NSLog(@"Testing string formatting: %@", constantString);
        
        return 0;
    }
}