#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Create a simple array
        NSMutableArray *fruits = [NSMutableArray arrayWithObjects:
            @"apple", @"banana", @"cherry", nil];
        
        // Create a simple dictionary  
        NSDictionary *dict = @{@"name": @"John", @"age": @42};
        
        // Set a breakpoint here
        printf("Breakpoint here - test expression evaluation\n");
        
        // LLDB should be able to evaluate:
        // po fruits[0]     -> should print "apple", not "(4156632232 elements)" 
        // po [fruits objectAtIndex:0] -> should print "apple"
        // po dict[@"name"] -> should print "John"
        
        return 0;
    }
}