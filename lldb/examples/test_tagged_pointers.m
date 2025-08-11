#import <Foundation/Foundation.h>
#import <stdio.h>

// Test program for GNUstep tagged pointer support in LLDB
// This creates objects that should be stored as tagged pointers (small objects)
// to validate the LLDB runtime bridge correctly identifies and formats them

int main() {
    @autoreleasepool {
        printf("=== GNUstep Tagged Pointer Test Program ===\n");
        
        // Test small integers - these should be tagged pointers
        NSNumber *smallInt1 = @42;
        NSNumber *smallInt2 = @(-17);
        NSNumber *smallInt3 = @0;
        NSNumber *smallInt4 = @1;
        
        // Test small floating point numbers (might be tagged depending on GNUstep version)
        NSNumber *smallFloat1 = @3.14f;
        NSNumber *smallFloat2 = @(-2.5f);
        
        // Test small doubles (might be tagged depending on GNUstep version)
        NSNumber *smallDouble1 = @1.5;
        NSNumber *smallDouble2 = @(-0.5);
        
        // Test boolean values (often tagged)
        NSNumber *boolTrue = @YES;
        NSNumber *boolFalse = @NO;
        
        // Test small constant strings (might be tagged as tiny strings)
        NSString *tinyString1 = @"A";
        NSString *tinyString2 = @"Hi";
        NSString *tinyString3 = @"Test";
        NSString *tinyString4 = @"X";
        NSString *tinyString5 = @"123";
        
        // Test regular objects for comparison (should NOT be tagged)
        NSNumber *bigInt = [NSNumber numberWithLongLong:0x7FFFFFFFFFFFFFFFLL];
        NSString *longString = @"This is a very long string that should not be tagged";
        NSMutableString *mutableString = [@"mutable" mutableCopy];
        
        // Test dates (might be tagged for common values)
        NSDate *now = [NSDate date];
        NSDate *epoch = [NSDate dateWithTimeIntervalSince1970:0];
        
        // Create collections with mixed tagged/untagged objects
        NSArray *mixedArray = @[
            smallInt1, smallInt2, tinyString1, tinyString2, 
            bigInt, longString, boolTrue, boolFalse
        ];
        
        NSDictionary *mixedDict = @{
            @"smallInt": smallInt1,
            @"tinyString": tinyString1,
            @"bigInt": bigInt,
            @"longString": longString,
            @"bool": boolTrue
        };
        
        // Print test summary
        printf("\nCreated test objects:\n");
        printf("Small integers: %s, %s, %s, %s\n", 
               [[smallInt1 description] UTF8String],
               [[smallInt2 description] UTF8String], 
               [[smallInt3 description] UTF8String],
               [[smallInt4 description] UTF8String]);
        
        printf("Small floats: %s, %s\n",
               [[smallFloat1 description] UTF8String],
               [[smallFloat2 description] UTF8String]);
               
        printf("Small doubles: %s, %s\n", 
               [[smallDouble1 description] UTF8String],
               [[smallDouble2 description] UTF8String]);
               
        printf("Booleans: %s, %s\n",
               [[boolTrue description] UTF8String],
               [[boolFalse description] UTF8String]);
               
        printf("Tiny strings: '%s', '%s', '%s', '%s', '%s'\n",
               [tinyString1 UTF8String], [tinyString2 UTF8String],
               [tinyString3 UTF8String], [tinyString4 UTF8String],
               [tinyString5 UTF8String]);
               
        printf("Regular objects: %s, '%s'\n",
               [[bigInt description] UTF8String],
               [longString UTF8String]);
               
        printf("Mixed array has %lu elements\n", (unsigned long)[mixedArray count]);
        printf("Mixed dictionary has %lu elements\n", (unsigned long)[mixedDict count]);
        
        // Debugging breakpoint location
        printf("\n=== BREAKPOINT: Set breakpoint here to test tagged pointer detection ===\n");
        printf("In LLDB, try:\n");
        printf("  po smallInt1     # Should show: 42\n");
        printf("  po smallInt2     # Should show: -17\n"); 
        printf("  po tinyString1   # Should show: A\n");
        printf("  po tinyString2   # Should show: Hi\n");
        printf("  po boolTrue      # Should show: 1\n");
        printf("  po mixedArray    # Should show all elements formatted correctly\n");
        printf("  po mixedDict     # Should show key/value pairs formatted correctly\n");
        printf("\n");
        
        // Keep objects alive for debugging
        printf("Test objects created successfully. Ready for debugging.\n");
        
        return 0;  // <-- SET BREAKPOINT HERE for LLDB testing
    }
}