#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== Testing Collection Count Issue ===\n");
        
        // Create test arrays with known counts
        NSArray *emptyArray = @[];
        NSArray *singleArray = @[@42];
        NSArray *smallArray = @[@1, @2, @3];
        NSArray *numberArray = @[@10, @20, @30, @40, @50];
        
        printf("emptyArray created: %p (expected count: 0)\n", emptyArray);
        printf("singleArray created: %p (expected count: 1)\n", singleArray);
        printf("smallArray created: %p (expected count: 3)\n", smallArray);
        printf("numberArray created: %p (expected count: 5)\n", numberArray);
        
        // Create test sets with known counts
        NSSet *emptySet = [NSSet set];
        NSSet *singleSet = [NSSet setWithObject:@42];
        NSSet *smallSet = [NSSet setWithObjects:@1, @2, @3, nil];
        NSSet *numberSet = [NSSet setWithObjects:@10, @20, @30, @40, @50, nil];
        
        printf("emptySet created: %p (expected count: 0)\n", emptySet);
        printf("singleSet created: %p (expected count: 1)\n", singleSet);
        printf("smallSet created: %p (expected count: 3)\n", smallSet);
        printf("numberSet created: %p (expected count: 5)\n", numberSet);
        
        printf("Ready for LLDB debugging - SET BREAKPOINT HERE\n"); // SET BREAKPOINT HERE
        
        return 0;
    }
}