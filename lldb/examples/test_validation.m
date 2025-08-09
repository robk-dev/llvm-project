#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Test 1: Array with NSConstantString first element
    NSArray *testArray = @[@"First", @"Second", @"Third"];
    
    // Test 2: Dictionary with potential key corruption
    NSDictionary *testDict = @{
      @"name": @"John Doe",
      @"age": @30,
      @"city": @"New York"
    };
    
    // Test 3: Nested structures
    NSDictionary *nested = @{
      @"array": @[@"One", @"Two", @"Three"],
      @"dict": @{@"key1": @"value1", @"key2": @"value2"}
    };
    
    // Breakpoint here for validation
    NSLog(@"=== VALIDATION TEST ===");
    NSLog(@"Array first element: %@", testArray[0]);
    NSLog(@"Dictionary: %@", testDict);
    NSLog(@"Nested: %@", nested);
    
    // Expected debugger display:
    // testArray: @["First", "Second", "Third"] ✓ (not @[<NSConstantString>, "Second", "Third"])
    // testDict: @{"name": "John Doe", "age": 30, "city": "New York"} ✓ (no duplicate keys)
    // nested: properly formatted with drill-down capability ✓
    
    return 0;
  }
}