#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    // NSDate test cases
    NSDate *currentDate = [NSDate date];
    NSDate *distantPast = [NSDate distantPast];
    NSDate *distantFuture = [NSDate distantFuture];
    NSDate *specificDate = [NSDate dateWithTimeIntervalSince1970:1609459200]; // 2021-01-01 00:00:00 UTC
    NSDate *recentDate = [NSDate dateWithTimeIntervalSinceNow:-3600]; // 1 hour ago
    NSDate *nilDate = nil;

    // NSCalendar test cases
    NSCalendar *gregorianCalendar = [NSCalendar currentCalendar];
    NSCalendar *explicitGregorian = [[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar];
#ifdef NSBuddhistCalendar  // If available in GNUstep
    NSCalendar *buddhistCalendar = [[NSCalendar alloc] initWithCalendarIdentifier:NSBuddhistCalendar];
#endif
    NSCalendar *nilCalendar = nil;

    // NSTimeZone test cases
    NSTimeZone *localTimeZone = [NSTimeZone localTimeZone];
    NSTimeZone *gmtTimeZone = [NSTimeZone timeZoneWithAbbreviation:@"GMT"];
    NSTimeZone *utcTimeZone = [NSTimeZone timeZoneWithName:@"UTC"];
    NSTimeZone *nyTimeZone = [NSTimeZone timeZoneWithName:@"America/New_York"];
    NSTimeZone *tokyoTimeZone = [NSTimeZone timeZoneWithName:@"Asia/Tokyo"];
    NSTimeZone *offsetTimeZone = [NSTimeZone timeZoneForSecondsFromGMT:3600]; // +1 hour
    NSTimeZone *nilTimeZone = nil;

    // NSDateFormatter for comparison (shows expected output)
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setDateStyle:NSDateFormatterMediumStyle];
    [formatter setTimeStyle:NSDateFormatterMediumStyle];

    printf("=== Date/Time Objects Test Program ===\n");
    printf("Current date formatted: %s\n", [[formatter stringFromDate:currentDate] cString]);
    printf("Specific date formatted: %s\n", [[formatter stringFromDate:specificDate] cString]);
    
    // Set breakpoint here for testing
    printf("Testing NSDate objects...\n");  // <- Breakpoint line 40

    // Test NSDate components
    NSCalendar *calendar = [NSCalendar currentCalendar];
    NSDateComponents *components = [calendar components:(NSYearCalendarUnit | NSMonthCalendarUnit | NSDayCalendarUnit | NSHourCalendarUnit | NSMinuteCalendarUnit | NSSecondCalendarUnit) 
                                               fromDate:currentDate];
    
    printf("Date components: Year=%ld, Month=%ld, Day=%ld\n", 
           (long)[components year], (long)[components month], (long)[components day]);

    // Test calendar calculations
    NSDate *futureDate = [calendar dateByAddingComponents:components toDate:currentDate options:0];
    NSTimeInterval interval = [futureDate timeIntervalSinceDate:currentDate];
    
    printf("Time interval test: %f seconds\n", interval);

    [formatter release];
    [explicitGregorian release];
#ifdef NSBuddhistCalendar
    [buddhistCalendar release];
#endif

    [pool drain];
    return 0;
}