#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test 1: Constant strings (NSConstantString)
        NSString *constantString1 = @"Hello, World!";
        NSString *constantString2 = @"Testing GNUstep NSString formatter";
        NSString *emptyConstant = @"";
        
        // Test 2: Mutable strings
        NSMutableString *mutableString1 = [[NSMutableString alloc] initWithString:@"Initial"];
        [mutableString1 appendString:@" content"];
        NSMutableString *mutableString2 = [NSMutableString stringWithCapacity:100];
        [mutableString2 setString:@"Mutable test string"];
        
        // Test 3: Strings created from C strings
        NSString *cStringBased = [NSString stringWithCString:"C String Content" 
                                                     encoding:NSUTF8StringEncoding];
        const char *utf8Chars = "UTF-8 encoded string: αβγδε";
        NSString *utf8String = [NSString stringWithUTF8String:utf8Chars];
        
        // Test 4: Format strings
        NSString *formatString = [NSString stringWithFormat:@"Formatted: %d, %@", 42, @"test"];
        NSString *formatWithFloat = [NSString stringWithFormat:@"Pi = %.2f", 3.14159];
        
        // Test 5: String with special characters
        NSString *specialChars = @"Special: \n\t\"quotes\" 'apostrophe' \\backslash\\";
        NSString *unicode = @"Unicode: 你好世界 🌍 😊";
        
        // Test 6: Long strings
        NSMutableString *longString = [NSMutableString stringWithCapacity:1000];
        for (int i = 0; i < 50; i++) {
            [longString appendFormat:@"Line %d: This is some content. ", i];
        }
        NSString *immutableLong = [longString copy];
        
        // Test 7: nil string (for edge case testing)
        NSString *nilString = nil;
        
        // Test 8: String from data
        NSData *data = [@"Data-based string" dataUsingEncoding:NSUTF8StringEncoding];
        NSString *dataString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        
        // Test 9: Substring operations
        NSString *originalString = @"This is the original string for substring testing";
        NSString *substring1 = [originalString substringToIndex:7];  // "This is"
        NSString *substring2 = [originalString substringFromIndex:12]; // "original string..."
        NSRange range = NSMakeRange(8, 3);
        NSString *substring3 = [originalString substringWithRange:range]; // "the"
        
        // Test 10: String comparisons and copies
        NSString *original = @"Original";
        NSString *copy1 = [original copy];
        NSString *copy2 = [[NSString alloc] initWithString:original];
        NSString *different = @"Different";
        
        // Test 11: Path strings (common in real code)
        NSString *pathString = @"/usr/local/bin/program";
        NSString *homeDir = NSHomeDirectory();
        NSString *tempDir = NSTemporaryDirectory();
        
        // Test 12: Collection of strings (for testing array formatting later)
        NSArray *stringArray = @[
            @"First",
            @"Second", 
            @"Third",
            constantString1,
            mutableString1
        ];
        
        // BREAKPOINT LINE - Set breakpoint here for testing
        NSLog(@"=== NSString Formatter Test Point ===");  // Line 69
        NSLog(@"All string variables are ready for inspection");
        
        // Print some strings to verify they work
        NSLog(@"Constant: %@", constantString1);
        NSLog(@"Mutable: %@", mutableString1);
        NSLog(@"UTF-8: %@", utf8String);
        NSLog(@"Format: %@", formatString);
        NSLog(@"Unicode: %@", unicode);
        NSLog(@"Substring: %@", substring1);
        NSLog(@"Path: %@", pathString);
        
        // Test string operations to ensure objects are valid
        BOOL equal = [constantString1 isEqualToString:@"Hello, World!"];
        NSLog(@"String equality test: %@", equal ? @"PASS" : @"FAIL");
        
        NSUInteger length = [mutableString1 length];
        NSLog(@"Mutable string length: %lu", (unsigned long)length);
        
        // Clean up
        [mutableString1 release];
        [mutableString2 release];
        [longString release];
        [dataString release];
        [copy2 release];
        
        NSLog(@"=== Test Complete ===");
        
        return 0;
    }
}