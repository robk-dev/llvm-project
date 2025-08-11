#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Test 1: Empty/new user defaults
        NSUserDefaults *emptyDefaults = [[NSUserDefaults alloc] init];
        NSLog(@"Empty defaults created"); // BREAKPOINT 1
        
        // Test 2: Standard user defaults with some values
        NSUserDefaults *standardDefaults = [NSUserDefaults standardUserDefaults];
        [standardDefaults setObject:@"TestApp" forKey:@"ApplicationName"];
        [standardDefaults setInteger:2025 forKey:@"Year"];
        [standardDefaults setBool:YES forKey:@"DebugMode"];
        [standardDefaults setObject:@[@"Feature1", @"Feature2"] forKey:@"EnabledFeatures"];
        [standardDefaults setObject:@{@"version": @"1.0", @"build": @"42"} forKey:@"AppInfo"];
        [standardDefaults synchronize];
        NSLog(@"Standard defaults configured"); // BREAKPOINT 2
        
        // Test 3: Multiple domains
        [standardDefaults setPersistentDomain:@{@"key1": @"value1"} forName:@"com.test.domain1"];
        [standardDefaults setPersistentDomain:@{@"key2": @"value2"} forName:@"com.test.domain2"];
        NSLog(@"Multiple domains added"); // BREAKPOINT 3
        
        // Test 4: Volatile domain
        [standardDefaults setVolatileDomain:@{@"temp": @"data"} forName:@"VolatileDomain"];
        NSLog(@"Volatile domain added"); // BREAKPOINT 4
        
        // Test 5: Remove all persistent domains (cleanup)
        [standardDefaults removePersistentDomainForName:@"com.test.domain1"];
        [standardDefaults removePersistentDomainForName:@"com.test.domain2"];
        NSLog(@"Domains removed"); // BREAKPOINT 5
        
        NSLog(@"All tests complete");
    }
    
    return 0;
}