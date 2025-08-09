#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Create test objects
        NSIndexSet *emptySet = [NSIndexSet indexSet];
        NSIndexSet *singleIndex = [NSIndexSet indexSetWithIndex:42];  
        NSIndexSet *rangeSet = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(10, 5)];
        
        printf("Objects created:\n");
        printf("emptySet = %p\n", emptySet);
        printf("singleIndex = %p\n", singleIndex);
        printf("rangeSet = %p\n", rangeSet);
        
        // Stop here for debugging
        printf("Ready for debugging\n"); // Breakpoint here
        
        return 0;
    }
}