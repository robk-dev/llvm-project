#import <Foundation/Foundation.h>

// Test all GNUstep formatters comprehensively
int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Test strings
    NSString *strings[] = {
        @"",                          // Empty
        @"Hello",                     // ASCII
        @"Unicode: 你好 👋",          // UTF-8 with emoji
        @"Hi",                        // Tagged
        nil
    };
    
    // Test numbers
    NSNumber *numbers[] = {
        [NSNumber numberWithInt:0],
        [NSNumber numberWithInt:42],
        [NSNumber numberWithInt:-17],
        [NSNumber numberWithFloat:3.14159f],
        [NSNumber numberWithDouble:2.71828],
        [NSNumber numberWithBool:YES],
        [NSNumber numberWithBool:NO],
        nil
    };
    
    // Test arrays
    NSArray *arrays[] = {
        [NSArray array],
        @[@"One"],
        @[@"A", @"B", @"C"],
        @[@1, @2, @3, @4, @5],
        nil
    };
    
    // Test dictionaries
    NSDictionary *dicts[] = {
        [NSDictionary dictionary],
        @{@"key": @"value"},
        @{@"name": @"John", @"age": @30, @"city": @"NYC"},
        nil
    };
    
    // Test sets
    NSSet *sets[] = {
        [NSSet set],
        [NSSet setWithObject:@"Single"],
        [NSSet setWithObjects:@"Red", @"Green", @"Blue", nil],
        nil
    };
    
    // Large collections for performance
    NSMutableArray *largeArray = [NSMutableArray array];
    for (int i = 0; i < 10000; i++) {
        [largeArray addObject:@(i)];
    }
    
    NSMutableDictionary *largeDict = [NSMutableDictionary dictionary];
    for (int i = 0; i < 5000; i++) {
        NSString *key = [NSString stringWithFormat:@"k%d", i];
        [largeDict setObject:@(i) forKey:key];
    }
    
    // Print marker for debugger
    NSLog(@"All test objects created. Set breakpoint here.");
    
    // Keep objects alive
    NSLog(@"Strings: %p", strings);
    NSLog(@"Numbers: %p", numbers);
    NSLog(@"Arrays: %p", arrays);
    NSLog(@"Dicts: %p", dicts);
    NSLog(@"Sets: %p", sets);
    NSLog(@"Large: %lu, %lu", [largeArray count], [largeDict count]);
    
    [pool drain];
    return 0;
}
