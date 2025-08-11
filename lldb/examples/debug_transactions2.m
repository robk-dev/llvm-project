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

- (instancetype)initWithAccountNumber:(NSString *)accountNumber
                                owner:(NSString *)ownerName {
  if (self = [super init]) {
    _accountNumber = accountNumber;
    _ownerName = ownerName;
    _balance = 0.0;
    _transactions = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)deposit:(double)amount description:(NSString *)description {
  self.balance += amount;
  NSDictionary *transaction = @{
    @"type" : @"deposit",
    @"amount" : @(amount),
    @"description" : description,
    @"balance" : @(self.balance)
  };
  [self.transactions addObject:transaction];
}

- (NSString *)description {
  return [NSString
      stringWithFormat:
          @"BankAccount(%@, owner=%@, balance=%.2f, transactions=%ld)",
          self.accountNumber, self.ownerName, self.balance,
          (long)self.transactions.count];
}

@end

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Create a bank account
    BankAccount *account = [[BankAccount alloc] initWithAccountNumber:@"ACC-001" owner:@"John Doe"];
    
    // Add some transactions
    [account deposit:1000.0 description:@"Initial deposit"];
    [account deposit:250.0 description:@"Salary"];
    [account deposit:100.0 description:@"Bonus"];
    [account deposit:50.0 description:@"Gift"];
    
    NSLog(@"Account created: %@", account);
    NSLog(@"Transactions array: %@", account.transactions);
    NSLog(@"Transaction count: %ld", (long)account.transactions.count);
    
    // BREAKPOINT HERE - Let's inspect the memory
    NSLog(@"About to return - debug here");  // Line 60
    
    return 0;
  }
}