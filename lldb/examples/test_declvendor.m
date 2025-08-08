#import <Foundation/Foundation.h>
#include <stdio.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== DeclVendor Test ===\n");
        
        // Test basic Foundation objects that should have method declarations
        NSString *testString = @"Hello, GNUstep!";
        NSNumber *testNumber = @42;
        NSArray *testArray = @[@"Apple", @"Banana", @"Cherry"];
        NSDictionary *testDict = @{@"key": @"value", @"name": @"test"};
        NSSet *testSet = [NSSet setWithObjects:@"A", @"B", @"C", nil];
        
        printf("String: %p\n", testString);
        printf("Number: %p\n", testNumber);  
        printf("Array: %p\n", testArray);
        printf("Dict: %p\n", testDict);
        printf("Set: %p\n", testSet);
        
        printf("=== Objects created successfully ===\n");
        return 0; // Breakpoint here
    }
}