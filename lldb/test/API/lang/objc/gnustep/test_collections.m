#import <Foundation/Foundation.h>

void test_corrupted_collections() {
    NSArray *nilArray = nil;
    NSDictionary *nilDict = nil;
    NSSet *nilSet = nil;
    
    // Create corrupted collections (simulated)
    // These would actually need special handling to truly corrupt
    NSArray *corruptedArray = nil;
    NSDictionary *corruptedDict = nil;
    NSSet *corruptedSet = nil;
    
    printf("Testing corrupted collections\n");
}

void test_large_collections() {
    NSMutableArray *veryLargeArray = [NSMutableArray array];
    NSMutableDictionary *veryLargeDict = [NSMutableDictionary dictionary];
    NSMutableSet *veryLargeSet = [NSMutableSet set];
    
    for (int i = 0; i < 10000; i++) {
        [veryLargeArray addObject:@(i)];
        [veryLargeDict setObject:@(i) forKey:[NSString stringWithFormat:@"key%d", i]];
        [veryLargeSet addObject:@(i)];
    }
    
    printf("Testing large collections\n");
}

void test_child_access() {
    NSArray *testArray = @[@"First", @"Second", @"Third"];
    NSDictionary *testDict = @{@"name": @"John", @"age": @30, @"city": @"NYC"};
    NSSet *testSet = [NSSet setWithObjects:@"A", @"B", @"C", nil];
    
    NSArray *nestedArray = @[@[@"Inner"]];
    NSDictionary *nestedDict = @{@"user": @{@"name": @"Alice"}};
    
    printf("Testing child access\n");
}

int main() {
    @autoreleasepool {
        // Array testing
        NSArray *emptyArray = @[];
        NSArray *singleArray = @[@"Hello"];
        NSArray *multiArray = @[@"Apple", @"Banana", @"Cherry"];
        
        NSMutableArray *largeArray = [NSMutableArray array];
        for (int i = 0; i < 100; i++) {
            [largeArray addObject:@(i)];
        }
        
        NSArray *nestedArray = @[@[@1, @2], @[@3, @4]];
        NSMutableArray *mutableArray = [NSMutableArray arrayWithArray:multiArray];
        
        NSArray *mixedArray = @[@"String", @42, @{@"key": @"value"}, [NSNull null]];
        
        printf("Array testing\n"); // Break here for array testing
        
        // Dictionary testing
        NSDictionary *emptyDict = @{};
        NSDictionary *singleDict = @{@"key": @"value"};
        NSDictionary *multiDict = @{@"name": @"John", @"age": @30, @"city": @"NYC"};
        
        NSMutableDictionary *largeDict = [NSMutableDictionary dictionary];
        for (int i = 0; i < 100; i++) {
            largeDict[[NSString stringWithFormat:@"key%d", i]] = @(i);
        }
        
        NSDictionary *nestedDict = @{@"user": @{@"name": @"Alice", @"age": @25}};
        NSMutableDictionary *mutableDict = [NSMutableDictionary dictionaryWithDictionary:multiDict];
        
        NSDictionary *mixedKeysDict = @{@"string": @1, @42: @"number key"};
        
        printf("Dictionary testing\n"); // Break here for dictionary testing
        
        // Set testing
        NSSet *emptySet = [NSSet set];
        NSSet *singleSet = [NSSet setWithObject:@"One"];
        NSSet *multiSet = [NSSet setWithObjects:@"A", @"B", @"C", nil];
        
        NSMutableSet *largeSet = [NSMutableSet set];
        for (int i = 0; i < 100; i++) {
            [largeSet addObject:@(i)];
        }
        
        NSMutableSet *mutableSet = [NSMutableSet setWithSet:multiSet];
        
        NSSet *mixedSet = [NSSet setWithObjects:@"String", @42, [NSNull null], nil];
        
        // Test uniqueness
        NSSet *uniqueSet = [NSSet setWithObjects:@"A", @"B", @"C", @"A", @"B", nil];
        
        printf("Set testing\n"); // Break here for set testing
        
        // Additional tests
        test_corrupted_collections();
        test_large_collections();
        test_child_access();
    }
    
    return 0;
}