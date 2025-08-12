// Test program for NSData, NSURL, and NSUUID formatters
// Build with: make test_data_url_uuid
// Debug with: ../build/bin/lldb test_data_url_uuid

#import <Foundation/Foundation.h>
#include <stdio.h>
#include <string.h>

@interface TestDataUrlUuid : NSObject
+ (void)testNSData;
+ (void)testNSURL;
+ (void)testNSUUID;
@end

@implementation TestDataUrlUuid

+ (void)testNSData {
    printf("\n=== Testing NSData objects ===\n");
    
    // Empty NSData
    NSData *emptyData = [NSData data];
    printf("Created empty NSData\n");
    
    // Small NSData with known bytes
    const char *smallBytes = "Hello, World!";
    NSData *smallData = [NSData dataWithBytes:smallBytes length:strlen(smallBytes)];
    printf("Created small NSData with %lu bytes\n", (unsigned long)[smallData length]);
    
    // Binary data with various byte values
    unsigned char binaryBytes[] = {0x00, 0x01, 0x02, 0x03, 0xFE, 0xFF, 0x41, 0x42, 0x43};
    NSData *binaryData = [NSData dataWithBytes:binaryBytes length:sizeof(binaryBytes)];
    printf("Created binary NSData with %lu bytes\n", (unsigned long)[binaryData length]);
    
    // Larger data chunk (simulate real-world scenario)
    NSMutableData *largeData = [NSMutableData dataWithCapacity:1024];
    for (int i = 0; i < 256; i++) {
        unsigned char byte = (unsigned char)(i % 256);
        [largeData appendBytes:&byte length:1];
    }
    printf("Created large NSData with %lu bytes\n", (unsigned long)[largeData length]);
    
    // NSMutableData with modifications
    NSMutableData *mutableData = [NSMutableData dataWithData:smallData];
    const char *appendBytes = " Appended!";
    [mutableData appendBytes:appendBytes length:strlen(appendBytes)];
    printf("Created NSMutableData with %lu bytes\n", (unsigned long)[mutableData length]);
    
    // Nil NSData (error condition)
    NSData *nilData = nil;
    printf("Created nil NSData\n");
    
    // BREAKPOINT 1: Examine NSData objects
    printf("BREAKPOINT 1: Set breakpoint here to examine NSData objects\n"); // Line ~48
}

+ (void)testNSURL {
    printf("\n=== Testing NSURL objects ===\n");
    
    // File URL - absolute path
    NSURL *fileURL = [NSURL fileURLWithPath:@"../README.md"];
    printf("Created file URL for absolute path\n");
    
    // File URL - relative path
    NSURL *relativeFileURL = [NSURL fileURLWithPath:@"./test_data_url_uuid.m"];
    printf("Created file URL for relative path\n");
    
    // HTTP URL - standard web URL
    NSURL *httpURL = [NSURL URLWithString:@"https://llvm.org/docs/"];
    printf("Created HTTP URL\n");
    
    // HTTP URL with query parameters
    NSURL *complexHttpURL = [NSURL URLWithString:@"https://github.com/llvm/llvm-project/search?q=objc&type=code"];
    printf("Created complex HTTP URL with query parameters\n");
    
    // FTP URL
    NSURL *ftpURL = [NSURL URLWithString:@"ftp://ftp.example.com/path/to/file.txt"];
    printf("Created FTP URL\n");
    
    // Custom scheme URL
    NSURL *customURL = [NSURL URLWithString:@"myapp://action?param1=value1&param2=value2"];
    printf("Created custom scheme URL\n");
    
    // Invalid URL (malformed)
    NSURL *invalidURL = [NSURL URLWithString:@"ht!tp://invalid-url-with-!@#-chars"];
    printf("Created potentially invalid URL\n");
    
    // Nil URL
    NSURL *nilURL = nil;
    printf("Created nil URL\n");
    
    // URL with international characters
    NSURL *internationalURL = [NSURL URLWithString:@"https://example.com/path/with-üñíçødé"];
    printf("Created URL with international characters\n");
    
    // BREAKPOINT 2: Examine NSURL objects
    printf("BREAKPOINT 2: Set breakpoint here to examine NSURL objects\n"); // Line ~84
}

