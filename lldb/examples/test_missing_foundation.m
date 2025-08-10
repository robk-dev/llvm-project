#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

@interface TestObject : NSObject
@property (nonatomic, strong) NSString *name;
@end

@implementation TestObject
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        printf("=== Testing Missing Foundation Formatters ===\n");
        
        // NSBundle Tests
        printf("\n--- NSBundle Tests ---\n");
        NSBundle *mainBundle = [NSBundle mainBundle];
        NSBundle *foundationBundle = [NSBundle bundleWithPath:@"/usr/local/lib/GNUstep/Libraries/gnustep-base/Versions/1.29/libgnustep-base.so"];
        NSBundle *nilBundle = nil;
        
        printf("Main bundle: %p\n", (void*)mainBundle);
        printf("Foundation bundle: %p\n", (void*)foundationBundle);
        printf("Nil bundle: %p\n", (void*)nilBundle);
        
        // NSProcessInfo Tests  
        printf("\n--- NSProcessInfo Tests ---\n");
        NSProcessInfo *processInfo = [NSProcessInfo processInfo];
        NSProcessInfo *nilProcessInfo = nil;
        
        printf("Process info: %p\n", (void*)processInfo);
        printf("Process name: %s\n", [[processInfo processName] UTF8String]);
        printf("PID: %d\n", [processInfo processIdentifier]);
        printf("Nil process info: %p\n", (void*)nilProcessInfo);
        
        // NSUserDefaults Tests
        printf("\n--- NSUserDefaults Tests ---\n");
        NSUserDefaults *standardDefaults = [NSUserDefaults standardUserDefaults];
        // GNUstep doesn't support initWithSuiteName, create a separate instance
        NSUserDefaults *customDefaults = [[NSUserDefaults alloc] init];
        NSUserDefaults *nilDefaults = nil;
        
        // Set some test values
        [standardDefaults setObject:@"test_value" forKey:@"test_key"];
        [standardDefaults setInteger:42 forKey:@"test_number"];
        [standardDefaults setBool:YES forKey:@"test_bool"];
        [standardDefaults synchronize];
        
        if (customDefaults) {
            [customDefaults setObject:@"custom_value" forKey:@"custom_key"];
            [customDefaults synchronize];
        }
        
        printf("Standard defaults: %p\n", (void*)standardDefaults);
        printf("Custom defaults: %p\n", (void*)customDefaults);
        printf("Nil defaults: %p\n", (void*)nilDefaults);
        
        // NSLocale Tests
        printf("\n--- NSLocale Tests ---\n");
        NSLocale *currentLocale = [NSLocale currentLocale];
        NSLocale *systemLocale = [NSLocale systemLocale];
        NSLocale *usLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
        NSLocale *frenchLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"fr_FR"];
        NSLocale *nilLocale = nil;
        
        printf("Current locale: %p\n", (void*)currentLocale);
        printf("System locale: %p\n", (void*)systemLocale);
        printf("US locale: %p\n", (void*)usLocale);
        printf("French locale: %p\n", (void*)frenchLocale);
        printf("Nil locale: %p\n", (void*)nilLocale);
        
        // NSScanner Tests
        printf("\n--- NSScanner Tests ---\n");
        NSString *testString = @"Hello 123 World 456.78 End";
        NSScanner *stringScanner = [NSScanner scannerWithString:testString];
        
        NSString *numberString = @"123.456 789 -42.5";
        NSScanner *numberScanner = [NSScanner scannerWithString:numberString];
        
        NSString *emptyString = @"";
        NSScanner *emptyScanner = [NSScanner scannerWithString:emptyString];
        
        NSScanner *nilScanner = nil;
        
        // Perform some scanning operations to test state
        NSString *scannedWord = nil;
        NSInteger scannedInt = 0;
        double scannedDouble = 0.0;
        
        [stringScanner scanUpToCharactersFromSet:[NSCharacterSet decimalDigitCharacterSet] intoString:&scannedWord];
        [stringScanner scanInteger:&scannedInt];
        
        [numberScanner scanDouble:&scannedDouble];
        
        printf("String scanner: %p (scanning: '%s')\n", (void*)stringScanner, [testString UTF8String]);
        printf("Number scanner: %p (scanning: '%s')\n", (void*)numberScanner, [numberString UTF8String]);
        printf("Empty scanner: %p (scanning: '')\n", (void*)emptyScanner);
        printf("Nil scanner: %p\n", (void*)nilScanner);
        
        printf("Scanned word: '%s', int: %ld, double: %.3f\n", 
               scannedWord ? [scannedWord UTF8String] : "(null)", (long)scannedInt, scannedDouble);
        
        // Additional edge cases
        printf("\n--- Edge Cases ---\n");
        
        // Bundle with invalid path
        NSBundle *invalidBundle = [NSBundle bundleWithPath:@"/nonexistent/path"];
        printf("Invalid bundle: %p\n", (void*)invalidBundle);
        
        // Locale with invalid identifier  
        NSLocale *invalidLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"invalid_locale"];
        printf("Invalid locale: %p\n", (void*)invalidLocale);
        
        // Scanner with nil string (should handle gracefully)
        // NSScanner *nilStringScanner = [NSScanner scannerWithString:nil]; // This would crash
        
        // Test objects for comparison
        TestObject *testObj = [[TestObject alloc] init];
        testObj.name = @"Test Object";
        printf("Test object for comparison: %p\n", (void*)testObj);
        
        printf("\n=== Ready for LLDB breakpoint ===\n");
        
        // Breakpoint line - all objects are still in scope
        printf("Set breakpoint here and test formatters\n"); // LINE 110
        
        // Debug breakpoint placeholder - set breakpoint on next line
        int debug_pause = 0; // <-- Set breakpoint here
        
        return 0;
    }
}