// Test program for dynamic type resolution
#import <Foundation/Foundation.h>

@interface TestClass : NSObject
@property (nonatomic, strong) NSString *name;
@property (nonatomic, assign) int value;
- (instancetype)initWithName:(NSString *)name value:(int)value;
@end

@implementation TestClass
- (instancetype)initWithName:(NSString *)name value:(int)value {
    self = [super init];
    if (self) {
        _name = name;
        _value = value;
    }
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"TestClass(name=%@, value=%d)", self.name, self.value];
}
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        // Test dynamic type resolution
        NSString *str = @"Hello World";
        NSNumber *num = @42;
        NSArray *arr = @[@"one", @"two", @"three"];
        NSDictionary *dict = @{@"key": @"value"};
        
        // Create custom object
        TestClass *custom = [[TestClass alloc] initWithName:@"Test" value:123];
        
        // Cast to id to test dynamic typing
        id generic_str = str;
        id generic_num = num;
        id generic_arr = arr;
        id generic_dict = dict;
        id generic_custom = custom;
        
        NSLog(@"String: %@", str);
        NSLog(@"Number: %@", num);
        NSLog(@"Array: %@", arr);
        NSLog(@"Dictionary: %@", dict);
        NSLog(@"Custom: %@", custom);
        
        NSLog(@"Test complete - set breakpoint after this line");
        
        return 0;
    }
}