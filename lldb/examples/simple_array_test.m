// Simple test for NSArray formatter
#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Create a simple array with strings
    NSArray *stringArray = [NSArray arrayWithObjects:
        @"First", 
        @"Second", 
        @"Third", 
        nil];
    
    // Create an empty array
    NSArray *emptyArray = [NSArray array];
    
    // Create a mutable array and add some items
    NSMutableArray *mutableArray = [NSMutableArray array];
    [mutableArray addObject:@"Apple"];
    [mutableArray addObject:@"Banana"];
    [mutableArray addObject:@"Cherry"];
    [mutableArray addObject:@"Date"];
    [mutableArray addObject:@"Elderberry"];
    
    printf("Arrays created. Set a breakpoint here to inspect.\n"); // Line 24 - breakpoint here
    
    // Access elements to test
    NSString *first = [stringArray objectAtIndex:0];
    NSString *second = [stringArray objectAtIndex:1];
    printf("First: %s, Second: %s\n", [first UTF8String], [second UTF8String]);
    
    [pool drain];
    return 0;
}