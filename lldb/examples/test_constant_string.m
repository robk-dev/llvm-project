#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Test 1: Direct array creation with string literals
    NSArray *test1 = @[@"First", @"Second", @"Third"];
    
    // Test 2: Array created using arrayWithObjects
    NSArray *test2 = [NSArray arrayWithObjects:@"Alpha", @"Beta", @"Gamma", nil];
    
    // Test 3: Mutable array
    NSMutableArray *test3 = [NSMutableArray arrayWithObjects:@"One", @"Two", @"Three", nil];
    
    // Breakpoint here
    NSLog(@"test1: %@", test1);
    NSLog(@"test2: %@", test2); 
    NSLog(@"test3: %@", test3);
    
    // Let's also check what the actual pointer values are
    for (int i = 0; i < [test1 count]; i++) {
      id obj = [test1 objectAtIndex:i];
      NSLog(@"test1[%d] = %p: %@", i, obj, obj);
    }
  }
  return 0;
}