#import <Foundation/Foundation.h>
#include <stdio.h>

// Custom class to test stepping through method calls
@interface StepTestClass : NSObject
@property (nonatomic, strong) NSString *name;
@property (nonatomic, assign) int value;

- (instancetype)initWithName:(NSString *)name value:(int)value;
- (void)simpleMethod;
- (int)methodWithReturn;
- (void)methodWithParameter:(NSString *)param;
- (void)nestedMethodCall;
- (void)deepMethodCall:(int)depth;
@end

@implementation StepTestClass

- (instancetype)initWithName:(NSString *)name value:(int)value {
    self = [super init];
    if (self) {
        _name = [name copy];
        _value = value;
    }
    return self;
}

- (void)simpleMethod {
    printf("StepTestClass: simpleMethod called\n");
    self.value = self.value + 1;
}

- (int)methodWithReturn {
    printf("StepTestClass: methodWithReturn called, returning %d\n", self.value);
    return self.value * 2;
}

- (void)methodWithParameter:(NSString *)param {
    printf("StepTestClass: methodWithParameter called with: %s\n", [param UTF8String]);
    self.name = [NSString stringWithFormat:@"%@_%@", self.name, param];
}

- (void)nestedMethodCall {
    printf("StepTestClass: nestedMethodCall - calling simpleMethod\n");
    [self simpleMethod];
    printf("StepTestClass: nestedMethodCall - calling methodWithReturn\n");
    int result = [self methodWithReturn];
    printf("StepTestClass: nestedMethodCall - got result: %d\n", result);
}

- (void)deepMethodCall:(int)depth {
    printf("StepTestClass: deepMethodCall depth=%d\n", depth);
    if (depth > 0) {
        [self deepMethodCall:(depth - 1)];
    }
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== Step-Through Trampoline Validation Test ===\n");
        
        // Create test object
        StepTestClass *testObj = [[StepTestClass alloc] initWithName:@"TestObject" value:42];
        printf("Created test object: %s with value %d\n", [testObj.name UTF8String], testObj.value);
        
        // Test Case 1: Simple method call
        printf("\n--- Test Case 1: Simple Method Call ---\n");
        [testObj simpleMethod];  // BREAKPOINT 1: Set breakpoint here, step into should land in method, not assembly
        
        // Test Case 2: Method with return value
        printf("\n--- Test Case 2: Method With Return ---\n");
        int result = [testObj methodWithReturn];  // BREAKPOINT 2: Step into should skip objc_msgSend
        printf("Main: Got result from methodWithReturn: %d\n", result);
        
        // Test Case 3: Method with parameter
        printf("\n--- Test Case 3: Method With Parameter ---\n");
        [testObj methodWithParameter:@"param"];  // BREAKPOINT 3: Test parameter passing through stepping
        
        // Test Case 4: Nested method calls
        printf("\n--- Test Case 4: Nested Method Calls ---\n");
        [testObj nestedMethodCall];  // BREAKPOINT 4: Test stepping through nested calls
        
        // Test Case 5: Deep recursion
        printf("\n--- Test Case 5: Deep Method Calls ---\n");
        [testObj deepMethodCall:3];  // BREAKPOINT 5: Test stepping through recursive calls
        
        // Test Case 6: Foundation method calls
        printf("\n--- Test Case 6: Foundation Method Calls ---\n");
        NSString *str = @"Hello";
        NSString *uppercased = [str uppercaseString];  // BREAKPOINT 6: Foundation method stepping
        printf("Uppercased: %s\n", [uppercased UTF8String]);
        
        // Test Case 7: Property access (getter/setter)
        printf("\n--- Test Case 7: Property Access ---\n");
        testObj.value = 100;  // BREAKPOINT 7: Setter call stepping
        int propValue = testObj.value;  // BREAKPOINT 8: Getter call stepping
        printf("Property value: %d\n", propValue);
        
        printf("\n=== Test Complete ===\n");
        printf("Final object state: name=%s, value=%d\n", [testObj.name UTF8String], testObj.value);
    }
    
    return 0;
}