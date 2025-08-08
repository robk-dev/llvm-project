#import <Foundation/Foundation.h>

// Custom class to test ivar visibility
@interface TestClass : NSObject {
  int _intValue;
  NSString *_stringValue;
  double _doubleValue;
  NSArray *_arrayValue;
}

@property int intValue;
@property (retain) NSString *stringValue;
@property double doubleValue;
@property (retain) NSArray *arrayValue;

- (id)initWithInt:(int)i string:(NSString *)s double:(double)d array:(NSArray *)a;
@end

@implementation TestClass

@synthesize intValue = _intValue;
@synthesize stringValue = _stringValue;
@synthesize doubleValue = _doubleValue;
@synthesize arrayValue = _arrayValue;

- (id)initWithInt:(int)i string:(NSString *)s double:(double)d array:(NSArray *)a {
  self = [super init];
  if (self) {
    _intValue = i;
    _stringValue = [s retain];
    _doubleValue = d;
    _arrayValue = [a retain];
  }
  return self;
}

- (void)dealloc {
  [_stringValue release];
  [_arrayValue release];
  [super dealloc];
}

@end

int main(int argc, char *argv[]) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  
  // Create test object
  TestClass *testObj = [[TestClass alloc] initWithInt:42
                                                string:@"Hello World"
                                                double:3.14159
                                                 array:@[@"One", @"Two", @"Three"]];
  
  // Create standard Foundation objects to test
  NSObject *baseObj = [[NSObject alloc] init];
  NSDictionary *dict = @{@"key1": @"value1", @"key2": @42};
  NSArray *array = @[@"Item1", @"Item2", @"Item3"];
  
  // Set breakpoint here to inspect objects
  NSLog(@"Test objects created");  // Line 59 - Set breakpoint here
  
  // Verify that isa is not shown in the debugger for these objects:
  // - testObj should show: _intValue, _stringValue, _doubleValue, _arrayValue
  // - baseObj should show no ivars (just NSObject has no user ivars)
  // - dict should show its key-value pairs, not isa
  // - array should show its elements, not isa
  
  NSLog(@"testObj: %@", testObj);
  NSLog(@"baseObj: %@", baseObj);
  NSLog(@"dict: %@", dict);
  NSLog(@"array: %@", array);
  
  [testObj release];
  [baseObj release];
  [pool drain];
  
  return 0;
}