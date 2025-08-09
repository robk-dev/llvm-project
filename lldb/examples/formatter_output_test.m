//===-- formatter_output_test.m ----------------------------------===//
//
// Test to capture actual formatter outputs and compare with expected
//
//===----------------------------------------------------------------===//

#import <Foundation/Foundation.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        
        printf("=== Foundation Formatter Output Test ===\n");
        
        // Create test objects
        NSString *testString = @"Hello, World!";
        NSAttributedString *attrString = [[NSAttributedString alloc] initWithString:testString];
        
        NSIndexPath *indexPath1 = [NSIndexPath indexPathWithIndex:0];
        NSIndexPath *indexPath2 = [NSIndexPath indexPathWithIndexes:(NSUInteger[]){1, 2, 3} length:3];
        NSIndexPath *indexPath3 = [NSIndexPath indexPathWithIndexes:(NSUInteger[]){5, 10, 15} length:3];
        
        NSException *exception = [NSException exceptionWithName:@"TestException"
                                                         reason:@"Test reason"
                                                       userInfo:nil];
        
        NSNumber *boolYes = [NSNumber numberWithBool:YES];
        NSNumber *boolNo = [NSNumber numberWithBool:NO];
        
        printf("Objects created - addresses:\n");
        printf("  testString: %p\n", testString);
        printf("  attrString: %p\n", attrString);
        printf("  indexPath1: %p\n", indexPath1);
        printf("  indexPath2: %p\n", indexPath2);
        printf("  indexPath3: %p\n", indexPath3);
        printf("  exception: %p\n", exception);
        printf("  boolYes: %p\n", boolYes);
        printf("  boolNo: %p\n", boolNo);
        
        printf("\n=== LLDB Testing Instructions ===\n");
        printf("1. Attach debugger: lldb -p %d\n", getpid());
        printf("2. Test formatters:\n");
        printf("   (lldb) po 0x%p  # testString - should show: \"Hello, World!\"\n", testString);
        printf("   (lldb) po 0x%p  # attrString - should show: \"Hello, World!\" (no attributes)\n", attrString);
        printf("   (lldb) po 0x%p  # indexPath1 - should show: 0\n", indexPath1);
        printf("   (lldb) po 0x%p  # indexPath2 - should show: 1.2.3\n", indexPath2);
        printf("   (lldb) po 0x%p  # indexPath3 - should show: 5.10.15\n", indexPath3);
        printf("   (lldb) po 0x%p  # exception - should show: NSException: TestException - Test reason\n", exception);
        printf("   (lldb) po 0x%p  # boolYes - should show: YES\n", boolYes);
        printf("   (lldb) po 0x%p  # boolNo - should show: NO\n", boolNo);
        
        printf("\nPress Ctrl+C to stop or wait 60 seconds...\n");
        sleep(60);  // Give time to attach debugger
        
        printf("Test complete.\n");
        return 0;
    }
}