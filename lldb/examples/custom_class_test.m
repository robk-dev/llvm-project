#import <Foundation/Foundation.h>
#import <string.h>
#import <stdio.h>

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
    
    // === SECTION 1B: Foundation Types (TDD Tests) ===
    // Test NSDate variations
    NSDate *futureDate = [NSDate dateWithTimeIntervalSinceNow:3600]; // 1 hour from now
    NSDate *pastDate = [NSDate dateWithTimeIntervalSince1970:0]; // Unix epoch
    NSDate *nilDate = nil; // Edge case: nil date
    
    // Test NSURL variations
    NSURL *webURL = [NSURL URLWithString:@"https://www.example.com/path?query=value"];
    NSURL *fileURL = [NSURL fileURLWithPath:@"/usr/local/bin/test"];
    NSURL *complexURL = [NSURL URLWithString:@"ftp://user:pass@host.com:8080/path/to/file.txt"];
    NSURL *nilURL = nil; // Edge case: nil URL
    NSURL *emptyURL = [NSURL URLWithString:@""]; // Edge case: empty string URL
    
    // Test NSError variations
    NSError *simpleError = [NSError errorWithDomain:@"TestDomain" code:404 userInfo:nil];
    NSError *detailedError = [NSError errorWithDomain:NSURLErrorDomain 
                                                 code:NSURLErrorFileDoesNotExist
                                             userInfo:@{
                                                NSLocalizedDescriptionKey: @"File not found",
                                                NSLocalizedFailureReasonErrorKey: @"The specified file does not exist",
                                                NSURLErrorFailingURLStringErrorKey: @"file:///missing.txt"
                                             }];
    NSError *nilError = nil; // Edge case: nil error
    
    // Test NSData variations
    const char *bytes = "Hello, World!";
    NSData *stringData = [NSData dataWithBytes:bytes length:strlen(bytes)];
    NSData *emptyData = [NSData data]; // Edge case: empty data
    NSData *largeData = [NSData dataWithBytes:"A very long string that contains lots of data for testing purposes" 
                                        length:66];
    NSMutableData *mutableData = [NSMutableData dataWithCapacity:100];
    [mutableData appendBytes:"Mutable" length:7];
    NSData *nilData = nil; // Edge case: nil data
    
    // Test NSUUID variations
    NSUUID *randomUUID = [NSUUID UUID];
    NSUUID *specificUUID = [[NSUUID alloc] initWithUUIDString:@"550e8400-e29b-41d4-a716-446655440000"];
    NSUUID *nilUUID = nil; // Edge case: nil UUID
    
    // === NEW HIGH-PRIORITY FOUNDATION OBJECTS ===
    // Test NSNull singleton
    NSNull *nullObject = [NSNull null];
    
    // Test NSException
    NSException *testException = [NSException exceptionWithName:@"TestException" 
                                                         reason:@"This is a test exception for debugging"
                                                       userInfo:@{@"errorCode": @404, @"context": @"Testing"}];
    
    // === PRIORITY 1 FORMATTERS: NSIndexSet, NSDecimalNumber, NSCharacterSet ===
    
    // Test NSIndexSet variations
    NSIndexSet *emptyIndexSet = [NSIndexSet indexSet];
    NSIndexSet *singleIndexSet = [NSIndexSet indexSetWithIndex:42];
    NSIndexSet *rangeIndexSet = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(10, 5)];
    NSMutableIndexSet *mutableIndexSet = [NSMutableIndexSet indexSet];
    [mutableIndexSet addIndex:1];
    [mutableIndexSet addIndex:3];
    [mutableIndexSet addIndex:5];
    [mutableIndexSet addIndexesInRange:NSMakeRange(100, 3)]; // Add 100, 101, 102
    NSIndexSet *nilIndexSet = nil; // Edge case: nil index set
    
    // Test NSDecimalNumber variations
    NSDecimalNumber *integerDecimal = [NSDecimalNumber decimalNumberWithString:@"42"];
    NSDecimalNumber *floatDecimal = [NSDecimalNumber decimalNumberWithString:@"123.456"];
    NSDecimalNumber *largeDecimal = [NSDecimalNumber decimalNumberWithString:@"999999999999999999.123456789"];
    NSDecimalNumber *negativeDecimal = [NSDecimalNumber decimalNumberWithString:@"-987.654"];
    NSDecimalNumber *zeroDecimal = [NSDecimalNumber zero];
    NSDecimalNumber *oneDecimal = [NSDecimalNumber one];
    NSDecimalNumber *notANumber = [NSDecimalNumber notANumber];
    NSDecimalNumber *nilDecimalNumber = nil; // Edge case: nil decimal number
    
    // Test NSCharacterSet variations
    NSCharacterSet *letterCharSet = [NSCharacterSet letterCharacterSet];
    NSCharacterSet *digitCharSet = [NSCharacterSet decimalDigitCharacterSet];
    NSCharacterSet *whitespaceCharSet = [NSCharacterSet whitespaceAndNewlineCharacterSet];
    NSCharacterSet *punctuationCharSet = [NSCharacterSet punctuationCharacterSet];
    NSCharacterSet *customCharSet = [NSCharacterSet characterSetWithCharactersInString:@"abc123!@#"];
    NSMutableCharacterSet *mutableCharSet = [NSMutableCharacterSet letterCharacterSet];
    [mutableCharSet addCharactersInString:@"0123456789"];
    NSCharacterSet *invertedCharSet = [[NSCharacterSet letterCharacterSet] invertedSet];
    NSCharacterSet *nilCharacterSet = nil; // Edge case: nil character set
    
    // Test NSAttributedString
    NSAttributedString *attrString = [[NSAttributedString alloc] 
        initWithString:@"Hello with attributes" 
        attributes:@{@"font": @"Helvetica", @"size": @12}];
    
    // Test NSIndexPath  
    NSUInteger indexes[] = {0, 1, 2};
    NSIndexPath *indexPath = [NSIndexPath indexPathWithIndexes:indexes length:3];
    
    // Test NSNotification
    NSNotification *notification = [NSNotification notificationWithName:@"TestNotification" 
                                                                 object:nil 
                                                               userInfo:@{@"timestamp": currentTime}];

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
    
    // Log the new high-priority objects for testing
    NSLog(@"NSNull: %@", nullObject);
    NSLog(@"NSException: %@", testException);
    NSLog(@"NSAttributedString: %@", attrString);
    NSLog(@"NSIndexPath: %@", indexPath);
    NSLog(@"NSNotification: %@", notification);

    // Inspect individual transactions
    for (NSDictionary *transaction in account.transactions) {
      NSLog(@"Transaction: %@", transaction);
    }
    
    // === SECTION 4: Test Foundation Types ===
    NSLog(@"Testing Foundation formatters:");
    NSLog(@"NSDate - current: %@", currentTime);
    NSLog(@"NSDate - future: %@", futureDate);
    NSLog(@"NSDate - epoch: %@", pastDate);
    
    NSLog(@"NSURL - web: %@", webURL);
    NSLog(@"NSURL - file: %@", fileURL);
    NSLog(@"NSURL - complex: %@", complexURL);
    
    NSLog(@"NSError - simple: %@", simpleError);
    NSLog(@"NSError - detailed: %@", detailedError);
    
    NSLog(@"NSData - string: %@", stringData);
    NSLog(@"NSData - empty: %@", emptyData);
    NSLog(@"NSData - mutable: %@", mutableData);
    
    NSLog(@"NSUUID - random: %@", randomUUID);
    NSLog(@"NSUUID - specific: %@", specificUUID);
    
    // === PRIORITY 1 FORMATTERS OUTPUT ===
    NSLog(@"=== PRIORITY 1 FORMATTERS: NSIndexSet, NSDecimalNumber, NSCharacterSet ===");
    
    NSLog(@"NSIndexSet - empty: %@", emptyIndexSet);
    NSLog(@"NSIndexSet - single (42): %@", singleIndexSet);
    NSLog(@"NSIndexSet - range (10-14): %@", rangeIndexSet);
    NSLog(@"NSMutableIndexSet - scattered: %@", mutableIndexSet);
    
    NSLog(@"NSDecimalNumber - integer: %@", integerDecimal);
    NSLog(@"NSDecimalNumber - float: %@", floatDecimal);
    NSLog(@"NSDecimalNumber - large: %@", largeDecimal);
    NSLog(@"NSDecimalNumber - negative: %@", negativeDecimal);
    NSLog(@"NSDecimalNumber - zero: %@", zeroDecimal);
    NSLog(@"NSDecimalNumber - one: %@", oneDecimal);
    NSLog(@"NSDecimalNumber - NaN: %@", notANumber);
    
    NSLog(@"NSCharacterSet - letters: %@", letterCharSet);
    NSLog(@"NSCharacterSet - digits: %@", digitCharSet);
    NSLog(@"NSCharacterSet - whitespace: %@", whitespaceCharSet);
    NSLog(@"NSCharacterSet - punctuation: %@", punctuationCharSet);
    NSLog(@"NSCharacterSet - custom: %@", customCharSet);
    NSLog(@"NSMutableCharacterSet - mixed: %@", mutableCharSet);
    NSLog(@"NSCharacterSet - inverted letters: %@", invertedCharSet);
    
    // Set a breakpoint here to test all Foundation formatters
    NSLog(@"Foundation formatter test breakpoint"); // Line for setting breakpoint
    
    // Add a simple pause to allow debugging
    printf("Press Enter to continue...\n");
    getchar();

    return 0;
  }
}
