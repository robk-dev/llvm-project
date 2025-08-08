#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Create test array with string literals
    NSArray *fruits = @[@"Apple", @"Banana", @"Cherry", @"Date"];
    
    // Print to verify
    NSLog(@"Fruits array: %@", fruits);
    
    // Set breakpoint here
    NSLog(@"Done"); // Line 11 - breakpoint here
    
    return 0;
  }
}