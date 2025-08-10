#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    
    // Test NSProcessInfo formatter (should now work)
    NSProcessInfo *processInfo = [NSProcessInfo processInfo];
    
    // Test NSUserDefaults formatter (should now work)
    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
    
    // Test NSCalendar formatter (should now work)
    NSCalendar *calendar = [NSCalendar currentCalendar];
    
    // Test NSLocale formatter (should now work)
    NSLocale *locale = [NSLocale currentLocale];
    
    // Test NSBundle formatter (was already working)
    NSBundle *bundle = [NSBundle mainBundle];
    
    // Test NSScanner formatter (was already working)
    NSScanner *scanner = [NSScanner scannerWithString:@"123 hello world"];
    
    // Test NSError formatter (was already working) 
    NSError *error = [NSError errorWithDomain:@"TestDomain" code:404 userInfo:@{NSLocalizedDescriptionKey: @"Not found"}];
    
    // Test NSDecimalNumber formatter (was already working)
    NSDecimalNumber *decimal = [NSDecimalNumber decimalNumberWithString:@"123.45"];
    
    // Test NSCharacterSet formatter (was already working)
    NSCharacterSet *charset = [NSCharacterSet decimalDigitCharacterSet];
    
    NSLog(@"Set breakpoint here for testing formatters");
    NSLog(@"ProcessInfo: %@", processInfo);
    NSLog(@"UserDefaults: %@", userDefaults);
    NSLog(@"Calendar: %@", calendar);
    NSLog(@"Locale: %@", locale);
    NSLog(@"Bundle: %@", bundle);
    NSLog(@"Scanner: %@", scanner);
    NSLog(@"Error: %@", error);
    NSLog(@"Decimal: %@", decimal);
    NSLog(@"CharacterSet: %@", charset);
    
    return 0;
  }
}