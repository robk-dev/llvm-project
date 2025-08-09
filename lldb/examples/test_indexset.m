#import <Foundation/Foundation.h>

int main() {
  @autoreleasepool {
    // Create various NSIndexSet instances for testing
    NSIndexSet *emptySet = [NSIndexSet indexSet];
    NSIndexSet *singleIndex = [NSIndexSet indexSetWithIndex:42];
    NSIndexSet *rangeSet = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(10, 5)];
    
    NSMutableIndexSet *mutableSet = [NSMutableIndexSet indexSet];
    [mutableSet addIndex:1];
    [mutableSet addIndex:3];
    [mutableSet addIndex:5];
    
    printf("Created NSIndexSet test objects\n");
    printf("emptySet: %p\n", emptySet);
    printf("singleIndex: %p\n", singleIndex);
    printf("rangeSet: %p\n", rangeSet);
    printf("mutableSet: %p\n", mutableSet);
    
    return 0; // Breakpoint here
  }
}