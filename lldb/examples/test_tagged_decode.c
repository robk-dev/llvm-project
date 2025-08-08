#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Test decoding GNUstep tagged strings
void decode_tagged_string(uint64_t addr) {
    // Check if it's a tagged string (tag = 4)
    if ((addr & 0x7) != 4) {
        printf("Not a tagged string (tag=%lu)\n", addr & 0x7);
        return;
    }
    
    // Extract length from bits 3-7
    int length = (addr >> 3) & 0x1f;
    printf("Length: %d\n", length);
    
    // Decode characters - each uses 7 bits, stored from bit 57 downward
    char result[32] = {0};
    
    for (int i = 0; i < length && i < 9; i++) {
        // Extract character at position i using the GNUstep formula
        uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
        char c = (addr & mask) >> (57 - (i * 7));
        result[i] = c;
        printf("  char[%d]: mask=0x%016lx, extracted='%c' (0x%02x)\n", 
               i, mask, (c >= 0x20 && c <= 0x7e) ? c : '?', c);
    }
    
    printf("Decoded string: \"%s\"\n", result);
}

int main() {
    // Test with the actual tagged pointer values we saw
    printf("Testing Apple (0x83c386cca000002c):\n");
    decode_tagged_string(0x83c386cca000002c);
    
    printf("\nTesting Banana (0x8587761dd8400034):\n");
    decode_tagged_string(0x8587761dd8400034);
    
    printf("\nTesting Cherry (0x87a32f2e5e400034):\n");
    decode_tagged_string(0x87a32f2e5e400034);
    
    return 0;
}