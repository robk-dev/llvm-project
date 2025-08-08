#import <Foundation/Foundation.h>
#include <stdio.h>
#include <objc/runtime.h>

// Simple test class with various methods
@interface TestMethodClass : NSObject

// Instance methods
- (void)simpleMethod;
- (NSString *)methodWithReturn;
- (void)methodWithArg:(NSString *)arg;
- (NSString *)methodWithArg:(NSString *)arg1 andArg:(NSNumber *)arg2;

// Class methods
+ (void)classMethod;
+ (NSString *)classMethodWithReturn;

@end

@implementation TestMethodClass

- (void)simpleMethod {
    NSLog(@"Simple method called");
}

- (NSString *)methodWithReturn {
    return @"Return value";
}

- (void)methodWithArg:(NSString *)arg {
    NSLog(@"Method with arg: %@", arg);
}

- (NSString *)methodWithArg:(NSString *)arg1 andArg:(NSNumber *)arg2 {
    return [NSString stringWithFormat:@"%@ - %@", arg1, arg2];
}

+ (void)classMethod {
    NSLog(@"Class method called");
}

+ (NSString *)classMethodWithReturn {
    return @"Class method return";
}

@end

// Test subclass to verify inheritance
@interface SubTestMethodClass : TestMethodClass

- (void)subclassMethod;
- (void)overriddenMethod;

@end

@implementation SubTestMethodClass

- (void)subclassMethod {
    NSLog(@"Subclass method");
}

- (void)overriddenMethod {
    NSLog(@"Overridden in subclass");
}

// Override parent method
- (void)simpleMethod {
    NSLog(@"Overridden simple method");
    [super simpleMethod];
}

@end

void print_methods(Class cls) {
    unsigned int count;
    Method *methods = class_copyMethodList(cls, &count);
    
    printf("Methods for class %s:\n", class_getName(cls));
    for (unsigned int i = 0; i < count; i++) {
        Method method = methods[i];
        SEL selector = method_getName(method);
        const char *name = sel_getName(selector);
        const char *type = method_getTypeEncoding(method);
        printf("  - %s (type: %s)\n", name, type);
    }
    
    free(methods);
    printf("\n");
}

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Create instances
        TestMethodClass *testObj = [[TestMethodClass alloc] init];
        SubTestMethodClass *subObj = [[SubTestMethodClass alloc] init];
        
        // Call some methods to make sure they're loaded
        [testObj simpleMethod];
        [subObj subclassMethod];
        [TestMethodClass classMethod];
        
        // Print runtime method info
        printf("=== Runtime Method Information ===\n\n");
        
        // Print instance methods
        print_methods([TestMethodClass class]);
        print_methods([SubTestMethodClass class]);
        
        // Print class methods (from metaclass)
        print_methods(object_getClass([TestMethodClass class]));
        print_methods(object_getClass([SubTestMethodClass class]));
        
        // Set breakpoint here for LLDB testing
        printf("Set breakpoint here to test LLDB introspection\n");  // Line 110
        
        // Keep objects alive for debugging
        NSString *result1 = [testObj methodWithReturn];
        NSString *result2 = [subObj methodWithArg:@"test" andArg:@42];
        
        printf("Test complete. Results: %s, %s\n", 
               [result1 UTF8String], [result2 UTF8String]);
    }
    
    return 0;
}