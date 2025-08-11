#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Simple array with tagged strings
        NSArray *fruits = @[@"apple", @"banana", @"cherry"];
        
        NSLog(@"Fruits array created: %@", fruits);
        NSLog(@"First fruit: %@", fruits[0]);
        
        // Breakpoint here to examine the array
        NSLog(@"Set breakpoint on this line");
        
        return 0;
    }
}