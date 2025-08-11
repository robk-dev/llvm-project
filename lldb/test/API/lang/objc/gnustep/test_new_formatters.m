#import <Foundation/Foundation.h>

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Test new formatters
    NSNull *nullObj = [NSNull null];
    
    NSException *exception = [NSException exceptionWithName:@"TestException" 
                                                     reason:@"Test reason" 
                                                   userInfo:nil];
    
    NSAttributedString *attrString = [[NSAttributedString alloc] 
        initWithString:@"Hello, World!"];
    
    NSIndexPath *indexPath = [NSIndexPath indexPathWithIndex:1];
    indexPath = [indexPath indexPathByAddingIndex:2];
    indexPath = [indexPath indexPathByAddingIndex:3];
    
    NSNotification *notification = [NSNotification 
        notificationWithName:@"TestNotification" 
                      object:nil 
                    userInfo:@{@"key": @"value"}];
    
    NSDate *date = [NSDate date];
    NSURL *url = [NSURL URLWithString:@"https://example.com"];
    NSData *data = [@"Test Data" dataUsingEncoding:NSUTF8StringEncoding];
    NSUUID *uuid = [NSUUID UUID];
    NSError *error = [NSError errorWithDomain:@"TestDomain" code:42 userInfo:nil];
    
    // Breakpoint line
    printf("Testing new formatters\n"); // Line 35
    
    [pool drain];
    return 0;
}
