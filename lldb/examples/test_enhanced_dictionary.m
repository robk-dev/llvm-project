//===-- test_enhanced_dictionary.m - Test enhanced dictionary formatter ----===//
//
// Test program for GNUstep enhanced dictionary formatter
// Tests various dictionary types and edge cases
//
//===----------------------------------------------------------------------===//

#import <Foundation/Foundation.h>
#import <objc/runtime.h>

// Custom class for testing as dictionary keys/values
@interface TestObject : NSObject
@property (nonatomic, strong) NSString *name;
@property (nonatomic, assign) NSInteger value;
- (instancetype)initWithName:(NSString *)name value:(NSInteger)value;
@end

@implementation TestObject
- (instancetype)initWithName:(NSString *)name value:(NSInteger)value {
    self = [super init];
    if (self) {
        _name = name;
        _value = value;
    }
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"TestObject(%@, %ld)", _name, (long)_value];
}

- (NSUInteger)hash {
    return [_name hash] ^ _value;
}

- (BOOL)isEqual:(id)object {
    if (![object isKindOfClass:[TestObject class]]) return NO;
    TestObject *other = (TestObject *)object;
    return [_name isEqualToString:other.name] && _value == other.value;
}

- (id)copyWithZone:(NSZone *)zone {
    return [[TestObject alloc] initWithName:_name value:_value];
}
@end

void test_empty_dictionary() {
    NSLog(@"=== Testing Empty Dictionary ===");
    NSDictionary *empty = @{};
    NSMutableDictionary *mutableEmpty = [NSMutableDictionary dictionary];
    
    NSLog(@"Empty dictionary: %@", empty);
    NSLog(@"Empty mutable dictionary: %@", mutableEmpty);
    
    // Breakpoint here to test formatter
    NSLog(@"Break here to test empty dictionaries"); // BREAK_EMPTY
}

void test_simple_dictionary() {
    NSLog(@"=== Testing Simple Dictionary ===");
    
    // String keys and values
    NSDictionary *stringDict = @{
        @"name": @"John Doe",
        @"city": @"New York",
        @"country": @"USA"
    };
    
    // Number keys and values
    NSDictionary *numberDict = @{
        @1: @100,
        @2: @200,
        @3: @300,
        @42: @4200
    };
    
    // Mixed types
    NSDictionary *mixedDict = @{
        @"string": @"Hello",
        @"number": @42,
        @"float": @3.14,
        @"bool": @YES
    };
    
    NSLog(@"String dictionary: %@", stringDict);
    NSLog(@"Number dictionary: %@", numberDict);
    NSLog(@"Mixed dictionary: %@", mixedDict);
    
    // Breakpoint here to test formatter
    NSLog(@"Break here to test simple dictionaries"); // BREAK_SIMPLE
}

void test_nested_dictionary() {
    NSLog(@"=== Testing Nested Dictionary ===");
    
    NSDictionary *nestedDict = @{
        @"user": @{
            @"name": @"Alice",
            @"age": @30,
            @"email": @"alice@example.com"
        },
        @"settings": @{
            @"theme": @"dark",
            @"notifications": @YES,
            @"language": @"en"
        },
        @"scores": @[@10, @20, @30, @40, @50]
    };
    
    NSLog(@"Nested dictionary: %@", nestedDict);
    
    // Breakpoint here to test formatter
    NSLog(@"Break here to test nested dictionary"); // BREAK_NESTED
}

void test_custom_objects_dictionary() {
    NSLog(@"=== Testing Custom Objects Dictionary ===");
    
    TestObject *obj1 = [[TestObject alloc] initWithName:@"Object1" value:100];
    TestObject *obj2 = [[TestObject alloc] initWithName:@"Object2" value:200];
    TestObject *obj3 = [[TestObject alloc] initWithName:@"Object3" value:300];
    
    NSDictionary *customDict = @{
        obj1: @"First",
        obj2: @"Second",
        obj3: @"Third"
    };
    
    NSDictionary *reverseCustomDict = @{
        @"First": obj1,
        @"Second": obj2,
        @"Third": obj3
    };
    
    NSLog(@"Custom object keys: %@", customDict);
    NSLog(@"Custom object values: %@", reverseCustomDict);
    
    // Breakpoint here to test formatter
    NSLog(@"Break here to test custom object dictionaries"); // BREAK_CUSTOM
}

