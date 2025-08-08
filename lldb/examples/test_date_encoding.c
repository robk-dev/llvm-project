#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main() {
    // Test with the known good value
    double val = 776366386.614403;
    uint64_t ieee_bits;
    memcpy(&ieee_bits, &val, sizeof(double));
    
    printf("Original value: %f\n", val);
    printf("IEEE 754 bits: 0x%016lx\n", ieee_bits);
    
    // Extract IEEE 754 components
    uint64_t ieee_frac = ieee_bits & 0xFFFFFFFFFFFFFULL;
    uint64_t ieee_exp = (ieee_bits >> 52) & 0x7FF;
    uint64_t ieee_sign = (ieee_bits >> 63) & 0x1;
    
    printf("IEEE components:\n");
    printf("  fraction: 0x%013lx\n", ieee_frac);
    printf("  exponent: 0x%03lx (%lu, biased by 1023)\n", ieee_exp, ieee_exp);
    printf("  sign: %lu\n", ieee_sign);
    
    // Now simulate the compression (subtract 0x3EF from exponent)
    int64_t compressed_exp = (int64_t)ieee_exp - 0x3EF;
    printf("\nCompressed exponent: %ld (0x%02lx)\n", compressed_exp, compressed_exp & 0xFF);
    
    // Build the compressed format
    uint64_t compressed = 0;
    compressed |= 6;  // tag
    compressed |= (ieee_frac << 3);  // fraction at bits 3-54
    compressed |= ((compressed_exp & 0xFF) << 55);  // 8-bit exponent at bits 55-62
    compressed |= (ieee_sign << 63);  // sign at bit 63
    
    printf("Compressed pointer: 0x%016lx\n", compressed);
    
    // Now decompress it back
    uint64_t dec_frac = (compressed >> 3) & 0xFFFFFFFFFFFFFULL;
    uint64_t dec_exp_bits = (compressed >> 55) & 0xFF;
    uint64_t dec_sign = (compressed >> 63) & 0x1;
    
    // Sign extend the exponent
    int64_t dec_exp = (int64_t)dec_exp_bits;
    if (dec_exp & 0x80) {
        dec_exp |= 0xFFFFFFFFFFFFFF00ULL;
    }
    dec_exp += 0x3EF;  // Add bias back
    
    printf("\nDecompressed components:\n");
    printf("  fraction: 0x%013lx\n", dec_frac);
    printf("  exponent: 0x%03lx (%ld)\n", dec_exp, dec_exp);
    printf("  sign: %lu\n", dec_sign);
    
    // Rebuild IEEE 754
    uint64_t rebuilt = 0;
    rebuilt |= dec_frac;
    rebuilt |= ((uint64_t)dec_exp << 52);
    rebuilt |= (dec_sign << 63);
    
    double rebuilt_val;
    memcpy(&rebuilt_val, &rebuilt, sizeof(double));
    
    printf("\nRebuilt value: %f\n", rebuilt_val);
    printf("Match: %s\n", (rebuilt_val == val) ? "YES" : "NO");
    
    return 0;
}