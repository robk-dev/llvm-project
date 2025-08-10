#include <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Simple dictionary test
        NSDictionary *simpleDict = @{
            @"name": @"John",
            @"age": @30
        };
        
        NSLog(@"Simple dict: %@", simpleDict);
        
        // Add a breakpoint here for debugging
        NSLog(@"Debug point reached");
        
        return 0;
    }
}