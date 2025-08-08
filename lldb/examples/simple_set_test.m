#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        // Create a simple NSMutableSet with string objects
        NSMutableSet *preferences = [[NSMutableSet alloc] initWithObjects:@"Dark Mode", @"Notifications", nil];
        [preferences addObject:@"Auto-save"];
        
        // Create a regular NSSet 
        NSSet *languages = [NSSet setWithObjects:@"English", @"Spanish", @"French", nil];
        
        // Breakpoint here to test NSSet formatters
        NSLog(@"Testing NSSet formatters");
        NSLog(@"Preferences: %@", preferences);  
        NSLog(@"Languages: %@", languages);
        
        return 0;
    }
}