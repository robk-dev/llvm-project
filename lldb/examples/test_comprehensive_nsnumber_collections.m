#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== Testing Comprehensive NSNumber Collections ===\n");
        
        // Test various NSNumber types in arrays
        NSArray *intArray = @[@1, @2, @3, @(-5), @100];
        NSArray *floatArray = @[@1.5f, @2.7f, @(-3.14f)];
        NSArray *doubleArray = @[@1.5, @2.7, @(-3.14)];
        NSArray *boolArray = @[@YES, @NO, @YES];
        NSArray *longLongArray = @[@1000000000LL, @(-2000000000LL)];
        
        // Test mixed array with different NSNumber types
        NSArray *mixedArray = @[@42, @3.14, @YES, @(-100LL)];
        
        // Test various NSNumber types in sets
        NSSet *intSet = [NSSet setWithArray:@[@10, @20, @30, @(-50), @1000]];
        NSSet *floatSet = [NSSet setWithArray:@[@1.1f, @2.2f, @(-3.3f)]];
        NSSet *doubleSet = [NSSet setWithArray:@[@1.1, @2.2, @(-3.3)]];
        NSSet *boolSet = [NSSet setWithArray:@[@YES, @NO]];
        NSSet *longLongSet = [NSSet setWithArray:@[@5000000000LL, @(-6000000000LL)]];
        
        // Test mixed set with different NSNumber types
        NSSet *mixedSet = [NSSet setWithArray:@[@123, @2.718, @NO, @(-999LL)]];
        
        // Test edge cases
        NSArray *edgeArray = @[@0, @(-0.0), @INFINITY, @(-INFINITY)];
        NSSet *edgeSet = [NSSet setWithArray:@[@0, @1]]; // NaN would be problematic
        
        printf("Arrays created - intArray: %p, floatArray: %p, mixedArray: %p\n", intArray, floatArray, mixedArray);
        printf("Sets created - intSet: %p, floatSet: %p, mixedSet: %p\n", intSet, floatSet, mixedSet);
        printf("Edge cases - edgeArray: %p, edgeSet: %p\n", edgeArray, edgeSet);
        
        printf("Ready for comprehensive NSNumber testing - SET BREAKPOINT HERE\n"); // SET BREAKPOINT HERE
        
        return 0;
    }
}