#import <Foundation/Foundation.h>

// Custom class for testing
@interface BankAccount : NSObject {
    NSInteger accountNumber;
    NSString *owner;
    double balance;
    NSMutableArray *transactions;
}
@property NSInteger accountNumber;
@property (retain) NSString *owner;
@property double balance;
@property (retain) NSMutableArray *transactions;
- (instancetype)initWithNumber:(NSInteger)number owner:(NSString *)owner;
- (void)deposit:(double)amount;
- (void)withdraw:(double)amount;
@end

@implementation BankAccount
@synthesize accountNumber, owner, balance, transactions;

- (instancetype)initWithNumber:(NSInteger)number owner:(NSString *)ownerName {
    self = [super init];
    if (self) {
        accountNumber = number;
        owner = [ownerName retain];
        balance = 0.0;
        transactions = [[NSMutableArray alloc] init];
    }
    return self;
}

- (void)deposit:(double)amount {
    balance += amount;
    [transactions addObject:@{@"type": @"deposit", @"amount": [NSNumber numberWithDouble:amount]}];
}

- (void)withdraw:(double)amount {
    balance -= amount;
    [transactions addObject:@{@"type": @"withdraw", @"amount": [NSNumber numberWithDouble:amount]}];
}

- (void)dealloc {
    [owner release];
    [transactions release];
    [super dealloc];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"BankAccount(%ld, owner=%@, balance=%.2f, transactions=%lu)",
            (long)accountNumber, owner, balance, (unsigned long)[transactions count]];
}
@end

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // String test objects
    NSString *emptyString = @"";
    NSString *asciiString = @"Hello, World!";
    NSString *utf8String = @"Unicode: 你好 👋";
    NSString *taggedString = @"Hi";  // Small enough for tagging
    
    // Number test objects
    NSNumber *intNumber = [NSNumber numberWithInt:42];
    NSNumber *floatNumber = [NSNumber numberWithFloat:3.14159f];
    NSNumber *boolNumber = [NSNumber numberWithBool:YES];
    NSNumber *taggedInt = [NSNumber numberWithInt:7];  // Small int, likely tagged
    
    // Array test objects
    NSArray *emptyArray = [NSArray array];
    NSArray *simpleArray = @[@"Apple", @"Banana", @"Cherry"];
    NSMutableArray *mutableArray = [NSMutableArray arrayWithObjects:@"One", @"Two", @"Three", nil];
    
    // Dictionary test objects
    NSDictionary *emptyDict = [NSDictionary dictionary];
    NSDictionary *simpleDict = @{@"name": @"John", @"age": @30};
    NSMutableDictionary *mutableDict = [NSMutableDictionary dictionaryWithObjectsAndKeys:
                                        @"Value1", @"Key1",
                                        @"Value2", @"Key2", nil];
    
    // Set test objects
    NSSet *emptySet = [NSSet set];
    NSSet *simpleSet = [NSSet setWithObjects:@"Red", @"Green", @"Blue", nil];
    NSMutableSet *mutableSet = [NSMutableSet setWithObjects:@"Cat", @"Dog", @"Bird", nil];
    
    // Nil object
    id nilObject = nil;
    
    // Custom class object
    BankAccount *account = [[BankAccount alloc] initWithNumber:12345 owner:@"John Doe"];
    [account deposit:1000.00];
    [account withdraw:50.00];
    [account deposit:75.00];
    
    // Nested collections
    NSArray *nestedArray = @[@[@"A", @"B"], @[@"C", @"D", @"E"]];
    NSDictionary *nestedDict = @{
        @"person": @{@"name": @"Alice", @"age": @25},
        @"address": @{@"street": @"123 Main St", @"city": @"Springfield"}
    };
    
    // Large collections for performance testing
    NSMutableArray *largeArray = [NSMutableArray array];
    for (int i = 0; i < 1000; i++) {
        [largeArray addObject:[NSNumber numberWithInt:i]];
    }
    
    NSMutableDictionary *largeDict = [NSMutableDictionary dictionary];
    for (int i = 0; i < 500; i++) {
        NSString *key = [NSString stringWithFormat:@"key%d", i];
        NSString *value = [NSString stringWithFormat:@"value%d", i];
        [largeDict setObject:value forKey:key];
    }
    
    // Print test info
    NSLog(@"GNUstep formatter test program ready");
    NSLog(@"String tests: %@, %@, %@, %@", emptyString, asciiString, utf8String, taggedString);
    NSLog(@"Number tests: %@, %@, %@, %@", intNumber, floatNumber, boolNumber, taggedInt);
    NSLog(@"Array tests: %lu, %lu, %lu", [emptyArray count], [simpleArray count], [mutableArray count]);
    NSLog(@"Dictionary tests: %lu, %lu, %lu", [emptyDict count], [simpleDict count], [mutableDict count]);
    NSLog(@"Set tests: %lu, %lu, %lu", [emptySet count], [simpleSet count], [mutableSet count]);
    NSLog(@"Custom class: %@", account);
    NSLog(@"Large collections: %lu items, %lu pairs", [largeArray count], [largeDict count]);
    
    // Break here for testing
    NSLog(@"All test objects created successfully");
    
    [account release];
    [pool drain];
    return 0;
}