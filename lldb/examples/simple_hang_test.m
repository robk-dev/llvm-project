#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Create simple test objects
        NSString *testString = @"Hello World";
        NSNumber *testNumber = @42;
        NSArray *testArray = @[@"Apple", @"Banana", @"Cherry"];
        NSDictionary *testDict = @{@"key1": @"value1", @"key2": @"value2"};
        
        // Create a custom object (this should cause the hanging issue)
        NSString *dummy = @"dummy";
        
        // This line should trigger a pause where we can test po commands
        NSLog(@"About to test po commands - set breakpoint here");
        NSLog(@"Objects created: %@, %@, %@, %@", testString, testNumber, testArray, testDict);
        
        // Sleep to allow debugging
        [NSThread sleepForTimeInterval:60.0];
        
        NSLog(@"Test completed");
    }
    return 0;
}