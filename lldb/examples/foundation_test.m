#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Test Foundation collection types
        
        // Array tests
        NSArray *immutableArray = @[@"Apple", @"Banana", @"Cherry", @"Date"];
        NSMutableArray *mutableArray = [NSMutableArray arrayWithArray:immutableArray];
        [mutableArray addObject:@"Elderberry"];
        
        // Dictionary tests
        NSDictionary *immutableDict = @{
            @"name": @"Foundation Test",
            @"version": @"1.0",
            @"features": @[@"arrays", @"dictionaries", @"strings"]
        };
        NSMutableDictionary *mutableDict = [NSMutableDictionary dictionaryWithDictionary:immutableDict];
        mutableDict[@"timestamp"] = [NSDate date];
        
        // String tests
        NSString *simpleString = @"Hello World";
        NSMutableString *mutableString = [NSMutableString stringWithString:simpleString];
        [mutableString appendString:@" - LLDB Bridge Test"];
        
        // Number tests
        NSNumber *intNumber = @42;
        NSNumber *floatNumber = @3.14159;
        NSNumber *boolNumber = @YES;
        
        // Nested structure test
        NSDictionary *nestedData = @{
            @"user": @{
                @"name": @"Test User",
                @"preferences": @{
                    @"theme": @"dark",
                    @"notifications": @YES,
                    @"recent_files": @[@"file1.txt", @"file2.txt", @"file3.txt"]
                }
            },
            @"statistics": @{
                @"files_processed": @156,
                @"success_rate": @0.95,
                @"last_run": [NSDate date]
            }
        };
        
        // Array of dictionaries
        NSArray *users = @[
            @{@"name": @"Alice", @"age": @25, @"role": @"developer"},
            @{@"name": @"Bob", @"age": @30, @"role": @"designer"},
            @{@"name": @"Charlie", @"age": @28, @"role": @"manager"}
        ];
        
        // Set breakpoint here to test Foundation type debugging
        NSLog(@"Foundation types debugging test");
        NSLog(@"Immutable array count: %lu", (unsigned long)immutableArray.count);
        NSLog(@"Mutable array count: %lu", (unsigned long)mutableArray.count);
        NSLog(@"Dictionary keys: %@", immutableDict.allKeys);
        NSLog(@"String length: %lu", (unsigned long)simpleString.length);
        NSLog(@"Numbers: int=%@, float=%@, bool=%@", intNumber, floatNumber, boolNumber);
        NSLog(@"Nested data: %@", nestedData);
        NSLog(@"Users: %@", users);
        
        return 0;
    }
}
