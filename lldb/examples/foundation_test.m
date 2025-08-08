#import <Foundation/Foundation.h>
#include <stdio.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== GNUstep Foundation Types Test ===\n");
        
        // NSDate test
        NSDate *currentDate = [NSDate date];
        NSDate *fixedDate = [NSDate dateWithTimeIntervalSinceReferenceDate:123456789.0];
        printf("Current date: %p\n", currentDate);
        printf("Fixed date: %p\n", fixedDate);
        
        // NSURL test  
        NSURL *url1 = [NSURL URLWithString:@"https://example.com/path"];
        NSURL *url2 = [NSURL URLWithString:@"file:///tmp/test.txt"];
        printf("URL 1: %p\n", url1);
        printf("URL 2: %p\n", url2);
        
        // NSError test
        NSError *error1 = [NSError errorWithDomain:@"TestDomain" code:404 userInfo:nil];
        NSError *error2 = [NSError errorWithDomain:@"NSCocoaErrorDomain" code:42 userInfo:nil];
        printf("Error 1: %p\n", error1);  
        printf("Error 2: %p\n", error2);
        
        // NSData test
        NSString *testString = @"Hello, World!";
        NSData *data1 = [testString dataUsingEncoding:NSUTF8StringEncoding];
        NSMutableData *data2 = [NSMutableData dataWithLength:128];
        printf("Data 1: %p\n", data1);
        printf("Data 2: %p\n", data2);
        
        // NSUUID test (if available)
        Class uuidClass = NSClassFromString(@"NSUUID");
        if (uuidClass) {
            NSUUID *uuid1 = [[uuidClass alloc] init];
            printf("UUID: %p\n", uuid1);
        }
        
        printf("=== Test objects created successfully ===\n");
        return 0; // Breakpoint here
    }
}