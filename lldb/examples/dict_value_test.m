// Test program for investigating NSDictionary value type display issues

#include <Foundation/Foundation.h>
#include <stdio.h>

int main() {
    @autoreleasepool {
        NSLog(@"=== Dictionary Value Type Test ===");
        
        // Create dictionary with mixed value types - this is where the issue occurs
        NSMutableDictionary *testDict = [NSMutableDictionary dictionary];
        
        // Add different value types
        [testDict setObject:@"John Doe" forKey:@"name"];              // String
        [testDict setObject:@30 forKey:@"age"];                       // NSNumber (integer)
        [testDict setObject:@3.14159 forKey:@"pi"];                   // NSNumber (double)
        [testDict setObject:@YES forKey:@"active"];                   // NSNumber (bool)
        
        // Add nested collections
        NSArray *skills = @[@"Programming", @"Design"];
        [testDict setObject:skills forKey:@"skills"];
        
        NSSet *tags = [NSSet setWithObjects:@"important", @"work", nil];
        [testDict setObject:tags forKey:@"tags"];
        
        NSDictionary *nested = @{@"level": @5, @"experience": @"Senior"};
        [testDict setObject:nested forKey:@"profile"];
        
        NSLog(@"Dictionary: %@", testDict);
        
        // This is our debug point - set breakpoint here
        NSLog(@"Debug breakpoint - dictionary ready for inspection");
        
        return 0;
    }
}