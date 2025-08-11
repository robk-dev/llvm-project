#import <Foundation/Foundation.h>
#import <stdio.h>

int main(int argc, char *argv[]) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  
  printf("Testing GSCInlineString objects:\n");
  
  // Create short strings that should be inline strings
  NSString *short_string = [[NSString alloc] initWithCString:"Hello" encoding:NSUTF8StringEncoding];
  NSString *short_unicode = [[NSString alloc] initWithCString:"Café" encoding:NSUTF8StringEncoding]; 
  NSString *empty_string = [[NSString alloc] initWithCString:"" encoding:NSUTF8StringEncoding];
  NSString *single_char = [[NSString alloc] initWithCString:"A" encoding:NSUTF8StringEncoding];
  
  // Also test NSString literals which might be constant strings
  NSString *literal = @"Literal";
  
  printf("Created test strings:\n");
  printf("short_string: %s (class: %s)\n", [short_string UTF8String], object_getClassName(short_string));
  printf("short_unicode: %s (class: %s)\n", [short_unicode UTF8String], object_getClassName(short_unicode));
  printf("empty_string: %s (class: %s)\n", [empty_string UTF8String], object_getClassName(empty_string));
  printf("single_char: %s (class: %s)\n", [single_char UTF8String], object_getClassName(single_char));
  printf("literal: %s (class: %s)\n", [literal UTF8String], object_getClassName(literal));
  
  // Breakpoint here for debugging
  printf("All strings created. Setting breakpoint for LLDB examination...\n");
  
  // Add debug breakpoint
  __builtin_debugtrap();
  
  [short_string release];
  [short_unicode release];
  [empty_string release];
  [single_char release];
  
  [pool drain];
  return 0;
}