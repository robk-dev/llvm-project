//===-- foundation_test_simple.m ----------------------------------------===//
//
// COMPREHENSIVE FOUNDATION FORMATTER TEST SUITE
// Simplified version without complex macros for reliable compilation
//
//===----------------------------------------------------------------------===//

#import <Foundation/Foundation.h>
#include <stdio.h>
#include <sys/time.h>

// Custom test class for validation
@interface TestObject : NSObject
@property (nonatomic, strong) NSString *name;
@property (nonatomic, assign) NSInteger value;
@end

@implementation TestObject
- (NSString *)description {
    return [NSString stringWithFormat:@"TestObject(name=%@, value=%ld)", self.name, (long)self.value];
}
@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        printf("=== COMPREHENSIVE FOUNDATION FORMATTER TEST SUITE ===\n\n");
        
        //===----------------------------------------------------------------------===//
        // STRING FORMATTERS
        //===----------------------------------------------------------------------===//
        printf("=== STRING FORMATTERS ===\n");
        
        NSString *emptyString = @"";
        NSString *shortString = @"Hello";
        NSString *longString = [@"" stringByPaddingToLength:100 withString:@"Lorem ipsum dolor sit amet " startingAtIndex:0];
        NSString *unicodeString = @"Hello 世界 🌍 ñoño";
        NSMutableString *mutableString = [NSMutableString stringWithString:@"Mutable"];
        NSString *nilString = nil;
        
        printf("Empty string: %p\n", (void*)emptyString);
        printf("Short string: %p\n", (void*)shortString);
        printf("Long string: %p\n", (void*)longString);
        printf("Unicode string: %p\n", (void*)unicodeString);
        printf("Mutable string: %p\n", (void*)mutableString);
        printf("Nil string: %p\n", (void*)nilString);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // NUMBER FORMATTERS
        //===----------------------------------------------------------------------===//
        printf("=== NUMBER FORMATTERS ===\n");
        
        NSNumber *zeroNumber = @0;
        NSNumber *smallInt = @42;                    // Likely tagged pointer
        NSNumber *largeInt = @999999999999999LL;     // Regular object
        NSNumber *floatNumber = @3.14159f;
        NSNumber *doubleNumber = @2.718281828459045;
        NSNumber *boolYES = @YES;
        NSNumber *boolNO = @NO;
        NSNumber *negativeNumber = @(-42);
        NSNumber *nilNumber = nil;
        
        printf("Zero: %p\n", (void*)zeroNumber);
        printf("Small int (tagged): %p\n", (void*)smallInt);
        printf("Large int: %p\n", (void*)largeInt);
        printf("Float: %p\n", (void*)floatNumber);
        printf("Double: %p\n", (void*)doubleNumber);
        printf("Bool YES: %p\n", (void*)boolYES);
        printf("Bool NO: %p\n", (void*)boolNO);
        printf("Negative: %p\n", (void*)negativeNumber);
        printf("Nil number: %p\n", (void*)nilNumber);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // ARRAY FORMATTERS
        //===----------------------------------------------------------------------===//
        printf("=== ARRAY FORMATTERS ===\n");
        
        NSArray *emptyArray = @[];
        NSArray *singleElementArray = @[@"Solo"];
        NSArray *smallArray = @[@"First", @"Second", @"Third"];
        NSArray *mixedArray = @[@"String", @42, @3.14, @YES];
        NSArray *nestedArray = @[@"Outer", @[@"Inner1", @"Inner2"], @"AfterNested"];
        NSMutableArray *mutableArray = [NSMutableArray arrayWithObjects:@"Mutable1", @"Mutable2", nil];
        NSArray *nilArray = nil;
        
        printf("Empty array: %p\n", (void*)emptyArray);
        printf("Single element: %p\n", (void*)singleElementArray);
        printf("Small array: %p\n", (void*)smallArray);
        printf("Mixed array: %p\n", (void*)mixedArray);
        printf("Nested array: %p\n", (void*)nestedArray);
        printf("Mutable array: %p\n", (void*)mutableArray);
        printf("Nil array: %p\n", (void*)nilArray);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // DICTIONARY FORMATTERS
        //===----------------------------------------------------------------------===//
        printf("=== DICTIONARY FORMATTERS ===\n");
        
        NSDictionary *emptyDict = @{};
        NSDictionary *singlePairDict = @{@"key": @"value"};
        NSDictionary *smallDict = @{@"name": @"John", @"age": @30, @"active": @YES};
        NSDictionary *nestedDict = @{@"outer": @{@"inner": @"nested"}, @"simple": @"value"};
        NSMutableDictionary *mutableDict = [NSMutableDictionary dictionaryWithDictionary:smallDict];
        NSDictionary *nilDict = nil;
        
        printf("Empty dict: %p\n", (void*)emptyDict);
        printf("Single pair: %p\n", (void*)singlePairDict);
        printf("Small dict: %p\n", (void*)smallDict);
        printf("Nested dict: %p\n", (void*)nestedDict);
        printf("Mutable dict: %p\n", (void*)mutableDict);
        printf("Nil dict: %p\n", (void*)nilDict);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // SET FORMATTERS
        //===----------------------------------------------------------------------===//
        printf("=== SET FORMATTERS ===\n");
        
        NSSet *emptySet = [NSSet set];
        NSSet *singleElementSet = [NSSet setWithObject:@"Solo"];
        NSSet *smallSet = [NSSet setWithObjects:@"Alpha", @"Beta", @"Gamma", nil];
        NSSet *mixedSet = [NSSet setWithObjects:@"String", @42, @3.14, @YES, nil];
        NSMutableSet *mutableSet = [NSMutableSet setWithObjects:@"Mutable1", @"Mutable2", nil];
        NSSet *nilSet = nil;
        
        printf("Empty set: %p\n", (void*)emptySet);
        printf("Single element set: %p\n", (void*)singleElementSet);
        printf("Small set: %p\n", (void*)smallSet);
        printf("Mixed set: %p\n", (void*)mixedSet);
        printf("Mutable set: %p\n", (void*)mutableSet);
        printf("Nil set: %p\n", (void*)nilSet);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // DATE FORMATTERS
        //===----------------------------------------------------------------------===//
        printf("=== DATE FORMATTERS ===\n");
        
        NSDate *now = [NSDate date];
        NSDate *pastDate = [NSDate dateWithTimeIntervalSince1970:0];
        NSDate *futureDate = [NSDate dateWithTimeIntervalSinceNow:86400];
        NSDate *nilDate = nil;
        
        printf("Current date: %p\n", (void*)now);
        printf("Unix epoch: %p\n", (void*)pastDate);
        printf("Tomorrow: %p\n", (void*)futureDate);
        printf("Nil date: %p\n", (void*)nilDate);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // DATA FORMATTERS
        //===----------------------------------------------------------------------===//
        printf("=== DATA FORMATTERS ===\n");
        
        NSData *emptyData = [NSData data];
        NSData *smallData = [@"Hello" dataUsingEncoding:NSUTF8StringEncoding];
        unsigned char binaryBytes[] = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
        NSData *binaryData = [NSData dataWithBytes:binaryBytes length:6];
        NSMutableData *mutableData = [NSMutableData dataWithData:smallData];
        NSData *nilData = nil;
        
        printf("Empty data: %p\n", (void*)emptyData);
        printf("Small data: %p\n", (void*)smallData);
        printf("Binary data: %p\n", (void*)binaryData);
        printf("Mutable data: %p\n", (void*)mutableData);
        printf("Nil data: %p\n", (void*)nilData);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // UUID FORMATTERS
        //===----------------------------------------------------------------------===//
        printf("=== UUID FORMATTERS ===\n");
        
        NSUUID *randomUUID = [[NSUUID alloc] init];
        NSUUID *zeroUUID = [[NSUUID alloc] initWithUUIDString:@"00000000-0000-0000-0000-000000000000"];
        NSUUID *nilUUID = nil;
        
        printf("Random UUID: %p\n", (void*)randomUUID);
        printf("Zero UUID: %p\n", (void*)zeroUUID);
        printf("Nil UUID: %p\n", (void*)nilUUID);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // URL FORMATTERS
        //===----------------------------------------------------------------------===//
        printf("=== URL FORMATTERS ===\n");
        
        NSURL *httpURL = [NSURL URLWithString:@"https://www.example.com/path?query=value#fragment"];
        NSURL *fileURL = [NSURL fileURLWithPath:@"/usr/local/bin/lldb"];
        NSURL *nilURL = nil;
        
        printf("HTTP URL: %p\n", (void*)httpURL);
        printf("File URL: %p\n", (void*)fileURL);
        printf("Nil URL: %p\n", (void*)nilURL);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // ERROR FORMATTERS
        //===----------------------------------------------------------------------===//
        printf("=== ERROR FORMATTERS ===\n");
        
        NSError *fileError = [NSError errorWithDomain:NSCocoaErrorDomain 
                                                 code:NSFileReadNoSuchFileError 
                                             userInfo:@{NSLocalizedDescriptionKey: @"File not found"}];
        NSError *customError = [NSError errorWithDomain:@"com.test.domain" 
                                                    code:1001 
                                                userInfo:nil];
        NSError *nilError = nil;
        
        printf("File error: %p\n", (void*)fileError);
        printf("Custom error: %p\n", (void*)customError);
        printf("Nil error: %p\n", (void*)nilError);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // INDEX SET FORMATTERS
        //===----------------------------------------------------------------------===//
        printf("=== INDEX SET FORMATTERS ===\n");
        
        NSIndexSet *emptyIndexSet = [NSIndexSet indexSet];
        NSIndexSet *singleIndex = [NSIndexSet indexSetWithIndex:5];
        NSIndexSet *rangeIndexSet = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(10, 5)];
        NSMutableIndexSet *mutableIndexSet = [NSMutableIndexSet indexSet];
        [mutableIndexSet addIndex:1];
        [mutableIndexSet addIndex:3];
        [mutableIndexSet addIndex:5];
        NSIndexSet *nilIndexSet = nil;
        
        printf("Empty index set: %p\n", (void*)emptyIndexSet);
        printf("Single index: %p\n", (void*)singleIndex);
        printf("Range indexes: %p\n", (void*)rangeIndexSet);
        printf("Mutable indexes: %p\n", (void*)mutableIndexSet);
        printf("Nil index set: %p\n", (void*)nilIndexSet);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // DECIMAL NUMBER FORMATTERS
        //===----------------------------------------------------------------------===//
        printf("=== DECIMAL NUMBER FORMATTERS ===\n");
        
        NSDecimalNumber *zero = [NSDecimalNumber zero];
        NSDecimalNumber *one = [NSDecimalNumber one];
        NSDecimalNumber *pi = [NSDecimalNumber decimalNumberWithString:@"3.141592653589793"];
        NSDecimalNumber *negative = [NSDecimalNumber decimalNumberWithString:@"-42.5"];
        NSDecimalNumber *nilDecimal = nil;
        
        printf("Zero decimal: %p\n", (void*)zero);
        printf("One decimal: %p\n", (void*)one);
        printf("Pi decimal: %p\n", (void*)pi);
        printf("Negative decimal: %p\n", (void*)negative);
        printf("Nil decimal: %p\n", (void*)nilDecimal);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // CHARACTER SET FORMATTERS
        //===----------------------------------------------------------------------===//
        printf("=== CHARACTER SET FORMATTERS ===\n");
        
        NSCharacterSet *letterCharSet = [NSCharacterSet letterCharacterSet];
        NSCharacterSet *digitCharSet = [NSCharacterSet decimalDigitCharacterSet];
        NSCharacterSet *customCharSet = [NSCharacterSet characterSetWithCharactersInString:@"abc123"];
        NSMutableCharacterSet *mutableCharSet = [NSMutableCharacterSet characterSetWithCharactersInString:@"xyz"];
        NSCharacterSet *nilCharSet = nil;
        
        printf("Letter char set: %p\n", (void*)letterCharSet);
        printf("Digit char set: %p\n", (void*)digitCharSet);
        printf("Custom char set: %p\n", (void*)customCharSet);
        printf("Mutable char set: %p\n", (void*)mutableCharSet);
        printf("Nil char set: %p\n", (void*)nilCharSet);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // CUSTOM OBJECT TEST
        //===----------------------------------------------------------------------===//
        printf("=== CUSTOM OBJECT TEST ===\n");
        
        TestObject *testObj = [[TestObject alloc] init];
        testObj.name = @"TestInstance";
        testObj.value = 42;
        
        printf("Custom object: %p\n", (void*)testObj);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // LARGE COLLECTION TESTS
        //===----------------------------------------------------------------------===//
        printf("=== LARGE COLLECTION TESTS ===\n");
        
        NSMutableArray *largeArray = [NSMutableArray array];
        for (int i = 0; i < 100; i++) {
            [largeArray addObject:[NSString stringWithFormat:@"Item-%d", i]];
        }
        printf("Large array (100 items): %p\n", (void*)largeArray);
        
        NSMutableDictionary *largeDict = [NSMutableDictionary dictionary];
        for (int i = 0; i < 50; i++) {
            [largeDict setObject:[NSString stringWithFormat:@"Value-%d", i]
                          forKey:[NSString stringWithFormat:@"Key-%d", i]];
        }
        printf("Large dict (50 pairs): %p\n", (void*)largeDict);
        
        NSMutableSet *largeSet = [NSMutableSet set];
        for (int i = 0; i < 75; i++) {
            [largeSet addObject:[NSString stringWithFormat:@"SetItem-%d", i]];
        }
        printf("Large set (75 objects): %p\n", (void*)largeSet);
        printf("\n");
        
        //===----------------------------------------------------------------------===//
        // NESTED COLLECTIONS TEST
        //===----------------------------------------------------------------------===//
        printf("=== NESTED COLLECTIONS TEST ===\n");
        
        NSDictionary *deeplyNested = @{
            @"level1": @{
                @"level2": @{
                    @"level3": @[@"deep", @"array", @"items"]
                }
            },
            @"arrays": @[
                @[@"nested", @"array", @1],
                @[@"another", @"nested", @2]
            ],
            @"sets": @{
                @"set1": [NSSet setWithObjects:@"a", @"b", @"c", nil],
                @"set2": [NSSet setWithObjects:@1, @2, @3, nil]
            }
        };
        printf("Deeply nested structure: %p\n", (void*)deeplyNested);
        printf("\n");
        
        printf("=== FINAL BREAKPOINT LOCATION ===\n");
        printf("Set breakpoint at line %d for comprehensive testing\n", __LINE__ + 2);
        printf("Test all objects above with LLDB formatters\n");
        printf("Breakpoint location for comprehensive validation\n"); // BREAKPOINT HERE
        
        printf("\n=== TEST SUITE COMPLETE ===\n");
        printf("All Foundation formatter types tested\n");
        printf("Ready for production validation\n");
        
        return 0;
    }
}