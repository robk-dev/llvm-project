#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        NSString *str = @"Test String";
        NSArray *arr = @[@"one", @"two", @"three"];
        NSDictionary *dict = @{@"key": @"value", @"num": @42};
        NSNumber *num = @42;
        
        NSLog(@"Setting breakpoint here"); // Line 10
    }
    return 0;
}