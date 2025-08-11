#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Create a string that will be GSCInlineString
        NSMutableArray *array = [[NSMutableArray alloc] init];
        [array addObject:@"Test"];
        [array addObject:@"String"];
        
        NSLog(@"Array created: %@", array);
        
        // Set breakpoint here
        NSLog(@"Done");
    }
    return 0;
}