#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Test various NSURL types to understand memory layout
        NSURL *httpURL = [NSURL URLWithString:@"https://example.com/path"];
        NSURL *fileURL = [NSURL fileURLWithPath:@"/tmp/test.txt"];
        NSURL *invalidURL = [NSURL URLWithString:@"not://a/valid/url"];
        
        printf("=== NSURL Memory Layout Debug ===\n");
        printf("httpURL: %p\n", (void*)httpURL);
        printf("fileURL: %p\n", (void*)fileURL);
        printf("invalidURL: %p\n", (void*)invalidURL);
        
        // Set breakpoint here for LLDB analysis
        printf("Set breakpoint here and examine URLs\n");
        
        return 0;
    }
}