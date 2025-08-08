#import <Foundation/Foundation.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Create a simple string
        NSString *apple = @"Apple";
        
        // Print its address
        printf("String 'Apple' at address: %p\n", apple);
        printf("Value at address: 0x%lx\n", (unsigned long)apple);
        
        // Check if it's a tagged pointer
        if (((uintptr_t)apple & 0x7) != 0) {
            printf("This is a tagged pointer with tag: %lu\n", ((uintptr_t)apple & 0x7));
            printf("Payload: 0x%lx\n", ((uintptr_t)apple >> 3));
        } else {
            printf("This is a regular object pointer\n");
        }
        
        printf("Set breakpoint here\n"); // Line 20
        
        return 0;
    }
}