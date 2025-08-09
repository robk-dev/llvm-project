//===-- foundation_formatter_test.m ----------------------------------===//
//
// Test program for debugging Foundation formatter issues
//
//===----------------------------------------------------------------===//

#import <Foundation/Foundation.h>

// Custom NSNumber subclass to test boolean detection
@interface MyCustomNumber : NSNumber
@end

@implementation MyCustomNumber
- (instancetype)initWithBool:(BOOL)value {
    return [super initWithBool:value];
}
@end

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        
        // Test 1: NSException - currently shows empty output
        NSException *exception = [NSException exceptionWithName:@"TestException"
                                                         reason:@"This is a test exception"
                                                       userInfo:nil];
        
        // Test 2: NSAttributedString - currently shows empty output
        NSString *baseString = @"Hello, World!";
        NSAttributedString *attrString = [[NSAttributedString alloc] initWithString:baseString];
        
        // Add some attributes
        NSMutableAttributedString *mutableAttrString = [[NSMutableAttributedString alloc] 
            initWithString:@"Bold text"];
        
        // Test 3: NSIndexPath - shows raw structure instead of clean format
        NSIndexPath *indexPath1 = [NSIndexPath indexPathWithIndex:0];
        NSIndexPath *indexPath2 = [NSIndexPath indexPathWithIndexes:(NSUInteger[]){1, 2, 3} length:3];
        NSIndexPath *indexPath3 = [NSIndexPath indexPathWithIndexes:(NSUInteger[]){5, 10, 15, 20, 25} length:5];
        
        // Test 4: NSBoolean - shows 1/0 instead of YES/NO
        NSNumber *boolYes = [NSNumber numberWithBool:YES];
        NSNumber *boolNo = [NSNumber numberWithBool:NO];
        
        // Create different NSNumber types for comparison
        NSNumber *intNumber = [NSNumber numberWithInt:42];
        NSNumber *floatNumber = [NSNumber numberWithFloat:3.14f];
        NSNumber *doubleNumber = [NSNumber numberWithDouble:2.71828];
        
        // Custom subclass booleans
        MyCustomNumber *customBoolYes = [[MyCustomNumber alloc] initWithBool:YES];
        MyCustomNumber *customBoolNo = [[MyCustomNumber alloc] initWithBool:NO];
        
        // Additional Foundation objects for comprehensive testing
        NSString *testString = @"Test string";
        NSArray *testArray = @[@"Apple", @"Banana", @"Cherry"];
        NSDictionary *testDict = @{@"name": @"John", @"age": @30};
        
        printf("=== Foundation Formatter Debug Test ===\n\n");
        
        printf("1. NSException Test:\n");
        printf("   Object: %p\n", exception);
        printf("   Name: %s\n", [exception.name UTF8String]);
        printf("   Reason: %s\n", [exception.reason UTF8String]);
        printf("   Expected formatter output: NSException: TestException - This is a test exception\n");
        printf("   BREAKPOINT HERE: Test NSException formatter\n\n");
        
        printf("2. NSAttributedString Test:\n");
        printf("   Simple: %p\n", attrString);
        printf("   Mutable: %p\n", mutableAttrString);
        printf("   Base string: %s\n", [baseString UTF8String]);
        printf("   Expected formatter output: \"Hello, World!\" (no attributes)\n");
        printf("   BREAKPOINT HERE: Test NSAttributedString formatter\n\n");
        
        printf("3. NSIndexPath Test:\n");
        printf("   Single index [0]: %p\n", indexPath1);
        printf("   Triple index [1,2,3]: %p\n", indexPath2);
        printf("   Five index [5,10,15,20,25]: %p\n", indexPath3);
        printf("   Expected formatter outputs: '0', '1.2.3', '5.10.15.20.25'\n");
        printf("   BREAKPOINT HERE: Test NSIndexPath formatter\n\n");
        
        printf("4. NSBoolean Test:\n");
        printf("   YES: %p (value=%d)\n", boolYes, [boolYes boolValue]);
        printf("   NO: %p (value=%d)\n", boolNo, [boolNo boolValue]);
        printf("   Custom YES: %p (value=%d)\n", customBoolYes, [customBoolYes boolValue]);
        printf("   Custom NO: %p (value=%d)\n", customBoolNo, [customBoolNo boolValue]);
        printf("   Expected formatter outputs: YES, NO, YES, NO\n");
        printf("   BREAKPOINT HERE: Test NSBoolean formatter\n\n");
        
        printf("5. Other NSNumber Test (for comparison):\n");
        printf("   Int 42: %p\n", intNumber);
        printf("   Float 3.14: %p\n", floatNumber);
        printf("   Double 2.71828: %p\n", doubleNumber);
        printf("   BREAKPOINT HERE: Test other NSNumber formatters\n\n");
        
        printf("6. Reference Objects Test:\n");
        printf("   String: %p\n", testString);
        printf("   Array: %p\n", testArray);
        printf("   Dict: %p\n", testDict);
        printf("   BREAKPOINT HERE: Test reference formatters\n\n");
        
        // Keep objects alive
        printf("Test complete. All objects should be formatted correctly in LLDB.\n");
        printf("Use 'po' command to test formatters.\n");
        
        return 0;
    }
}