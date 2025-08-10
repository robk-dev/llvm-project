#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    // Create NSCalendar test cases
    NSCalendar *gregorianCalendar = [NSCalendar currentCalendar];
    NSCalendar *explicitGregorian = [[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar];
    
    printf("Calendar objects created\n"); // Breakpoint here
    
    [explicitGregorian release];
    [pool drain];
    return 0;
}