#import <Foundation/Foundation.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        NSString *str1 = @"Apple";
        NSString *str2 = @"Banana"; 
        NSString *str3 = @"Cherry";
        
        // Print the pointer values
        printf("str1 (Apple):  %p = 0x%016lx\n", str1, (unsigned long)str1);
        printf("str2 (Banana): %p = 0x%016lx\n", str2, (unsigned long)str2);
        printf("str3 (Cherry): %p = 0x%016lx\n", str3, (unsigned long)str3);
        
        // Print bytes of each pointer
        uint8_t *bytes1 = (uint8_t*)&str1;
        uint8_t *bytes2 = (uint8_t*)&str2;
        uint8_t *bytes3 = (uint8_t*)&str3;
        
        printf("\nBytes of str1 pointer: ");
        for (int i = 0; i < 8; i++) {
            printf("%02x ", bytes1[i]);
            if (bytes1[i] >= 32 && bytes1[i] <= 126) {
                printf("('%c') ", bytes1[i]);
            }
        }
        printf("\n");
        
        printf("Bytes of str2 pointer: ");
        for (int i = 0; i < 8; i++) {
            printf("%02x ", bytes2[i]);
            if (bytes2[i] >= 32 && bytes2[i] <= 126) {
                printf("('%c') ", bytes2[i]);
            }
        }
        printf("\n");
        
        printf("Bytes of str3 pointer: ");
        for (int i = 0; i < 8; i++) {
            printf("%02x ", bytes3[i]);
            if (bytes3[i] >= 32 && bytes3[i] <= 126) {
                printf("('%c') ", bytes3[i]);
            }
        }
        printf("\n");
        
        return 0;
    }
}