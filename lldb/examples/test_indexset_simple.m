#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Test NSIndexSet
        NSIndexSet *emptyIndexSet = [NSIndexSet indexSet];
        NSIndexSet *singleIndexSet = [NSIndexSet indexSetWithIndex:5];
        NSIndexSet *rangeIndexSet = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(10, 5)];
        NSMutableIndexSet *mutableIndexSet = [NSMutableIndexSet indexSet];
        [mutableIndexSet addIndex:1];
        [mutableIndexSet addIndex:3];
        [mutableIndexSet addIndex:7];
        
        printf("Ready for LLDB inspection\n"); // SET BREAKPOINT HERE
        
        return 0;
    }
}