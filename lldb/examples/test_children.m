#import <Foundation/Foundation.h>
#include <unistd.h>

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    // Create test collections  
    NSArray *fruits = @[ @"apple", @"banana", @"cherry" ];
    NSDictionary *personInfo = @{
      @"name" : @"John Doe",
      @"age" : @30,
      @"skills" : @[ @"Objective-C", @"Swift", @"Python" ]
    };
    NSSet *languages = [NSSet setWithObjects:@"English", @"Spanish", @"French", nil];
    
    // Print summaries (these work)
    NSLog(@"Fruits: %@", fruits);
    NSLog(@"PersonInfo: %@", personInfo);
    NSLog(@"Languages: %@", languages);
    
    // Sleep to allow debugging
    NSLog(@"Sleeping for debugging... PID: %d", getpid());
    sleep(30);  // Give us time to attach and test
    
    return 0;
  }
}