#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Test various string lengths to trigger GSCInlineString
        // GNUstep uses inline storage for small strings
        
        // Very short strings (likely to be inline)
        NSMutableString *short1 = [NSMutableString stringWithString:@"Hi"];
        NSMutableString *short2 = [NSMutableString stringWithString:@"Test"];
        NSMutableString *short3 = [NSMutableString stringWithString:@"ABC"];
        
        // Medium strings (may or may not be inline)
        NSMutableString *medium1 = [NSMutableString stringWithString:@"Hello World"];
        NSMutableString *medium2 = [NSMutableString stringWithString:@"Testing 123"];
        
        // Test in array to see expansion behavior
        NSMutableArray *growingArray = [NSMutableArray array];
        
        // Add various string types
        [growingArray addObject:@"Constant"];  // NSConstantString
        [growingArray addObject:short1];       // Likely GSCInlineString
        [growingArray addObject:short2];       // Likely GSCInlineString
        [growingArray addObject:short3];       // Likely GSCInlineString
        [growingArray addObject:medium1];      // Maybe GSCInlineString
        [growingArray addObject:medium2];      // Maybe GSCInlineString
        
        // Create strings that might trigger inline storage
        for (int i = 0; i < 5; i++) {
            NSMutableString *str = [NSMutableString stringWithFormat:@"S%d", i];
            [growingArray addObject:str];
        }
        
        // Test with Unicode to see if that affects inline storage
        NSMutableString *unicode1 = [NSMutableString stringWithString:@"😀"];
        NSMutableString *unicode2 = [NSMutableString stringWithString:@"Hello 世界"];
        [growingArray addObject:unicode1];
        [growingArray addObject:unicode2];
        
        NSLog(@"Array has %lu strings", (unsigned long)[growingArray count]);
        NSLog(@"First string: %@", growingArray[0]);
        NSLog(@"Short string: %@", short1);
        
        // Print class names to verify what we're dealing with
        for (id obj in growingArray) {
            NSLog(@"Class: %@ Value: %@", NSStringFromClass([obj class]), obj);
        }
        
        NSLog(@"=== Breakpoint here to examine strings ===");
        
        // Test modifying inline strings
        [short1 appendString:@"!"];
        [short2 appendString:@" More"];
        
        NSLog(@"Modified short1: %@", short1);
        NSLog(@"Modified short2: %@", short2);
        
        NSLog(@"=== Test complete ===");
        return 0;
    }
}