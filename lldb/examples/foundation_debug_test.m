#import <Foundation/Foundation.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    printf("Creating Foundation test objects...\n");
    
    // NSDate tests
    NSDate *currentDate = [NSDate date];
    NSDate *pastDate = [NSDate dateWithTimeIntervalSince1970:0];
    NSDate *futureDate = [NSDate dateWithTimeIntervalSinceNow:86400];
    NSDate *specificDate = [NSDate dateWithTimeIntervalSinceReferenceDate:0];
    NSDate *nilDate = nil;
    
    // NSURL tests  
    NSURL *httpURL = [NSURL URLWithString:@"https://example.com/path?query=value"];
    NSURL *fileURL = [NSURL fileURLWithPath:@"/path/to/file.txt"];
    NSURL *nilURL = nil;
    
    // NSData tests
    const char *test_data = "Hello World!";
    NSData *smallData = [NSData dataWithBytes:test_data length:strlen(test_data)];
    NSData *emptyData = [NSData data];
    NSData *nilData = nil;
    
    // NSUUID tests
    NSUUID *randomUUID = [[NSUUID alloc] init];
    NSUUID *specificUUID = [[NSUUID alloc] initWithUUIDString:@"550E8400-E29B-41D4-A716-446655440000"];
    NSUUID *nilUUID = nil;
    
    // NSError tests
    NSError *simpleError = [NSError errorWithDomain:@"TestDomain" code:42 userInfo:nil];
    NSError *nilError = nil;
    
    printf("Foundation objects created successfully!\n");
    printf("Process ID: %d\n", getpid());
    printf("Attach debugger now and press Enter to continue...\n");
    getchar(); // Wait for user input
    
    printf("Objects still available for debugging:\n");
    printf("currentDate: %p\n", currentDate);
    printf("httpURL: %p\n", httpURL);
    printf("smallData: %p\n", smallData);
    printf("randomUUID: %p\n", randomUUID);
    printf("simpleError: %p\n", simpleError);
    
    // Keep objects alive until here
    [pool release];
    return 0;
}