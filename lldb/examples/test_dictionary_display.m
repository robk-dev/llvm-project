#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Test 1: Basic dictionary with string keys and values
    NSDictionary *basicDict = @{
      @"name": @"John Doe",
      @"occupation": @"Developer",
      @"city": @"San Francisco"
    };
    
    // Test 2: Dictionary with mixed value types
    NSDictionary *mixedDict = @{
      @"string": @"Hello",
      @"number": @42,
      @"array": @[@"one", @"two", @"three"],
      @"nested": @{@"key": @"value", @"count": @5}
    };
    
    // Test 3: Mutable dictionary
    NSMutableDictionary *mutableDict = [NSMutableDictionary dictionary];
    [mutableDict setObject:@"Alice" forKey:@"firstName"];
    [mutableDict setObject:@"Smith" forKey:@"lastName"];
    [mutableDict setObject:@30 forKey:@"age"];
    [mutableDict setObject:@[@"reading", @"hiking"] forKey:@"hobbies"];
    
    // Test 4: Dictionary with nested collections for drill-down
    NSDictionary *nestedCollections = @{
      @"users": @[@"Alice", @"Bob", @"Charlie"],
      @"preferences": [NSSet setWithObjects:@"Dark Mode", @"Auto-save", nil],
      @"settings": @{
        @"theme": @"dark",
        @"fontSize": @14,
        @"notifications": @YES
      }
    };
    
    // Breakpoint here to test formatters
    NSLog(@"Dictionary formatter test");
    NSLog(@"basicDict: %@", basicDict);
    NSLog(@"mixedDict: %@", mixedDict);
    NSLog(@"mutableDict: %@", mutableDict);
    NSLog(@"nestedCollections: %@", nestedCollections);
    
    return 0;
  }
}