#import <Foundation/Foundation.h>
#include <stdio.h>

int main() {
    @autoreleasepool {
        NSDate *date = [NSDate date];
        
        // Print the pointer value
        printf("Date pointer: %p\n", date);
        printf("Date pointer (hex): 0x%lx\n", (unsigned long)date);
        
        // Print the actual time interval
        NSTimeInterval interval = [date timeIntervalSinceReferenceDate];
        printf("Time interval since reference date: %f\n", interval);
        
        // Reference date is 2001-01-01 00:00:00 UTC
        // Current date should be around August 2025
        // That's about 24.5 years = ~773 million seconds
        
        NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
        [formatter setDateFormat:@"yyyy-MM-dd HH:mm:ss 'UTC'"];
        [formatter setTimeZone:[NSTimeZone timeZoneWithName:@"UTC"]];
        NSString *dateString = [formatter stringFromDate:date];
        printf("Formatted date: %s\n", [dateString UTF8String]);
        
        // Check if it's a tagged pointer
        if (((unsigned long)date & 0x7) == 6) {
            printf("This is a tagged NSDate (tag 6)\n");
        }
        
        return 0;
    }
}