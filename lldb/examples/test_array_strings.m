#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test array with string elements
        NSArray *fruits = @[@"apple", @"banana", @"cherry", @"date"];
        
        NSMutableArray *colors = [[NSMutableArray alloc] initWithObjects:@"red", @"green", @"blue", nil];
        [colors addObject:@"yellow"];
        
        // Mixed array with different types
        NSArray *mixed = @[@"string", @42, @3.14, @YES];
        
        // Array with Unicode strings
        NSArray *unicode = @[@"Hello", @"世界", @"🌍", @"Émoji"];
        
        // Set breakpoint here
        NSLog(@"Fruits: %@", fruits);
        NSLog(@"Colors: %@", colors);
        NSLog(@"Mixed: %@", mixed);
        NSLog(@"Unicode: %@", unicode);
        
        return 0;
    }
}