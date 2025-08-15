// Minimal test with GNUstep runtime initialization
#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  // Try explicit GNUstep initialization
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  
  // Test if the issue is NSNumber-specific or general
  NSString *greeting = @"Hello";
  
  // Try a simple init without @autoreleasepool
  NSMutableDictionary *dict = [NSMutableDictionary alloc];
  dict = [dict init];  // Test if init works now
  
  NSLog(@"Testing: %@", greeting);
  
  [pool release];
  return 0;
}
