#import <Foundation/Foundation.h>
#include <float.h>
#include <limits.h>
#include <math.h>

/**
 * Comprehensive NSNumber Formatter Test Suite
 * 
 * Tests all GNUstep NSNumber encoding formats:
 * 1. Tagged pointer numbers (small objects with tags 1, 2, 3, 5)
 * 2. Regular heap NSNumber objects (IntNumber, FloatNumber, etc.)
 * 3. Edge cases and special values
 * 4. Performance validation
 * 
 * Based on GNUstepNumberFormatters.cpp implementation analysis.
 */

// Forward declarations for debugging helpers
void print_tagged_pointer_info(void* ptr, const char* description);
void validate_number_display(NSNumber* number, const char* expected, const char* description);

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    NSLog(@"=== GNUstep NSNumber Formatter Comprehensive Test Suite ===");
    
    // ========================================================================
    // SECTION 1: Tagged Pointer Numbers (NSSmallInt - Tag 1)
    // ========================================================================
    NSLog(@"\n--- SECTION 1: Tagged Pointer Integers (Tag 1) ---");
    
    // Small integers that should be encoded as tagged pointers
    // Value = pointer >> 3, so test values that fit in available bits
    NSNumber *tagged_ints[] = {
        [NSNumber numberWithInt:0],           // Zero
        [NSNumber numberWithInt:1],           // Positive small
        [NSNumber numberWithInt:-1],          // Negative small
        [NSNumber numberWithInt:42],          // Common test value
        [NSNumber numberWithInt:-42],         // Negative common
        [NSNumber numberWithInt:127],         // Small positive boundary
        [NSNumber numberWithInt:-128],        // Small negative boundary
        [NSNumber numberWithInt:1024],        // Larger value
        [NSNumber numberWithInt:-1024],       // Larger negative
        [NSNumber numberWithLongLong:0x1FFFFFFFFFFFFFFFLL >> 3],  // Max tagged int
        [NSNumber numberWithLongLong:(-0x1FFFFFFFFFFFFFFFLL) >> 3], // Min tagged int
        nil
    };
    
    NSLog(@"Testing tagged pointer integers (expect tag=1, value=ptr>>3):");
    for (int i = 0; tagged_ints[i] != nil; i++) {
        NSNumber *num = tagged_ints[i];
        print_tagged_pointer_info((__bridge void*)num, [[NSString stringWithFormat:@"tagged_int[%d] = %@", i, num] UTF8String]);
    }
    
    // ========================================================================
    // SECTION 2: Tagged Pointer Floats (NSSmallFloat - Tag 5)
    // ========================================================================
    NSLog(@"\n--- SECTION 2: Tagged Pointer Floats (Tag 5) ---");
    
    NSNumber *tagged_floats[] = {
        [NSNumber numberWithFloat:0.0f],        // Zero float
        [NSNumber numberWithFloat:1.0f],        // Simple positive
        [NSNumber numberWithFloat:-1.0f],       // Simple negative
        [NSNumber numberWithFloat:3.14159f],    // Pi
        [NSNumber numberWithFloat:-3.14159f],   // Negative Pi
        [NSNumber numberWithFloat:1.5f],        // Simple fraction
        [NSNumber numberWithFloat:0.5f],        // Half
        [NSNumber numberWithFloat:0.25f],       // Quarter
        [NSNumber numberWithFloat:100.0f],      // Integer as float
        [NSNumber numberWithFloat:FLT_MIN],     // Minimum positive float
        [NSNumber numberWithFloat:FLT_MAX],     // Maximum float (might not be tagged)
        nil
    };
    
    NSLog(@"Testing tagged pointer floats (expect tag=5, encoded in double format):");
    for (int i = 0; tagged_floats[i] != nil; i++) {
        NSNumber *num = tagged_floats[i];
        print_tagged_pointer_info((__bridge void*)num, [[NSString stringWithFormat:@"tagged_float[%d] = %@", i, num] UTF8String]);
    }
    
    // ========================================================================
    // SECTION 3: Tagged Pointer Doubles (Tags 2 & 3)  
    // ========================================================================
    NSLog(@"\n--- SECTION 3: Tagged Pointer Doubles (Tags 2 & 3) ---");
    
    NSNumber *tagged_doubles[] = {
        [NSNumber numberWithDouble:0.0],          // Zero double
        [NSNumber numberWithDouble:1.0],          // Simple positive
        [NSNumber numberWithDouble:-1.0],         // Simple negative
        [NSNumber numberWithDouble:2.718281828],  // e
        [NSNumber numberWithDouble:-2.718281828], // Negative e
        [NSNumber numberWithDouble:0.123456789],  // Decimal
        [NSNumber numberWithDouble:1000.0],       // Larger value
        [NSNumber numberWithDouble:DBL_MIN],      // Minimum positive double
        nil
    };
    
    NSLog(@"Testing tagged pointer doubles (expect tag=2 or 3, various encodings):");
    for (int i = 0; tagged_doubles[i] != nil; i++) {
        NSNumber *num = tagged_doubles[i];
        print_tagged_pointer_info((__bridge void*)num, [[NSString stringWithFormat:@"tagged_double[%d] = %@", i, num] UTF8String]);
    }
    
    // ========================================================================
    // SECTION 4: Regular NSNumber Objects (Heap-allocated)
    // ========================================================================
    NSLog(@"\n--- SECTION 4: Regular NSNumber Objects ---");
    
    // Create numbers that are definitely too large to be tagged pointers
    NSNumber *heap_numbers[] = {
        // Large integers that won't fit in tagged pointers
        [NSNumber numberWithLongLong:0x7FFFFFFFFFFFFFFFLL],     // Max int64
        [NSNumber numberWithLongLong:-0x8000000000000000LL],    // Min int64 
        [NSNumber numberWithUnsignedLongLong:0xFFFFFFFFFFFFFFFFULL], // Max uint64
        
        // Large floats/doubles that won't be tagged
        [NSNumber numberWithDouble:DBL_MAX],                    // Maximum double
        [NSNumber numberWithDouble:-DBL_MAX],                   // Minimum double
        
        // Special float values
        [NSNumber numberWithFloat:NAN],                         // Not a Number
        [NSNumber numberWithFloat:INFINITY],                    // Positive infinity
        [NSNumber numberWithFloat:-INFINITY],                   // Negative infinity
        [NSNumber numberWithDouble:NAN],                        // Double NaN
        [NSNumber numberWithDouble:INFINITY],                   // Double positive infinity  
        [NSNumber numberWithDouble:-INFINITY],                  // Double negative infinity
        
        // Edge case: Denormalized numbers
        [NSNumber numberWithFloat:FLT_MIN / 2.0f],             // Subnormal float
        [NSNumber numberWithDouble:DBL_MIN / 2.0],             // Subnormal double
        nil
    };
    
    NSLog(@"Testing heap-allocated NSNumber objects (expect regular object format):");
    for (int i = 0; heap_numbers[i] != nil; i++) {
        NSNumber *num = heap_numbers[i];
        NSLog(@"heap_number[%d] = %@ (ptr=%p, class=%@)", i, num, (__bridge void*)num, [num class]);
    }
    
    // ========================================================================
    // SECTION 5: Boolean NSNumber Objects  
    // ========================================================================
    NSLog(@"\n--- SECTION 5: Boolean NSNumbers ---");
    
    NSNumber *bool_numbers[] = {
        [NSNumber numberWithBool:YES],
        [NSNumber numberWithBool:NO],
        @YES,  // Shorthand literal
        @NO,   // Shorthand literal
        nil
    };
    
    NSLog(@"Testing boolean NSNumbers (expect YES/NO display):");
    for (int i = 0; bool_numbers[i] != nil; i++) {
        NSNumber *num = bool_numbers[i];
        NSLog(@"bool_number[%d] = %@ (ptr=%p, class=%@)", i, num, (__bridge void*)num, [num class]);
    }
    
    // ========================================================================
    // SECTION 6: Numeric Literals and Various Creation Methods
    // ========================================================================  
    NSLog(@"\n--- SECTION 6: Numeric Literals and Creation Methods ---");
    
    NSNumber *literal_numbers[] = {
        @0,         // Integer literal
        @42,        // Positive integer literal
        @-17,       // Negative integer literal  
        @3.14f,     // Float literal
        @2.718,     // Double literal
        @0xFF,      // Hex literal (255)
        @077,       // Octal literal (63)
        
        // Different creation methods
        [[NSNumber alloc] initWithInt:12345],
        [[NSNumber alloc] initWithFloat:1.23f],
        [[NSNumber alloc] initWithDouble:4.56789],
        [[NSNumber alloc] initWithBool:YES],
        nil
    };
    
    NSLog(@"Testing various NSNumber creation methods:");
    for (int i = 0; literal_numbers[i] != nil; i++) {
        NSNumber *num = literal_numbers[i];
        NSLog(@"literal_number[%d] = %@ (ptr=%p, class=%@)", i, num, (__bridge void*)num, [num class]);
    }
    
    // ========================================================================
    // SECTION 7: Edge Cases and Error Conditions
    // ========================================================================
    NSLog(@"\n--- SECTION 7: Edge Cases ---");
    
    // Test nil handling
    NSNumber *nil_number = nil;
    NSLog(@"nil_number = %@ (should display 'nil')", nil_number);
    
    // Test very large array to validate performance
    NSLog(@"Creating large array of numbers for performance testing...");
    NSMutableArray *large_number_array = [NSMutableArray arrayWithCapacity:10000];
    for (int i = 0; i < 10000; i++) {
        [large_number_array addObject:@(i)];
        if (i % 1000 == 0) {
            // Add some variety
            [large_number_array addObject:@(i * 3.14159)];
            [large_number_array addObject:@(i % 2 == 0 ? YES : NO)];
        }
    }
    
    NSLog(@"Large number array created with %lu elements", (unsigned long)[large_number_array count]);
    NSLog(@"Sample: first=%@, middle=%@, last=%@", 
          [large_number_array objectAtIndex:0],
          [large_number_array objectAtIndex:5000], 
          [large_number_array lastObject]);
    
    // ========================================================================  
    // SECTION 8: Specific Type Testing (for class name detection)
    // ========================================================================
    NSLog(@"\n--- SECTION 8: Specific Type Analysis ---");
    
    NSNumber *type_test_numbers[] = {
        [NSNumber numberWithChar:'A'],           // NSCharNumber  
        [NSNumber numberWithUnsignedChar:255],   // NSUnsignedCharNumber
        [NSNumber numberWithShort:12345],        // NSShortNumber
        [NSNumber numberWithUnsignedShort:65535], // NSUnsignedShortNumber
        [NSNumber numberWithInt:123456],         // NSIntNumber
        [NSNumber numberWithUnsignedInt:4294967295U], // NSUnsignedIntNumber
        [NSNumber numberWithLong:987654321L],    // NSLongNumber  
        [NSNumber numberWithUnsignedLong:987654321UL], // NSUnsignedLongNumber
        [NSNumber numberWithLongLong:123456789012345LL], // NSLongLongNumber
        [NSNumber numberWithUnsignedLongLong:123456789012345ULL], // NSUnsignedLongLongNumber
        [NSNumber numberWithFloat:1.234f],       // NSFloatNumber
        [NSNumber numberWithDouble:5.678901],    // NSDoubleNumber
        nil
    };
    
    NSLog(@"Testing specific numeric types for class name detection:");
    for (int i = 0; type_test_numbers[i] != nil; i++) {
        NSNumber *num = type_test_numbers[i];
        NSLog(@"type_test[%d] = %@ (class=%@)", i, num, [num class]);
    }
    
    // ========================================================================
    // DEBUG BREAKPOINT LOCATION
    // ========================================================================
    NSLog(@"\n=== SET BREAKPOINT HERE FOR MANUAL TESTING ===");
    NSLog(@"Use LLDB commands to test formatters:");
    NSLog(@"  po tagged_ints[0]    # Should show integer value");  
    NSLog(@"  po tagged_floats[3]  # Should show 3.14159");
    NSLog(@"  po bool_numbers[0]   # Should show YES");
    NSLog(@"  po heap_numbers[0]   # Should show large integer");
    NSLog(@"  po nil_number        # Should show nil");
    
    // Keep all arrays in scope for debugging
    NSLog(@"Test arrays in scope: tagged_ints=%p, tagged_floats=%p, tagged_doubles=%p", 
          tagged_ints, tagged_floats, tagged_doubles);
    NSLog(@"More arrays: heap_numbers=%p, bool_numbers=%p, literal_numbers=%p, type_test_numbers=%p",
          heap_numbers, bool_numbers, literal_numbers, type_test_numbers);
    NSLog(@"Large array: %p (count=%lu)", large_number_array, (unsigned long)[large_number_array count]);
    
    // Manual pause for LLDB testing - attach with: kill -STOP <pid>
    NSLog(@"Process ID: %d - Use 'kill -STOP %d' from another terminal to pause for debugging", getpid(), getpid());
    sleep(30);  // Give time to attach debugger
    
    [pool drain];
    return 0;
}

/**
 * Debug helper to analyze tagged pointer encoding
 * Prints low 3 bits to determine tag value
 */
void print_tagged_pointer_info(void* ptr, const char* description) {
    uintptr_t addr = (uintptr_t)ptr;
    uint64_t tag = addr & 0x07;  // Low 3 bits
    int64_t value = 0;
    
    if (tag == 1) {
        // NSSmallInt: value = ptr >> 3
        value = ((int64_t)addr) >> 3;
    }
    
    NSLog(@"%s: ptr=%p, tag=%lu, decoded_value=%ld", description, ptr, (unsigned long)tag, (long)value);
}

/**
 * Validation helper for expected formatter output
 * Note: This would be used in automated testing scenarios
 */
void validate_number_display(NSNumber* number, const char* expected, const char* description) {
    // This function would be used to validate formatter output in automated tests
    // For manual testing, we rely on LLDB's 'po' command to show the formatter result
    NSLog(@"Validation point: %s should display as '%s'", description, expected);
}