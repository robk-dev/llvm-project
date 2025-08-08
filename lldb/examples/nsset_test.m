// NSSet formatter test for GNUstep LLDB plugin
#import <Foundation/Foundation.h>

@interface CustomObject : NSObject
{
    NSString *name;
    int value;
}
- (id)initWithName:(NSString *)n value:(int)v;
- (NSString *)description;
@end

@implementation CustomObject
- (id)initWithName:(NSString *)n value:(int)v {
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
    return [NSString stringWithFormat:@"CustomObject(%@, %d)", name, value];
}

- (NSUInteger)hash {
    return [name hash] + value;
}

- (BOOL)isEqual:(id)object {
    if (![object isKindOfClass:[CustomObject class]]) {
        return NO;
    }
    CustomObject *other = (CustomObject *)object;
    return [name isEqualToString:other->name] && value == other->value;
}
@end

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Test 1: Empty set
    NSSet *emptySet = [NSSet set];
    NSLog(@"Empty set: %@", emptySet);
    
    // Test 2: Set with single object
    NSSet *singleSet = [NSSet setWithObject:@"Hello"];
    NSLog(@"Single object set: %@", singleSet);
    
    // Test 3: Set with multiple strings
    NSSet *stringSet = [NSSet setWithObjects:@"Apple", @"Banana", @"Cherry", @"Date", @"Elderberry", nil];
    NSLog(@"String set count: %lu", (unsigned long)[stringSet count]);
    
    // Test 4: Set with numbers
    NSSet *numberSet = [NSSet setWithObjects:
        [NSNumber numberWithInt:42],
        [NSNumber numberWithDouble:3.14159],
        [NSNumber numberWithBool:YES],
        [NSNumber numberWithLong:1234567890L],
        nil];
    NSLog(@"Number set count: %lu", (unsigned long)[numberSet count]);
    
    // Test 5: Mixed set
    NSSet *mixedSet = [NSSet setWithObjects:
        @"String",
        [NSNumber numberWithInt:123],
        [NSArray arrayWithObjects:@"A", @"B", @"C", nil],
        [NSDictionary dictionaryWithObjectsAndKeys:@"value1", @"key1", @"value2", @"key2", nil],
        nil];
    NSLog(@"Mixed set count: %lu", (unsigned long)[mixedSet count]);
    
    // Test 6: Mutable set
    NSMutableSet *mutableSet = [NSMutableSet set];
    [mutableSet addObject:@"First"];
    [mutableSet addObject:@"Second"];
    [mutableSet addObject:@"Third"];
    NSLog(@"Mutable set count: %lu", (unsigned long)[mutableSet count]);
    
    // Test 7: Set with custom objects
    CustomObject *obj1 = [[CustomObject alloc] initWithName:@"Object1" value:100];
    CustomObject *obj2 = [[CustomObject alloc] initWithName:@"Object2" value:200];
    CustomObject *obj3 = [[CustomObject alloc] initWithName:@"Object3" value:300];
    
    NSSet *customSet = [NSSet setWithObjects:obj1, obj2, obj3, nil];
    NSLog(@"Custom object set count: %lu", (unsigned long)[customSet count]);
    
    // Test 8: Large set
    NSMutableSet *largeSet = [NSMutableSet set];
    for (int i = 0; i < 100; i++) {
        [largeSet addObject:[NSString stringWithFormat:@"Item%d", i]];
    }
    NSLog(@"Large set count: %lu", (unsigned long)[largeSet count]);
    
    // Test 9: NSCountedSet
    NSCountedSet *countedSet = [NSCountedSet set];
    [countedSet addObject:@"Apple"];
    [countedSet addObject:@"Apple"];
    [countedSet addObject:@"Apple"];
    [countedSet addObject:@"Banana"];
    [countedSet addObject:@"Banana"];
    NSLog(@"Counted set unique objects: %lu", (unsigned long)[countedSet count]);
    NSLog(@"Apple count: %lu", (unsigned long)[countedSet countForObject:@"Apple"]);
    NSLog(@"Banana count: %lu", (unsigned long)[countedSet countForObject:@"Banana"]);
    
    // Test 10: Nested sets
    NSSet *innerSet1 = [NSSet setWithObjects:@"Inner1", @"Inner2", nil];
    NSSet *innerSet2 = [NSSet setWithObjects:@"Inner3", @"Inner4", nil];
    NSSet *nestedSet = [NSSet setWithObjects:innerSet1, innerSet2, @"Outer", nil];
    NSLog(@"Nested set count: %lu", (unsigned long)[nestedSet count]);
    
    // Breakpoint location for debugging
    NSLog(@"All tests completed. Set breakpoint here to inspect sets."); // Set breakpoint here
    
    // Cleanup
    [obj1 release];
    [obj2 release];
    [obj3 release];
    
    [pool drain];
    return 0;
}