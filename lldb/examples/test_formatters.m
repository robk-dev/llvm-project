#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test NSCharacterSet
        NSCharacterSet *digits = [NSCharacterSet decimalDigitCharacterSet];
        NSCharacterSet *letters = [NSCharacterSet letterCharacterSet];
        NSCharacterSet *whitespace = [NSCharacterSet whitespaceCharacterSet];
        NSCharacterSet *custom = [NSCharacterSet characterSetWithCharactersInString:@"123abc"];
        
        // Test NSNotification
        NSDate *currentTime = [NSDate date];
        NSNotification *notification = [NSNotification notificationWithName:@"TestNotification" 
                                                                     object:nil 
                                                                   userInfo:@{@"timestamp": currentTime}];
        
        NSLog(@"digits class: %@", NSStringFromClass([digits class]));
        NSLog(@"notification class: %@", NSStringFromClass([notification class]));
        
        // Breakpoint here
        printf("Testing formatters\n");  // Line 21
        printf("digits: %p\n", digits);
        printf("notification: %p\n", notification);
        
        // Keep objects alive for debugging
        if (argc > 100) {  // Never true, but prevents optimization
            NSLog(@"%@", digits);
            NSLog(@"%@", notification);
        }
        
        return 0;
    }
}