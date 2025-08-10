#import <Foundation/Foundation.h>

@interface SimpleAccount : NSObject {
  NSString *_accountId;
  double _balance;
}
@property(nonatomic, strong) NSString *accountId;
@property(nonatomic, assign) double balance;
@end

@implementation SimpleAccount

@synthesize accountId = _accountId;
@synthesize balance = _balance;

- (instancetype)initWithId:(NSString *)accountId balance:(double)balance {
  if (self = [super init]) {
    _accountId = accountId;
    _balance = balance;
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"SimpleAccount(id=%@, balance=%.2f)", 
          self.accountId, self.balance];
}

@end

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    NSString *greeting = @"Hello, ISA Test!";
    NSNumber *testNumber = @42;
    
    SimpleAccount *account = [[SimpleAccount alloc] initWithId:@"TEST-001" balance:500.0];
    
    NSLog(@"Simple ISA debugging test");
    NSLog(@"Greeting: %@", greeting);
    NSLog(@"Number: %@", testNumber);
    NSLog(@"Account: %@", account);
    
    // Set breakpoint here for testing
    NSLog(@"Breakpoint location - test po commands");
    
    return 0;
  }
}