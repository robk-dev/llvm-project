#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Test different NSNumber types
        NSNumber *intNum = @4;
        NSNumber *floatNum = @3.14f;
        NSNumber *boolNum = @YES;
        
        // Test in dictionary
        NSDictionary *testDict = @{
            @"count": @4,
            @"pi": @3.14f,
            @"enabled": @YES
        };
        
        // Set breakpoint here
        NSLog(@"Testing NSNumbers");
        NSLog(@"intNum = %@", intNum);
        NSLog(@"testDict = %@", testDict);
    }
    return 0;
}