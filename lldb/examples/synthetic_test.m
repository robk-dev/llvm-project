#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Test NSArray synthetic children
    NSArray *fruits = @[ @"apple", @"banana", @"cherry" ];
    
    // Test NSDictionary synthetic children  
    NSDictionary *person = @{
      @"name": @"John",
      @"age": @30,
      @"city": @"Boston"
    };
    
    // Test nested collections
    NSArray *nested = @[
      @"string",
      @42,
      @{@"key1": @"value1", @"key2": @"value2"},
      @[@"nested1", @"nested2"]
    ];
    
    // Breakpoint here for testing
    NSLog(@"Testing synthetic children");
    
    return 0;
  }
}