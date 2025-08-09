#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Create test NSError objects
        NSError *simpleError = [NSError errorWithDomain:@"TestDomain" code:404 userInfo:nil];
        NSError *detailedError = [NSError errorWithDomain:@"NSCocoaErrorDomain" 
                                                     code:42 
                                                 userInfo:@{
                                                     @"description": @"Test error",
                                                     @"helpAnchor": @"TestHelp"
                                                 }];
        
        printf("Simple error: %s\n", [[simpleError description] UTF8String]);
        printf("Detailed error: %s\n", [[detailedError description] UTF8String]);
        
        // Breakpoint here for LLDB testing
        printf("Ready for LLDB inspection\n"); // LINE 18 - SET BREAKPOINT HERE
        
        return 0;
    }
}