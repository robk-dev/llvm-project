#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Simple string array to test formatting
        NSArray *fruits = @[@"apple", @"banana", @"cherry", @"date"];
        
        NSLog(@"Test array created with %lu elements", (unsigned long)[fruits count]);
        NSLog(@"Array: %@", fruits);
        
        // Test individual elements
        for (NSUInteger i = 0; i < [fruits count]; i++) {
            NSString *fruit = fruits[i];
            NSLog(@"Element %lu: %@", (unsigned long)i, fruit);
        }
        
        // Create a custom object with an array
        NSMutableArray *items = [NSMutableArray arrayWithArray:fruits];
        [items addObject:@"elderberry"];
        
        NSLog(@"Mutable array: %@", items);
        
        return 0; // Breakpoint here
    }
}