+ (void)testNSUUID {
    printf("\n=== Testing NSUUID objects ===\n");
    
    // Random UUID (most common case)
    NSUUID *randomUUID = [[NSUUID alloc] init];
    printf("Created random UUID\n");
    
    // UUID from string (valid format)
    NSString *uuidString = @"550e8400-e29b-41d4-a716-446655440000";
    NSUUID *stringUUID = [[NSUUID alloc] initWithUUIDString:uuidString];
    printf("Created UUID from valid string\n");
    
    // Another random UUID to show different values
    NSUUID *anotherUUID = [[NSUUID alloc] init];
    printf("Created another random UUID\n");
    
    // UUID from invalid string (should be nil)
    NSUUID *invalidStringUUID = [[NSUUID alloc] initWithUUIDString:@"invalid-uuid-string"];
    printf("Attempted to create UUID from invalid string\n");
    
    // Nil UUID
    NSUUID *nilUUID = nil;
    printf("Created nil UUID\n");
    
    // UUID with all zeros (special case)
    NSString *zeroUUIDString = @"00000000-0000-0000-0000-000000000000";
    NSUUID *zeroUUID = [[NSUUID alloc] initWithUUIDString:zeroUUIDString];
    printf("Created zero UUID\n");
    
    // UUID with all ones (another special case)
    NSString *onesUUIDString = @"ffffffff-ffff-ffff-ffff-ffffffffffff";
    NSUUID *onesUUID = [[NSUUID alloc] initWithUUIDString:onesUUIDString];
    printf("Created ones UUID\n");
    
    // BREAKPOINT 3: Examine NSUUID objects
    printf("BREAKPOINT 3: Set breakpoint here to examine NSUUID objects\n"); // Line ~114
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("Starting NSData, NSURL, and NSUUID formatter testing\n");
        printf("================================================\n");
        
        // Create all NSData objects for testing
        printf("\n=== Creating NSData objects ===\n");
        NSData *emptyData = [NSData data];
        const char *smallBytes = "Hello, World!";
        NSData *smallData = [NSData dataWithBytes:smallBytes length:strlen(smallBytes)];
        unsigned char binaryBytes[] = {0x00, 0x01, 0x02, 0x03, 0xFE, 0xFF, 0x41, 0x42, 0x43};
        NSData *binaryData = [NSData dataWithBytes:binaryBytes length:sizeof(binaryBytes)];
        NSMutableData *largeData = [NSMutableData dataWithCapacity:1024];
        for (int i = 0; i < 256; i++) {
            unsigned char byte = (unsigned char)(i % 256);
            [largeData appendBytes:&byte length:1];
        }
        NSMutableData *mutableData = [NSMutableData dataWithData:smallData];
        const char *appendBytes = " Appended!";
        [mutableData appendBytes:appendBytes length:strlen(appendBytes)];
        NSData *nilData = nil;
        printf("All NSData objects created\n");
        
        // Create all NSURL objects for testing
        printf("\n=== Creating NSURL objects ===\n");
        NSURL *fileURL = [NSURL fileURLWithPath:@"../README.md"];
        NSURL *relativeFileURL = [NSURL fileURLWithPath:@"./test_data_url_uuid.m"];
        NSURL *httpURL = [NSURL URLWithString:@"https://llvm.org/docs/"];
        NSURL *complexHttpURL = [NSURL URLWithString:@"https://github.com/llvm/llvm-project/search?q=objc&type=code"];
        NSURL *ftpURL = [NSURL URLWithString:@"ftp://ftp.example.com/path/to/file.txt"];
        NSURL *customURL = [NSURL URLWithString:@"myapp://action?param1=value1&param2=value2"];
        NSURL *invalidURL = [NSURL URLWithString:@"ht!tp://invalid-url-with-!@#-chars"];
        NSURL *nilURL = nil;
        NSURL *internationalURL = [NSURL URLWithString:@"https://example.com/path/with-üñíçødé"];
        printf("All NSURL objects created\n");
        
        // Create all NSUUID objects for testing  
        printf("\n=== Creating NSUUID objects ===\n");
        NSUUID *randomUUID = [[NSUUID alloc] init];
        NSString *uuidString = @"550e8400-e29b-41d4-a716-446655440000";
        NSUUID *stringUUID = [[NSUUID alloc] initWithUUIDString:uuidString];
        NSUUID *anotherUUID = [[NSUUID alloc] init];
        NSUUID *invalidStringUUID = [[NSUUID alloc] initWithUUIDString:@"invalid-uuid-string"];
        NSUUID *nilUUID = nil;
        NSString *zeroUUIDString = @"00000000-0000-0000-0000-000000000000";
        NSUUID *zeroUUID = [[NSUUID alloc] initWithUUIDString:zeroUUIDString];
        NSString *onesUUIDString = @"ffffffff-ffff-ffff-ffff-ffffffffffff";
        NSUUID *onesUUID = [[NSUUID alloc] initWithUUIDString:onesUUIDString];
        printf("All NSUUID objects created\n");
        
        printf("\n=== MASTER BREAKPOINT: All objects ready for inspection ===\n");
        // BREAKPOINT HERE: All objects are in scope for comprehensive testing
        printf("Set breakpoint here to test all formatters at once\n"); // Line ~160
        
        printf("\n=== Testing complete ===\n");
        
        return 0;
    }
}