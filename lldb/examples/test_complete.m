#import <Foundation/Foundation.h>

@interface TestClass : NSObject
@property (nonatomic, strong) NSString *name;
@property (nonatomic, strong) NSNumber *value;
@end

@implementation TestClass
@end

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Test dictionary with string keys
    NSDictionary *personInfo = @{
      @"name": @"John Doe",
      @"occupation": @"Developer",
      @"age": @30
    };
    
    // Test nested collections
    NSDictionary *nestedData = @{
      @"items": @[@"one", @"two", @"three"],
      @"config": @{@"enabled": @YES, @"timeout": @60}
    };
    
    // Test set
    NSSet *fruits = [NSSet setWithObjects:@"Apple", @"Banana", @"Cherry", nil];
    
    // Test mutable set
    NSMutableSet *colors = [NSMutableSet setWithObjects:@"Red", @"Green", @"Blue", nil];
    
    // Test custom object
    TestClass *obj = [[TestClass alloc] init];
    obj.name = @"Test Object";
    obj.value = @42;
    
    NSLog(@"Set breakpoint here");
    NSLog(@"PersonInfo: %@", personInfo);
    NSLog(@"NestedData: %@", nestedData);
    NSLog(@"Fruits: %@", fruits);
    NSLog(@"Colors: %@", colors);
    NSLog(@"Object: %@", obj);
    
    return 0;
  }
}