#import <Foundation/Foundation.h>
#import <objc/runtime.h>

// Test program for GSCInlineString formatter
// This tests the inline string implementation in GNUstep

void test_inline_strings() {
    NSLog(@"=== Testing GSCInlineString Formatter ===");
    
    // Test 1: Short inline strings (most common case)
    NSString *short1 = @"Hi";
    NSString *short2 = @"Test";
    NSString *short3 = @"Hello";
    NSLog(@"Short inline strings created");
    
    // Test 2: Medium length inline strings
    NSString *medium1 = @"This is a test";
    NSString *medium2 = @"Hello, World!";
    NSString *medium3 = @"GNUstep rocks!";
    NSLog(@"Medium inline strings created");
    
    // Test 3: Unicode inline strings
    NSString *unicode1 = @"café";
    NSString *unicode2 = @"日本語";
    NSString *unicode3 = @"🎉🎊";
    NSString *unicode4 = @"Привет мир";
    NSLog(@"Unicode inline strings created");
    
    // Test 4: Edge cases
    NSString *empty = @"";
    NSString *single = @"x";
    NSString *spaces = @"   ";
    NSString *newline = @"line1\nline2";
    NSString *tab = @"col1\tcol2";
    NSLog(@"Edge case strings created");
    
    // Test 5: Strings in containers (to test integration)
    NSArray *stringArray = @[short1, medium1, unicode1];
    NSDictionary *stringDict = @{
        @"short": short2,
        @"medium": medium2,
        @"unicode": unicode2
    };
    NSSet *stringSet = [NSSet setWithObjects:short3, medium3, unicode3, nil];
    NSLog(@"Strings in containers created");
    
    // Test 6: NSBundle paths (common use of inline strings)
    NSBundle *mainBundle = [NSBundle mainBundle];
    NSString *bundlePath = [mainBundle bundlePath];
    NSString *resourcePath = [mainBundle resourcePath];
    NSString *executablePath = [mainBundle executablePath];
    NSLog(@"Bundle paths created");
    
    // Test 7: NSUserDefaults keys (another common use)
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:@"value1" forKey:@"testKey1"];
    [defaults setObject:@"value2" forKey:@"testKey2"];
    [defaults setObject:unicode4 forKey:@"unicodeKey"];
    NSString *key1 = @"testKey1";
    NSString *key2 = @"testKey2";
    NSString *key3 = @"unicodeKey";
    NSLog(@"UserDefaults keys created");
    
    // Test 8: String concatenation (may create inline strings)
    NSString *concat1 = [@"Hello" stringByAppendingString:@" World"];
    NSString *concat2 = [NSString stringWithFormat:@"Number: %d", 42];
    NSString *concat3 = [[short1 stringByAppendingString:@" "] stringByAppendingString:short2];
    NSLog(@"Concatenated strings created");
    
    // Test 9: Substring operations
    NSString *source = @"Hello, wonderful world!";
    NSString *sub1 = [source substringToIndex:5];
    NSString *sub2 = [source substringFromIndex:7];
    NSString *sub3 = [source substringWithRange:NSMakeRange(7, 9)];
    NSLog(@"Substrings created");
    
    // Test 10: Different character encodings
    const char *utf8String = "UTF-8 String";
    NSString *fromUTF8 = [NSString stringWithUTF8String:utf8String];
    NSString *fromCString = [NSString stringWithCString:"C String" encoding:NSASCIIStringEncoding];
    NSLog(@"Strings from C strings created");
    
    // Print class names for verification
    NSLog(@"Classes of created strings:");
    NSLog(@"  short1: %@", NSStringFromClass([short1 class]));
    NSLog(@"  medium1: %@", NSStringFromClass([medium1 class]));
    NSLog(@"  unicode1: %@", NSStringFromClass([unicode1 class]));
    NSLog(@"  concat1: %@", NSStringFromClass([concat1 class]));
    NSLog(@"  sub1: %@", NSStringFromClass([sub1 class]));
    NSLog(@"  fromUTF8: %@", NSStringFromClass([fromUTF8 class]));
    
    // Breakpoint location for LLDB testing
    NSLog(@"=== Ready for LLDB inspection ==="); // Set breakpoint here (line 89)
    
    // Keep variables alive
    NSLog(@"Variables ready: %@, %@, %@", short1, unicode1, concat1);
}

int main(int argc, char *argv[]) {
    @autoreleasepool {
        test_inline_strings();
    }
    return 0;
}