//===-- test_subscript_debug.m ---------------------------*- ObjC -*-===//
//
// Interactive test program for runtime method forwarding
//
//===----------------------------------------------------------------------===//

#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSLog(@"Starting subscript forwarding test for LLDB");
        
        // Create test objects
        NSArray *testArray = @[@"Apple", @"Banana", @"Cherry"];
        NSDictionary *testDict = @{
            @"name": @"John Doe",
            @"occupation": @"Developer"
        };
        
        NSMutableArray *mutableArray = [NSMutableArray arrayWithArray:testArray];
        NSMutableDictionary *mutableDict = [NSMutableDictionary dictionaryWithDictionary:testDict];
        
        NSLog(@"Test objects created - set breakpoint here");  // LINE 22
        
        // These operations should work with our forwarding
        id element0 = testArray[0];
        id name = testDict[@"name"];
        
        NSLog(@"Operations completed: %@, %@", element0, name);  // LINE 27
        
        return 0;
    }
}