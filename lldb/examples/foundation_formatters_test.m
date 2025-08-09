#import <Foundation/Foundation.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    printf("=== Foundation Formatters Comprehensive Test ===\n");
    
    // Test NSDate formatters
    printf("\n--- NSDate Tests ---\n");
    NSDate *currentDate = [NSDate date];
    NSDate *pastDate = [NSDate dateWithTimeIntervalSince1970:0]; // Unix epoch
    NSDate *futureDate = [NSDate dateWithTimeIntervalSinceNow:86400]; // Tomorrow
    NSDate *distantPast = [NSDate distantPast];
    NSDate *distantFuture = [NSDate distantFuture];
    NSDate *specificDate = [NSDate dateWithTimeIntervalSinceReferenceDate:0]; // NSDate epoch
    NSDate *nilDate = nil;
    
    printf("Current date: %p\n", currentDate);
    printf("Past date: %p\n", pastDate);
    printf("Future date: %p\n", futureDate);
    printf("Distant past: %p\n", distantPast);
    printf("Distant future: %p\n", distantFuture);
    printf("Specific date: %p\n", specificDate);
    printf("Nil date: %p\n", nilDate);
    
    // Test NSCalendarDate if available (deprecated in newer iOS but available in GNUstep)
    #if defined(__GNUSTEP__)
    NSCalendarDate *calendarDate = [NSCalendarDate calendarDate];
    printf("Calendar date: %p\n", calendarDate);
    #endif
    
    // Test NSURL formatters
    printf("\n--- NSURL Tests ---\n");
    NSURL *httpURL = [NSURL URLWithString:@"https://example.com/path?query=value"];
    NSURL *fileURL = [NSURL fileURLWithPath:@"/path/to/file.txt"];
    NSURL *complexURL = [NSURL URLWithString:@"https://user:pass@example.com:8080/path/to/resource?param1=value1&param2=value2#fragment"];
    NSURL *invalidURL = [NSURL URLWithString:@"not-a-valid-url"];
    NSURL *nilURL = nil;
    
    printf("HTTP URL: %p\n", httpURL);
    printf("File URL: %p\n", fileURL);
    printf("Complex URL: %p\n", complexURL);
    printf("Invalid URL: %p\n", invalidURL);
    printf("Nil URL: %p\n", nilURL);
    
    // Test NSData formatters
    printf("\n--- NSData Tests ---\n");
    NSData *emptyData = [NSData data];
    
    const char *small_data = "Hello World!";
    NSData *smallData = [NSData dataWithBytes:small_data length:strlen(small_data)];
    
    // Create medium sized data (100 bytes)
    uint8_t medium_bytes[100];
    for (int i = 0; i < 100; i++) {
        medium_bytes[i] = i % 256;
    }
    NSData *mediumData = [NSData dataWithBytes:medium_bytes length:100];
    
    // Create large data (10KB)
    NSMutableData *largeData = [NSMutableData dataWithCapacity:10240];
    for (int i = 0; i < 10240; i++) {
        uint8_t byte = i % 256;
        [largeData appendBytes:&byte length:1];
    }
    
    NSData *nilData = nil;
    
    printf("Empty data: %p\n", emptyData);
    printf("Small data: %p\n", smallData);
    printf("Medium data: %p\n", mediumData);
    printf("Large data: %p\n", largeData);
    printf("Nil data: %p\n", nilData);
    
    // Test NSMutableData as well
    NSMutableData *mutableData = [NSMutableData dataWithData:smallData];
    [mutableData appendData:smallData];
    printf("Mutable data: %p\n", mutableData);
    
    // Test NSUUID formatters
    printf("\n--- NSUUID Tests ---\n");
    NSUUID *randomUUID = [[NSUUID alloc] init];
    NSUUID *specificUUID = [[NSUUID alloc] initWithUUIDString:@"550E8400-E29B-41D4-A716-446655440000"];
    NSUUID *anotherUUID = [[NSUUID alloc] initWithUUIDString:@"6ba7b810-9dad-11d1-80b4-00c04fd430c8"];
    NSUUID *invalidUUID = [[NSUUID alloc] initWithUUIDString:@"invalid-uuid-string"];
    NSUUID *nilUUID = nil;
    
    printf("Random UUID: %p\n", randomUUID);
    printf("Specific UUID: %p\n", specificUUID);
    printf("Another UUID: %p\n", anotherUUID);
    printf("Invalid UUID: %p\n", invalidUUID);
    printf("Nil UUID: %p\n", nilUUID);
    
    // Test NSError formatters
    printf("\n--- NSError Tests ---\n");
    NSError *simpleError = [NSError errorWithDomain:@"TestDomain" code:42 userInfo:nil];
    
    NSDictionary *userInfo = @{
        NSLocalizedDescriptionKey: @"Something went wrong",
        NSLocalizedFailureReasonErrorKey: @"The operation failed",
        @"CustomKey": @"CustomValue"
    };
    NSError *complexError = [NSError errorWithDomain:@"com.example.MyApp" code:1001 userInfo:userInfo];
    NSError *nilError = nil;
    
    printf("Simple error: %p\n", simpleError);
    printf("Complex error: %p\n", complexError);
    printf("Nil error: %p\n", nilError);
    
    // Performance test - Create objects quickly to test response time
    printf("\n--- Performance Test (100 objects) ---\n");
    NSMutableArray *performanceObjects = [NSMutableArray array];
    
    for (int i = 0; i < 100; i++) {
        [performanceObjects addObject:[NSDate dateWithTimeIntervalSinceNow:i]];
        [performanceObjects addObject:[NSURL URLWithString:[NSString stringWithFormat:@"https://example.com/test%d", i]]];
        [performanceObjects addObject:[NSData dataWithBytes:&i length:sizeof(i)]];
        [performanceObjects addObject:[[NSUUID alloc] init]];
    }
    
    printf("Created %lu performance test objects\n", (unsigned long)[performanceObjects count]);
    
    // Edge cases
    printf("\n--- Edge Cases ---\n");
    
    // Very long URL
    NSMutableString *longURLString = [NSMutableString stringWithString:@"https://example.com/"];
    for (int i = 0; i < 100; i++) {
        [longURLString appendFormat:@"very-long-path-segment-%d/", i];
    }
    NSURL *longURL = [NSURL URLWithString:longURLString];
    printf("Long URL: %p\n", longURL);
    
    // Very large data object (1MB)
    NSMutableData *megabyteData = [NSMutableData dataWithCapacity:1024*1024];
    for (int i = 0; i < 1024*1024; i++) {
        uint8_t byte = i % 256;
        [megabyteData appendBytes:&byte length:1];
    }
    printf("Megabyte data: %p\n", megabyteData);
    
    // Date with extreme values
    NSDate *extremelyOldDate = [NSDate dateWithTimeIntervalSinceReferenceDate:-1000000000];
    NSDate *extremelyNewDate = [NSDate dateWithTimeIntervalSinceReferenceDate:1000000000];
    printf("Extremely old date: %p\n", extremelyOldDate);
    printf("Extremely new date: %p\n", extremelyNewDate);
    
    printf("\n=== Setting breakpoint - use LLDB to inspect objects ===\n");
    printf("All Foundation objects created successfully!\n");
    
    // Breakpoint target - inspect objects here
    int dummy = 42; // <-- SET BREAKPOINT HERE
    
    [pool release];
    return 0;
}