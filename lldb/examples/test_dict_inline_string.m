//===-- test_dict_inline_string.m - Test dictionary with GSCInlineString ---===//

#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== Testing Dictionary with Inline Strings ===\n");
        
        // Create keys that will be GSCInlineString (dynamic creation)
        NSString *key1 = [NSString stringWithFormat:@"key%d", 1];
        NSString *key2 = [@"key" stringByAppendingString:@"2"];
        NSString *key3 = [[NSString alloc] initWithUTF8String:"key3"];
        
        // Create values  
        NSString *val1 = @"value1";
        NSString *val2 = @"value2";
        NSString *val3 = @"value3";
        
        // Create dictionary with inline string keys
        NSDictionary *dict = @{
            key1: val1,
            key2: val2,
            key3: val3
        };
        
        printf("Dictionary created with %lu entries\n", (unsigned long)[dict count]);
        
        // Print the dictionary 
        NSLog(@"Dictionary: %@", dict);
        
        // Print each key-value pair
        for (NSString *key in dict) {
            NSString *value = dict[key];
            printf("Key: %s (class: %s), Value: %s\n", 
                   [key UTF8String],
                   object_getClassName(key),
                   [value UTF8String]);
        }
        
        // BREAKPOINT HERE - Line 38
        printf("Set breakpoint at line 38 to inspect dict\n");
        
        return 0;
    }
}