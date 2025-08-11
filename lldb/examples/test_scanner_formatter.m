#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test 1: Basic scanner with string
        NSString *testString = @"Hello World 123";
        NSScanner *scanner1 = [NSScanner scannerWithString:testString];
        
        // Test 2: Scanner with empty string
        NSScanner *scanner2 = [NSScanner scannerWithString:@""];
        
        // Test 3: Scanner that has scanned some content
        NSScanner *scanner3 = [NSScanner scannerWithString:@"The answer is 42 and more"];
        NSString *prefix;
        int number;
        [scanner3 scanString:@"The answer is " intoString:&prefix];
        [scanner3 scanInt:&number];
        
        // Test 4: Scanner with Unicode string
        NSScanner *scanner4 = [NSScanner scannerWithString:@"Hello 世界 🌍"];
        
        // Test 5: Scanner at end
        NSScanner *scanner5 = [NSScanner scannerWithString:@"short"];
        NSString *all;
        [scanner5 scanString:@"short" intoString:&all];
        
        printf("Setting breakpoint here\n"); // Line 27 - breakpoint target
        
        // Print some info to verify objects are created
        printf("scanner1: %p (string: %s)\n", scanner1, [testString UTF8String]);
        printf("scanner2: %p (empty string)\n", scanner2);
        printf("scanner3: %p (partially scanned, number=%d)\n", scanner3, number);
        printf("scanner4: %p (unicode)\n", scanner4);
        printf("scanner5: %p (at end)\n", scanner5);
        
        return 0;
    }
}