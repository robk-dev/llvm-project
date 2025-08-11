#import <Foundation/Foundation.h>
#include <stdio.h>

// Custom class for testing object validation
@interface ValidatorTestClass : NSObject
@property (nonatomic, strong) NSString *name;
@property (nonatomic, assign) BOOL isValid;

- (instancetype)initWithName:(NSString *)name;
- (BOOL)respondsToTestSelector;
- (void)performTestAction;
+ (instancetype)createValidObject;
+ (instancetype)createCorruptedObject;
@end

@implementation ValidatorTestClass

- (instancetype)initWithName:(NSString *)name {
    self = [super init];
    if (self) {
        _name = [name copy];
        _isValid = YES;
    }
    return self;
}

- (BOOL)respondsToTestSelector {
    return [self respondsToSelector:@selector(performTestAction)];
}

- (void)performTestAction {
    printf("ValidatorTestClass: performTestAction called on %s\n", [self.name UTF8String]);
}

+ (instancetype)createValidObject {
    return [[ValidatorTestClass alloc] initWithName:@"ValidObject"];
}

+ (instancetype)createCorruptedObject {
    // Create object but don't fully initialize (simulates corruption)
    ValidatorTestClass *obj = [ValidatorTestClass alloc];
    // Don't call init - this should create an object that might fail validation
    return obj;
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== Object Checker Validation Test ===\n");
        
        // Test Case 1: Valid object
        printf("\n--- Test Case 1: Valid Object ---\n");
        ValidatorTestClass *validObj = [ValidatorTestClass createValidObject];
        printf("Created valid object: %p\n", validObj);
        
        // Test Case 2: Nil object  
        printf("\n--- Test Case 2: Nil Object ---\n");
        ValidatorTestClass *nilObj = nil;
        printf("Nil object: %p\n", nilObj);
        
        // Test Case 3: Foundation objects
        printf("\n--- Test Case 3: Foundation Objects ---\n");
        NSString *string = @"TestString";
        NSArray *array = @[@"item1", @"item2"];
        NSDictionary *dict = @{@"key": @"value"};
        NSNumber *number = @42;
        
        printf("String: %p\n", string);
        printf("Array: %p\n", array);
        printf("Dictionary: %p\n", dict);
        printf("Number: %p\n", number);
        
        // Test Case 4: Method calls that should trigger object checking
        printf("\n--- Test Case 4: Method Calls for Object Validation ---\n");
        
        // These method calls should trigger CreateObjectChecker when used in conditional breakpoints
        [validObj performTestAction];  // CONDITIONAL BREAKPOINT 1: validObj != nil
        
        if (validObj) {
            BOOL responds = [validObj respondsToTestSelector];  // CONDITIONAL BREAKPOINT 2: [validObj respondsToSelector:@selector(performTestAction)]
            printf("Valid object responds to selector: %s\n", responds ? "YES" : "NO");
        }
        
        // Test with Foundation objects
        if (string) {
            NSUInteger length = [string length];  // CONDITIONAL BREAKPOINT 3: [(id)string respondsToSelector:@selector(length)]
            printf("String length: %lu\n", length);
        }
        
        if (array) {
            NSUInteger count = [array count];  // CONDITIONAL BREAKPOINT 4: [array isKindOfClass:[NSArray class]]
            printf("Array count: %lu\n", count);
        }
        
        // Test Case 5: Object validation edge cases
        printf("\n--- Test Case 5: Edge Cases ---\n");
        
        // Cast to id for generic object testing
        id genericObj = validObj;
        if ([genericObj respondsToSelector:@selector(performTestAction)]) {  // CONDITIONAL BREAKPOINT 5: [genericObj respondsToSelector:@selector(performTestAction)]
            [genericObj performTestAction];
        }
        
        // Test with potentially corrupted object
        ValidatorTestClass *corruptedObj = [ValidatorTestClass createCorruptedObject];
        printf("Corrupted object: %p\n", corruptedObj);
        
        // This should be handled gracefully by object checker
        if (corruptedObj != nil) {  // CONDITIONAL BREAKPOINT 6: corruptedObj != nil && [corruptedObj respondsToSelector:@selector(performTestAction)]
            printf("Corrupted object is not nil, but may not respond to selectors properly\n");
        }
        
        printf("\n=== Object Checker Test Complete ===\n");
        printf("Conditional breakpoints should have triggered CreateObjectChecker\n");
        printf("All object validations should have been handled safely\n");
    }
    
    return 0;
}