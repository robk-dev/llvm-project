#import <Foundation/Foundation.h>

void debug_point() {
    // Breakpoint marker function
}

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Create a simple dictionary with NSNumber values
    NSDictionary *testDict = @{
      @"first": @1,
      @"second": @2,
      @"third": @42
    };
    
    NSNumber *directNumber = @42;
    
    // Set breakpoint here to inspect
    debug_point();
    NSLog(@"Test dictionary: %@", testDict);
    NSLog(@"Direct number: %@", directNumber);
    
    return 0;
  }
}