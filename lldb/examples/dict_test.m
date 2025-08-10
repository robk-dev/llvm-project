#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Create a simple dictionary with various string types
    NSDictionary *dict = @{
      @"key1" : @"simple string",        // Tagged string
      @"key2" : @"Developer",             // NSConstantString
      @"key3" : @"John Doe2",             // NSConstantString
      @"key4" : @42,                      // Tagged number
      @"key5" : [NSString stringWithFormat:@"formatted %d", 123], // Dynamic string
    };
    
    NSLog(@"Dictionary: %@", dict);
    
    // Set breakpoint here
    NSLog(@"Test complete");
  }
  return 0;
}