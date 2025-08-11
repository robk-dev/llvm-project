//===-- test_subscript_forwarding.m --------------------------*- ObjC -*-===//
//
// Test program for runtime method forwarding to enable modern subscript syntax
// This tests array[index] and dict[key] forwarding to objectAtIndex: and objectForKey:
//
//===----------------------------------------------------------------------===//

#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSLog(@"Testing runtime method forwarding for modern subscript syntax");
        
        // Test NSArray subscript forwarding
        NSArray *testArray = @[@"Apple", @"Banana", @"Cherry"];
        NSLog(@"Created array: %@", testArray);
        
        // These should work through forwarding: array[index] -> [array objectAtIndex:index]
        id firstElement = testArray[0];   // Should forward to objectAtIndex:
        id secondElement = testArray[1];  // Should forward to objectAtIndex:
        NSLog(@"First element (via subscript): %@", firstElement);
        NSLog(@"Second element (via subscript): %@", secondElement);
        
        // Compare with traditional method calls
        id firstTraditional = [testArray objectAtIndex:0];
        id secondTraditional = [testArray objectAtIndex:1];
        NSLog(@"First element (traditional): %@", firstTraditional);
        NSLog(@"Second element (traditional): %@", secondTraditional);
        
        // Test NSDictionary subscript forwarding
        NSDictionary *testDict = @{
            @"name": @"John Doe",
            @"occupation": @"Developer",
            @"language": @"Objective-C"
        };
        NSLog(@"Created dictionary: %@", testDict);
        
        // These should work through forwarding: dict[key] -> [dict objectForKey:key]
        id name = testDict[@"name"];           // Should forward to objectForKey:
        id occupation = testDict[@"occupation"]; // Should forward to objectForKey:
        NSLog(@"Name (via subscript): %@", name);
        NSLog(@"Occupation (via subscript): %@", occupation);
        
        // Compare with traditional method calls
        id nameTraditional = [testDict objectForKey:@"name"];
        id occupationTraditional = [testDict objectForKey:@"occupation"];
        NSLog(@"Name (traditional): %@", nameTraditional);
        NSLog(@"Occupation (traditional): %@", occupationTraditional);
        
        // Test NSMutableArray subscript forwarding
        NSMutableArray *mutableArray = [NSMutableArray arrayWithArray:testArray];
        NSLog(@"Created mutable array: %@", mutableArray);
        
        id mutableFirst = mutableArray[0];  // Should forward to objectAtIndex:
        NSLog(@"Mutable array first element (via subscript): %@", mutableFirst);
        
        // Test NSMutableDictionary subscript forwarding
        NSMutableDictionary *mutableDict = [NSMutableDictionary dictionaryWithDictionary:testDict];
        NSLog(@"Created mutable dictionary: %@", mutableDict);
        
        id mutableName = mutableDict[@"name"];  // Should forward to objectForKey:
        NSLog(@"Mutable dictionary name (via subscript): %@", mutableName);
        
        NSLog(@"All subscript forwarding tests completed successfully!");
        return 0;
    }
}