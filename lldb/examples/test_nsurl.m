#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test various NSURL formats
        NSURL *httpUrl = [NSURL URLWithString:@"https://example.com"];
        NSURL *fileUrl = [NSURL fileURLWithPath:@"/usr/local/bin/test"];
        NSURL *relativeUrl = [NSURL URLWithString:@"/path/to/resource" relativeToURL:httpUrl];
        NSURL *complexUrl = [NSURL URLWithString:@"https://user:pass@example.com:8080/path?query=value#fragment"];
        NSURL *nilUrl = nil;
        NSURL *emptyUrl = [NSURL URLWithString:@""];
        
        NSLog(@"httpUrl: %@", httpUrl);
        NSLog(@"fileUrl: %@", fileUrl);
        NSLog(@"relativeUrl: %@", relativeUrl);
        NSLog(@"complexUrl: %@", complexUrl);
        NSLog(@"nilUrl: %@", nilUrl);
        NSLog(@"emptyUrl: %@", emptyUrl);
        
        // Set breakpoint here to inspect URLs
        NSLog(@"All URLs created. Set breakpoint here.");  // Line 21
        
        return 0;
    }
}