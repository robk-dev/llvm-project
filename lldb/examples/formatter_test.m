#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Test NSString
    NSString *str = @"Hello, World!";
    NSLog(@"String test");  // Line 7 - breakpoint here
    
    // Test NSNumber
    NSNumber *num = @42;
    NSNumber *floatNum = @3.14159;
    NSNumber *boolYes = @YES;
    NSNumber *boolNo = @NO;
    NSLog(@"Number test");  // Line 14 - breakpoint here
    
    // Test NSArray
    NSArray *array = @[@"Apple", @"Banana", @"Cherry"];
    NSLog(@"Array test");  // Line 18 - breakpoint here
    
    // Test NSDictionary  
    NSDictionary *dict = @{
      @"name": @"John Doe",
      @"age": @30,
      @"city": @"New York"
    };
    NSLog(@"Dictionary test");  // Line 26 - breakpoint here
    
    // Test NSSet
    NSSet *set = [NSSet setWithObjects:@"Red", @"Green", @"Blue", nil];
    NSLog(@"Set test");  // Line 30 - breakpoint here
    
    NSLog(@"All tests complete");
  }
  return 0;
}