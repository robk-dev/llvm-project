#include <stdio.h>
#include <stdint.h>

int main() {
    // The values we see in memory
    uint64_t values[] = {
        0xc3c386cca000002c,  // Should be "Apple"
        0xc587761dd8400034,  // Should be "Banana"
        0xc7a32f2e5e400034,  // Should be "Cherry"
        0xc987a65000000024   // Should be "Pear"
    };
    
    const char* expected[] = {"Apple", "Banana", "Cherry", "Pear"};
    
    for (int v = 0; v < 4; v++) {
        uint64_t val = values[v];
        printf("\nTesting 0x%016llx (expected: %s)\n", (unsigned long long)val, expected[v]);
        
        int tag = val & 7;
        int len = (val >> 3) & 0x1f;
        printf("Tag: %d, Length from bits 3-7: %d\n", tag, len);
        
        // Try extracting characters using the GNUstep formula
        printf("Characters using (57 - i*7) formula:\n");
        for (int i = 0; i < 8; i++) {
            uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
            char c = (val & mask) >> (57 - (i * 7));
            if (c >= 32 && c <= 126) {
                printf("  [%d]: 0x%02x = '%c'\n", i, c, c);
            } else if (c != 0) {
                printf("  [%d]: 0x%02x = (non-printable)\n", i, c);
            }
        }
        
        // The actual "Apple" string has these ASCII values:
        // A=0x41, p=0x70, p=0x70, l=0x6c, e=0x65
        // Let's see if we can find these patterns
        printf("Looking for expected characters:\n");
        for (int i = 0; i < strlen(expected[v]); i++) {
            printf("  '%c' = 0x%02x\n", expected[v][i], expected[v][i]);
        }
    }
    
    return 0;
}