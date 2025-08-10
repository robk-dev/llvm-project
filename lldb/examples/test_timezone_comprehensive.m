#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    printf("=== NSTimeZone Comprehensive Formatter Test ===\n");
    printf("This program tests the enhanced NSTimeZone formatter for LLDB\n\n");

    // Simple fixed-offset timezones (GSAbsTimeZone)
    NSTimeZone *gmtTimeZone = [NSTimeZone timeZoneWithAbbreviation:@"GMT"];
    NSTimeZone *utcTimeZone = [NSTimeZone timeZoneWithName:@"UTC"];
    NSTimeZone *posOffsetTZ = [NSTimeZone timeZoneForSecondsFromGMT:3600];     // +1 hour
    NSTimeZone *negOffsetTZ = [NSTimeZone timeZoneForSecondsFromGMT:-3600];    // -1 hour
    NSTimeZone *halfHourTZ = [NSTimeZone timeZoneForSecondsFromGMT:1800];      // +30 minutes
    
    // Complex zoneinfo-based timezones (GSTimeZone) 
    NSTimeZone *newYorkTZ = [NSTimeZone timeZoneWithName:@"America/New_York"];
    NSTimeZone *losAngelesTZ = [NSTimeZone timeZoneWithName:@"America/Los_Angeles"];
    NSTimeZone *tokyoTZ = [NSTimeZone timeZoneWithName:@"Asia/Tokyo"];
    NSTimeZone *londonTZ = [NSTimeZone timeZoneWithName:@"Europe/London"];
    NSTimeZone *sydneyTZ = [NSTimeZone timeZoneWithName:@"Australia/Sydney"];
    
    // Special timezone types
    NSTimeZone *localTZ = [NSTimeZone localTimeZone];
    NSTimeZone *systemTZ = [NSTimeZone systemTimeZone];
    NSTimeZone *defaultTZ = [NSTimeZone defaultTimeZone];
    
    // Edge cases
    NSTimeZone *nilTimeZone = nil;
    NSTimeZone *invalidTZ = [NSTimeZone timeZoneWithName:@"Invalid/Timezone"];
    
    printf("Test timezones created successfully\n");
    printf("Local timezone: %s\n", [[localTZ name] cString]);
    printf("System timezone: %s\n", [[systemTZ name] cString]);
    
    // Create some dates to test timezone interactions
    NSDate *currentDate = [NSDate date];
    NSDate *winterDate = [NSDate dateWithString:@"2024-01-15 12:00:00 +0000"];
    NSDate *summerDate = [NSDate dateWithString:@"2024-07-15 12:00:00 +0000"];
    
    printf("Test dates created\n");
    
    // Test timezone calculations
    printf("GMT offset for New York: %ld seconds\n", (long)[newYorkTZ secondsFromGMT]);
    printf("GMT offset for Tokyo: %ld seconds\n", (long)[tokyoTZ secondsFromGMT]);
    
    // ** MAIN BREAKPOINT FOR FORMATTER TESTING **
    printf("=== BREAKPOINT: Test timezone formatters here ===\n");  // <- Breakpoint line 41
    
    // Additional timezone information for comprehensive testing
    printf("Timezone testing details:\n");
    printf("- Fixed offset timezones should display as GSAbsTimeZone(name=\"...\", offset=...)\n");
    printf("- Complex timezones should display as GSTimeZone(name=\"...\", offset=..., dst=...)\n");
    printf("- Local timezone should display as NSLocalTimeZone(name=\"...\", current_offset=...)\n");
    printf("- Nil timezone should display as nil\n");
    printf("- Invalid timezone should display as nil or error\n");
    
    // Test timezone abbreviations and display names
    if (newYorkTZ) {
        printf("New York abbreviation: %s\n", [[newYorkTZ abbreviation] cString]);
        printf("New York display name: %s\n", [[newYorkTZ name] cString]);
    }
    
    if (tokyoTZ) {
        printf("Tokyo abbreviation: %s\n", [[tokyoTZ abbreviation] cString]);
        printf("Tokyo display name: %s\n", [[tokyoTZ name] cString]);
    }

    // Test DST information for complex timezones
    if (newYorkTZ && winterDate) {
        BOOL isWinterDST = [newYorkTZ isDaylightSavingTimeForDate:winterDate];
        BOOL isSummerDST = [newYorkTZ isDaylightSavingTimeForDate:summerDate];
        printf("New York DST - Winter: %s, Summer: %s\n", 
               isWinterDST ? "YES" : "NO", 
               isSummerDST ? "YES" : "NO");
    }
    
    printf("=== Test completed - all timezone objects ready for LLDB inspection ===\n");

    [pool drain];
    return 0;
}