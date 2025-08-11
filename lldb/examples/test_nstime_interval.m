#import <Foundation/Foundation.h>
#include <stdio.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        
        // Test various NSTimeInterval scenarios
        NSTimeInterval timestamp1 = [[NSDate date] timeIntervalSinceReferenceDate];  // Current timestamp
        NSTimeInterval timestamp2 = [[NSDate dateWithTimeIntervalSince1970:1234567890] timeIntervalSinceReferenceDate]; // Historic timestamp
        
        NSTimeInterval duration1 = 3661.5;        // 1 hour, 1 minute, 1.5 seconds
        NSTimeInterval duration2 = 123.456;       // 2 minutes, 3.456 seconds
        NSTimeInterval duration3 = 0.001234;      // 1.234 milliseconds
        NSTimeInterval duration4 = 0.000001234;   // 1.234 microseconds
        NSTimeInterval duration5 = 86401.0;       // 1 day, 1 second
        
        NSTimeInterval negative_duration = -3661.5; // Negative duration
        NSTimeInterval zero_duration = 0.0;         // Zero duration
        
        // Special values
        NSTimeInterval nan_value = 0.0 / 0.0;       // NaN
        NSTimeInterval inf_value = 1.0 / 0.0;       // Infinity
        NSTimeInterval neg_inf_value = -1.0 / 0.0;  // Negative infinity
        
        printf("Created various NSTimeInterval test values\n");
        printf("Current timestamp: %f\n", timestamp1);
        printf("Historic timestamp: %f\n", timestamp2);
        printf("Duration 1: %f seconds\n", duration1);
        printf("Duration 2: %f seconds\n", duration2);
        printf("Duration 3: %f seconds\n", duration3);
        printf("Duration 4: %f seconds\n", duration4);
        printf("Duration 5: %f seconds\n", duration5);
        printf("Negative duration: %f seconds\n", negative_duration);
        printf("Zero duration: %f seconds\n", zero_duration);
        printf("NaN value: %f\n", nan_value);
        printf("Infinity value: %f\n", inf_value);
        printf("Negative infinity value: %f\n", neg_inf_value);
        
        // Breakpoint target - set breakpoint here to test formatter
        printf("Test complete - set breakpoint here to examine variables\n");
        
        return 0;
    }
}