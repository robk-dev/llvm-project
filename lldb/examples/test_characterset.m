/*
 * Comprehensive NSCharacterSet Formatter Test
 * 
 * This test program creates various NSCharacterSet instances to validate
 * the formatter requirements:
 * - Show predefined set names when possible (e.g., "Letters", "Digits", "Whitespace")
 * - Show character count/sample for custom sets
 * - Handle inverted sets, empty sets, and complex character sets
 */

#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        NSLog(@"=== NSCharacterSet Formatter Test Starting ===");
        
        // Test Case 1: Standard predefined character sets
        NSCharacterSet *letters = [NSCharacterSet letterCharacterSet];
        NSCharacterSet *uppercaseLetters = [NSCharacterSet uppercaseLetterCharacterSet];
        NSCharacterSet *lowercaseLetters = [NSCharacterSet lowercaseLetterCharacterSet];
        NSCharacterSet *digits = [NSCharacterSet decimalDigitCharacterSet];
        NSCharacterSet *alphanumerics = [NSCharacterSet alphanumericCharacterSet];
        NSLog(@"Standard sets: letters, uppercase, lowercase, digits, alphanumerics");
        
        // Test Case 2: Whitespace and control character sets
        NSCharacterSet *whitespace = [NSCharacterSet whitespaceCharacterSet];
        NSCharacterSet *whitespacesAndNewlines = [NSCharacterSet whitespaceAndNewlineCharacterSet];
        NSCharacterSet *controlCharacters = [NSCharacterSet controlCharacterSet];
        NSCharacterSet *nonBaseCharacters = [NSCharacterSet nonBaseCharacterSet];
        NSLog(@"Whitespace and control sets: whitespace, whitespace+newlines, control, non-base");
        
        // Test Case 3: Punctuation and symbol sets
        NSCharacterSet *punctuation = [NSCharacterSet punctuationCharacterSet];
        NSCharacterSet *symbols = [NSCharacterSet symbolCharacterSet];
        NSCharacterSet *illegalCharacters = [NSCharacterSet illegalCharacterSet];
        NSLog(@"Punctuation and symbols: punctuation, symbols, illegal");
        
        // Test Case 4: Custom character sets from strings
        NSCharacterSet *vowels = [NSCharacterSet characterSetWithCharactersInString:@"aeiouAEIOU"];
        NSCharacterSet *consonants = [NSCharacterSet characterSetWithCharactersInString:@"bcdfghjklmnpqrstvwxyzBCDFGHJKLMNPQRSTVWXYZ"];
        NSCharacterSet *binaryDigits = [NSCharacterSet characterSetWithCharactersInString:@"01"];
        NSCharacterSet *hexDigits = [NSCharacterSet characterSetWithCharactersInString:@"0123456789abcdefABCDEF"];
        NSLog(@"Custom string sets: vowels, consonants, binary digits, hex digits");
        
        // Test Case 5: Custom character sets from ranges
        NSCharacterSet *asciiRange = [NSCharacterSet characterSetWithRange:NSMakeRange(32, 95)]; // Printable ASCII
        NSCharacterSet *extendedAscii = [NSCharacterSet characterSetWithRange:NSMakeRange(128, 128)]; // Extended ASCII
        NSLog(@"Range-based sets: printable ASCII [32-126], extended ASCII [128-255]");
        
        // Test Case 6: Single character sets
        NSCharacterSet *singleChar = [NSCharacterSet characterSetWithCharactersInString:@"@"];
        NSCharacterSet *singleUnicode = [NSCharacterSet characterSetWithCharactersInString:@"🎯"];
        NSLog(@"Single character sets: '@' symbol, Unicode emoji");
        
        // Test Case 7: Empty character set
        NSCharacterSet *emptySet = [NSCharacterSet characterSetWithCharactersInString:@""];
        NSLog(@"Empty character set");
        
        // Test Case 8: Inverted character sets
        NSCharacterSet *notDigits = [[NSCharacterSet decimalDigitCharacterSet] invertedSet];
        NSCharacterSet *notLetters = [[NSCharacterSet letterCharacterSet] invertedSet];
        NSCharacterSet *notWhitespace = [[NSCharacterSet whitespaceCharacterSet] invertedSet];
        NSLog(@"Inverted sets: not digits, not letters, not whitespace");
        
        // Test Case 9: Complex inverted custom sets
        NSCharacterSet *notVowels = [[NSCharacterSet characterSetWithCharactersInString:@"aeiouAEIOU"] invertedSet];
        NSCharacterSet *notPrintableAscii = [asciiRange invertedSet];
        NSLog(@"Complex inverted: not vowels, not printable ASCII");
        
        // Test Case 10: Mutable character sets
        NSMutableCharacterSet *mutableSet = [[NSMutableCharacterSet alloc] init];
        [mutableSet addCharactersInString:@"abc"];
        [mutableSet addCharactersInRange:NSMakeRange('0', 10)]; // Add digits 0-9
        [mutableSet removeCharactersInString:@"b"];
        NSLog(@"Mutable set: started empty, added 'abc' and digits 0-9, removed 'b'");
        
        // Test Case 11: Mutable set operations
        NSMutableCharacterSet *combinedSet = [[NSMutableCharacterSet alloc] init];
        [combinedSet formUnionWithCharacterSet:letters];
        [combinedSet formUnionWithCharacterSet:digits];
        [combinedSet formIntersectionWithCharacterSet:alphanumerics];
        NSLog(@"Combined set: union of letters and digits, intersected with alphanumerics");
        
        // Test Case 12: Bitmap-based character set
        NSMutableData *bitmap = [[NSMutableData alloc] initWithLength:8192]; // 65536 bits / 8
        unsigned char *bytes = (unsigned char *)[bitmap mutableBytes];
        // Set bits for some specific characters
        bytes['A' / 8] |= (1 << ('A' % 8));
        bytes['B' / 8] |= (1 << ('B' % 8));
        bytes['C' / 8] |= (1 << ('C' % 8));
        NSCharacterSet *bitmapSet = [NSCharacterSet characterSetWithBitmapRepresentation:bitmap];
        NSLog(@"Bitmap-based set: manually set bits for A, B, C");
        
        // Test Case 13: Unicode category sets (if available)
        NSCharacterSet *decomposables = [NSCharacterSet decomposableCharacterSet];
        NSCharacterSet *capitalizedLetters = [NSCharacterSet capitalizedLetterCharacterSet];
        NSLog(@"Unicode categories: decomposables, capitalized letters");
        
        // Test Case 14: Large custom sets
        NSMutableString *largeString = [[NSMutableString alloc] init];
        for (int i = 0; i < 1000; i++) {
            [largeString appendFormat:@"%c", (char)((i % 26) + 'a')];
        }
        NSCharacterSet *largeCustomSet = [NSCharacterSet characterSetWithCharactersInString:largeString];
        NSLog(@"Large custom set: 1000 character string (but only 26 unique)");
        
        // Test Case 15: URL-related character sets
        NSCharacterSet *urlUserAllowed = [NSCharacterSet URLUserAllowedCharacterSet];
        NSCharacterSet *urlPasswordAllowed = [NSCharacterSet URLPasswordAllowedCharacterSet];
        NSCharacterSet *urlHostAllowed = [NSCharacterSet URLHostAllowedCharacterSet];
        NSCharacterSet *urlPathAllowed = [NSCharacterSet URLPathAllowedCharacterSet];
        NSCharacterSet *urlQueryAllowed = [NSCharacterSet URLQueryAllowedCharacterSet];
        NSCharacterSet *urlFragmentAllowed = [NSCharacterSet URLFragmentAllowedCharacterSet];
        NSLog(@"URL character sets: user, password, host, path, query, fragment");
        
        // Test Case 16: Nil handling
        NSCharacterSet *nilSet = nil;
        NSLog(@"Nil character set for error handling test");
        
        NSLog(@"=== Breakpoint location for LLDB testing ===");
        printf("Testing formatters with a pause...\n"); // Stop here for debugging
        // LLDB test commands:
        // (lldb) b test_characterset.m:120
        // (lldb) run
        // (lldb) po letters             # Expected: "Letters" or "Letter Characters"
        // (lldb) po digits              # Expected: "Digits" or "Decimal Digits"
        // (lldb) po whitespace          # Expected: "Whitespace" or "Whitespace Characters"
        // (lldb) po vowels              # Expected: "10 characters: aeiouAEIOU" or similar
        // (lldb) po binaryDigits        # Expected: "2 characters: 01"
        // (lldb) po hexDigits           # Expected: "22 characters: 0123456789abcdefABCDEF" or truncated
        // (lldb) po singleChar          # Expected: "1 character: @"
        // (lldb) po singleUnicode       # Expected: "1 character: 🎯"
        // (lldb) po emptySet            # Expected: "Empty character set" or "0 characters"
        // (lldb) po notDigits           # Expected: "Inverted Decimal Digits" or similar
        // (lldb) po notVowels           # Expected: "Inverted custom set" or character count
        // (lldb) po mutableSet          # Expected: "8 characters" or show content
        // (lldb) po combinedSet         # Expected: "Alphanumeric Characters" or equivalent
        // (lldb) po bitmapSet           # Expected: "3 characters: ABC"
        // (lldb) po urlPathAllowed      # Expected: "URL Path Allowed" or character count
        // (lldb) po nilSet              # Expected: "(null)" or safe error handling
        
        return 0;
    }
}