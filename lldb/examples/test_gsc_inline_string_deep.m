//===-- test_gsc_inline_string_deep.m - Deep GSCInlineString testing ---===//
//
// Comprehensive test for GSCInlineString encoding and decoding
//
//===----------------------------------------------------------------------===//

#import <Foundation/Foundation.h>
#include <objc/runtime.h>

void inspect_string_memory(NSString *str, const char *label) {
    printf("\n=== %s ===\n", label);
    printf("String content: %s\n", [str UTF8String]);
    printf("String length: %lu\n", (unsigned long)[str length]);
    printf("String class: %s\n", class_getName([str class]));
    printf("String address: %p\n", str);
    
    // Check if it's a tagged pointer first
    uintptr_t addr = (uintptr_t)str;
    uint64_t tag = addr & 0x7;
    if (tag == 4) {  // GSTinyString has tag 4
        printf("Tagged pointer detected (GSTinyString, tag = %llu)\n", (unsigned long long)tag);
        
        // For tagged strings, decode the content from the pointer bits
        if (tag == 4) {
            printf("GSTinyString encoding detected\n");
            int length = (addr >> 3) & 0x1F;
            printf("Encoded length: %d\n", length);
            printf("Characters encoded in pointer:\n");
            for (int i = 0; i < length && i < 9; i++) {
                uint64_t mask = 0xFE00000000000000ULL >> (i * 7);
                char c = (addr & mask) >> (57 - (i * 7));
                printf("  [%d]: '%c' (0x%02x)\n", i, c, (unsigned char)c);
            }
        }
    } else {
        // Only dump memory for non-tagged pointers
        printf("Regular object pointer - dumping memory:\n");
        uint64_t *ptr = (uint64_t *)str;
        printf("Memory dump (first 8 quadwords):\n");
        for (int i = 0; i < 8; i++) {
            printf("  [%d] 0x%016llx\n", i, (unsigned long long)ptr[i]);
        }
    }
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== Deep GSCInlineString Analysis ===\n");
        
        // Test 1: String literals (should be NSConstantString)
        NSString *literal1 = @"Hello";
        NSString *literal2 = @"World";
        inspect_string_memory(literal1, "String Literal 1");
        inspect_string_memory(literal2, "String Literal 2");
        
        // Test 2: Dynamic string creation (likely GSCInlineString)
        NSString *dynamic1 = [NSString stringWithFormat:@"Test%d", 123];
        NSString *dynamic2 = [@"Hello" stringByAppendingString:@" World"];
        NSString *dynamic3 = [[NSString alloc] initWithUTF8String:"Dynamic"];
        inspect_string_memory(dynamic1, "Dynamic String 1 (format)");
        inspect_string_memory(dynamic2, "Dynamic String 2 (append)");
        inspect_string_memory(dynamic3, "Dynamic String 3 (initWithUTF8)");
        
        // Test 3: Create dictionary with various string keys
        NSDictionary *dict = @{
            @"key1": @"value1",
            @"key2": @"value2",
            @"key3": @"value3",
            [NSString stringWithFormat:@"key%d", 4]: @"value4",
            [@"key" stringByAppendingString:@"5"]: @"value5"
        };
        
        printf("\n=== Dictionary Keys Analysis ===\n");
        for (NSString *key in dict) {
            inspect_string_memory(key, [[NSString stringWithFormat:@"Dict Key: %@", key] UTF8String]);
        }
        
        // Test 4: Mutable strings
        NSMutableString *mutable = [NSMutableString stringWithString:@"Mutable"];
        [mutable appendString:@" String"];
        inspect_string_memory(mutable, "Mutable String");
        
        // Test 5: Short strings that might be tagged
        NSString *tiny1 = [NSString stringWithFormat:@"A"];
        NSString *tiny2 = [NSString stringWithFormat:@"AB"];
        NSString *tiny3 = [NSString stringWithFormat:@"ABC"];
        inspect_string_memory(tiny1, "Tiny String 1");
        inspect_string_memory(tiny2, "Tiny String 2");
        inspect_string_memory(tiny3, "Tiny String 3");
        
        // Test 6: Bundle path (known to create GSCInlineString)
        NSBundle *bundle = [NSBundle mainBundle];
        NSString *bundlePath = [bundle bundlePath];
        if (bundlePath) {
            inspect_string_memory(bundlePath, "Bundle Path");
        }
        
        // Breakpoint location for LLDB inspection
        printf("\n=== BREAKPOINT HERE - Line 81 ===\n");
        printf("Inspect variables:\n");
        printf("  literal1, literal2 - NSConstantString\n");
        printf("  dynamic1, dynamic2, dynamic3 - GSCInlineString\n");
        printf("  dict - NSDictionary with mixed keys\n");
        printf("  tiny1, tiny2, tiny3 - Possible tagged strings\n");
        
        return 0;
    }
}