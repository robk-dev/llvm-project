#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSError *simpleError = [NSError errorWithDomain:@"TestDomain" code:404 userInfo:nil];
        NSError *detailedError = [NSError errorWithDomain:@"NSCocoaErrorDomain" 
                                                     code:42 
                                                 userInfo:@{@"description": @"Test error"}];
        
        NSLog(@"Simple error: %@", simpleError);
        NSLog(@"Detailed error: %@", detailedError);
        
        // Dummy line to set a breakpoint
        printf("Errors created - set breakpoint here\n");
        
        return 0;
    }
}