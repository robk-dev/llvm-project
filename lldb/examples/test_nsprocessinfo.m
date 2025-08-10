#import <Foundation/Foundation.h>
#include <stdio.h>

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        printf("=== NSProcessInfo Formatter Test ===\n");
        
        // Get the NSProcessInfo singleton
        NSProcessInfo *processInfo = [NSProcessInfo processInfo];
        NSProcessInfo *nilProcessInfo = nil;
        
        printf("Process info: %p\n", (void*)processInfo);
        printf("Process name: %s\n", [[processInfo processName] UTF8String]);
        printf("PID: %d\n", [processInfo processIdentifier]);
        printf("Nil process info: %p\n", (void*)nilProcessInfo);
        
        // DEBUG: Set breakpoint here
        int debug_pause = 0; // <-- Set breakpoint here
        
        return 0;
    }
}