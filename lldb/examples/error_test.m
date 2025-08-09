#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Create NSError objects
        NSError *simpleError = [NSError errorWithDomain:@"TestDomain" code:404 userInfo:nil];
        NSError *detailedError = [NSError errorWithDomain:@"NSCocoaErrorDomain" 
                                                     code:42 
                                                 userInfo:@{@"key": @"value"}];
        
        printf("Simple error: %p\n", simpleError);  // Breakpoint here
        printf("Detailed error: %p\n", detailedError);
        
        // Keep variables in scope for debugging
        volatile void *keep_alive = simpleError;
        (void)keep_alive;
        return 0;
    }
}