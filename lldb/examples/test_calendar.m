#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test NSCalendar
        NSCalendar *gregorianCalendar = [NSCalendar currentCalendar];
        NSLog(@"Current calendar: %@", gregorianCalendar);
        
        // Try to create specific calendar types if available
        NSCalendar *buddhist = [[NSCalendar alloc] initWithCalendarIdentifier:@"NSBuddhistCalendar"];
        NSCalendar *hebrew = [[NSCalendar alloc] initWithCalendarIdentifier:@"NSHebrewCalendar"];
        
        // Set a breakpoint here to inspect calendars
        printf("Break here to inspect calendars\n"); // Line 14
        
        // Print calendar info
        NSLog(@"Gregorian: %@", gregorianCalendar);
        if (buddhist) NSLog(@"Buddhist: %@", buddhist);
        if (hebrew) NSLog(@"Hebrew: %@", hebrew);
        
        return 0;
    }
}