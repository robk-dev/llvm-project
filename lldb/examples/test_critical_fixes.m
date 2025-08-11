#import <Foundation/Foundation.h>

@interface TestClass : NSObject
@property NSString *name;
@end

@implementation TestClass
@end

int main() {
    @autoreleasepool {
        // Test 1: NSConstantString
        NSString *constant = @"Hello World";
        NSLog(@"NSConstantString: %@", constant);
        
        // Test 2: Tagged pointers in arrays
        NSArray *numberArray = @[@1, @2, @3, @4, @5];
        NSLog(@"Number array: %@", numberArray);
        
        // Test 3: Mixed array with floats
        NSArray *mixedArray = @[@"string", @42, @3.14, @YES];
        NSLog(@"Mixed array: %@", mixedArray);
        
        // Test 4: Dictionary
        NSDictionary *dict = @{@"key1": @"value1", @"key2": @42};
        NSLog(@"Dictionary: %@", dict);
        
        // Test 5: Custom class
        TestClass *obj = [TestClass new];
        obj.name = @"Test Object";
        NSLog(@"Custom object: %@", obj);
        
        NSLog(@"All tests completed - set breakpoint here");
        return 0;
    }
}