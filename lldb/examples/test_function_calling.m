#import <Foundation/Foundation.h>
#import <objc/runtime.h>

// Custom class for testing
@interface TestClass : NSObject
@property (nonatomic, strong) NSString *name;
@property (nonatomic, assign) int value;
@end

@implementation TestClass
- (NSString *)description {
    return [NSString stringWithFormat:@"TestClass(name=%@, value=%d)", self.name, self.value];
}
@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test with basic Foundation objects
        NSString *str = @"Hello, GNUstep!";
        NSNumber *num = @42;
        NSArray *array = @[@"One", @"Two", @"Three"];
        NSDictionary *dict = @{@"key1": @"value1", @"key2": @"value2"};
        
        // Test with custom class
        TestClass *custom = [[TestClass alloc] init];
        custom.name = @"Test Object";
        custom.value = 123;
        
        // Print class names for verification
        printf("String class: %s\n", class_getName([str class]));
        printf("Number class: %s\n", class_getName([num class]));
        printf("Array class: %s\n", class_getName([array class]));
        printf("Dictionary class: %s\n", class_getName([dict class]));
        printf("Custom class: %s\n", class_getName([custom class]));
        
        // Breakpoint location - test function calling from here
        printf("Set breakpoint here to test runtime function calling\n");
        
        // Keep objects alive for debugging
        printf("str = %s\n", [str UTF8String]);
        printf("num = %ld\n", (long)[num integerValue]);
        printf("array count = %lu\n", (unsigned long)[array count]);
        printf("dict count = %lu\n", (unsigned long)[dict count]);
        printf("custom = %s\n", [[custom description] UTF8String]);
    }
    
    return 0;
}