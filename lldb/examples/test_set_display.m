#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Test 1: Simple NSSet with string literals
    NSSet *simpleSet = [NSSet setWithObjects:@"Apple", @"Banana", @"Cherry", nil];
    
    // Test 2: NSMutableSet
    NSMutableSet *mutableSet = [NSMutableSet set];
    [mutableSet addObject:@"Dark Mode"];
    [mutableSet addObject:@"Notifications"];
    [mutableSet addObject:@"Auto-save"];
    
    // Test 3: Set with mixed types
    NSSet *mixedSet = [NSSet setWithObjects:
      @"String",
      @42,
      @YES,
      @[@"nested", @"array"],
      nil
    ];
    
    // Breakpoint here
    NSLog(@"Set formatter test");
    NSLog(@"simpleSet: %@", simpleSet);
    NSLog(@"mutableSet: %@", mutableSet);
    NSLog(@"mixedSet: %@", mixedSet);
    
    return 0;
  }
}