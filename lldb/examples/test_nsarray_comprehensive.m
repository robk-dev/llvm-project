#import <Foundation/Foundation.h>
#import <stdio.h>

@interface TestObject : NSObject
@property (nonatomic, retain) NSString *name;
@property (nonatomic, assign) int value;
- (instancetype)initWithName:(NSString *)name value:(int)value;
@end

@implementation TestObject
- (instancetype)initWithName:(NSString *)name value:(int)value {
    self = [super init];
    if (self) {
        _name = [name retain];
        _value = value;
    }
    return self;
}

- (void)dealloc {
    [_name release];
    [super dealloc];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<TestObject: %@ = %d>", _name, _value];
}
@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        printf("=== Comprehensive NSArray Formatter Test ===\n");
        
        // ========================================
        // Test 1: Empty Arrays
        // ========================================
        
        NSArray *emptyArray = [NSArray array];
        NSArray *emptyAlloc = [[NSArray alloc] init];
        NSMutableArray *emptyMutable = [NSMutableArray array];
        
        // ========================================
        // Test 2: Single Element Arrays
        // ========================================
        
        NSArray *singleString = @[@"OnlyElement"];
        NSArray *singleNumber = @[@42];
        NSArray *singleNil = [NSArray arrayWithObject:[NSNull null]];
        
        // ========================================
        // Test 3: String Arrays
        // ========================================
        
        NSArray *shortStringArray = @[@"Apple", @"Banana", @"Cherry"];
        
        NSArray *longStringArray = @[
            @"First element",
            @"Second element with a longer string",
            @"Third",
            @"Fourth element that contains special chars: \n\t\"",
            @"Fifth element with Unicode: 你好 🌍",
            @"Sixth",
            @"Seventh",
            @"Eighth",
            @"Ninth",
            @"Tenth and final"
        ];
        
        // ========================================
        // Test 4: Number Arrays
        // ========================================
        
        NSArray *intArray = @[@1, @2, @3, @4, @5];
        
        NSArray *mixedNumberArray = @[
            @42,                          // int
            @3.14159,                     // double
            @YES,                         // BOOL
            @(-100),                      // negative int
            @0,                           // zero
            [NSNumber numberWithFloat:2.71828f],  // float
            [NSNumber numberWithLongLong:9999999999LL]  // long long
        ];
        
        // ========================================
        // Test 5: Mixed Type Arrays
        // ========================================
        
        NSArray *mixedTypes = @[
            @"String",
            @123,
            [NSNull null],
            @[@"Nested", @"Array"],
            @{@"key": @"value"},
            [[TestObject alloc] initWithName:@"TestObj" value:99]
        ];
        
        // ========================================
        // Test 6: Nested Arrays
        // ========================================
        
        NSArray *nestedSimple = @[
            @[@"A", @"B"],
            @[@"C", @"D"],
            @[@"E", @"F"]
        ];
        
        NSArray *deeplyNested = @[
            @"Level 1",
            @[
                @"Level 2",
                @[
                    @"Level 3",
                    @[@"Level 4"]
                ]
            ]
        ];
        
        // ========================================
        // Test 7: Mutable Arrays
        // ========================================
        
        NSMutableArray *mutableSmall = [NSMutableArray arrayWithObjects:@"One", @"Two", @"Three", nil];
        
        NSMutableArray *mutableLarge = [NSMutableArray arrayWithCapacity:100];
        for (int i = 0; i < 100; i++) {
            [mutableLarge addObject:[NSString stringWithFormat:@"Item %d", i]];
        }
        
        NSMutableArray *mutableModified = [NSMutableArray arrayWithArray:@[@"Initial", @"Values"]];
        [mutableModified addObject:@"Added"];
        [mutableModified insertObject:@"Inserted" atIndex:1];
        [mutableModified removeObjectAtIndex:0];  // Remove "Initial"
        // Result should be: @"Inserted", @"Values", @"Added"
        
        // ========================================
        // Test 8: Arrays with Custom Objects
        // ========================================
        
        TestObject *obj1 = [[TestObject alloc] initWithName:@"Alpha" value:1];
        TestObject *obj2 = [[TestObject alloc] initWithName:@"Beta" value:2];
        TestObject *obj3 = [[TestObject alloc] initWithName:@"Gamma" value:3];
        
        NSArray *customObjects = @[obj1, obj2, obj3];
        
        // ========================================
        // Test 9: Arrays with nil/NSNull
        // ========================================
        
        NSArray *withNulls = @[
            @"Before null",
            [NSNull null],
            @"After null",
            [NSNull null],
            @"End"
        ];
        
        // ========================================
        // Test 10: Very Large Array
        // ========================================
        
        NSMutableArray *hugeArray = [NSMutableArray arrayWithCapacity:1000];
        for (int i = 0; i < 1000; i++) {
            [hugeArray addObject:@(i)];
        }
        NSArray *hugeImmutable = [hugeArray copy];
        
        // ========================================
        // Test 11: Arrays from Other Methods
        // ========================================
        
        NSArray *pathComponents = [@"/usr/local/bin/program" pathComponents];
        NSArray *sortedArray = [@[@"Zebra", @"Apple", @"Mango", @"Banana"] 
                                sortedArrayUsingSelector:@selector(compare:)];
        
        // ========================================
        // Test 12: Literal Syntax Arrays
        // ========================================
        
        NSArray *literalEmpty = @[];
        NSArray *literalSimple = @[@"A", @"B", @"C"];
        NSArray *literalMixed = @[@"String", @42, @YES, @3.14];
        
        // ========================================
        // Test 13: nil Array
        // ========================================
        
        NSArray *nilArray = nil;
        
        // ========================================
        // BREAKPOINT - Set breakpoint here for testing
        // ========================================
        
        printf("All NSArray objects created. Set breakpoint here.\n"); // Line for breakpoint
        
        // Verification prints
        printf("\nVerifying array counts:\n");
        printf("emptyArray count: %lu\n", (unsigned long)[emptyArray count]);
        printf("shortStringArray count: %lu\n", (unsigned long)[shortStringArray count]);
        printf("mixedTypes count: %lu\n", (unsigned long)[mixedTypes count]);
        printf("mutableLarge count: %lu\n", (unsigned long)[mutableLarge count]);
        printf("hugeArray count: %lu\n", (unsigned long)[hugeArray count]);
        
        // Test element access
        if ([shortStringArray count] > 0) {
            printf("First element of shortStringArray: %s\n", 
                   [[shortStringArray objectAtIndex:0] UTF8String]);
        }
        
        // Test mutable array modifications worked
        printf("mutableModified contents: ");
        for (NSString *item in mutableModified) {
            printf("%s ", [item UTF8String]);
        }
        printf("\n");
        
        // ========================================
        // Test 14: Array Enumeration
        // ========================================
        
        printf("\nEnumerating shortStringArray:\n");
        for (NSString *item in shortStringArray) {
            printf("  - %s\n", [item UTF8String]);
        }
        
        // ========================================
        // Test 15: Array as Part of Dictionary
        // ========================================
        
        NSDictionary *dictWithArrays = @{
            @"empty": emptyArray,
            @"strings": shortStringArray,
            @"numbers": intArray,
            @"mixed": mixedTypes
        };
        
        printf("\nDictionary with arrays created.\n");
        
        // Another breakpoint location for dictionary testing
        printf("Dictionary ready for inspection.\n"); // Alternative breakpoint
        
        // Clean up
        [emptyAlloc release];
        [obj1 release];
        [obj2 release];
        [obj3 release];
        [hugeImmutable release];
        
        printf("\n=== Test Complete ===\n");
        
        return 0;
    }
}