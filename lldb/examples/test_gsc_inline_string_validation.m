/*
 * Comprehensive validation test for GSCInlineString synthetic provider
 * 
 * This test validates:
 * 1. GSCInlineString synthetic children display (should show content, not <unknown type>)
 * 2. GSCInlineString summary formatting (po command)
 * 3. Regression testing for all other string types
 * 4. Unicode string handling
 * 5. Edge cases (empty strings, nil objects)
 */

#include <Foundation/Foundation.h>
#include <stdio.h>

// Force creation of different string types for testing
NSString* createGSCInlineString(void) {
    // Dynamic strings typically create GSCInlineString objects in GNUstep
    return [NSString stringWithFormat:@"Test %d", 123];
}

NSString* createAnotherGSCInlineString(void) {
    return [NSString stringWithFormat:@"Dynamic %@", @"content"];
}

NSString* createShortGSCInlineString(void) {
    return [NSString stringWithFormat:@"Short"];
}

NSString* createLongGSCInlineString(void) {
    return [NSString stringWithFormat:@"This is a longer string that should still be handled correctly by GSCInlineString formatter"];
}

int main(int argc, char *argv[]) {
    @autoreleasepool {
        printf("=== GSCInlineString Validation Test ===\n");
        
        // Test 1: GSCInlineString objects (dynamic creation)
        NSString *shortString = createGSCInlineString();
        NSString *dynamicString = createAnotherGSCInlineString(); 
        NSString *shortDynamic = createShortGSCInlineString();
        NSString *longString = createLongGSCInlineString();
        
        // Test 2: Other string types for regression testing
        NSString *constString = @"Constant String";
        NSMutableString *mutableString = [NSMutableString stringWithString:@"Mutable"];
        NSString *unicodeString = @"Hello 世界 🌍";
        NSString *emptyString = @"";
        NSMutableString *emptyMutable = [NSMutableString string];
        
        // Test 3: Nil pointer edge case
        NSString *nilString = nil;
        
        // Test 4: More complex mutable string operations
        [mutableString appendString:@" Content"];
        [emptyMutable appendString:@"Now not empty"];
        
        // Test 5: String with various unicode characters
        NSString *complexUnicode = @"Test: αβγ δεζ 中文 🚀🔥⭐";
        
        // Test 6: String created from data (another path)
        NSData *stringData = [@"From Data" dataUsingEncoding:NSUTF8StringEncoding];
        NSString *dataString = [[NSString alloc] initWithData:stringData encoding:NSUTF8StringEncoding];
        
        printf("Created various string types for validation:\n");
        printf("- shortString: %s (class: %s)\n", 
               [shortString UTF8String], 
               [NSStringFromClass([shortString class]) UTF8String]);
        printf("- dynamicString: %s (class: %s)\n", 
               [dynamicString UTF8String],
               [NSStringFromClass([dynamicString class]) UTF8String]);
        printf("- longString: %.50s... (class: %s)\n", 
               [longString UTF8String],
               [NSStringFromClass([longString class]) UTF8String]);
        printf("- constString: %s (class: %s)\n", 
               [constString UTF8String],
               [NSStringFromClass([constString class]) UTF8String]);
        printf("- mutableString: %s (class: %s)\n", 
               [mutableString UTF8String],
               [NSStringFromClass([mutableString class]) UTF8String]);
        printf("- unicodeString: %s (class: %s)\n", 
               [unicodeString UTF8String],
               [NSStringFromClass([unicodeString class]) UTF8String]);
        printf("- complexUnicode: %s (class: %s)\n", 
               [complexUnicode UTF8String],
               [NSStringFromClass([complexUnicode class]) UTF8String]);
        printf("- dataString: %s (class: %s)\n", 
               [dataString UTF8String],
               [NSStringFromClass([dataString class]) UTF8String]);
        
        // Break here for LLDB validation
        printf("\n*** BREAK HERE FOR LLDB VALIDATION ***\n");
        printf("Test strings are ready for inspection.\n");
        
        return 0; // <- Set breakpoint here
    }
}