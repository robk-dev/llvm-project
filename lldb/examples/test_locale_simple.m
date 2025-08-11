#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    NSLocale *locale = [NSLocale currentLocale];
    NSLocale *usLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
    
    // Breakpoint here
    printf("Testing NSLocale formatter\n");
    printf("Current locale: %p\n", locale);
    printf("US locale: %p\n", usLocale);
    
    return 0;
  }
}