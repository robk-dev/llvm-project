//
// test_synthetic_comprehensive.m
// Comprehensive test for synthetic children providers
//

#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Create collections for testing
    NSArray *fruits = [NSArray arrayWithObjects:@"apple", @"banana", @"cherry", @"date", nil];
    
    NSDictionary *personInfo = [NSDictionary dictionaryWithObjectsAndKeys:
                                @"John Doe", @"name",
                                [NSNumber numberWithInt:42], @"age",
                                @"Developer", @"occupation",
                                nil];
    
    NSSet *colors = [NSSet setWithObjects:@"red", @"green", @"blue", nil];
    
    // Simple loop to keep variables in scope for debugging
    for (int i = 0; i < 1; i++) {
        NSLog(@"Arrays: %lu, Dictionary: %lu, Set: %lu", 
              (unsigned long)[fruits count], 
              (unsigned long)[personInfo count],
              (unsigned long)[colors count]);
        
        // BREAKPOINT HERE - all collections are ready
        break; // Exit after first iteration
    }
    
    [pool drain];
    return 0;
}