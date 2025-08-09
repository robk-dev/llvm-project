#import <Foundation/Foundation.h>
#import <sys/time.h>

double getCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int main() {
    @autoreleasepool {
        printf("=== NSIndexSet Formatter Validation ===\n\n");
        
        // Test Case 1: Empty index set
        NSIndexSet *emptySet = [NSIndexSet indexSet];
        
        // Test Case 2: Single index
        NSIndexSet *singleIndex = [NSIndexSet indexSetWithIndex:42];
        
        // Test Case 3: Small contiguous range
        NSIndexSet *smallRange = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(10, 5)];
        
        // Test Case 4: Large contiguous range  
        NSIndexSet *largeRange = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, 1000)];
        
        // Test Case 5: Scattered indexes (using NSMutableIndexSet)
        NSMutableIndexSet *scatteredSet = [NSMutableIndexSet indexSet];
        [scatteredSet addIndex:1];
        [scatteredSet addIndex:5];
        [scatteredSet addIndex:12];
        [scatteredSet addIndex:20];
        [scatteredSet addIndex:100];
        
        // Test Case 6: Multiple contiguous ranges
        NSMutableIndexSet *multiRangeSet = [NSMutableIndexSet indexSet];
        [multiRangeSet addIndexesInRange:NSMakeRange(1, 3)]; // 1,2,3
        [multiRangeSet addIndexesInRange:NSMakeRange(10, 2)]; // 10,11  
        [multiRangeSet addIndexesInRange:NSMakeRange(20, 4)]; // 20,21,22,23
        
        // Test Case 7: Edge case - Index at 0
        NSIndexSet *zeroIndex = [NSIndexSet indexSetWithIndex:0];
        
        // Test Case 8: Null object test
        NSIndexSet *nullSet = nil;
        
        // Test Case 9: Performance test - Large sparse set
        NSMutableIndexSet *largeSet = [NSMutableIndexSet indexSet];
        for (int i = 0; i < 100; i++) {
            [largeSet addIndex:(i * 1000)]; // Add indexes: 0, 1000, 2000, ..., 99000
        }
        
        printf("All test objects created successfully.\n");
        printf("emptySet count: %lu\n", [emptySet count]);
        printf("singleIndex count: %lu\n", [singleIndex count]);  
        printf("smallRange count: %lu\n", [smallRange count]);
        printf("largeRange count: %lu\n", [largeRange count]);
        printf("scatteredSet count: %lu\n", [scatteredSet count]);
        printf("multiRangeSet count: %lu\n", [multiRangeSet count]);
        printf("zeroIndex count: %lu\n", [zeroIndex count]);
        printf("largeSet count: %lu\n", [largeSet count]);
        
        // Performance test: Create and format many IndexSets
        printf("\nPerformance test: Creating 1000 IndexSets...\n");
        double startTime = getCurrentTime();
        
        for (int i = 0; i < 1000; i++) {
            NSIndexSet *tempSet = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(i, 10)];
            (void)tempSet; // Prevent optimization
        }
        
        double endTime = getCurrentTime();
        double elapsed = endTime - startTime;
        printf("Created 1000 IndexSets in %.6f seconds (%.3f ms average)\n", elapsed, (elapsed * 1000.0) / 1000.0);
        
        // Validation complete
        printf("\nReady for LLDB validation. All objects in scope.\n");
        printf("Set breakpoint after this line to test formatter output.\n"); // BREAKPOINT HERE
        
        return 0;
    }
}