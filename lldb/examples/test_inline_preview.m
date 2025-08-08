#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Simple test arrays
    NSArray *shortArray = @[@"A", @"B"];
    NSArray *mediumArray = @[@"Red", @"Green", @"Blue", @"Yellow", @"Purple"];
    NSArray *emptyArray = @[];
    
    // Dictionary test
    NSDictionary *simpleDict = @{@"key1": @"value1", @"key2": @"value2"};
    NSDictionary *emptyDict = @{};
    
    // Set test
    NSSet *simpleSet = [NSSet setWithObjects:@"Apple", @"Banana", @"Cherry", nil];
    NSSet *emptySet = [NSSet set];
    
    printf("Collections created - set breakpoint here\n"); // Line 18
    
    [pool drain];
    return 0;
}