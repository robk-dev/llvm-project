//===-- test_dictionary_po.m - Test dictionary po command fallback ----===//
//
// Simple test program to validate dictionary `po` command functionality
// with the new formatter fallback system
//
//===----------------------------------------------------------------------===//

#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSLog(@"=== Testing Dictionary po Command Fallback ===");
        
        // Create various dictionaries to test
        NSDictionary *empty = @{};
        
        NSDictionary *stringDict = @{
            @"name": @"John",
            @"city": @"NYC", 
            @"country": @"USA"
        };
        
        NSDictionary *numberDict = @{
            @1: @100,
            @2: @200,
            @42: @4200
        };
        
        NSDictionary *mixedDict = @{
            @"string": @"Hello",
            @"number": @42,
            @"bool": @YES
        };
        
        NSMutableDictionary *mutableDict = [NSMutableDictionary dictionary];
        mutableDict[@"key1"] = @"value1";
        mutableDict[@"key2"] = @"value2";
        
        // Nested dictionary
        NSDictionary *nested = @{
            @"user": @{@"name": @"Alice", @"age": @30},
            @"prefs": @{@"theme": @"dark", @"lang": @"en"}
        };
        
        NSLog(@"All dictionaries created successfully");
        NSLog(@"empty: %@", empty);
        NSLog(@"stringDict: %@", stringDict); 
        NSLog(@"numberDict: %@", numberDict);
        NSLog(@"mixedDict: %@", mixedDict);
        NSLog(@"mutableDict: %@", mutableDict);
        NSLog(@"nested: %@", nested);
        
        // Pause here for debugging - set breakpoint at line below
        NSLog(@"BREAKPOINT: Set breakpoint here to test po commands"); // LINE 50
        
        return 0;
    }
}