#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        NSArray *fruits = @[@"Apple", @"Banana", @"Cherry"];
        
        printf("Array created with %lu elements\n", (unsigned long)[fruits count]);
        printf("Set breakpoint here to inspect 'fruits'\n"); // Line 8
        
        return 0;
    }
}