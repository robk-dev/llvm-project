#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        NSCharacterSet *digits = [NSCharacterSet decimalDigitCharacterSet];
        NSCharacterSet *letters = [NSCharacterSet letterCharacterSet];
        NSCharacterSet *whitespace = [NSCharacterSet whitespaceCharacterSet];
        NSCharacterSet *custom = [NSCharacterSet characterSetWithCharactersInString:@"123abc"];
        
        NSLog(@"digits class: %@", NSStringFromClass([digits class]));
        NSLog(@"letters class: %@", NSStringFromClass([letters class]));
        NSLog(@"whitespace class: %@", NSStringFromClass([whitespace class]));
        NSLog(@"custom class: %@", NSStringFromClass([custom class]));
        
        // Set breakpoint here
        printf("Testing character sets\n");  // Line 16
        printf("digits: %p\n", digits);
        printf("custom: %p\n", custom);
    }
    return 0;
}