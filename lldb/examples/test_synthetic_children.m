//
// test_synthetic_children.m
// Simple test for synthetic children providers
//

#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Create simple collections for testing
    NSArray *fruits = [NSArray arrayWithObjects:@"apple", @"banana", @"cherry", @"date", nil];
    NSLog(@"Fruits array created with %lu elements", (unsigned long)[fruits count]);
    
    NSDictionary *personInfo = [NSDictionary dictionaryWithObjectsAndKeys:
                                @"John Doe", @"name",
                                [NSNumber numberWithInt:42], @"age",
                                @"Developer", @"occupation",
                                nil];
    NSLog(@"Person info dictionary created with %lu entries", (unsigned long)[personInfo count]);
    
    NSSet *colors = [NSSet setWithObjects:@"red", @"green", @"blue", nil];
    NSLog(@"Colors set created with %lu elements", (unsigned long)[colors count]);
    
    // Breakpoint here for testing
    NSLog(@"Collections ready for inspection");
    
    [pool drain];
    return 0;
}