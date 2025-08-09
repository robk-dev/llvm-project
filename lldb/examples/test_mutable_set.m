#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test NSMutableSet
        NSMutableSet *mutableSet = [[NSMutableSet alloc] init];
        
        // Add some elements
        [mutableSet addObject:@"First"];
        [mutableSet addObject:@"Second"];
        [mutableSet addObject:@"Third"];
        [mutableSet addObject:@42];
        
        NSLog(@"Mutable set created with %lu objects", (unsigned long)[mutableSet count]); // BREAKPOINT HERE
        
        // Test immutable set
        NSSet *immutableSet = [NSSet setWithObjects:@"Apple", @"Banana", @"Cherry", nil];
        
        NSLog(@"Immutable set created with %lu objects", (unsigned long)[immutableSet count]);
        
        // Keep objects alive
        NSLog(@"Sets ready for inspection");
        
        return 0;
    }
}