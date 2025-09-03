#import <Foundation/Foundation.h>

// Custom class with class methods for testing runtime introspection
@interface TestClass : NSObject
+ (instancetype)createDefault;
+ (instancetype)createWithValue:(NSInteger)value;
+ (NSString *)classDescription;
+ (NSArray *)availableOptions;
- (instancetype)initWithValue:(NSInteger)value;
- (NSString *)description;
@property NSInteger value;
@end

@implementation TestClass
@synthesize value;

+ (instancetype)createDefault {
    return [[self alloc] initWithValue:42];
}

+ (instancetype)createWithValue:(NSInteger)value {
    return [[self alloc] initWithValue:value];
}

+ (NSString *)classDescription {
    return @"TestClass for runtime introspection testing";
}

+ (NSArray *)availableOptions {
    return @[@"option1", @"option2", @"option3"];
}

- (instancetype)initWithValue:(NSInteger)val {
    self = [super init];
    if (self) {
        value = val;
    }
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"TestClass(value=%ld)", (long)value];
}

@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSLog(@"Testing custom class method introspection...");
        
        // Test class methods
        TestClass *defaultObj = [TestClass createDefault];
        TestClass *customObj = [TestClass createWithValue:100];
        NSString *classDesc = [TestClass classDescription];
        NSArray *options = [TestClass availableOptions];
        
        NSLog(@"Created objects: %@, %@", defaultObj, customObj);
        NSLog(@"Class description: %@", classDesc);
        NSLog(@"Available options: %@", options);
        
        // Break here for LLDB testing
        NSLog(@"Ready for LLDB class method testing");
        
        return 0;
    }
}