void test_large_dictionary() {
    NSLog(@"=== Testing Large Dictionary ===");
    
    NSMutableDictionary *largeDict = [NSMutableDictionary dictionary];
    
    // Create a dictionary with 100 entries
    for (int i = 0; i < 100; i++) {
        NSString *key = [NSString stringWithFormat:@"key_%03d", i];
        NSNumber *value = @(i * 10);
        largeDict[key] = value;
    }
    
    NSLog(@"Large dictionary with %lu entries", (unsigned long)[largeDict count]);
    
    // Breakpoint here to test formatter
    NSLog(@"Break here to test large dictionary"); // BREAK_LARGE
}

void test_mutable_dictionary() {
    NSLog(@"=== Testing Mutable Dictionary ===");
    
    NSMutableDictionary *mutableDict = [NSMutableDictionary dictionary];
    
    // Add some initial values
    mutableDict[@"initial"] = @"value";
    mutableDict[@"count"] = @0;
    
    NSLog(@"Initial mutable dictionary: %@", mutableDict);
    
    // Modify dictionary
    mutableDict[@"added"] = @"new value";
    mutableDict[@"count"] = @1;
    [mutableDict removeObjectForKey:@"initial"];
    
    NSLog(@"Modified mutable dictionary: %@", mutableDict);
    
    // Breakpoint here to test formatter
    NSLog(@"Break here to test mutable dictionary"); // BREAK_MUTABLE
}

void test_edge_cases() {
    NSLog(@"=== Testing Edge Cases ===");
    
    // Dictionary with nil values (shouldn't compile with literals, so use mutable)
    NSMutableDictionary *dictWithNulls = [NSMutableDictionary dictionary];
    dictWithNulls[@"key1"] = @"value1";
    dictWithNulls[@"key2"] = [NSNull null];
    dictWithNulls[@"key3"] = @"value3";
    
    // Dictionary with duplicate keys (last wins)
    NSDictionary *dupKeys = @{
        @"key": @"value1",
        @"key": @"value2"  // This will overwrite
    };
    
    // Dictionary with special characters in keys
    NSDictionary *specialKeys = @{
        @"key with spaces": @"value1",
        @"key.with.dots": @"value2",
        @"key:with:colons": @"value3",
        @"key=with=equals": @"value4"
    };
    
    NSLog(@"Dictionary with nulls: %@", dictWithNulls);
    NSLog(@"Dictionary with dup keys: %@", dupKeys);
    NSLog(@"Dictionary with special keys: %@", specialKeys);
    
    // Breakpoint here to test formatter
    NSLog(@"Break here to test edge cases"); // BREAK_EDGE
}

void test_dictionary_performance() {
    NSLog(@"=== Testing Dictionary Performance ===");
    
    NSMutableDictionary *perfDict = [NSMutableDictionary dictionary];
    
    // Create a dictionary with various types
    for (int i = 0; i < 20; i++) {
        NSString *key = [NSString stringWithFormat:@"key_%d", i];
        
        if (i % 4 == 0) {
            perfDict[key] = @(i);  // Number
        } else if (i % 4 == 1) {
            perfDict[key] = [NSString stringWithFormat:@"string_%d", i];  // String
        } else if (i % 4 == 2) {
            perfDict[key] = @[@(i), @(i+1), @(i+2)];  // Array
        } else {
            perfDict[key] = @{@"nested": @(i)};  // Nested dictionary
        }
    }
    
    NSLog(@"Performance test dictionary ready");
    
    // Breakpoint here to test formatter performance
    NSLog(@"Break here to test performance"); // BREAK_PERF
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSLog(@"=== GNUstep Enhanced Dictionary Formatter Test ===");
        NSLog(@"Testing with GNUstep runtime on: %s", class_getName([NSDictionary class]));
        
        test_empty_dictionary();
        test_simple_dictionary();
        test_nested_dictionary();
        test_custom_objects_dictionary();
        test_large_dictionary();
        test_mutable_dictionary();
        test_edge_cases();
        test_dictionary_performance();
        
        NSLog(@"=== All Dictionary Tests Complete ===");
        
        // Final breakpoint to examine all variables
        NSLog(@"Break here for final examination"); // BREAK_FINAL
    }
    return 0;
}