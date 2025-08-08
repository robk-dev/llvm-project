#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test 1: Array with regular string objects
        NSArray *fruits = @[@"Apple", @"Banana", @"Cherry", @"Date", @"Elderberry"];
        NSLog(@"Fruits array: %@", fruits);
        
        // Test 2: Array with mixed content
        NSArray *mixed = @[@"String", @42, @3.14, @YES, @"Another String"];
        NSLog(@"Mixed array: %@", mixed);
        
        // Test 3: Array with short strings (might be tagged)
        NSArray *shortStrings = @[@"A", @"B", @"C", @"D", @"E"];
        NSLog(@"Short strings: %@", shortStrings);
        
        // Test 4: Nested arrays
        NSArray *nested = @[@"First", @[@"Nested1", @"Nested2"], @"Last"];
        NSLog(@"Nested array: %@", nested);
        
        // Set a breakpoint here to examine arrays in LLDB
        NSLog(@"Setting breakpoint location...");
        
        // Test individual element access
        for (NSUInteger i = 0; i < fruits.count; i++) {
            NSString *fruit = fruits[i];
            NSLog(@"Fruit[%lu]: %@", (unsigned long)i, fruit);
        }
        
        return 0;
    }
}