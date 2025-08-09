//===-- simple_formatter_test.m ----------------------------------===//
//
// Simpler test program for debugging Foundation formatter issues
//
//===----------------------------------------------------------------===//

#import <Foundation/Foundation.h>

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        
        // Test objects
        NSException *exception = [NSException exceptionWithName:@"TestException"
                                                         reason:@"This is a test exception"
                                                       userInfo:nil];
        
        NSAttributedString *attrString = [[NSAttributedString alloc] initWithString:@"Hello, World!"];
        
        NSIndexPath *indexPath = [NSIndexPath indexPathWithIndexes:(NSUInteger[]){1, 2, 3} length:3];
        
        NSNumber *boolYes = [NSNumber numberWithBool:YES];
        NSNumber *boolNo = [NSNumber numberWithBool:NO];
        NSNumber *intNum = [NSNumber numberWithInt:42];
        
        printf("Objects created - setting breakpoint here\n");  // Line 24
        
        return 0;
    }
}