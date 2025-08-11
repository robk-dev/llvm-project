#import <Foundation/Foundation.h>

@interface BankAccount : NSObject {
  NSString *_accountNumber;
  NSString *_ownerName;
  double _balance;
}
@property(nonatomic, strong) NSString *accountNumber;
@property(nonatomic, strong) NSString *ownerName;
@property(nonatomic, assign) double balance;
@end

@implementation BankAccount
@synthesize accountNumber = _accountNumber;
@synthesize ownerName = _ownerName;
@synthesize balance = _balance;

- (instancetype)initWithAccountNumber:(NSString *)accountNumber
                                owner:(NSString *)ownerName {
  if (self = [super init]) {
    _accountNumber = accountNumber;
    _ownerName = ownerName;
    _balance = 0.0;
  }
  return self;
}
@end

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    NSLog(@"=== Class Descriptor Test ===");
    
    BankAccount *account = [[BankAccount alloc] initWithAccountNumber:@"TEST-001" owner:@"John Doe"];
    account.balance = 1000.0;
    
    NSLog(@"Account created: %@", account);
    NSLog(@"Account number: %@", account.accountNumber);
    NSLog(@"Owner: %@", account.ownerName);  
    NSLog(@"Balance: %.2f", account.balance);
    
    // Breakpoint here to test class descriptor
    NSLog(@"=== Class Descriptor Test Complete ===");
    
    return 0;
  }
}