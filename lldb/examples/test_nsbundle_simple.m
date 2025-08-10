#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        printf("=== Testing NSBundle Formatter ===\n");
        
        // Create NSBundle objects to test
        NSBundle *mainBundle = [NSBundle mainBundle];
        NSBundle *foundationBundle = [NSBundle bundleWithPath:@"/usr/local/lib/GNUstep/Libraries/gnustep-base/Versions/1.29/libgnustep-base.so"];
        NSBundle *nilBundle = nil;
        NSBundle *invalidBundle = [NSBundle bundleWithPath:@"/nonexistent/path"];
        
        printf("Main bundle: %p\n", (void*)mainBundle);
        printf("Foundation bundle: %p\n", (void*)foundationBundle);
        printf("Nil bundle: %p\n", (void*)nilBundle);
        printf("Invalid bundle: %p\n", (void*)invalidBundle);
        
        printf("\n=== Ready for debugging ===\n");
        
        // This is where we'll set our breakpoint
        int stop_here = 0;
        
        printf("All bundles are in scope - set breakpoint here\n");
        stop_here = 1; // Breakpoint line
        
        return 0;
    }
}