#import <Foundation/Foundation.h>
#import <stdio.h>

@interface SimpleTest : NSObject {
  NSString *_name;
  NSNumber *_count;
  NSMutableArray *_items;
}
@property(nonatomic, strong) NSString *name;
@property(nonatomic, strong) NSNumber *count;
@property(nonatomic, strong) NSMutableArray *items;
@end

@implementation SimpleTest
@synthesize name = _name;
@synthesize count = _count;
@synthesize items = _items;

- (instancetype)initWithName:(NSString *)name {
  if (self = [super init]) {
    _name = name;
    _count = @(0);
    _items = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)addItem:(NSString *)item {
  [_items addObject:item];
  _count = @([_items count]);
}

- (NSString *)description {
  return [NSString stringWithFormat:@"SimpleTest(name=%@, count=%@, items=%ld)", 
          _name, _count, (long)[_items count]];
}
@end

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    printf("=== Memory Layout Debug Test ===\n");
    
    // Create a simple test object
    SimpleTest *test = [[SimpleTest alloc] initWithName:@"MemoryTest"];
    
    printf("Created test object at: %p\n", test);
    printf("Expected items count: 0\n");
    
    // Add some items
    [test addItem:@"Item1"];
    [test addItem:@"Item2"];
    [test addItem:@"Item3"];
    [test addItem:@"Item4"];
    
    printf("Added 4 items\n");
    printf("Expected items count: 4\n");
    printf("Actual items count: %ld\n", (long)[test.items count]);
    
    // Print object info
    NSLog(@"Test object: %@", test);
    
    // Manual memory examination
    printf("\n=== Memory Layout Analysis ===\n");
    printf("Object pointer: %p\n", test);
    printf("Name pointer: %p\n", test.name);  
    printf("Count pointer: %p\n", test.count);
    printf("Items pointer: %p\n", test.items);
    
    // Print actual values
    printf("Name value: %s\n", [test.name UTF8String]);
    printf("Count value: %ld\n", (long)[test.count integerValue]);
    printf("Items count: %ld\n", (long)[test.items count]);
    
    // Test basic collections
    NSArray *simpleArray = @[@"A", @"B", @"C"];
    NSDictionary *simpleDict = @{@"key1": @"value1", @"key2": @"value2"};
    NSSet *simpleSet = [NSSet setWithObjects:@"X", @"Y", @"Z", nil];
    
    printf("\n=== Basic Collections Test ===\n");
    printf("Array count: %ld\n", (long)[simpleArray count]);
    printf("Dict count: %ld\n", (long)[simpleDict count]);  
    printf("Set count: %ld\n", (long)[simpleSet count]);
    
    // BREAKPOINT HERE FOR TESTING
    printf("Set breakpoint here for memory analysis\n");
    
    return 0;
  }
}