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
        
        // Test 6: Scanner with whitespace and special characters
        NSScanner *scanner6 = [NSScanner scannerWithString:@"  \t\ntest\r\n  "];
        [scanner6 setCharactersToBeSkipped:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        
        // Test 7: Scanner with numbers
        NSScanner *scanner7 = [NSScanner scannerWithString:@"3.14159 2.71828"];
        double pi;
        [scanner7 scanDouble:&pi];
        
        // Test 8: Scanner with hex numbers
        NSScanner *scanner8 = [NSScanner scannerWithString:@"0xDEADBEEF 0xCAFEBABE"];
        unsigned int hex;
        [scanner8 scanHexInt:&hex];
        
        // Test 9: Scanner with very long string
        NSMutableString *longString = [NSMutableString string];
        for (int i = 0; i < 100; i++) {
            [longString appendFormat:@"Line %d ", i];
        }
        NSScanner *scanner9 = [NSScanner scannerWithString:longString];
        
        // Test 10: Scanner with case sensitivity off
        NSScanner *scanner10 = [NSScanner scannerWithString:@"Case INSENSITIVE Test"];
        [scanner10 setCaseSensitive:NO];
        
        printf("All scanners created. Set breakpoint here.\n");
        
        // Display scanner info
        printf("scanner1: %p (basic)\n", scanner1);
        printf("scanner2: %p (empty)\n", scanner2);
        printf("scanner3: %p (partial, scanned=%d)\n", scanner3, number);
        printf("scanner4: %p (unicode)\n", scanner4);
        printf("scanner5: %p (at end)\n", scanner5);
        printf("scanner6: %p (whitespace)\n", scanner6);
        printf("scanner7: %p (numbers, pi=%.5f)\n", scanner7, pi);
        printf("scanner8: %p (hex, value=0x%X)\n", scanner8, hex);
        printf("scanner9: %p (very long)\n", scanner9);
        printf("scanner10: %p (case insensitive)\n", scanner10);
        
        return 0;
    }
}