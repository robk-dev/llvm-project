#import <Foundation/Foundation.h>
#import <stdio.h>

int main(int argc, char *argv[]) {
  @autoreleasepool {
    // Test 1: Empty dictionary
    NSDictionary *emptyDict = [[NSDictionary alloc] init];
    NSLog(@"Empty dictionary: %@", emptyDict);
    
    // Test 2: Dictionary with one key-value pair
    NSDictionary *singleDict = @{@"name": @"John"};
    NSLog(@"Single pair dictionary: %@", singleDict);
    
    // Test 3: Dictionary with multiple key-value pairs
    NSDictionary *multiDict = @{
      @"name": @"Alice",
      @"age": @25,
      @"city": @"Boston",
      @"active": @YES
    };
    NSLog(@"Multiple pairs dictionary: %@", multiDict);
    
    // Test 4: Nested dictionary
    NSDictionary *nestedDict = @{
      @"user": @{
        @"firstName": @"Bob",
        @"lastName": @"Smith"
      },
      @"settings": @{
        @"theme": @"dark",
        @"notifications": @YES
      }
    };
    NSLog(@"Nested dictionary: %@", nestedDict);
    
    // Test 5: Mutable dictionary
    NSMutableDictionary *mutableDict = [NSMutableDictionary dictionary];
    [mutableDict setObject:@"value1" forKey:@"key1"];
    [mutableDict setObject:@"value2" forKey:@"key2"];
    [mutableDict setObject:@42 forKey:@"number"];
    NSLog(@"Mutable dictionary: %@", mutableDict);
    
    // Test 6: Dictionary with mixed types
    NSDictionary *mixedDict = @{
      @"string": @"Hello",
      @"number": @123,
      @"float": @3.14,
      @"array": @[@1, @2, @3],
      @"bool": @NO,
      @"null": [NSNull null]
    };
    NSLog(@"Mixed types dictionary: %@", mixedDict);
    
    // Set a breakpoint here for debugging
    printf("Set breakpoint here to inspect dictionaries\n");
    
    // Test 7: Large dictionary
    NSMutableDictionary *largeDict = [NSMutableDictionary dictionary];
    for (int i = 0; i < 100; i++) {
      NSString *key = [NSString stringWithFormat:@"key_%d", i];
      NSString *value = [NSString stringWithFormat:@"value_%d", i];
      [largeDict setObject:value forKey:key];
    }
    NSLog(@"Large dictionary has %lu entries", (unsigned long)[largeDict count]);
    
    // Test 8: Dictionary with nil value handling
    NSMutableDictionary *nilTestDict = [NSMutableDictionary dictionary];
    [nilTestDict setObject:@"valid" forKey:@"key1"];
    // This would cause exception: [nilTestDict setObject:nil forKey:@"key2"];
    [nilTestDict setObject:[NSNull null] forKey:@"key2"]; // Use NSNull for nil
    NSLog(@"Dictionary with NSNull: %@", nilTestDict);
    
    printf("Dictionary tests completed. Examine with LLDB.\n");
    
    return 0;
  }
}