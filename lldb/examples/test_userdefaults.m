// test_userdefaults.m - Comprehensive NSUserDefaults formatter test
// Tests NSUserDefaults with various data types and edge cases
// Expected formatter output should show dictionary-like display of defaults

#import <Foundation/Foundation.h>

void test_nil_defaults() {
    NSUserDefaults *nilDefaults = nil;
    NSLog(@"Testing nil NSUserDefaults"); // Breakpoint 1: po nilDefaults
    // Expected: nil
}

void test_empty_defaults() {
    // Create a custom domain with no values
    NSUserDefaults *defaults = [[NSUserDefaults alloc] init];
    NSString *testDomain = @"com.test.empty";
    [defaults removePersistentDomainForName:testDomain];
    
    NSLog(@"Testing empty defaults domain"); // Breakpoint 2: po defaults
    // Expected: NSUserDefaults with minimal system defaults
}

void test_simple_defaults() {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    // Set some simple values
    [defaults setObject:@"TestUser" forKey:@"username"];
    [defaults setInteger:42 forKey:@"userAge"];
    [defaults setBool:YES forKey:@"darkModeEnabled"];
    [defaults setFloat:3.14159f forKey:@"piValue"];
    [defaults setDouble:2.71828 forKey:@"eulerNumber"];
    
    // Synchronize to ensure values are persisted
    [defaults synchronize];
    
    NSLog(@"Testing simple defaults"); // Breakpoint 3: po defaults
    // Expected: NSUserDefaults containing:
    // username = "TestUser"
    // userAge = 42
    // darkModeEnabled = YES
    // piValue = 3.14159
    // eulerNumber = 2.71828
}

void test_complex_defaults() {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    // Complex data types
    NSArray *favoriteColors = @[@"Red", @"Green", @"Blue"];
    NSDictionary *userPrefs = @{
        @"theme": @"dark",
        @"fontSize": @14,
        @"autoSave": @YES
    };
    NSDate *lastLogin = [NSDate date];
    NSData *binaryData = [@"Binary content" dataUsingEncoding:NSUTF8StringEncoding];
    NSURL *websiteURL = [NSURL URLWithString:@"https://example.com"];
    
    [defaults setObject:favoriteColors forKey:@"favoriteColors"];
    [defaults setObject:userPrefs forKey:@"userPreferences"];
    [defaults setObject:lastLogin forKey:@"lastLogin"];
    [defaults setObject:binaryData forKey:@"cachedData"];
    [defaults setObject:[websiteURL absoluteString] forKey:@"websiteURL"];
    
    [defaults synchronize];
    
    NSLog(@"Testing complex defaults"); // Breakpoint 4: po defaults
    // Expected: NSUserDefaults with nested collections and various types
}

void test_edge_cases() {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    // Edge cases - NSNull is not allowed in NSUserDefaults
    // We'll test removal of a key instead
    [defaults removeObjectForKey:@"nullValue"];
    [defaults setObject:@"" forKey:@"emptyString"];
    [defaults setObject:@[] forKey:@"emptyArray"];
    [defaults setObject:@{} forKey:@"emptyDictionary"];
    
    // Very long string
    NSMutableString *longString = [NSMutableString string];
    for (int i = 0; i < 1000; i++) {
        [longString appendString:@"LongString"];
    }
    [defaults setObject:longString forKey:@"veryLongString"];
    
    // Large array
    NSMutableArray *largeArray = [NSMutableArray array];
    for (int i = 0; i < 100; i++) {
        [largeArray addObject:@(i)];
    }
    [defaults setObject:largeArray forKey:@"largeArray"];
    
    [defaults synchronize];
    
    NSLog(@"Testing edge cases"); // Breakpoint 5: po defaults
    // Expected: Properly handle edge cases, truncate if needed
}

void test_volatile_domains() {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    // Test volatile domain (not persisted)
    NSDictionary *volatileSettings = @{
        @"tempSetting1": @"volatile value",
        @"tempSetting2": @123,
        @"tempSetting3": @YES
    };
    
    [defaults setVolatileDomain:volatileSettings forName:@"VolatileDomain"];
    
    NSLog(@"Testing volatile domains"); // Breakpoint 6: po defaults
    // Expected: Show both persistent and volatile domains
}

