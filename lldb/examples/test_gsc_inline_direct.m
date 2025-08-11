//===-- test_gsc_inline_direct.m ----------------------------------------===//
// Test program to directly create GSCInlineString objects for testing synthetic provider
//===----------------------------------------------------------------------===//

#import <Foundation/Foundation.h>
#import <stdio.h>

int main(int argc, const char * argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    printf("Testing GSCInlineString synthetic children provider...\n");
    
    // These operations are more likely to create GSCInlineString objects
    NSMutableString *mutableStr = [NSMutableString string];
    [mutableStr appendString:@"Hello"];
    [mutableStr appendString:@" "];
    [mutableStr appendString:@"World"];
    
    // String formatting operations often create inline strings
    NSString *formatted = [NSString stringWithFormat:@"Number: %d", 42];
    
    // Create strings from C strings 
    NSString *fromCString = [NSString stringWithUTF8String:"Dynamic string"];
    
    // Copy mutable string to immutable - this often creates GSCInlineString
    NSString *copied = [NSString stringWithString:mutableStr];
    
    printf("Mutable string: %s\n", [mutableStr UTF8String]);
    printf("Formatted string: %s\n", [formatted UTF8String]);
    printf("From C string: %s\n", [fromCString UTF8String]);
    printf("Copied string: %s\n", [copied UTF8String]);
    
    // Put breakpoint here to examine the objects
    printf("Ready for LLDB inspection...\n"); // BREAKPOINT HERE
    
    [pool release];
    return 0;
}