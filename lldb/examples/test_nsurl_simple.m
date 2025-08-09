#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Create test URLs
        NSURL *webURL = [NSURL URLWithString:@"https://www.example.com/path?query=value"];
        NSURL *fileURL = [NSURL fileURLWithPath:@"/usr/local/bin/test"];
        NSURL *complexURL = [NSURL URLWithString:@"ftp://user:pass@host.com:8080/path/to/file.txt"];
        
        // Print them to verify they're created correctly
        NSLog(@"webURL: %@", webURL);
        NSLog(@"fileURL: %@", fileURL);
        NSLog(@"complexURL: %@", complexURL);
        
        // Breakpoint here to test formatters
        NSLog(@"Test NSURL formatters here"); // LINE 16 - SET BREAKPOINT HERE
        
        return 0;
    }
}