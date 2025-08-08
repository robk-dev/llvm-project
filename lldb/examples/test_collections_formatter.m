//
// test_collections_formatter.m
// Comprehensive test program for NSDictionary and NSSet formatters
// 
// This program validates the GNUstep LLDB formatters for collection types.
// Each test case includes expected LLDB output for verification.
//
// Build: make test_collections_formatter
// Debug: lldb test_collections_formatter
//        (lldb) b main
//        (lldb) run
//        (lldb) po <variable_name>
//

#import <Foundation/Foundation.h>

// Custom test class for generic object formatting
@interface TestObject : NSObject {
    NSString *name;
    NSInteger value;
}
- (instancetype)initWithName:(NSString *)n value:(NSInteger)v;
- (NSString *)description;
@end

@implementation TestObject
- (instancetype)initWithName:(NSString *)n value:(NSInteger)v {
    self = [super init];
    if (self) {
        name = [n retain];
        value = v;
    }
    return self;
}

- (void)dealloc {
    [name release];
    [super dealloc];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"TestObject(%@, %ld)", name, (long)value];
}
@end

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    NSLog(@"=== NSDictionary Formatter Tests ===");
    
    // Test 1: Empty dictionary
    // Expected LLDB output: {}
    NSDictionary *emptyDict = [NSDictionary dictionary];
    NSLog(@"Empty dictionary created");  // Breakpoint 1
    
    // Test 2: Single element dictionary
    // Expected LLDB output: { key1 = value1 }
    NSDictionary *singleDict = [NSDictionary dictionaryWithObject:@"value1" 
                                                           forKey:@"key1"];
    NSLog(@"Single element dictionary created");  // Breakpoint 2
    
    // Test 3: Multiple elements with tagged strings
    // Expected LLDB output: { name = "John Doe"; age = 42; city = "New York" }
    NSDictionary *multiDict = [NSDictionary dictionaryWithObjectsAndKeys:
                                @"John Doe", @"name",
                                [NSNumber numberWithInt:42], @"age",
                                @"New York", @"city",
                                nil];
    NSLog(@"Multi-element dictionary created");  // Breakpoint 3
    
    // Test 4: Dictionary with mixed types
    // Expected LLDB output: { string = "Hello"; number = 123; array = (1, 2, 3); null = <null> }
    NSArray *innerArray = [NSArray arrayWithObjects:
                           [NSNumber numberWithInt:1],
                           [NSNumber numberWithInt:2],
                           [NSNumber numberWithInt:3],
                           nil];
    NSDictionary *mixedDict = [NSDictionary dictionaryWithObjectsAndKeys:
                                @"Hello", @"string",
                                [NSNumber numberWithInt:123], @"number",
                                innerArray, @"array",
                                [NSNull null], @"null",
                                nil];
    NSLog(@"Mixed types dictionary created");  // Breakpoint 4
    
    // Test 5: Nested dictionary
    // Expected LLDB output: { outer = { inner = "nested value" }; level = 1 }
    NSDictionary *innerDict = [NSDictionary dictionaryWithObject:@"nested value" 
                                                          forKey:@"inner"];
    NSDictionary *nestedDict = [NSDictionary dictionaryWithObjectsAndKeys:
                                 innerDict, @"outer",
                                 [NSNumber numberWithInt:1], @"level",
                                 nil];
    NSLog(@"Nested dictionary created");  // Breakpoint 5
    
    // Test 6: Dictionary with custom objects
    // Expected LLDB output: { obj1 = TestObject(First, 100); obj2 = TestObject(Second, 200) }
    TestObject *obj1 = [[[TestObject alloc] initWithName:@"First" value:100] autorelease];
    TestObject *obj2 = [[[TestObject alloc] initWithName:@"Second" value:200] autorelease];
    NSDictionary *customDict = [NSDictionary dictionaryWithObjectsAndKeys:
                                 obj1, @"obj1",
                                 obj2, @"obj2",
                                 nil];
    NSLog(@"Custom objects dictionary created");  // Breakpoint 6
    
    // Test 7: Large dictionary (for performance/limit testing)
    // Expected LLDB output: { key0 = value0; key1 = value1; ... key99 = value99 } (truncated if needed)
    NSMutableDictionary *largeDict = [NSMutableDictionary dictionary];
    for (int i = 0; i < 100; i++) {
        NSString *key = [NSString stringWithFormat:@"key%d", i];
        NSString *value = [NSString stringWithFormat:@"value%d", i];
        [largeDict setObject:value forKey:key];
    }
    NSLog(@"Large dictionary created with %lu elements", (unsigned long)[largeDict count]);  // Breakpoint 7
    
    // Test 8: Mutable dictionary
    // Expected LLDB output: { mutable = YES; count = 3 }
    NSMutableDictionary *mutableDict = [NSMutableDictionary dictionary];
    [mutableDict setObject:@"YES" forKey:@"mutable"];
    [mutableDict setObject:[NSNumber numberWithInt:3] forKey:@"count"];
    [mutableDict setObject:@"test" forKey:@"type"];
    // Remove one to test mutation
    [mutableDict removeObjectForKey:@"type"];
    NSLog(@"Mutable dictionary modified");  // Breakpoint 8
    
    NSLog(@"\n=== NSSet Formatter Tests ===");
    
    // Test 9: Empty set
    // Expected LLDB output: ()
    NSSet *emptySet = [NSSet set];
    NSLog(@"Empty set created");  // Breakpoint 9
    
    // Test 10: Single element set
    // Expected LLDB output: (element1)
    NSSet *singleSet = [NSSet setWithObject:@"element1"];
    NSLog(@"Single element set created");  // Breakpoint 10
    
    // Test 11: Multiple elements set
    // Expected LLDB output: ("Apple", "Banana", "Cherry") - order may vary
    NSSet *multiSet = [NSSet setWithObjects:@"Apple", @"Banana", @"Cherry", nil];
    NSLog(@"Multi-element set created");  // Breakpoint 11
    
    // Test 12: Set with mixed types
    // Expected LLDB output: ("String", 42, (1, 2)) - order may vary
    NSArray *setArray = [NSArray arrayWithObjects:
                         [NSNumber numberWithInt:1],
                         [NSNumber numberWithInt:2],
                         nil];
    NSSet *mixedSet = [NSSet setWithObjects:
                       @"String",
                       [NSNumber numberWithInt:42],
                       setArray,
                       nil];
    NSLog(@"Mixed types set created");  // Breakpoint 12
    
    // Test 13: Set containing other collections
    // Expected LLDB output: ({key = value}, (a, b, c)) - order may vary
    NSDictionary *setDict = [NSDictionary dictionaryWithObject:@"value" forKey:@"key"];
    NSArray *setArray2 = [NSArray arrayWithObjects:@"a", @"b", @"c", nil];
    NSSet *collectionSet = [NSSet setWithObjects:setDict, setArray2, nil];
    NSLog(@"Set with collections created");  // Breakpoint 13
    
    // Test 14: Set with custom objects
    // Expected LLDB output: (TestObject(SetObj1, 111), TestObject(SetObj2, 222)) - order may vary
    TestObject *setObj1 = [[[TestObject alloc] initWithName:@"SetObj1" value:111] autorelease];
    TestObject *setObj2 = [[[TestObject alloc] initWithName:@"SetObj2" value:222] autorelease];
    NSSet *customSet = [NSSet setWithObjects:setObj1, setObj2, nil];
    NSLog(@"Custom objects set created");  // Breakpoint 14
    
    // Test 15: Large set (for performance/limit testing)
    // Expected LLDB output: (item0, item1, item2, ... item149) - truncated if needed
    NSMutableSet *largeSet = [NSMutableSet set];
    for (int i = 0; i < 150; i++) {
        NSString *item = [NSString stringWithFormat:@"item%d", i];
        [largeSet addObject:item];
    }
    NSLog(@"Large set created with %lu elements", (unsigned long)[largeSet count]);  // Breakpoint 15
    
    // Test 16: Mutable set
    // Expected LLDB output: ("first", "second", "third") - order may vary
    NSMutableSet *mutableSet = [NSMutableSet set];
    [mutableSet addObject:@"first"];
    [mutableSet addObject:@"second"];
    [mutableSet addObject:@"third"];
    [mutableSet addObject:@"fourth"];
    // Remove one to test mutation
    [mutableSet removeObject:@"fourth"];
    NSLog(@"Mutable set modified");  // Breakpoint 16
    
    NSLog(@"\n=== Complex Nested Structure Test ===");
    
    // Test 17: Deeply nested structure
    // Expected LLDB output: Complex nested structure with multiple levels
    NSSet *level3Set = [NSSet setWithObjects:@"deep1", @"deep2", nil];
    NSDictionary *level3Dict = [NSDictionary dictionaryWithObject:level3Set forKey:@"deepSet"];
    
    NSArray *level2Array = [NSArray arrayWithObjects:
                            @"mid1",
                            level3Dict,
                            [NSNumber numberWithInt:999],
                            nil];
    
    NSDictionary *level2Dict = [NSDictionary dictionaryWithObjectsAndKeys:
                                level2Array, @"midArray",
                                @"midValue", @"midKey",
                                nil];
    
    NSSet *level1Set = [NSSet setWithObjects:
                        level2Dict,
                        @"topLevel",
                        [NSNumber numberWithFloat:3.14f],
                        nil];
    
    NSDictionary *complexStructure = [NSDictionary dictionaryWithObjectsAndKeys:
                                      level1Set, @"topSet",
                                      @"rootValue", @"rootKey",
                                      [NSNull null], @"nullValue",
                                      nil];
    NSLog(@"Complex nested structure created");  // Breakpoint 17
    
    NSLog(@"\n=== Edge Cases ===");
    
    // Test 18: Dictionary with nil value handling (should not crash)
    NSDictionary *nilDict = nil;
    NSLog(@"Nil dictionary reference");  // Breakpoint 18
    
    // Test 19: Set with nil value handling (should not crash)
    NSSet *nilSet = nil;
    NSLog(@"Nil set reference");  // Breakpoint 19
    
    // Test 20: Dictionary with very long string keys/values
    NSMutableString *longString = [NSMutableString string];
    for (int i = 0; i < 500; i++) {
        [longString appendString:@"x"];
    }
    NSDictionary *longStringDict = [NSDictionary dictionaryWithObject:longString 
                                                               forKey:@"veryLongKey"];
    NSLog(@"Dictionary with long strings created");  // Breakpoint 20
    
    NSLog(@"\n=== All tests completed ===");
    NSLog(@"Set breakpoints at each NSLog statement to inspect variables");
    NSLog(@"Use 'po <variable>' to test formatter output");
    
    // Keep all objects alive for debugging
    NSLog(@"Keeping objects alive for inspection...");  // Final Breakpoint
    
    // Summary of all test variables for final inspection
    NSLog(@"Test variables summary:");
    NSLog(@"  Dictionaries: emptyDict, singleDict, multiDict, mixedDict, nestedDict");
    NSLog(@"                customDict, largeDict, mutableDict, longStringDict");
    NSLog(@"  Sets: emptySet, singleSet, multiSet, mixedSet, collectionSet");
    NSLog(@"        customSet, largeSet, mutableSet");
    NSLog(@"  Complex: complexStructure");
    NSLog(@"  Edge cases: nilDict, nilSet");
    
    [pool drain];
    return 0;
}