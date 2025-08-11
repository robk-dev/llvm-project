#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Get the shared user defaults instance
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        
        // Set some test values
        [defaults setObject:@"TestValue1" forKey:@"TestKey1"];
        [defaults setInteger:42 forKey:@"TestInteger"];
        [defaults setBool:YES forKey:@"TestBool"];
        [defaults setObject:@[@"item1", @"item2", @"item3"] forKey:@"TestArray"];
        
        // Synchronize to ensure values are persisted
        [defaults synchronize];
        
        // Create a breakpoint here
        NSLog(@"UserDefaults test ready"); // BREAKPOINT HERE
        
        // Read some values back
        NSString *value1 = [defaults objectForKey:@"TestKey1"];
        NSInteger intValue = [defaults integerForKey:@"TestInteger"];
        BOOL boolValue = [defaults boolForKey:@"TestBool"];
        NSArray *arrayValue = [defaults objectForKey:@"TestArray"];
        
        NSLog(@"Values: %@, %ld, %d, %@", value1, (long)intValue, boolValue, arrayValue);
    }
    
    return 0;
}