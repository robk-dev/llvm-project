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

    // === ADDITIONAL FOUNDATION TYPES FOR COMPREHENSIVE TESTING ===
    
    // Test NSScanner variations (text parsing debugging)
    NSString *testString = @"Hello 123 World 456.78 End";
    NSScanner *stringScanner = [NSScanner scannerWithString:testString];
    NSString *numberString = @"123.456 789 -42.5";
    NSScanner *numberScanner = [NSScanner scannerWithString:numberString];
    NSScanner *emptyScanner = [NSScanner scannerWithString:@""];
    
    // Perform some scanning operations to test state
    NSString *scannedWord = nil;
    NSInteger scannedInt = 0;
    double scannedDouble = 0.0;
    
    [stringScanner scanUpToCharactersFromSet:[NSCharacterSet decimalDigitCharacterSet] intoString:&scannedWord];
    [stringScanner scanInteger:&scannedInt];
    [numberScanner scanDouble:&scannedDouble];
    
    NSScanner *nilScanner = nil; // Edge case: nil scanner
    
    // Test NSBundle variations (app/framework debugging)
    NSBundle *mainBundle = [NSBundle mainBundle];
    NSBundle *foundationBundle = [NSBundle bundleWithPath:@"/usr/local/lib/GNUstep/Libraries/gnustep-base/Versions/1.29/libgnustep-base.so"];
    NSBundle *invalidBundle = [NSBundle bundleWithPath:@"/nonexistent/path"];
    NSBundle *nilBundle = nil; // Edge case: nil bundle
    
    // Test NSProcessInfo variations (system debugging)
    NSProcessInfo *processInfo = [NSProcessInfo processInfo];
    NSProcessInfo *nilProcessInfo = nil; // Edge case: nil process info
    
    // Test NSUserDefaults variations (preferences debugging)
    NSUserDefaults *standardDefaults = [NSUserDefaults standardUserDefaults];
    NSUserDefaults *customDefaults = [[NSUserDefaults alloc] init];
    
    // Set some test values for UserDefaults testing
    [standardDefaults setObject:@"test_value" forKey:@"test_key"];
    [standardDefaults setInteger:42 forKey:@"test_number"];
    [standardDefaults setBool:YES forKey:@"test_bool"];
    [standardDefaults synchronize];
    
    if (customDefaults) {
        [customDefaults setObject:@"custom_value" forKey:@"custom_key"];
        [customDefaults synchronize];
    }
    
    NSUserDefaults *nilDefaults = nil; // Edge case: nil defaults
    
    // Test NSLocale variations (internationalization debugging)
    NSLocale *currentLocale = [NSLocale currentLocale];
    NSLocale *systemLocale = [NSLocale systemLocale];
    NSLocale *usLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
    NSLocale *frenchLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"fr_FR"];
    NSLocale *invalidLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"invalid_locale"];
    NSLocale *nilLocale = nil; // Edge case: nil locale
    
    // Test NSCalendar variations (date/time system debugging)
    NSCalendar *currentCalendar = [NSCalendar currentCalendar];
    NSCalendar *gregorianCalendar = [[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar];
    NSCalendar *autoupdatingCalendar = [NSCalendar autoupdatingCurrentCalendar];

    // Set breakpoint here to inspect complex nested structures
    NSLog(@"Complex custom class debugging test");
    NSLog(@"Account: %@", account);
    NSLog(@"Account summary: %@", accountSummary);
    
    // Log the new high-priority objects for testing
    // NSLog(@"NSNull: %@", nullObject);
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
    
    NSLog(@"NSCalendar - current: %@", currentCalendar);
    NSLog(@"NSCalendar - gregorian: %@", gregorianCalendar);
    NSLog(@"NSCalendar - autoupdating: %@", autoupdatingCalendar);
    
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
    
    // === SECTION 5: Comprehensive Formatter Testing ===
    NSLog(@"=== Comprehensive Formatter Demo ===");
    
    // === NSString Comprehensive Testing ===
    NSLog(@"--- NSString Formatter Tests ---");
    NSString *emptyString = @"";
    NSString *unicodeString = @"Hello 世界 🌍 Émoji";
    NSString *longString = @"This is a very long string that tests how our formatter handles extended content and whether it truncates or displays properly in the debugger view.";
    NSMutableString *mutableString = [NSMutableString stringWithString:@"Mutable"];
    [mutableString appendString:@" String"];
    NSString *nilString = nil;
    
    // === NSNumber Comprehensive Testing ===
    NSLog(@"--- NSNumber Formatter Tests ---");
    NSNumber *positiveInt = @(12345);
    NSNumber *negativeInt = @(-9876);
    NSNumber *largeInt = @(9223372036854775807LL); // LLONG_MAX
    NSNumber *floatNum = @(3.14159f);
    NSNumber *doubleNum = @(2.718281828459045);
    NSNumber *boolYES = @(YES);
    NSNumber *boolNO = @(NO);
    NSNumber *charNum = @('A');
    NSNumber *zeroNum = @(0);
    NSNumber *nilNumber = nil;
    
    // === NSArray Comprehensive Testing ===
    NSLog(@"--- NSArray Formatter Tests ---");
    NSArray *emptyArray = @[];
    NSArray *mixedTypeArray = @[@"string", @42, @3.14, @YES, [NSDate date]];
    NSArray *stringArray = @[@"First", @"Second", @"Third", @"Fourth", @"Fifth"];
    NSArray *numberArray = @[@1, @2, @3, @4, @5, @6, @7, @8, @9, @10];
    NSMutableArray *growingArray = [[NSMutableArray alloc] init];
    for (int i = 0; i < 5; i++) {
        [growingArray addObject:[NSString stringWithFormat:@"Item %d", i]];
    }
    
    // Nested arrays
    NSArray *nestedArray = @[
        @[@"Level1-A", @"Level1-B"],
        @[@"Level2-A", @"Level2-B", @"Level2-C"],
        @[@42, @3.14, @YES]
    ];
    NSArray *nilArray = nil;
    
    // === NSSet Comprehensive Testing ===
    NSLog(@"--- NSSet Formatter Tests ---");
    NSSet *emptySet = [NSSet set];
    NSSet *mixedSet = [NSSet setWithObjects:@"String", @42, @3.14, @YES, nil];
    NSSet *stringSet = [NSSet setWithObjects:@"Alpha", @"Beta", @"Gamma", @"Delta", nil];
    NSSet *numberSet = [NSSet setWithObjects:@1, @2, @3, @4, @5, nil];
    NSMutableSet *growingSet = [[NSMutableSet alloc] init];
    [growingSet addObject:@"Dynamic"];
    [growingSet addObject:@"Content"];
    [growingSet addObject:@"Added"];
    
    // Set with duplicate attempts (should remain unique)
    NSMutableSet *uniqueSet = [[NSMutableSet alloc] init];
    [uniqueSet addObject:@"Unique"];
    [uniqueSet addObject:@"Unique"]; // Duplicate - should not add
    [uniqueSet addObject:@"Different"];
    NSSet *nilSet = nil;
    
    // === NSDictionary Comprehensive Testing ===
    NSLog(@"--- NSDictionary Formatter Tests ---");
    NSDictionary *emptyDict = @{};
    NSDictionary *simpleDict = @{@"key1": @"value1", @"key2": @"value2"};
    NSDictionary *mixedDict = @{
        @"string": @"text",
        @"number": @42,
        @"bool": @YES,
        @"array": @[@1, @2, @3],
        @"nested": @{@"inner": @"value"}
    };
    
    // Complex nested dictionary
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
            @"expires": currentTime,
            @"permissions": [NSSet setWithObjects:@"read", @"write", @"admin", nil]
        }
    };
    
    NSMutableDictionary *mutableDict = [[NSMutableDictionary alloc] init];
    [mutableDict setObject:@"Initial" forKey:@"status"];
    [mutableDict setObject:@42 forKey:@"count"];
    NSDictionary *nilDict = nil;
    
    // === NSIndexSet Comprehensive Testing ===
    NSLog(@"--- NSIndexSet Formatter Tests ---");
    NSMutableIndexSet *complexIndexSet = [[NSMutableIndexSet alloc] init];
    [complexIndexSet addIndex:0];
    [complexIndexSet addIndex:2];
    [complexIndexSet addIndex:4];
    [complexIndexSet addIndexesInRange:NSMakeRange(10, 5)]; // 10-14
    [complexIndexSet addIndexesInRange:NSMakeRange(100, 10)]; // 100-109
    [complexIndexSet addIndex:1000];
    
    NSIndexSet *largeRangeSet = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(50, 20)];
    NSMutableIndexSet *scatteredSet = [[NSMutableIndexSet alloc] init];
    [scatteredSet addIndex:1];
    [scatteredSet addIndex:7];
    [scatteredSet addIndex:42];
    [scatteredSet addIndex:100];
    [scatteredSet addIndex:999];
    
    // === NSDecimalNumber Comprehensive Testing ===
    NSLog(@"--- NSDecimalNumber Formatter Tests ---");
    NSDecimalNumber *precisionDecimal = [NSDecimalNumber decimalNumberWithString:@"123.456789012345"];
    NSDecimalNumber *scientificDecimal = [NSDecimalNumber decimalNumberWithString:@"1.23E+10"];
    NSDecimalNumber *veryLargeDecimal = [NSDecimalNumber decimalNumberWithString:@"999999999999999999999999999999.999999"];
    NSDecimalNumber *verySmallDecimal = [NSDecimalNumber decimalNumberWithString:@"0.000000000000000001"];
    NSDecimalNumber *currencyDecimal = [NSDecimalNumber decimalNumberWithString:@"1299.99"];
    
    // Decimal arithmetic results
    NSDecimalNumber *sum = [integerDecimal decimalNumberByAdding:floatDecimal];
    NSDecimalNumber *product = [currencyDecimal decimalNumberByMultiplyingBy:[NSDecimalNumber decimalNumberWithString:@"1.08"]]; // With tax
    
    // === NSCharacterSet Comprehensive Testing ===
    NSLog(@"--- NSCharacterSet Formatter Tests ---");
    NSCharacterSet *alphanumericSet = [NSCharacterSet alphanumericCharacterSet];
    NSCharacterSet *controlSet = [NSCharacterSet controlCharacterSet];
    NSCharacterSet *symbolSet = [NSCharacterSet symbolCharacterSet];
    NSCharacterSet *newlineSet = [NSCharacterSet newlineCharacterSet];
    
    NSMutableCharacterSet *customCombinedSet = [[NSMutableCharacterSet alloc] init];
    [customCombinedSet formUnionWithCharacterSet:[NSCharacterSet letterCharacterSet]];
    [customCombinedSet formUnionWithCharacterSet:[NSCharacterSet decimalDigitCharacterSet]];
    [customCombinedSet addCharactersInString:@"!@#$%"];
    
    NSCharacterSet *rangeSet = [NSCharacterSet characterSetWithRange:NSMakeRange('A', 26)];
    
    // === Performance Testing with Large Collections ===
    NSLog(@"--- Performance Test Collections (Large Objects) ---");
    
    // Large array (100 elements)
    NSMutableArray *largeArray = [[NSMutableArray alloc] initWithCapacity:100];
    for (int i = 0; i < 100; i++) {
        [largeArray addObject:[NSString stringWithFormat:@"Item_%03d", i]];
    }
    
    // Large set (50 unique elements)
    NSMutableSet *largeSet = [[NSMutableSet alloc] initWithCapacity:50];
    for (int i = 0; i < 50; i++) {
        [largeSet addObject:[NSString stringWithFormat:@"SetItem_%02d", i]];
    }
    
    // Large dictionary (30 key-value pairs)
    NSMutableDictionary *largeDictionary = [[NSMutableDictionary alloc] initWithCapacity:30];
    for (int i = 0; i < 30; i++) {
        NSString *key = [NSString stringWithFormat:@"key_%02d", i];
        NSDictionary *value = @{
            @"index": @(i),
            @"name": [NSString stringWithFormat:@"Value %d", i],
            @"timestamp": [NSDate dateWithTimeIntervalSinceNow:i]
        };
        [largeDictionary setObject:value forKey:key];
    }
    
    // Complex nested structure for performance testing
    NSDictionary *performanceTestStructure = @{
        @"metadata": @{
            @"version": @"1.0",
            @"created": currentTime,
            @"elements": @(100)
        },
        @"large_array": largeArray,
        @"large_set": largeSet,
        @"large_dictionary": largeDictionary,
        @"nested_collections": @{
            @"arrays": @[
                @[@"A1", @"A2", @"A3"],
                @[@"B1", @"B2", @"B3", @"B4"],
                @[@"C1", @"C2"]
            ],
            @"sets": @[
                [NSSet setWithObjects:@"Set1A", @"Set1B", nil],
                [NSSet setWithObjects:@"Set2A", @"Set2B", @"Set2C", nil]
            ]
        }
    };
    
    // === Edge Case Testing ===
    NSLog(@"--- Edge Case Testing ---");
    
    // Arrays with nil elements (should handle gracefully)
    NSMutableArray *arrayWithNils = [[NSMutableArray alloc] init];
    [arrayWithNils addObject:@"Valid"];
    [arrayWithNils addObject:[NSNull null]]; // NSNull instead of nil
    [arrayWithNils addObject:@"AnotherValid"];
    
    // Dictionary with NSNull values
    NSDictionary *dictWithNulls = @{
        @"validKey": @"validValue",
        @"nullKey": [NSNull null],
        @"anotherKey": @"anotherValue"
    };
    
    // Large index set for boundary testing (but within reasonable bounds)
    NSMutableIndexSet *boundaryIndexSet = [[NSMutableIndexSet alloc] init];
    [boundaryIndexSet addIndex:0]; // First possible index
    [boundaryIndexSet addIndex:1000000]; // Large but reasonable index
    [boundaryIndexSet addIndexesInRange:NSMakeRange(999990, 5)]; // 999990-999994
    
    // Decimal numbers at boundaries
    NSDecimalNumber *maxDecimal = [NSDecimalNumber maximumDecimalNumber];
    NSDecimalNumber *minDecimal = [NSDecimalNumber minimumDecimalNumber];
    
    NSLog(@"=== Comprehensive Formatter Test Objects Created ===");
    NSLog(@"String tests: empty=%@, unicode=%@, long=%@, mutable=%@", 
          emptyString, unicodeString, longString, mutableString);
    NSLog(@"Number tests: positive=%@, negative=%@, float=%@, bool=%@", 
          positiveInt, negativeInt, floatNum, boolYES);
    NSLog(@"Array tests: empty=%@, mixed=%@, nested=%@", 
          emptyArray, mixedTypeArray, nestedArray);
    NSLog(@"Set tests: empty=%@, mixed=%@, unique=%@", 
          emptySet, mixedSet, uniqueSet);
    NSLog(@"Dictionary tests: simple=%@, complex structure ready", simpleDict);
    NSLog(@"IndexSet tests: complex=%@, scattered=%@", complexIndexSet, scatteredSet);
    NSLog(@"DecimalNumber tests: precision=%@, currency=%@, sum=%@", 
          precisionDecimal, currencyDecimal, sum);
    NSLog(@"CharacterSet tests: alphanumeric=%@, custom=%@", 
          alphanumericSet, customCombinedSet);
    NSLog(@"Performance tests: large_array(%ld), large_set(%ld), large_dict(%ld)", 
          (long)largeArray.count, (long)largeSet.count, (long)largeDictionary.count);
    
    NSLog(@"=== End Formatter Demo - Set Breakpoint Here ===");
    
    // // Add a simple pause to allow debugging
    // printf("Press Enter to continue...\n");
    // getchar();

    return 0;
  }
}
