#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        printf("=== NSIndexSet Comprehensive Test Program ===\n\n");
        
        // Test Case 1: Empty index set
        NSIndexSet *emptySet = [NSIndexSet indexSet];
        printf("Test 1 - Empty set:\n");
        printf("  emptySet: %p, count: %lu\n", emptySet, (unsigned long)[emptySet count]);
        printf("  Expected LLDB output: \"0 indexes\"\n\n");
        
        // Test Case 2: Single index
        NSIndexSet *singleIndex = [NSIndexSet indexSetWithIndex:42];
        printf("Test 2 - Single index:\n");
        printf("  singleIndex: %p, count: %lu\n", singleIndex, (unsigned long)[singleIndex count]);
        printf("  Expected LLDB output: \"1 index: 42\"\n\n");
        
        // Test Case 3: Small contiguous range
        NSIndexSet *smallRange = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(10, 5)];
        printf("Test 3 - Small contiguous range:\n");
        printf("  smallRange: %p, count: %lu\n", smallRange, (unsigned long)[smallRange count]);
        printf("  Contains: [10, 11, 12, 13, 14]\n");
        printf("  Expected LLDB output: \"5 indexes in [10-14]\"\n\n");
        
        // Test Case 4: Large contiguous range  
        NSIndexSet *largeRange = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, 1000)];
        printf("Test 4 - Large contiguous range:\n");
        printf("  largeRange: %p, count: %lu\n", largeRange, (unsigned long)[largeRange count]);
        printf("  Expected LLDB output: \"1000 indexes in [0-999]\"\n\n");
        
        // Test Case 5: Scattered indexes (using NSMutableIndexSet)
        NSMutableIndexSet *scatteredSet = [NSMutableIndexSet indexSet];
        [scatteredSet addIndex:1];
        [scatteredSet addIndex:5];
        [scatteredSet addIndex:12];
        [scatteredSet addIndex:20];
        [scatteredSet addIndex:100];
        printf("Test 5 - Scattered indexes:\n");
        printf("  scatteredSet: %p, count: %lu\n", scatteredSet, (unsigned long)[scatteredSet count]);
        printf("  Contains: 1, 5, 12, 20, 100\n");
        printf("  Expected LLDB output: \"5 indexes\"\n\n");
        
        // Test Case 6: Multiple contiguous ranges
        NSMutableIndexSet *multiRangeSet = [NSMutableIndexSet indexSet];
        [multiRangeSet addIndexesInRange:NSMakeRange(1, 3)]; // 1,2,3
        [multiRangeSet addIndexesInRange:NSMakeRange(10, 2)]; // 10,11  
        [multiRangeSet addIndexesInRange:NSMakeRange(20, 4)]; // 20,21,22,23
        printf("Test 6 - Multiple ranges:\n");
        printf("  multiRangeSet: %p, count: %lu\n", multiRangeSet, (unsigned long)[multiRangeSet count]);
        printf("  Contains: [1-3], [10-11], [20-23]\n");
        printf("  Expected LLDB output: \"9 indexes\"\n\n");
        
        // Test Case 7: Edge case - Index at 0
        NSIndexSet *zeroIndex = [NSIndexSet indexSetWithIndex:0];
        printf("Test 7 - Index at zero:\n");
        printf("  zeroIndex: %p, count: %lu\n", zeroIndex, (unsigned long)[zeroIndex count]);
        printf("  Expected LLDB output: \"1 index: 0\"\n\n");
        
        // Test Case 8: Edge case - Very large index  
        NSIndexSet *largeIndex = [NSIndexSet indexSetWithIndex:UINT64_MAX - 1];
        printf("Test 8 - Very large index:\n");
        printf("  largeIndex: %p, count: %lu\n", largeIndex, (unsigned long)[largeIndex count]);
        printf("  Expected LLDB output: \"1 index: %llu\"\n\n", (unsigned long long)(UINT64_MAX - 1));
        
        // Test Case 9: Performance test - Large sparse set
        NSMutableIndexSet *largeSet = [NSMutableIndexSet indexSet];
        for (int i = 0; i < 100; i++) {
            [largeSet addIndex:(i * 1000)]; // Add indexes: 0, 1000, 2000, ..., 99000
        }
        printf("Test 9 - Performance test (large sparse):\n");
        printf("  largeSet: %p, count: %lu\n", largeSet, (unsigned long)[largeSet count]);
        printf("  Expected LLDB output: \"100 indexes\" (should render in <50ms)\n\n");
        
        // Test Case 10: Null object test
        NSIndexSet *nullSet = nil;
        printf("Test 10 - Null object:\n");
        printf("  nullSet: %p\n", nullSet);
        printf("  Expected LLDB output: \"(null)\"\n\n");

        printf("=== All test objects created. Set breakpoint after this line ===\n");
        
        // Performance timing test
        NSDate *startTime = [NSDate date];
        for (int i = 0; i < 1000; i++) {
            NSIndexSet *tempSet = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(i, 10)];
            (void)tempSet; // Suppress unused warning
        }
        NSTimeInterval elapsed = [[NSDate date] timeIntervalSinceDate:startTime];
        printf("Created 1000 IndexSets in %.3f seconds\n", elapsed);
        
        return 0; // SET BREAKPOINT HERE for LLDB testing
    }
}