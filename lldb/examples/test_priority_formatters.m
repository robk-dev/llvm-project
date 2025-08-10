#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Test NSDecimalNumber
        NSDecimalNumber *zero = [NSDecimalNumber zero];
        NSDecimalNumber *one = [NSDecimalNumber one];
        NSDecimalNumber *decimal = [NSDecimalNumber decimalNumberWithString:@"123.45"];
        NSDecimalNumber *notANumber = [NSDecimalNumber notANumber];
        
        // Test NSCharacterSet
        NSCharacterSet *digits = [NSCharacterSet decimalDigitCharacterSet];
        NSCharacterSet *letters = [NSCharacterSet letterCharacterSet];
        NSCharacterSet *whitespace = [NSCharacterSet whitespaceCharacterSet];
        NSMutableCharacterSet *custom = [NSMutableCharacterSet characterSetWithCharactersInString:@"abc123"];
        
        // Test NSError again to confirm fix
        NSError *testError = [NSError errorWithDomain:@"TestDomain" code:404 userInfo:nil];
        
        printf("Ready for LLDB inspection\n"); // SET BREAKPOINT HERE
        
        return 0;
    }
}