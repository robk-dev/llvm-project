#import <Foundation/Foundation.h>

@interface BankAccount : NSObject {
    NSString *_accountNumber;
    NSString *_ownerName;
    double _balance;
    NSMutableArray *_transactions;
}
@property(nonatomic, strong) NSString *accountNumber;
@property(nonatomic, strong) NSString *ownerName;
@property(nonatomic, assign) double balance;
@property(nonatomic, strong) NSMutableArray *transactions;
@end

@implementation BankAccount
@synthesize accountNumber = _accountNumber;
@synthesize ownerName = _ownerName;
@synthesize balance = _balance;
@synthesize transactions = _transactions;

- (instancetype)initWithAccountNumber:(NSString *)accountNumber owner:(NSString *)ownerName {
    if (self = [super init]) {
        _accountNumber = accountNumber;
        _ownerName = ownerName;
        _balance = 0.0;
        _transactions = [[NSMutableArray alloc] init];
    }
    return self;
}

- (void)addTransaction:(NSString *)type amount:(double)amount description:(NSString *)desc {
    self.balance += ([type isEqualToString:@"deposit"] ? amount : -amount);
    NSDictionary *transaction = @{
        @"type": type,
        @"amount": @(amount),
        @"description": desc,
        @"balance": @(self.balance),
        @"timestamp": [NSDate date]  // Current time - should show as readable date, not hex
    };
    [self.transactions addObject:transaction];
}
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSLog(@"=== COMPREHENSIVE VALIDATION: All Specialist Agent Fixes ===");
        
        // Create current time for testing NSDate/NSTimeInterval formatters in nested dictionaries
        NSDate *currentTime = [NSDate date];
        NSTimeInterval timeInterval = [currentTime timeIntervalSince1970];
        
        NSLog(@"Current time: %@", currentTime);
        NSLog(@"Time interval: %f", timeInterval);
        
        // Issue 1 & 2 VALIDATION: NSNumber display in collections + custom class array counts
        NSArray *numberArray = @[@1, @2, @3, @42, @100];
        NSSet *numberSet = [NSSet setWithArray:numberArray];
        
        BankAccount *account = [[BankAccount alloc] initWithAccountNumber:@"ACC-001" owner:@"John Doe"];
        [account addTransaction:@"deposit" amount:1000.0 description:@"Initial deposit"];
        [account addTransaction:@"deposit" amount:250.0 description:@"Salary"];
        [account addTransaction:@"withdrawal" amount:100.0 description:@"Groceries"];
        [account addTransaction:@"withdrawal" amount:50.0 description:@"Gas"];
        
        // Issue 3 VALIDATION: GSCInlineString content visibility
        NSString *shortString = [NSString stringWithFormat:@"Test %d", 123];  
        NSString *dynamicString = [NSString stringWithFormat:@"Dynamic %@", @"content"];
        
        // Issue 4 VALIDATION: NSDate/NSTimeInterval in nested dictionaries (should show readable dates, not hex)
        NSDictionary *complexDict = @{
            @"user": @{
                @"id": @12345,
                @"name": @"John Doe",
                @"preferences": @{
                    @"theme": @"dark",
                    @"notifications": @YES,
                    @"languages": @[@"en", @"es", @"fr"]
                }
            },
            @"session": @{
                @"token": @"abc123def456",
                @"expires": currentTime,  // <-- Should show readable date, not hex
                @"interval": [NSNumber numberWithDouble:timeInterval],
                @"permissions": [NSSet setWithObjects:@"read", @"write", @"admin", nil]
            },
            @"timing": @{
                @"created": currentTime,
                @"modified": [NSDate dateWithTimeIntervalSinceNow:3600], // 1 hour from now
                @"duration": [NSNumber numberWithDouble:123.45]
            }
        };
        
        // Issue 5 VALIDATION: Modern subscript syntax (runtime method forwarding)
        NSArray *testArray = @[@"Apple", @"Banana", @"Cherry"];
        NSDictionary *testDict = @{@"fruit": @"Apple", @"count": @5, @"fresh": @YES};
        
        NSLog(@"=== VALIDATION CHECKPOINT - All test objects created ===");
        NSLog(@"Ready for comprehensive validation:");
        NSLog(@"1. numberArray/numberSet: Should show actual values (1,2,3...) not -136564472");
        NSLog(@"2. account._transactions: Should show 4 elements, not garbage count");
        NSLog(@"3. shortString/dynamicString: GSCInlineString should show content, not <unknown type>");
        NSLog(@"4. complexDict expires/created/modified: Should show readable dates, not hex values");
        NSLog(@"5. Modern subscript: array[0] and dict[@\"key\"] should work in expressions");
        
        // Test modern subscript syntax manually
        NSString *firstFruit = testArray[0];  // Should work via runtime forwarding
        id fruitValue = testDict[@"fruit"];   // Should work via runtime forwarding
        
        NSLog(@"Modern subscript test results:");
        NSLog(@"testArray[0] = %@", firstFruit);
        NSLog(@"testDict[@\"fruit\"] = %@", fruitValue);
        
        return 0;
    }
}