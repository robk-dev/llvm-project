#import <Foundation/Foundation.h>

// Simple custom class for testing
@interface BankAccount : NSObject
@property (nonatomic, strong) NSString *accountNumber;
@property (nonatomic, strong) NSString *ownerName;
@property (nonatomic, assign) double balance;
@property (nonatomic, assign) NSInteger transactionCount;
@end

@implementation BankAccount
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        // Create test objects
        BankAccount *account = [[BankAccount alloc] init];
        account.accountNumber = @"ACC-001";
        account.ownerName = @"John Doe";
        account.balance = 1000.0;
        account.transactionCount = 4;
        
        // Create potentially problematic objects
        NSIndexPath *indexPath = [NSIndexPath indexPathWithIndexes:(NSUInteger[]){0, 1, 2} length:3];
        
        NSException *exception = [NSException exceptionWithName:@"TestException"
                                                         reason:@"This is a test exception"
                                                       userInfo:@{@"context": @"Testing", @"errorCode": @404}];
        
        NSNotification *notification = [NSNotification notificationWithName:@"TestNotification"
                                                                      object:nil
                                                                    userInfo:@{@"timestamp": [NSDate date]}];
        
        NSLog(@"Created test objects");
        NSLog(@"Account: %@", account);
        NSLog(@"IndexPath: %@", indexPath);  
        NSLog(@"Exception: %@", exception);
        NSLog(@"Notification: %@", notification);
        
        // Set breakpoint here for crash test
        NSLog(@"Ready for debugging test");
        
        return 0;
    }
}