void test_suite_registration() {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    // Register defaults (fallback values)
    NSDictionary *registrationDict = @{
        @"defaultUsername": @"Guest",
        @"defaultTheme": @"light",
        @"defaultFontSize": @12,
        @"defaultAutoSave": @NO
    };
    
    [defaults registerDefaults:registrationDict];
    
    NSLog(@"Testing suite registration"); // Breakpoint 7: po defaults
    // Expected: Show registered defaults as fallback values
}

void test_removal_operations() {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    // Set then remove values
    [defaults setObject:@"ToBeRemoved" forKey:@"temporaryKey"];
    [defaults synchronize];
    
    // Verify it's set
    NSString *value = [defaults stringForKey:@"temporaryKey"];
    NSLog(@"Before removal: %@", value);
    
    // Remove it
    [defaults removeObjectForKey:@"temporaryKey"];
    [defaults synchronize];
    
    NSLog(@"Testing after removal"); // Breakpoint 8: po defaults
    // Expected: temporaryKey should not appear in defaults
}

void test_performance() {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    // Performance test with many keys
    NSDate *start = [NSDate date];
    
    for (int i = 0; i < 1000; i++) {
        NSString *key = [NSString stringWithFormat:@"perfKey%d", i];
        [defaults setInteger:i forKey:key];
    }
    
    [defaults synchronize];
    
    NSTimeInterval elapsed = -[start timeIntervalSinceNow];
    NSLog(@"Setting 1000 keys took: %.3f seconds", elapsed);
    
    // Reading performance
    start = [NSDate date];
    for (int i = 0; i < 1000; i++) {
        NSString *key = [NSString stringWithFormat:@"perfKey%d", i];
        NSInteger value = [defaults integerForKey:key];
        (void)value; // Suppress unused warning
    }
    
    elapsed = -[start timeIntervalSinceNow];
    NSLog(@"Reading 1000 keys took: %.3f seconds", elapsed);
    
    NSLog(@"Testing performance with many keys"); // Breakpoint 9: po defaults
    // Expected: Formatter should handle large number of keys efficiently
    
    // Clean up performance test keys
    for (int i = 0; i < 1000; i++) {
        NSString *key = [NSString stringWithFormat:@"perfKey%d", i];
        [defaults removeObjectForKey:key];
    }
    [defaults synchronize];
}

void test_custom_domains() {
    // Test custom domain functionality
    NSUserDefaults *defaults = [[NSUserDefaults alloc] init];
    NSString *customDomain = @"com.test.customdomain";
    
    NSDictionary *domainDict = @{
        @"customSetting1": @"value1",
        @"customSetting2": @42,
        @"customArray": @[@"item1", @"item2", @"item3"]
    };
    
    [defaults setPersistentDomain:domainDict forName:customDomain];
    
    // Access via custom domain
    NSDictionary *retrieved = [defaults persistentDomainForName:customDomain];
    NSLog(@"Custom domain contents: %@", retrieved);
    
    NSLog(@"Testing custom domains"); // Breakpoint 10: po defaults
    // Expected: Show custom domain with its settings
}

int main(int argc, char *argv[]) {
    @autoreleasepool {
        NSLog(@"=== NSUserDefaults Formatter Test Suite ===");
        
        test_nil_defaults();
        test_empty_defaults();
        test_simple_defaults();
        test_complex_defaults();
        test_edge_cases();
        test_volatile_domains();
        test_suite_registration();
        test_removal_operations();
        test_performance();
        test_custom_domains();
        
        NSLog(@"=== All NSUserDefaults tests complete ===");
        
        // Final test: standard user defaults with everything
        NSUserDefaults *finalDefaults = [NSUserDefaults standardUserDefaults];
        NSLog(@"Final defaults state"); // Breakpoint 11: po finalDefaults
        
        return 0;
    }
}