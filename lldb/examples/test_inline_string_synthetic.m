//===-- test_inline_string_synthetic.m ----------------------------------===//
// Test program to validate GSCInlineString synthetic children provider
//===----------------------------------------------------------------------===//

#import <Foundation/Foundation.h>
#import <stdio.h>

int main(int argc, const char * argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    printf("Testing GSCInlineString synthetic children provider...\n");
    
    // Create various inline string types
    NSString *shortString = @"Hello";
    NSString *longerString = @"This is a longer string that might use GSCInlineString";
    NSString *emptyString = @"";
    NSString *unicodeString = @"Unicode: 👋🌍🎉";
    
    // These are likely to be GSCInlineString objects in GNUstep
    printf("Short string: %s\n", [shortString UTF8String]);
    printf("Longer string: %s\n", [longerString UTF8String]);
    printf("Empty string: '%s'\n", [emptyString UTF8String]);
    printf("Unicode string: %s\n", [unicodeString UTF8String]);
    
    // Put breakpoint here to examine the objects
    printf("Ready for LLDB inspection...\n"); // BREAKPOINT HERE
    
    [pool release];
    return 0;
}