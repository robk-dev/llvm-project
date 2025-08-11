#import <Foundation/Foundation.h>
#include <objc/runtime.h>

int main(int argc, char *argv[]) {
  @autoreleasepool {
    NSMutableString *mutable = [NSMutableString stringWithString:@"Test"];
    
    // Print class and memory info
    NSLog(@"Class: %@", NSStringFromClass([mutable class]));
    NSLog(@"Mutable string: %@", mutable);
    
    // Get raw memory
    void *obj_ptr = (__bridge void *)mutable;
    uint64_t *memory = (uint64_t *)obj_ptr;
    
    NSLog(@"Memory layout:");
    NSLog(@"  [0] ISA: %p", (void *)memory[0]);
    NSLog(@"  [1] field1: %p", (void *)memory[1]);
    NSLog(@"  [2] field2: %p", (void *)memory[2]);
    NSLog(@"  [3] field3: %p", (void *)memory[3]);
    
    // Try to access _source ivar if exists
    Ivar sourceIvar = class_getInstanceVariable([mutable class], "_source");
    if (sourceIvar) {
      NSLog(@"Has _source ivar at offset %td", ivar_getOffset(sourceIvar));
      id source = object_getIvar(mutable, sourceIvar);
      NSLog(@"_source class: %@", NSStringFromClass([source class]));
      NSLog(@"_source value: %@", source);
    }
    
    // List all ivars
    unsigned int ivarCount;
    Ivar *ivars = class_copyIvarList([mutable class], &ivarCount);
    NSLog(@"Instance variables for %@:", NSStringFromClass([mutable class]));
    for (unsigned int i = 0; i < ivarCount; i++) {
      Ivar ivar = ivars[i];
      NSLog(@"  %s (offset %td)", ivar_getName(ivar), ivar_getOffset(ivar));
    }
    free(ivars);
    
    NSLog(@"Debug breakpoint here"); // Set breakpoint
  }
  return 0;
}