#import <Foundation/Foundation.h>
#import <stdio.h>

int main(int argc, char *argv[]) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  
  printf("Testing GSCInlineString in collections:\n");
  
  // Create strings that should be inline strings
  NSString *unicode1 = [[NSString alloc] initWithCString:"Café" encoding:NSUTF8StringEncoding];
  NSString *unicode2 = [[NSString alloc] initWithCString:"Naïve" encoding:NSUTF8StringEncoding]; 
  NSString *unicode3 = [[NSString alloc] initWithCString:"Résumé" encoding:NSUTF8StringEncoding];
  
  printf("String classes:\n");
  printf("unicode1: %s (class: %s)\n", [unicode1 UTF8String], object_getClassName(unicode1));
  printf("unicode2: %s (class: %s)\n", [unicode2 UTF8String], object_getClassName(unicode2));
  printf("unicode3: %s (class: %s)\n", [unicode3 UTF8String], object_getClassName(unicode3));
  
  // Put them in an array
  NSArray *strings = [NSArray arrayWithObjects:unicode1, unicode2, unicode3, nil];
  
  // Put them in a dictionary
  NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
                        unicode1, @"first",
                        unicode2, @"second", 
                        unicode3, @"third",
                        nil];
  
  printf("\nTesting collection display:\n");
  printf("Array created with %lu elements\n", (unsigned long)[strings count]);
  printf("Dictionary created with %lu elements\n", (unsigned long)[dict count]);
  
  // Breakpoint here for debugging
  printf("Collections created. Setting breakpoint for LLDB examination...\n");
  __builtin_debugtrap();
  
  [unicode1 release];
  [unicode2 release];
  [unicode3 release];
  
  [pool drain];
  return 0;
}