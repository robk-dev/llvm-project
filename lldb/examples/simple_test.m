#import <Foundation/Foundation.h>

@interface Person : NSObject
@property (nonatomic, strong) NSString *name;
@property (nonatomic, assign) NSInteger age;
@property (nonatomic, strong) NSArray *hobbies;
@end

@implementation Person
- (NSString *)description {
    return [NSString stringWithFormat:@"Person(name=%@, age=%ld, hobbies=%@)", 
            self.name, (long)self.age, self.hobbies];
}
@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Create a simple person object
        Person *person = [[Person alloc] init];
        person.name = @"Alice Johnson";
        person.age = 25;
        person.hobbies = @[@"Programming", @"Reading", @"Hiking"];
        
        // Create some Foundation objects
        NSString *greeting = @"Hello, LLDB Bridge!";
        NSArray *numbers = @[@1, @2, @3, @42];
        NSDictionary *config = @{
            @"debug": @YES,
            @"timeout": @30,
            @"name": @"LLDB Bridge Test"
        };
        
        // Set breakpoint here to test debugging
        NSLog(@"Testing LLDB Bridge with custom objects");
        NSLog(@"Person: %@", person);
        NSLog(@"Greeting: %@", greeting);
        NSLog(@"Numbers count: %lu", (unsigned long)numbers.count);
        NSLog(@"Config: %@", config);
        
        return 0;
    }
}

