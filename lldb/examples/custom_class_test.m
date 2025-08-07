#import <Foundation/Foundation.h>

@interface BankAccount : NSObject {
  NSString *_accountNumber;
  NSString *_ownerName;
  double _balance;
  NSMutableArray *_transactions;
  NSMutableSet *_authorizedUsers;
}
@property(nonatomic, strong) NSString *accountNumber;
@property(nonatomic, strong) NSString *ownerName;
@property(nonatomic, assign) double balance;
@property(nonatomic, strong) NSMutableArray *transactions;
@property(nonatomic, strong) NSMutableSet *authorizedUsers;
@end

@implementation BankAccount

@synthesize accountNumber = _accountNumber;
@synthesize ownerName = _ownerName;
@synthesize balance = _balance;
@synthesize transactions = _transactions;
@synthesize authorizedUsers = _authorizedUsers;

- (instancetype)initWithAccountNumber:(NSString *)accountNumber
                                owner:(NSString *)ownerName {
  if (self = [super init]) {
    _accountNumber = accountNumber;
    _ownerName = ownerName;
    _balance = 0.0;
    _transactions = [[NSMutableArray alloc] init];
    _authorizedUsers = [[NSMutableSet alloc] init];
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

- (BOOL)withdraw:(double)amount description:(NSString *)description {
  if (amount > self.balance) {
    return NO;
  }

  self.balance -= amount;
  NSDictionary *transaction = @{
    @"type" : @"withdrawal",
    @"amount" : @(amount),
    @"description" : description,
    @"balance" : @(self.balance)
  };
  [self.transactions addObject:transaction];
  return YES;
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
    // === SECTION 1: Basic Objects ===
    NSString *greeting2 = @"Hello, Enhanced Debugging!";
    NSNumber *magicNumber = @42;
    NSDate *currentTime = [NSDate date];

    // === SECTION 2: Collections ===
    NSArray *fruits = @[ @"apple", @"banana", @"cherry", @"date" ];
    NSMutableArray *colors =
        [[NSMutableArray alloc] initWithObjects:@"red", @"green", @"blue", nil];
    [colors addObject:@"yellow"];

    NSDictionary *personInfo = @{
      @"name" : @"John Doe",
      @"namerr" : @"John Doe2",
      @"age" : @30,
      @"occupation" : @"Developer",
      @"skills" : @[ @"Objective-C", @"Swift", @"Python" ]
    };

    // Test sets
    NSSet *availableLanguages = [NSSet setWithObjects:@"English", @"Spanish", @"French", @"German", nil];
    NSMutableSet *preferences = [[NSMutableSet alloc] initWithObjects:@"Dark Mode", @"Notifications", nil];
    [preferences addObject:@"Auto-save"];

    // Create a bank account
    BankAccount *account =
        [[BankAccount alloc] initWithAccountNumber:@"ACC-001"
                                             owner:@"John Doe"];

    // Add authorized users to the set
    [account.authorizedUsers addObject:@"John Doe"];
    [account.authorizedUsers addObject:@"Jane Doe"];
    [account.authorizedUsers addObject:@"Bob Smith"];
    [account.authorizedUsers addObject:@"Alice Johnson"];

    // Perform some transactions
    [account deposit:1000.0 description:@"Initial deposit"];
    [account deposit:250.0 description:@"Salary"];
    [account withdraw:100.0 description:@"Groceries"];
    [account withdraw:50.0 description:@"Gas"];

    // Create a summary dictionary
    NSDictionary *accountSummary = @{
      @"account" : account,
      @"summary" : @{
        @"total_transactions" : @(account.transactions.count),
        @"current_balance" : @(account.balance),
        @"account_status" : account.balance > 0 ? @"active" : @"overdrawn"
      }
    };

    // Set breakpoint here to inspect complex nested structures
    NSLog(@"Complex custom class debugging test");
    NSLog(@"Account: %@", account);
    NSLog(@"Account summary: %@", accountSummary);

    // Inspect individual transactions
    for (NSDictionary *transaction in account.transactions) {
      NSLog(@"Transaction: %@", transaction);
    }

    return 0;
  }
}
