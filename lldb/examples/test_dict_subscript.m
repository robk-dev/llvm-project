#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Create a test dictionary
        NSDictionary *testDict = @{
            @"fruit": @"Apple",
            @"vegetable": @"Carrot",
            @"number": @42
        };
        
        // Create a test array for comparison
        NSArray *testArray = @[@"First", @"Second", @"Third"];
        
        // Test point for debugging
        NSLog(@"Dictionary: %@", testDict);
        NSLog(@"Array: %@", testArray);
        
        // Try subscript access (for debugging)
        id fruit = [testDict objectForKey:@"fruit"];
        id firstItem = [testArray objectAtIndex:0];
        
        NSLog(@"Fruit via objectForKey: %@", fruit);
        NSLog(@"First item via objectAtIndex: %@", firstItem);
        
        // These lines would use subscript syntax if uncommented:
        // id fruitSubscript = testDict[@"fruit"];
        // id firstSubscript = testArray[0];
        
        return 0; // Breakpoint here
    }
}