//===-- test_orderedset.m ------------------------------------------------===//
//
// Test program for NSOrderedSet formatter validation
//
// This program creates various NSOrderedSet instances to test the GNUstep
// NSOrderedSet formatter implementation. It demonstrates:
// - Empty ordered sets
// - Small ordered sets with mixed types
// - Large ordered sets  
// - Mutable vs immutable ordered sets
// - Nested collections
// - Tagged pointer handling
// - Uniqueness constraint behavior
//
// Compile with:
// clang -fobjc-runtime=gnustep-2.1 -fblocks -fno-strict-aliasing \
//       -fexceptions -fobjc-exceptions -g -gdwarf-5 -O0 \
//       -fno-omit-frame-pointer -I/usr/local/include/GNUstep \
//       -I/usr/include/GNUstep -fconstant-string-class=NSConstantString \
//       -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -DDEBUG=1 \
//       -L/usr/local/lib -Wl,-rpath,/usr/local/lib -g \
//       -o test_orderedset test_orderedset.m \
//       -lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm
//
//===----------------------------------------------------------------------===//

#import <Foundation/Foundation.h>
#import <stdio.h>

// Helper function to create test data
NSArray* CreateTestNumbers() {
    return @[@42, @3.14159, @-123, @0, @999999];
}

NSArray* CreateTestStrings() {
    return @[@"Apple", @"Banana", @"Cherry", @"Date", @"Elderberry"];
}

NSArray* CreateTestMixed() {
    return @[@"String1", @42, @3.14, @"String2", @100];
}

void TestEmptyOrderedSet() {
    printf("\n=== Testing Empty NSOrderedSet ===\n");
    
    NSOrderedSet *emptySet = [NSOrderedSet orderedSet];
    printf("Empty ordered set: %p (count: %lu)\n", 
           (void*)emptySet, (unsigned long)[emptySet count]);
    
    NSMutableOrderedSet *emptyMutableSet = [NSMutableOrderedSet orderedSet];
    printf("Empty mutable ordered set: %p (count: %lu)\n", 
           (void*)emptyMutableSet, (unsigned long)[emptyMutableSet count]);
}

void TestSmallOrderedSet() {
    printf("\n=== Testing Small NSOrderedSet ===\n");
    
    // Test with numbers
    NSArray *numbers = CreateTestNumbers();
    NSOrderedSet *numberSet = [NSOrderedSet orderedSetWithArray:numbers];
    printf("Number ordered set: %p (count: %lu)\n", 
           (void*)numberSet, (unsigned long)[numberSet count]);
    
    // Test with strings
    NSArray *strings = CreateTestStrings();
    NSOrderedSet *stringSet = [NSOrderedSet orderedSetWithArray:strings];
    printf("String ordered set: %p (count: %lu)\n", 
           (void*)stringSet, (unsigned long)[stringSet count]);
    
    // Test with mixed types
    NSArray *mixed = CreateTestMixed();
    NSOrderedSet *mixedSet = [NSOrderedSet orderedSetWithArray:mixed];
    printf("Mixed ordered set: %p (count: %lu)\n", 
           (void*)mixedSet, (unsigned long)[mixedSet count]);
    
    // Test order preservation
    printf("First element: %p\n", (void*)[numberSet firstObject]);
    printf("Last element: %p\n", (void*)[numberSet lastObject]);
    printf("Element at index 2: %p\n", (void*)[numberSet objectAtIndex:2]);
}

void TestMutableOrderedSet() {
    printf("\n=== Testing NSMutableOrderedSet ===\n");
    
    NSMutableOrderedSet *mutableSet = [NSMutableOrderedSet orderedSet];
    
    // Add elements one by one
    [mutableSet addObject:@"First"];
    [mutableSet addObject:@42];
    [mutableSet addObject:@"Second"];
    printf("After adding 3 elements: %p (count: %lu)\n", 
           (void*)mutableSet, (unsigned long)[mutableSet count]);
    
    // Test uniqueness constraint - adding duplicate should not increase count
    [mutableSet addObject:@"First"]; // Duplicate - should not be added again
    printf("After adding duplicate: %p (count: %lu)\n", 
           (void*)mutableSet, (unsigned long)[mutableSet count]);
    
    // Insert at specific index
    [mutableSet insertObject:@"Inserted" atIndex:1];
    printf("After inserting at index 1: %p (count: %lu)\n", 
           (void*)mutableSet, (unsigned long)[mutableSet count]);
    
    // Remove object
    [mutableSet removeObject:@42];
    printf("After removing number: %p (count: %lu)\n", 
           (void*)mutableSet, (unsigned long)[mutableSet count]);
}

void TestLargeOrderedSet() {
    printf("\n=== Testing Large NSOrderedSet ===\n");
    
    NSMutableArray *largeArray = [NSMutableArray array];
    
    // Create array with 50 elements
    for (int i = 0; i < 50; i++) {
        [largeArray addObject:[NSString stringWithFormat:@"Element_%d", i]];
    }
    
    NSOrderedSet *largeSet = [NSOrderedSet orderedSetWithArray:largeArray];
    printf("Large ordered set: %p (count: %lu)\n", 
           (void*)largeSet, (unsigned long)[largeSet count]);
    
    // Test some specific elements
    printf("Element at index 0: %p\n", (void*)[largeSet objectAtIndex:0]);
    printf("Element at index 25: %p\n", (void*)[largeSet objectAtIndex:25]);
    printf("Element at index 49: %p\n", (void*)[largeSet objectAtIndex:49]);
}

