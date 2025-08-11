#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test 1: System locale
        NSLocale *systemLocale = [NSLocale systemLocale];
        NSLog(@"System locale: %@", systemLocale);
        
        // Test 2: Current locale
        NSLocale *currentLocale = [NSLocale currentLocale];
        NSLog(@"Current locale: %@", currentLocale);
        
        // Test 3: Specific locales
        NSLocale *usLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
        NSLog(@"US locale: %@", usLocale);
        
        NSLocale *frLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"fr_FR"];
        NSLog(@"French locale: %@", frLocale);
        
        NSLocale *jpLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"ja_JP"];
        NSLog(@"Japanese locale: %@", jpLocale);
        
        NSLocale *cnLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"zh_CN"];
        NSLog(@"Chinese locale: %@", cnLocale);
        
        // Test 4: Locale with script
        NSLocale *cnTradLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"zh_Hant_TW"];
        NSLog(@"Traditional Chinese locale: %@", cnTradLocale);
        
        // Test 5: Auto-updating current locale (if available)
        NSLocale *autoLocale = [NSLocale autoupdatingCurrentLocale];
        NSLog(@"Auto-updating locale: %@", autoLocale);
        
        // Print class names for debugging
        NSLog(@"systemLocale class: %@", NSStringFromClass([systemLocale class]));
        NSLog(@"currentLocale class: %@", NSStringFromClass([currentLocale class]));
        NSLog(@"usLocale class: %@", NSStringFromClass([usLocale class]));
        
        // Add a sleep to allow setting breakpoint
        sleep(1);
        
        // Breakpoint here for debugging
        NSLog(@"All locales created. Set breakpoint here to test formatters.");
        
        // Test 6: Get locale properties
        NSString *identifier = [usLocale localeIdentifier];
        NSString *language = [usLocale objectForKey:NSLocaleLanguageCode];
        NSString *country = [usLocale objectForKey:NSLocaleCountryCode];
        NSString *currency = [usLocale objectForKey:NSLocaleCurrencyCode];
        
        NSLog(@"US Locale properties:");
        NSLog(@"  Identifier: %@", identifier);
        NSLog(@"  Language: %@", language);
        NSLog(@"  Country: %@", country);
        NSLog(@"  Currency: %@", currency);
        
        return 0;
    }
}