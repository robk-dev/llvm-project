#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSArray *fruits = @[@"apple", @"banana", @"cherry"];
        
        // Print some debug info
        NSLog(@"Array: %@", fruits);
        for (id fruit in fruits) {
            NSLog(@"Fruit: %@ (class: %@)", fruit, [fruit class]);
        }
        
        return 0; // Breakpoint here
    }
}