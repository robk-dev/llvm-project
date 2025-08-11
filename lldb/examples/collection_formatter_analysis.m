#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        printf("=== Collection Formatter Analysis Test ===\n");
        
        // Empty collections
        NSArray *emptyArray = @[];
        NSDictionary *emptyDict = @{};
        NSSet *emptySet = [NSSet set];
        
        // Small collections (should show inline elements)
        NSArray *smallArray = @[@"one", @2, @3];
        NSDictionary *smallDict = @{@"key1": @"value1", @"key2": @42};
        NSSet *smallSet = [NSSet setWithObjects:@"apple", @"banana", nil];
        
        // Large collections (should truncate)
        NSMutableArray *largeArray = [NSMutableArray array];
        for (int i = 1; i <= 10; i++) {
            [largeArray addObject:[NSString stringWithFormat:@"item%d", i]];
        }
        
        NSMutableDictionary *largeDict = [NSMutableDictionary dictionary];
        for (int i = 1; i <= 8; i++) {
            [largeDict setObject:[NSString stringWithFormat:@"value%d", i] 
                          forKey:[NSString stringWithFormat:@"key%d", i]];
        }
        
        // Nested collections
        NSArray *nestedArray = @[@"outer", @[@"inner1", @"inner2"], @"end"];
        NSDictionary *nestedDict = @{@"data": smallArray, @"count": @3};
        
        // Mixed types
        NSArray *mixedArray = @[@"string", @42, @YES, @NO, [NSNull null]];
        NSDictionary *mixedDict = @{
            @"string": @"hello",
            @"number": @3.14,
            @"bool": @YES,
            @"array": @[@1, @2],
            @"null": [NSNull null]
        };
        
        // BREAKPOINT HERE - we'll examine these with LLDB
        printf("Collections created, ready for debugging\n");
        
        return 0;
    }
}