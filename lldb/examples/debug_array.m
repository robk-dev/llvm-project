#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Simple test - just one string in array
    NSArray *simple = @[ @"test" ];
    
    // Manual inspection
    printf("Array address: %p\n", (void*)simple);
    printf("Array class: %s\n", [NSStringFromClass([simple class]) UTF8String]);
    printf("Array count: %lu\n", (unsigned long)[simple count]);
    
    // Memory dump
    id *ptr = (id*)simple;
    printf("isa: %p\n", (void*)ptr[0]);
    printf("field1: %p\n", (void*)ptr[1]); 
    printf("field2: %p\n", (void*)ptr[2]);
    
    return 0;
  }
}