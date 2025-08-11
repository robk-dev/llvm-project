#include <Foundation/Foundation.h>
#include <stdio.h>

int main() {
    @autoreleasepool {
        // Test NSDate
        NSDate *currentTime = [NSDate date];
        NSDate *specificTime = [NSDate dateWithTimeIntervalSinceReferenceDate:100000.0];
        NSDate *futureTime = [NSDate dateWithTimeIntervalSinceNow:3600.0];
        
        // Test NSTimeInterval (which is just double)
        NSTimeInterval interval = 123456.789;
        
        // Test in dictionary context (the reported issue)
        NSDictionary *timeDict = @{
            @"currentTime": currentTime,
            @"specificTime": specificTime,
            @"futureTime": futureTime,
            @"interval": @(interval)  // NSNumber wrapping NSTimeInterval
        };
        
        // Complex nested dictionary like the user's example
        NSDictionary *complexDict = @{
            @"user": @{
                @"id": @12345,
                @"name": @"John Doe",
                @"preferences": @{
                    @"theme": @"dark",
                    @"notifications": @YES,
                    @"languages": @[@"en", @"es", @"fr"]
                }
            },
            @"session": @{
                @"token": @"abc123def456",
                @"expires": currentTime,  // This should show readable date
                @"permissions": [NSSet setWithObjects:@"read", @"write", @"admin", nil]
            }
        };
        
        printf("Test objects created:\n");
        printf("currentTime: %p\n", currentTime);
        printf("timeDict: %p\n", timeDict);
        printf("complexDict: %p\n", complexDict);
        printf("All date formatter tests ready\n");
        
        return 0; // Set breakpoint here
    }
}