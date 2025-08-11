#import <Foundation/Foundation.h>

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Create an array with nil elements - this is causing infinite recursion
    NSArray *arrayWithNils = @[@"first", [NSNull null], @"third"];
    
    // Also test other problematic cases
    NSArray *emptyArray = @[];
    NSArray *nullArray = nil;
    NSArray *arrayWithOneNil = @[[NSNull null]];
    
    // Test array with mixed content that might cause recursion
    NSArray *mixedArray = @[@"string", @42, [NSNull null], @[@"nested"]];
    
    // Set a breakpoint here to test the formatters
    printf("Arrays created successfully\n");
    
    [pool release];
    return 0;
}