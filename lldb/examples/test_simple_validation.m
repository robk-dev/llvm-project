#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        NSArray *fruits = @[@"Apple", @"Banana", @"Cherry"];
        NSDictionary *person = @{@"name": @"John", @"age": @30};
        NSSet *colors = [NSSet setWithObjects:@"Red", @"Green", @"Blue", nil];
        NSNumber *number = @42;
        NSString *string = @"Hello, World!";
        
        NSLog(@"Break here to test formatters");
        NSLog(@"fruits = %@", fruits);
        NSLog(@"person = %@", person);
        NSLog(@"colors = %@", colors);
        NSLog(@"number = %@", number);
        NSLog(@"string = %@", string);
    }
    return 0;
}