void TestNestedCollections() {
    printf("\n=== Testing Nested Collections in NSOrderedSet ===\n");
    
    // Create nested collections
    NSArray *innerArray = @[@"A", @"B", @"C"];
    NSSet *innerSet = [NSSet setWithObjects:@"X", @"Y", @"Z", nil];
    NSDictionary *innerDict = @{@"key1": @"value1", @"key2": @"value2"};
    
    NSOrderedSet *nestedSet = [NSOrderedSet orderedSetWithObjects:
                               innerArray, innerSet, innerDict, @"string", @123, nil];
    printf("Nested collections ordered set: %p (count: %lu)\n", 
           (void*)nestedSet, (unsigned long)[nestedSet count]);
    
    // Test each nested element
    printf("Inner array: %p\n", (void*)[nestedSet objectAtIndex:0]);
    printf("Inner set: %p\n", (void*)[nestedSet objectAtIndex:1]);
    printf("Inner dictionary: %p\n", (void*)[nestedSet objectAtIndex:2]);
    printf("String element: %p\n", (void*)[nestedSet objectAtIndex:3]);
    printf("Number element: %p\n", (void*)[nestedSet objectAtIndex:4]);
}

void TestTaggedPointers() {
    printf("\n=== Testing Tagged Pointers in NSOrderedSet ===\n");
    
    // Create ordered set with likely tagged pointers
    NSOrderedSet *taggedSet = [NSOrderedSet orderedSetWithObjects:
                               @"Short", @42, @3.14f, @YES, @NO, 
                               @0, @1, @(-1), @"", @"A", nil];
    printf("Tagged pointers ordered set: %p (count: %lu)\n", 
           (void*)taggedSet, (unsigned long)[taggedSet count]);
    
    // Access individual tagged elements
    for (NSUInteger i = 0; i < [taggedSet count]; i++) {
        id obj = [taggedSet objectAtIndex:i];
        printf("Tagged element [%lu]: %p\n", (unsigned long)i, (void*)obj);
    }
}

void TestUniquenessConstraint() {
    printf("\n=== Testing Uniqueness Constraint ===\n");
    
    // Create array with duplicates
    NSArray *arrayWithDuplicates = @[@"A", @"B", @"C", @"A", @"B", @"D"];
    printf("Original array count: %lu\n", (unsigned long)[arrayWithDuplicates count]);
    
    // Create ordered set - should remove duplicates but preserve order
    NSOrderedSet *uniqueSet = [NSOrderedSet orderedSetWithArray:arrayWithDuplicates];
    printf("Ordered set from array with duplicates: %p (count: %lu)\n", 
           (void*)uniqueSet, (unsigned long)[uniqueSet count]);
    
    // Verify first occurrences are preserved
    printf("First element: %p\n", (void*)[uniqueSet objectAtIndex:0]); // Should be "A"
    printf("Second element: %p\n", (void*)[uniqueSet objectAtIndex:1]); // Should be "B"  
    printf("Third element: %p\n", (void*)[uniqueSet objectAtIndex:2]);  // Should be "C"
    printf("Fourth element: %p\n", (void*)[uniqueSet objectAtIndex:3]); // Should be "D"
}

void TestOrderedSetConversion() {
    printf("\n=== Testing NSOrderedSet Conversions ===\n");
    
    NSOrderedSet *originalSet = [NSOrderedSet orderedSetWithObjects:
                                 @"First", @42, @"Second", @3.14, nil];
    printf("Original ordered set: %p (count: %lu)\n", 
           (void*)originalSet, (unsigned long)[originalSet count]);
    
    // Convert to array - should preserve order
    NSArray *arrayFromSet = [originalSet array];
    printf("Array from ordered set: %p (count: %lu)\n", 
           (void*)arrayFromSet, (unsigned long)[arrayFromSet count]);
    
    // Convert to set - loses order but gains set operations
    NSSet *setFromOrderedSet = [originalSet set];
    printf("Set from ordered set: %p (count: %lu)\n", 
           (void*)setFromOrderedSet, (unsigned long)[setFromOrderedSet count]);
    
    // Convert back to ordered set
    NSOrderedSet *backToOrderedSet = [NSOrderedSet orderedSetWithSet:setFromOrderedSet];
    printf("Back to ordered set: %p (count: %lu)\n", 
           (void*)backToOrderedSet, (unsigned long)[backToOrderedSet count]);
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== GNUstep NSOrderedSet Formatter Test Program ===\n");
        printf("This program tests various NSOrderedSet scenarios for debugging.\n");
        
        TestEmptyOrderedSet();
        TestSmallOrderedSet();
        TestMutableOrderedSet();
        TestLargeOrderedSet();
        TestNestedCollections();
        TestTaggedPointers();
        TestUniquenessConstraint();
        TestOrderedSetConversion();
        
        printf("\n=== Test Complete - Set breakpoint and examine objects ===\n");
        printf("Recommended LLDB commands:\n");
        printf("  (lldb) b test_orderedset.m:200\n");  // Set breakpoint at end
        printf("  (lldb) po emptySet\n");
        printf("  (lldb) po numberSet\n");
        printf("  (lldb) po stringSet\n");
        printf("  (lldb) po mixedSet\n");
        printf("  (lldb) po mutableSet\n");
        printf("  (lldb) po largeSet\n");
        printf("  (lldb) po nestedSet\n");
        printf("  (lldb) po taggedSet\n");
        printf("  (lldb) po uniqueSet\n");
        printf("\n");
        
        // Keep variables alive for debugging
        NSOrderedSet *finalSet = [NSOrderedSet orderedSetWithObjects:
                                  @"Debug", @"Me", @"Please", nil];
        
        return 0; // Breakpoint here for final testing
    }
}