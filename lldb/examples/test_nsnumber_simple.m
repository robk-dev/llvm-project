#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== Testing NSNumber Simple ===\n");
        
        // Create individual NSNumber objects  
        NSNumber *num1 = @1;
        NSNumber *num2 = @2;
        NSNumber *num3 = @3;
        
        printf("num1 created: %p\n", num1);
        printf("num2 created: %p\n", num2);
        printf("num3 created: %p\n", num3);
        
        printf("Ready for individual NSNumber inspection\n"); // SET BREAKPOINT HERE
        
        return 0;
    }
}