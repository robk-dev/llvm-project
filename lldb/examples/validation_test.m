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
        @"balance": @(self.balance)
    };
    [self.transactions addObject:transaction];
}
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSLog(@"=== VALIDATION TEST: Specialist Agent Fixes ===");
        
        // Issue 1: NSNumber display in collections (should show actual values, not -136564472)
        NSArray *numberArray = @[@1, @2, @3, @42, @100];
        NSSet *numberSet = [NSSet setWithArray:numberArray];
        NSLog(@"Created numberArray: %@", numberArray);
        NSLog(@"Created numberSet: %@", numberSet);
        
        // Issue 2: GSCInlineString content visibility
        NSString *shortString = [NSString stringWithFormat:@"Test %d", 123];  // Should create GSCInlineString
        NSString *longString = [NSString stringWithFormat:@"This is a longer string that might be handled differently: %d", 456];
        NSLog(@"Created shortString: %@", shortString);
        NSLog(@"Created longString: %@", longString);
        
        // Issue 3: Custom class NSMutableArray showing garbage counts (should show 4, not 4156636650)
        BankAccount *account = [[BankAccount alloc] initWithAccountNumber:@"ACC-001" owner:@"John Doe"];
        [account addTransaction:@"deposit" amount:1000.0 description:@"Initial deposit"];
        [account addTransaction:@"deposit" amount:250.0 description:@"Salary"];
        [account addTransaction:@"withdrawal" amount:100.0 description:@"Groceries"];
        [account addTransaction:@"withdrawal" amount:50.0 description:@"Gas"];
        NSLog(@"Created account with %ld transactions", (long)account.transactions.count);
        
        // Additional test objects
        NSMutableArray *mixedArray = [NSMutableArray arrayWithObjects:@"string", @42, @3.14, @YES, nil];
        NSDictionary *testDict = @{@"key1": @"value1", @"key2": @42, @"key3": numberArray};
        
        NSLog(@"=== BREAKPOINT HERE - All test objects created ===");
        NSLog(@"Ready for validation of:");
        NSLog(@"1. numberArray elements showing actual values (not -136564472)");
        NSLog(@"2. numberSet elements showing actual values");
        NSLog(@"3. shortString/longString GSCInlineString content visibility");
        NSLog(@"4. account._transactions showing correct count (4, not garbage)");
        NSLog(@"5. Nested collection drill-down functionality");
        
        // Force use of all variables to prevent optimization
        [mixedArray addObject:testDict];
        [mixedArray addObject:account];
        
        return 0;
    }
}