#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Test basic dictionary
        NSDictionary *dict = @{
            @"name": @"John Doe",
            @"age": @30,
            @"city": @"New York"
        };
        
        // Test set
        NSSet *set = [NSSet setWithObjects:@"apple", @"banana", @"cherry", nil];
        
        // Test array (should already work)
        NSArray *array = @[@"one", @"two", @"three"];
        
        // Set breakpoint here
        NSLog(@"Testing formatters");
        
        return 0;
    }
}