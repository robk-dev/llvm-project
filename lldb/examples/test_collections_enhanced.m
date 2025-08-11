#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Test NSArray with tagged strings
        NSArray *fruits = @[@"Apple", @"Banana", @"Cherry"];
        NSLog(@"fruits = %@", fruits);
        
        // Test NSDictionary with string keys and values
        NSDictionary *personInfo = @{
            @"name": @"John Doe",
            @"occupation": @"Developer",
            @"city": @"San Francisco"
        };
        NSLog(@"personInfo = %@", personInfo);
        
        // Test NSSet with strings
        NSSet *languages = [NSSet setWithObjects:@"ObjC", @"Swift", @"Python", nil];
        NSLog(@"languages = %@", languages);
        
        // Set a breakpoint here to test formatters
        NSLog(@"Test complete");  // Breakpoint here on line 21
    }
    return 0;
}