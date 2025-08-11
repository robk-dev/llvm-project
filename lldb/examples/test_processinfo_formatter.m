#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        NSProcessInfo *processInfo = [NSProcessInfo processInfo];
        NSLocale *locale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
        
        NSLog(@"ProcessInfo class: %@", NSStringFromClass([processInfo class]));
        NSLog(@"Locale class: %@", NSStringFromClass([locale class]));
        
        NSLog(@"Set breakpoint here");
        
        return 0;
    }
}