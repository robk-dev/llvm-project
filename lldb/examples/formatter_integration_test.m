//
// formatter_integration_test.m
// Comprehensive Integration Test for GNUstep LLDB Formatter Fixes
//
// This test validates all formatter improvements:
// 1. Id type dispatcher for proper value formatting
// 2. ISA recursion filtering  
// 3. Collection value display improvements
// 4. Tagged pointer handling
// 5. NSNumber in dictionary shows actual value (30) not hex
// 6. NSArray elements are expandable and show proper values
// 7. NSDictionary shows both keys and values properly
// 8. NSSet shows actual element values in summary
// 9. Custom objects don't show infinite isa recursion
// 10. Nested collections format properly
//
// Build: 
//   CC=/home/robk/code/llvm-project/build/bin/clang
//   $CC -fobjc-runtime=gnustep-2.1 -fblocks -g -gdwarf-5 -O0 \
//       -I/usr/local/include/GNUstep -fconstant-string-class=NSConstantString \
//       -L/usr/local/lib -Wl,-rpath,/usr/local/lib \
//       -o formatter_integration_test formatter_integration_test.m \
//       -lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm
//
// Debug:
//   /home/robk/code/llvm-project/build/bin/lldb formatter_integration_test
//   (lldb) source formatter_integration_test.lldb
//

#import <Foundation/Foundation.h>

// Custom class to test ISA recursion fix
@interface Person : NSObject {
    NSString *_name;
    NSInteger _age;
    NSString *_occupation;
}
@property(nonatomic, strong) NSString *name;
@property(nonatomic, assign) NSInteger age;
@property(nonatomic, strong) NSString *occupation;
- (instancetype)initWithName:(NSString *)name age:(NSInteger)age occupation:(NSString *)occupation;
@end

@implementation Person
@synthesize name = _name;
@synthesize age = _age;
@synthesize occupation = _occupation;

- (instancetype)initWithName:(NSString *)name age:(NSInteger)age occupation:(NSString *)occupation {
    self = [super init];
    if (self) {
        _name = name;
        _age = age;
        _occupation = occupation;
    }
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"Person(%@, %ld, %@)", 
            self.name, (long)self.age, self.occupation];
}
@end

// Custom class with nested objects
@interface Company : NSObject {
    NSString *_companyName;
    NSMutableArray *_employees;
    NSDictionary *_departments;
}
@property(nonatomic, strong) NSString *companyName;
@property(nonatomic, strong) NSMutableArray *employees;
@property(nonatomic, strong) NSDictionary *departments;
- (instancetype)initWithName:(NSString *)name;
@end

@implementation Company
@synthesize companyName = _companyName;
@synthesize employees = _employees;
@synthesize departments = _departments;

- (instancetype)initWithName:(NSString *)name {
    self = [super init];
    if (self) {
        _companyName = name;
        _employees = [[NSMutableArray alloc] init];
        _departments = [[NSDictionary alloc] init];
    }
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"Company(%@, employees=%ld, departments=%ld)",
            self.companyName, (long)[self.employees count], (long)[self.departments count]];
}
@end

void print_test_header(const char *test_name) {
    printf("\n");
    printf("================================================================================\n");
    printf("TEST: %s\n", test_name);
    printf("================================================================================\n");
}

void print_expected(const char *expected) {
    printf("EXPECTED: %s\n", expected);
}

