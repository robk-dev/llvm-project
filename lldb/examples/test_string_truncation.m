#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test array with strings that should NOT be truncated
        NSArray *skills = @[@"Objective-C", @"Swift", @"Python", @"JavaScript", @"C++"];
        
        NSLog(@"Skills array created: %@", skills);
        
        // Set breakpoint here
        NSLog(@"Breakpoint here to examine skills array");
        
        return 0;
    }
}