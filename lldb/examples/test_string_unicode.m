#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test different string types with Unicode
        NSString *asciiString = @"Hello World";
        NSString *unicodeString = @"Hello 世界 🌍";
        NSString *multiLine = @"Line 1\nLine 2\nLine 3";
        
        // Test mutable strings
        NSMutableString *mutableString = [NSMutableString stringWithString:@"Mutable Test"];
        [mutableString appendString:@" - Modified"];
        
        // Test constant strings
        NSConstantString *constString = @"Constant String Test";
        
        // Test string with special characters
        NSString *specialChars = @"Special: <>&\"'@#$%^&*()";
        
        // Test empty and nil
        NSString *emptyString = @"";
        NSString *nilString = nil;
        
        // Set breakpoint here
        NSLog(@"ASCII: %@", asciiString);
        NSLog(@"Unicode: %@", unicodeString);
        NSLog(@"Mutable: %@", mutableString);
        NSLog(@"Constant: %@", constString);
        NSLog(@"Special: %@", specialChars);
        NSLog(@"MultiLine: %@", multiLine);
        NSLog(@"Empty: %@", emptyString);
        NSLog(@"Nil: %@", nilString);
        
        return 0;
    }
}