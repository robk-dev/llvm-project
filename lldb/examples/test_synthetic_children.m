#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Simple array with tagged strings
        NSArray *fruits = @[@"apple", @"banana", @"cherry"];
        
        // Dictionary with tagged strings
        NSDictionary *dict = @{
            @"name": @"John",
            @"age": @42,
            @"city": @"Boston"
        };
        
        // Set with mixed objects
        NSSet *set = [NSSet setWithObjects:@"one", @"two", @"three", nil];
        
        NSLog(@"Arrays: %@", fruits);
        NSLog(@"Dictionary: %@", dict);
        NSLog(@"Set: %@", set);
        
        // Breakpoint here
        NSLog(@"Ready for debugging");
        sleep(2);
    }
    return 0;
}