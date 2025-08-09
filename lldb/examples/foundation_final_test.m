//===-- foundation_final_test.m ----------------------------------===//
//
// Final comprehensive test for Foundation formatter fixes
//
//===----------------------------------------------------------------===//

#import <Foundation/Foundation.h>
#include <stdio.h>

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        
        printf("=== Foundation Formatter Final Test ===\n");
        printf("Testing NSAttributedString and NSIndexPath fixes\n\n");
        
        // Test 1: NSAttributedString with different content types
        NSString *simpleString = @"Hello, World!";
        NSAttributedString *attrString1 = [[NSAttributedString alloc] initWithString:simpleString];
        
        NSString *complexString = @"This is a complex string with special characters: éñ中文";
        NSAttributedString *attrString2 = [[NSAttributedString alloc] initWithString:complexString];
        
        NSAttributedString *emptyAttr = [[NSAttributedString alloc] initWithString:@""];
        
        printf("NSAttributedString Test Objects:\n");
        printf("  Simple: %p -> \"%s\"\n", attrString1, [simpleString UTF8String]);
        printf("  Complex: %p -> \"%s\"\n", attrString2, [complexString UTF8String]);
        printf("  Empty: %p -> \"\"\n", emptyAttr);
        printf("Expected formatter output:\n");
        printf("  Simple: \"Hello, World!\" (no attributes)\n");
        printf("  Complex: \"This is a complex string with special characters: éñ中文\" (no attributes)\n");
        printf("  Empty: \"\" (no attributes)\n\n");
        
        // Test 2: NSIndexPath with different configurations
        NSIndexPath *singleIndex = [NSIndexPath indexPathWithIndex:0];
        NSIndexPath *doubleIndex = [NSIndexPath indexPathWithIndexes:(NSUInteger[]){1, 2} length:2];
        NSIndexPath *tripleIndex = [NSIndexPath indexPathWithIndexes:(NSUInteger[]){1, 2, 3} length:3];
        NSIndexPath *longIndex = [NSIndexPath indexPathWithIndexes:(NSUInteger[]){5, 10, 15, 20, 25} length:5];
        
        printf("NSIndexPath Test Objects:\n");
        printf("  Single [0]: %p\n", singleIndex);
        printf("  Double [1,2]: %p\n", doubleIndex);
        printf("  Triple [1,2,3]: %p\n", tripleIndex);
        printf("  Long [5,10,15,20,25]: %p\n", longIndex);
        printf("Expected formatter output:\n");
        printf("  Single: 0\n");
        printf("  Double: 1.2\n");
        printf("  Triple: 1.2.3\n");
        printf("  Long: 5.10.15.20.25\n\n");
        
        // Test 3: Reference objects to ensure other formatters still work
        NSString *testString = @"Reference string";
        NSNumber *testNumber = [NSNumber numberWithInt:42];
        NSArray *testArray = @[@"A", @"B", @"C"];
        NSDictionary *testDict = @{@"key": @"value"};
        
        printf("Reference Objects (should still work):\n");
        printf("  String: %p\n", testString);
        printf("  Number: %p\n", testNumber);
        printf("  Array: %p\n", testArray);
        printf("  Dict: %p\n", testDict);
        
        printf("\n=== LLDB Testing Instructions ===\n");
        printf("Run in LLDB and test:\n");
        printf("  (lldb) po attrString1     # Should show: \"Hello, World!\" (no attributes)\n");
        printf("  (lldb) po attrString2     # Should show complex string with no attributes\n");
        printf("  (lldb) po emptyAttr       # Should show: \"\" (no attributes)\n");
        printf("  (lldb) po singleIndex     # Should show: 0\n");
        printf("  (lldb) po doubleIndex     # Should show: 1.2\n");
        printf("  (lldb) po tripleIndex     # Should show: 1.2.3\n");
        printf("  (lldb) po longIndex       # Should show: 5.10.15.20.25\n");
        
        printf("\nPress Enter to exit...\n");
        getchar();
        
        return 0;
    }
}