#import <Foundation/Foundation.h>
#include <unistd.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Test the problematic strings
        NSArray *skills = @[ @"Objective-C", @"Swift", @"Python" ];
        
        NSDictionary *preferences = @{
            @"darkMode": @"enabled",
            @"theme": @"blue"
        };
        
        NSMutableSet *authorizedUsers = [[NSMutableSet alloc] init];
        [authorizedUsers addObject:@"John Doe"];
        [authorizedUsers addObject:@"Jane Doe"];
        
        NSDictionary *summary = @{
            @"account_status": @"active",
            @"current_balance": @1100,
            @"total_transactions": @4
        };
        
        NSLog(@"Test objects created");
        
        // Set breakpoint here
        printf("Breakpoint here\n");
        sleep(5);  // Give time to attach debugger
        
        return 0;
    }
}