int main(int argc, char *argv[]) {
    @autoreleasepool {
        printf("GNUstep LLDB Formatter Integration Test Suite\n");
        printf("==============================================\n\n");
        
        // ========================================================================
        // TEST 1: NSNumber in NSDictionary shows actual value, not hex
        // ========================================================================
        print_test_header("NSNumber in NSDictionary Value Display");
        print_expected("{ age = 30; height = 5.9; isActive = 1; pi = 3.14159 }");
        
        NSDictionary *numberDict = @{
            @"age": @30,                    // Integer - should show "30" not "0x1e"
            @"height": @5.9,                 // Float - should show "5.9"
            @"isActive": @YES,               // Boolean - should show "1" or "YES"
            @"pi": @3.14159                  // Double - should show "3.14159"
        };
        
        // Tagged pointer numbers (small integers)
        NSDictionary *taggedNumberDict = @{
            @"small": @1,                    // Tagged pointer - should show "1"
            @"negative": @-42,               // Tagged negative - should show "-42"
            @"zero": @0                      // Tagged zero - should show "0"
        };
        
        NSLog(@"Number dictionary created"); // BREAKPOINT 1
        
        // ========================================================================
        // TEST 2: NSArray elements are expandable and show proper values
        // ========================================================================
        print_test_header("NSArray Element Display and Expansion");
        print_expected("(@\"First\", @\"Second\", @30, @3.14, <Person object>)");
        
        Person *person1 = [[Person alloc] initWithName:@"Alice" age:25 occupation:@"Engineer"];
        NSArray *mixedArray = @[
            @"First",                        // String element
            @"Second",                       // Another string
            @30,                             // Number - should show value not hex
            @3.14,                           // Float number
            person1                          // Custom object
        ];
        
        // Nested array to test expansion
        NSArray *nestedArray = @[
            @"Level1",
            @[@"Level2A", @"Level2B", @42], // Nested array with mixed types
            @{@"key": @"value"}              // Dictionary in array
        ];
        
        NSLog(@"Array with mixed types created"); // BREAKPOINT 2
        
        // ========================================================================
        // TEST 3: NSDictionary shows both keys AND values properly
        // ========================================================================
        print_test_header("NSDictionary Key-Value Display");
        print_expected("{ name = \"John Doe\"; age = 30; occupation = \"Developer\" }");
        
        NSDictionary *personInfo = @{
            @"name": @"John Doe",
            @"age": @30,                    // Value should be "30" not hex
            @"occupation": @"Developer"
        };
        
        // Dictionary with various value types
        NSDictionary *complexDict = @{
            @"string": @"Hello World",
            @"number": @42,
            @"array": @[@1, @2, @3],
            @"dict": @{@"nested": @"value"},
            @"null": [NSNull null],
            @"person": person1
        };
        
        NSLog(@"Dictionary with proper key-value display"); // BREAKPOINT 3
        
        // ========================================================================
        // TEST 4: NSSet shows actual element values in summary
        // ========================================================================
        print_test_header("NSSet Element Value Display");
        print_expected("(\"Apple\", \"Banana\", 42, <Person object>)");
        
        NSSet *mixedSet = [NSSet setWithObjects:
            @"Apple",
            @"Banana",
            @42,                             // Should show "42" not hex
            person1,                         // Custom object
            nil
        ];
        
        // Set with only numbers to verify tagged pointer handling
        NSSet *numberSet = [NSSet setWithObjects:
            @1, @2, @3, @42, @100, @-50,
            nil
        ];
        
        NSLog(@"Set with actual values in summary"); // BREAKPOINT 4
        
        // ========================================================================
        // TEST 5: Custom objects don't show infinite ISA recursion
        // ========================================================================
        print_test_header("Custom Object ISA Recursion Prevention");
        print_expected("Person(Alice, 25, Engineer) - NO isa chain");
        
        Person *person2 = [[Person alloc] initWithName:@"Bob" age:30 occupation:@"Manager"];
        Company *company = [[Company alloc] initWithName:@"TechCorp"];
        [company.employees addObject:person1];
        [company.employees addObject:person2];
        
        // Create departments dictionary
        company.departments = @{
            @"Engineering": @[person1],
            @"Management": @[person2],
            @"HR": @[]
        };
        
        NSLog(@"Custom objects without ISA recursion"); // BREAKPOINT 5
        
        // ========================================================================
        // TEST 6: Tagged pointer handling
        // ========================================================================
        print_test_header("Tagged Pointer Decoding");
        print_expected("Small integers and strings decoded properly");
        
        // Small integers (typically tagged)
        NSNumber *tagged1 = @1;
        NSNumber *tagged2 = @42;
        NSNumber *tagged3 = @-100;
        
        // Small strings (may be tagged on some architectures)
        NSString *shortStr = @"Hi";
        NSString *taggedStr = [NSString stringWithFormat:@"%d", 1];
        
        NSArray *taggedArray = @[tagged1, tagged2, tagged3, shortStr, taggedStr];
        NSDictionary *taggedDict = @{
            @"one": tagged1,
            @"fortytwo": tagged2,
            @"negative": tagged3,
            @"short": shortStr
        };
        
        NSLog(@"Tagged pointers decoded correctly"); // BREAKPOINT 6
        
        // ========================================================================
        // TEST 7: Deeply nested collections format properly
        // ========================================================================
        print_test_header("Nested Collection Formatting");
        print_expected("Properly formatted nested structures at all levels");
        
        // Create a complex nested structure
        NSDictionary *level3 = @{
            @"deep": @"value",
            @"count": @999
        };
        
        NSArray *level2Array = @[
            @"item1",
            @"item2",
            level3,
            @123
        ];
        
        NSDictionary *level2Dict = @{
            @"array": level2Array,
            @"string": @"Level 2",
            @"number": @456
        };
        
        NSSet *level1Set = [NSSet setWithObjects:
            level2Dict,
            @"SetItem",
            @789,
            nil
        ];
        
        NSDictionary *deeplyNested = @{
            @"topLevel": @"root",
            @"theSet": level1Set,
            @"directArray": @[@1, @2, @{@"inline": @"dict"}],
            @"metadata": @{
                @"version": @1,
                @"created": @"today",
                @"complex": @{
                    @"nested": @{
                        @"very": @{
                            @"deep": @"value"
                        }
                    }
                }
            }
        };
        
        NSLog(@"Deeply nested collections formatted"); // BREAKPOINT 7
        
        // ========================================================================
        // TEST 8: Collection with nil and NSNull handling
        // ========================================================================
        print_test_header("Nil and NSNull in Collections");
        print_expected("Proper handling of nil and NSNull");
        
        NSDictionary *nullDict = @{
            @"null": [NSNull null],
            @"string": @"not null",
            @"number": @42
        };
        
        NSArray *nullArray = @[
            @"first",
            [NSNull null],
            @"third",
            @0  // Zero is not null
        ];
        
        NSSet *nullSet = [NSSet setWithObjects:
            @"element",
            [NSNull null],
            @123,
            nil  // nil terminates, won't be in set
        ];
        
        NSLog(@"Collections with NSNull handled"); // BREAKPOINT 8
        
        // ========================================================================
        // TEST 9: Large collections (performance and truncation)
        // ========================================================================
        print_test_header("Large Collection Handling");
        print_expected("Large collections formatted efficiently with truncation if needed");
        
        NSMutableArray *largeArray = [NSMutableArray array];
        NSMutableDictionary *largeDict = [NSMutableDictionary dictionary];
        NSMutableSet *largeSet = [NSMutableSet set];
        
        for (int i = 0; i < 1000; i++) {
            [largeArray addObject:@(i)];
            
            NSString *key = [NSString stringWithFormat:@"key_%d", i];
            [largeDict setObject:@(i * 2) forKey:key];
            
            [largeSet addObject:@(i * 3)];
        }
        
        NSLog(@"Large collections created"); // BREAKPOINT 9
        
        // ========================================================================
        // TEST 10: Mutable vs Immutable collections
        // ========================================================================
        print_test_header("Mutable vs Immutable Collection Display");
        print_expected("Both mutable and immutable collections format identically");
        
        // Immutable collections
        NSArray *immutableArray = @[@"a", @"b", @"c"];
        NSDictionary *immutableDict = @{@"key": @"value"};
        NSSet *immutableSet = [NSSet setWithObjects:@"x", @"y", @"z", nil];
        
        // Mutable collections with same content
        NSMutableArray *mutableArray = [NSMutableArray arrayWithArray:immutableArray];
        NSMutableDictionary *mutableDict = [NSMutableDictionary dictionaryWithDictionary:immutableDict];
        NSMutableSet *mutableSet = [NSMutableSet setWithSet:immutableSet];
        
        // Modify mutable collections
        [mutableArray addObject:@"d"];
        [mutableDict setObject:@"value2" forKey:@"key2"];
        [mutableSet addObject:@"w"];
        
        NSLog(@"Mutable and immutable collections"); // BREAKPOINT 10
        
        // ========================================================================
        // FINAL: Comprehensive validation structure
        // ========================================================================
        print_test_header("Final Comprehensive Validation");
        print_expected("All formatters working together in complex structure");
        
        Person *ceo = [[Person alloc] initWithName:@"CEO Name" age:50 occupation:@"Chief Executive"];
        Company *techCorp = [[Company alloc] initWithName:@"TechCorp International"];
        [techCorp.employees addObject:ceo];
        [techCorp.employees addObject:person1];
        [techCorp.employees addObject:person2];
        
        NSDictionary *finalTest = @{
            @"company": techCorp,
            @"statistics": @{
                @"employeeCount": @([techCorp.employees count]),
                @"revenue": @1000000.50,
                @"founded": @2020,
                @"isPublic": @YES
            },
            @"locations": @[@"New York", @"San Francisco", @"London"],
            @"partners": [NSSet setWithObjects:
                @"PartnerA",
                @"PartnerB", 
                @"PartnerC",
                nil
            ],
            @"metadata": @{
                @"lastUpdated": @"2024-01-15",
                @"version": @2,
                @"tags": @[@"tech", @"software", @"innovation"]
            }
        };
        
        NSLog(@"=== ALL TESTS COMPLETE ==="); // BREAKPOINT 11
        NSLog(@"Validate each test by examining variables at breakpoints");
        NSLog(@"Use 'po <variable>' to test formatter output");
        NSLog(@"Use 'frame variable <variable>' for detailed view");
        
        // Print summary of all test variables
        printf("\n");
        printf("TEST VARIABLE REFERENCE:\n");
        printf("------------------------\n");
        printf("numberDict         - NSNumbers in dictionary (Test 1)\n");
        printf("taggedNumberDict   - Tagged pointer numbers (Test 1)\n");
        printf("mixedArray         - Array with mixed types (Test 2)\n");
        printf("nestedArray        - Nested array structure (Test 2)\n");
        printf("personInfo         - Simple person dictionary (Test 3)\n");
        printf("complexDict        - Dictionary with various types (Test 3)\n");
        printf("mixedSet           - Set with mixed objects (Test 4)\n");
        printf("numberSet          - Set with only numbers (Test 4)\n");
        printf("person1, person2   - Custom Person objects (Test 5)\n");
        printf("company            - Custom Company object (Test 5)\n");
        printf("taggedArray/Dict   - Tagged pointer collections (Test 6)\n");
        printf("deeplyNested       - Complex nested structure (Test 7)\n");
        printf("nullDict/Array/Set - Collections with NSNull (Test 8)\n");
        printf("largeArray/Dict/Set- Large collections (Test 9)\n");
        printf("mutable/immutable  - Collection mutability test (Test 10)\n");
        printf("finalTest          - Comprehensive structure (Test 11)\n");
        
        // Keep objects alive for debugging
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
    }
    return 0;
}