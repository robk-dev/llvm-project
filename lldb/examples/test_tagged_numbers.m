#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Create small integers that should be tagged
        NSNumber *smallInt = @7;           // Should be tagged (tag 1)
        NSNumber *anotherInt = @42;        // Should be tagged (tag 1)  
        NSNumber *negativeInt = @(-5);     // Should be tagged (tag 1)
        
        // Test in collections
        NSDictionary *dict = @{
            @"small": smallInt,
            @"normal": anotherInt,
            @"negative": negativeInt
        };
        
        NSSet *set = [NSSet setWithObjects:smallInt, anotherInt, negativeInt, nil];
        NSArray *array = @[smallInt, anotherInt, negativeInt];
        
        // Check pointer values to see if they're tagged
        printf("smallInt ptr: %p (tag: %d)\n", (void*)smallInt, (int)((uintptr_t)smallInt & 7));
        printf("anotherInt ptr: %p (tag: %d)\n", (void*)anotherInt, (int)((uintptr_t)anotherInt & 7));
        printf("negativeInt ptr: %p (tag: %d)\n", (void*)negativeInt, (int)((uintptr_t)negativeInt & 7));
        
        // Set breakpoint here
        NSLog(@"Testing tagged numbers");
        
        return 0;
    }
}