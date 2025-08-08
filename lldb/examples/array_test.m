// Test program for GNUstep NSArray formatter
#import <Foundation/Foundation.h>

@interface Person : NSObject
{
    NSString *name;
    int age;
}
- (id)initWithName:(NSString *)n age:(int)a;
- (NSString *)description;
@end

@implementation Person
- (id)initWithName:(NSString *)n age:(int)a {
    self = [super init];
    if (self) {
        name = [n retain];
        age = a;
    }
    return self;
}

- (void)dealloc {
    [name release];
    [super dealloc];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"Person: %@ (%d years)", name, age];
}
@end

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Test 1: Empty array
    NSArray *emptyArray = [NSArray array];
    printf("Created empty array\n");  // Line 37 - breakpoint here
    
    // Test 2: Array with strings
    NSArray *stringArray = [NSArray arrayWithObjects:
        @"First", 
        @"Second", 
        @"Third", 
        @"Fourth", 
        @"Fifth",
        nil];
    printf("Created string array with %lu elements\n", (unsigned long)[stringArray count]); // Line 47
    
    // Test 3: Array with numbers
    NSArray *numberArray = [NSArray arrayWithObjects:
        [NSNumber numberWithInt:42],
        [NSNumber numberWithDouble:3.14159],
        [NSNumber numberWithBool:YES],
        [NSNumber numberWithLongLong:9876543210LL],
        nil];
    printf("Created number array with %lu elements\n", (unsigned long)[numberArray count]); // Line 56
    
    // Test 4: Array with custom objects
    Person *person1 = [[Person alloc] initWithName:@"Alice" age:30];
    Person *person2 = [[Person alloc] initWithName:@"Bob" age:25];
    Person *person3 = [[Person alloc] initWithName:@"Charlie" age:35];
    
    NSArray *objectArray = [NSArray arrayWithObjects:person1, person2, person3, nil];
    printf("Created object array with %lu elements\n", (unsigned long)[objectArray count]); // Line 64
    
    // Test 5: Nested arrays
    NSArray *nestedArray = [NSArray arrayWithObjects:
        stringArray,
        numberArray,
        objectArray,
        nil];
    printf("Created nested array with %lu elements\n", (unsigned long)[nestedArray count]); // Line 72
    
    // Test 6: Mutable array
    NSMutableArray *mutableArray = [NSMutableArray array];
    [mutableArray addObject:@"Dynamic 1"];
    [mutableArray addObject:@"Dynamic 2"];
    [mutableArray addObject:@"Dynamic 3"];
    printf("Created mutable array with %lu elements\n", (unsigned long)[mutableArray count]); // Line 79
    
    // Test 7: Large array
    NSMutableArray *largeArray = [NSMutableArray array];
    for (int i = 0; i < 100; i++) {
        [largeArray addObject:[NSString stringWithFormat:@"Item %d", i]];
    }
    printf("Created large array with %lu elements\n", (unsigned long)[largeArray count]); // Line 86
    
    // Test 8: Array with nil in the middle (should be interesting)
    NSArray *mixedArray = [NSArray arrayWithObjects:
        @"Start",
        [NSNull null],  // NSNull is used to represent nil in collections
        @"End",
        nil];
    printf("Created mixed array with null\n"); // Line 94
    
    // Keep arrays alive for debugging
    printf("All arrays created. Set breakpoints to inspect.\n"); // Line 97
    
    // Clean up
    [person1 release];
    [person2 release];
    [person3 release];
    
    [pool drain];
    return 0;
}