#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    NSString *greeting = @"Hello";
    NSArray *fruits = @[ @"apple", @"banana" ];
    
    // Breakpoint here
    NSLog(@"Testing: %@ and %@", greeting, fruits);
    return 0;
  }
}