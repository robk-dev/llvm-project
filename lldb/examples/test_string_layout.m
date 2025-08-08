#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test different types of strings
        NSString *constant1 = @"Apple";
        NSString *constant2 = @"Banana";
        NSString *constant3 = @"Cherry";
        
        // Print addresses and examine memory layout
        printf("String addresses:\n");
        printf("constant1 (%s): %p\n", [constant1 UTF8String], constant1);
        printf("constant2 (%s): %p\n", [constant2 UTF8String], constant2);
        printf("constant3 (%s): %p\n", [constant3 UTF8String], constant3);
        
        // Check if they're tagged pointers
        uintptr_t addr1 = (uintptr_t)constant1;
        uintptr_t addr2 = (uintptr_t)constant2;
        uintptr_t addr3 = (uintptr_t)constant3;
        
        printf("\nTagged pointer check (low 3 bits):\n");
        printf("constant1: 0x%lx, tag: %lu\n", addr1, addr1 & 7);
        printf("constant2: 0x%lx, tag: %lu\n", addr2, addr2 & 7);
        printf("constant3: 0x%lx, tag: %lu\n", addr3, addr3 & 7);
        
        // If not tagged, examine the memory layout
        if ((addr1 & 7) == 0) {
            printf("\nMemory layout of constant1 (not tagged):\n");
            uint64_t *ptr = (uint64_t *)constant1;
            printf("  offset 0 (isa):     0x%lx\n", ptr[0]);
            printf("  offset 8:           0x%lx\n", ptr[1]);
            printf("  offset 16:          0x%lx\n", ptr[2]);
            printf("  offset 24:          0x%lx\n", ptr[3]);
            
            // Try to read as C string from different offsets
            char **str_at_8 = (char **)(ptr + 1);
            char **str_at_16 = (char **)(ptr + 2);
            char **str_at_24 = (char **)(ptr + 3);
            
            printf("\nString pointer locations:\n");
            if (*str_at_8) printf("  At offset 8: \"%s\"\n", *str_at_8);
            if (*str_at_16) printf("  At offset 16: \"%s\"\n", *str_at_16);
            if (*str_at_24) printf("  At offset 24: \"%s\"\n", *str_at_24);
        }
        
        // Create an array to test
        NSArray *fruits = @[constant1, constant2, constant3];
        printf("\nArray created with %lu elements\n", [fruits count]);
        
        // Breakpoint location for testing
        printf("Set breakpoint here to test formatters\n"); // Line 50
        
        return 0;
    }
}