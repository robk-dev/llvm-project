#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Create a simple NSMutableSet
        NSMutableSet *testSet = [[NSMutableSet alloc] init];
        
        // Add some elements
        [testSet addObject:@"First"];
        [testSet addObject:@"Second"];
        [testSet addObject:@"Third"];
        [testSet addObject:@"Fourth"];
        
        NSLog(@"Set count: %lu", (unsigned long)[testSet count]); // BREAKPOINT HERE
        
        // Test with NSSet
        NSSet *immutableSet = [testSet copy];
        
        NSLog(@"Immutable set count: %lu", (unsigned long)[immutableSet count]);
        
        return 0;
    }
}