#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    // Basic timezone test cases for the formatter
    NSTimeZone *localTimeZone = [NSTimeZone localTimeZone];
    NSTimeZone *gmtTimeZone = [NSTimeZone timeZoneWithAbbreviation:@"GMT"];
    NSTimeZone *utcTimeZone = [NSTimeZone timeZoneWithName:@"UTC"];
    NSTimeZone *offsetTimeZone = [NSTimeZone timeZoneForSecondsFromGMT:3600]; // +1 hour
    NSTimeZone *nilTimeZone = nil;

    // Try complex timezones if they exist
    NSTimeZone *nyTimeZone = [NSTimeZone timeZoneWithName:@"America/New_York"];
    NSTimeZone *tokyoTimeZone = [NSTimeZone timeZoneWithName:@"Asia/Tokyo"];

    printf("=== NSTimeZone Test Program ===\n");
    printf("Local timezone: %s\n", [[localTimeZone name] cString]);
    printf("GMT timezone: %s\n", [[gmtTimeZone name] cString]);
    
    // Set breakpoint here for testing the formatter
    printf("Testing NSTimeZone objects...\n");  // <- Breakpoint line 19
    
    printf("GMT offset: %ld seconds\n", (long)[gmtTimeZone secondsFromGMT]);
    printf("Offset timezone offset: %ld seconds\n", (long)[offsetTimeZone secondsFromGMT]);

    [pool drain];
    return 0;
}