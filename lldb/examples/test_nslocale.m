#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Test NSLocale variations (internationalization debugging)
    NSLocale *currentLocale = [NSLocale currentLocale];
    NSLocale *systemLocale = [NSLocale systemLocale];
    NSLocale *usLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
    NSLocale *frenchLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"fr_FR"];
    NSLocale *japaneseLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"ja_JP"];
    NSLocale *germanLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"de_DE"];
    NSLocale *invalidLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"invalid_locale"];
    NSLocale *nilLocale = nil; // Edge case: nil locale
    
    // Set breakpoint here to test NSLocale formatter
    NSLog(@"NSLocale test program");
    
    // Stop here for debugging - set a breakpoint on the sleep call
    sleep(60); // Sleep for 60 seconds
    NSLog(@"Current locale: %@", currentLocale);
    NSLog(@"System locale: %@", systemLocale); 
    NSLog(@"US locale: %@", usLocale);
    NSLog(@"French locale: %@", frenchLocale);
    NSLog(@"Japanese locale: %@", japaneseLocale);
    NSLog(@"German locale: %@", germanLocale);
    NSLog(@"Invalid locale: %@", invalidLocale);
    NSLog(@"Nil locale: %@", nilLocale);
    
    return 0;
  }
}