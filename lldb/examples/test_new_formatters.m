#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== Testing New Formatters ===\n");
        
        // Test NSBundle formatter
        NSBundle *mainBundle = [NSBundle mainBundle];
        printf("Main bundle created: %p\n", mainBundle);
        
        // Test NSProcessInfo formatter  
        NSProcessInfo *processInfo = [NSProcessInfo processInfo];
        printf("Process info created: %p\n", processInfo);
        
        // Test NSUserDefaults formatter
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        printf("User defaults created: %p\n", defaults);
        
        // Test NSCalendar formatter
        NSCalendar *calendar = [NSCalendar currentCalendar];
        printf("Calendar created: %p\n", calendar);
        
        // Test NSLocale formatter
        NSLocale *locale = [NSLocale currentLocale];
        printf("Locale created: %p\n", locale);
        
        // Test NSScanner formatter
        NSScanner *scanner = [NSScanner scannerWithString:@"Hello World 123"];
        printf("Scanner created: %p\n", scanner);
        
        printf("Ready for LLDB inspection\n"); // SET BREAKPOINT HERE
        
        return 0;
    }
}