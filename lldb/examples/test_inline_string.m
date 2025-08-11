#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Create some inline strings - these are typically small strings
        NSString *str1 = @"Hello";
        NSString *str2 = [NSString stringWithString:@"World"];
        NSString *str3 = [[NSString alloc] initWithUTF8String:"Test"];
        NSString *str4 = [NSString stringWithFormat:@"Number %d", 42];
        
        // Also test NSUserDefaults which contains GSCInlineString internally
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        [defaults setObject:@"TestValue" forKey:@"TestKey"];
        
        // Bundle paths also use GSCInlineString
        NSBundle *bundle = [NSBundle mainBundle];
        NSString *bundlePath = [bundle bundlePath];
        
        printf("Test point for GSCInlineString formatting\n");
        printf("str1: %s\n", [str1 UTF8String]);
        printf("str2: %s\n", [str2 UTF8String]);
        printf("str3: %s\n", [str3 UTF8String]);
        printf("str4: %s\n", [str4 UTF8String]);
        printf("bundlePath: %s\n", [bundlePath UTF8String]);
        
        // Set a breakpoint here to inspect the variables
        printf("Done - inspect variables above\n");  // Line 27
    }
    return 0;
}