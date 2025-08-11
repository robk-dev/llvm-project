// Comprehensive expression evaluation regression test
// This test covers all the critical expression evaluation scenarios to prevent regressions

#import <Foundation/Foundation.h>

@interface TestCustomClass : NSObject {
    NSString *name;
    int value;
}
@property (nonatomic, retain) NSString *name;
@property (nonatomic, assign) int value;
- (NSString *)description;
- (int)getValue;
@end

@implementation TestCustomClass
@synthesize name, value;

- (NSString *)description {
    return [NSString stringWithFormat:@"TestCustomClass(name=%@, value=%d)", self.name, self.value];
}

- (int)getValue {
    return self.value;
}

- (void)dealloc {
    [name release];
    [super dealloc];
}
@end

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Test objects for comprehensive expression evaluation testing
    TestCustomClass *customObj = [[TestCustomClass alloc] init];
    customObj.name = @"TestObject";
    customObj.value = 42;
    
    NSString *testString = @"Hello World";
    NSNumber *testNumber = [NSNumber numberWithInt:123];
    NSArray *testArray = [NSArray arrayWithObjects:@"one", @"two", @"three", nil];
    NSDate *testDate = [NSDate date];
    
    // Nil object for edge case testing
    NSString *nilString = nil;
    
    // === BREAKPOINT HERE FOR TESTING ===
    printf("Test objects created - set breakpoint here\n");
    printf("customObj: %s\n", [[customObj description] UTF8String]);
    printf("testString: %s\n", [testString UTF8String]);
    printf("testNumber: %d\n", [testNumber intValue]);
    printf("testArray count: %lu\n", (unsigned long)[testArray count]);
    // === BREAKPOINT HERE FOR TESTING ===
    
    [customObj release];
    [pool release];
    return 0;
}