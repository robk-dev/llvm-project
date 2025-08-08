#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Simple test sets
    NSSet *emptySet = [NSSet set];
    NSSet *simpleSet = [NSSet setWithObjects:@"Apple", @"Banana", @"Cherry", nil];
    NSMutableSet *mutableSet = [NSMutableSet setWithObjects:@"One", @"Two", @"Three", nil];
    
    NSLog(@"Sets created");
    NSLog(@"Empty set: %@", emptySet);
    NSLog(@"Simple set: %@", simpleSet);
    NSLog(@"Mutable set: %@", mutableSet);
    NSLog(@"Done"); // Set breakpoint here

    [pool drain];
    return 0;
}