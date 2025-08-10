#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Test various NSError types to understand memory layout
        NSError *fileError = [NSError errorWithDomain:NSCocoaErrorDomain 
                                                  code:NSFileNoSuchFileError 
                                              userInfo:@{NSLocalizedDescriptionKey: @"File not found"}];
        
        NSError *customError = [NSError errorWithDomain:@"com.example.MyDomain" 
                                                    code:42 
                                                userInfo:nil];
        
        printf("=== NSError Memory Layout Debug ===\n");
        printf("fileError: %p\n", (void*)fileError);
        printf("customError: %p\n", (void*)customError);
        
        printf("fileError domain: %s\n", [[fileError domain] UTF8String]);
        printf("fileError code: %ld\n", (long)[fileError code]);
        printf("customError domain: %s\n", [[customError domain] UTF8String]);
        printf("customError code: %ld\n", (long)[customError code]);
        
        // Set breakpoint here for LLDB analysis
        printf("Set breakpoint here and examine errors\n");
        
        return 0;
    }
}