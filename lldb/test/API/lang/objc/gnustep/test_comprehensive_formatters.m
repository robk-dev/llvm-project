#import <Foundation/Foundation.h>

// Custom test class for formatter validation
@interface TestObject : NSObject {
    NSString *name;
    NSInteger value;
}
@property (nonatomic, retain) NSString *name;
@property (nonatomic, assign) NSInteger value;
@end

@implementation TestObject
@synthesize name, value;

- (NSString *)description {
    return [NSString stringWithFormat:@"TestObject(name=%@, value=%ld)", 
            self.name, (long)self.value];
}

- (void)dealloc {
    [name release];
    [super dealloc];
}
@end

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // ======================================
    // 1. STRING FORMATTERS - All Variants
    // ======================================
    
    // Tagged pointer string (short)
    NSString *taggedString = @"Hello";
    
    // Constant string
    NSString *constantString = @"This is a constant string literal";
    
    // Mutable string
    NSMutableString *mutableString = [NSMutableString stringWithString:@"Mutable"];
    [mutableString appendString:@" String"];
    
    // Empty string
    NSString *emptyString = @"";
    
    // Unicode string
    NSString *unicodeString = @"Hello 世界 🌍";
    
    // Very long string
    NSMutableString *longString = [NSMutableString string];
    for (int i = 0; i < 100; i++) {
        [longString appendFormat:@"Line %d ", i];
    }
    
    // ======================================
    // 2. NUMBER FORMATTERS - All Types
    // ======================================
    
    // Tagged pointer numbers
    NSNumber *taggedInt = @42;
    NSNumber *taggedBool = @YES;
    NSNumber *taggedFloat = @3.14f;
    
    // Regular numbers
    NSNumber *largeInt = @9999999999999LL;
    NSNumber *negativeNum = @-100;
    NSNumber *doubleNum = @3.141592653589793;
    
    // Special values
    NSNumber *zeroNum = @0;
    NSNumber *maxInt = @(NSIntegerMax);
    NSNumber *minInt = @(NSIntegerMin);
    
    // ======================================
    // 3. COLLECTION FORMATTERS - Basic
    // ======================================
    
    // Arrays
    NSArray *emptyArray = @[];
    NSArray *simpleArray = @[@"Apple", @"Banana", @"Cherry"];
    NSArray *numberArray = @[@1, @2, @3, @4, @5];
    NSArray *mixedArray = @[@"String", @42, @YES, [NSNull null]];
    
    NSMutableArray *mutableArray = [NSMutableArray arrayWithArray:simpleArray];
    [mutableArray addObject:@"Date"];
    
    // Dictionaries
    NSDictionary *emptyDict = @{};
    NSDictionary *simpleDict = @{
        @"name": @"John Doe",
        @"age": @30,
        @"active": @YES
    };
    
    NSMutableDictionary *mutableDict = [NSMutableDictionary dictionaryWithDictionary:simpleDict];
    [mutableDict setObject:@"Developer" forKey:@"role"];
    
    // Sets
    NSSet *emptySet = [NSSet set];
    NSSet *simpleSet = [NSSet setWithObjects:@"Red", @"Green", @"Blue", nil];
    NSSet *numberSet = [NSSet setWithObjects:@1, @2, @3, @4, @5, nil];
    
    NSMutableSet *mutableSet = [NSMutableSet setWithSet:simpleSet];
    [mutableSet addObject:@"Yellow"];
    
    // Ordered Sets
    NSOrderedSet *orderedSet = [NSOrderedSet orderedSetWithObjects:@"First", @"Second", @"Third", nil];
    NSMutableOrderedSet *mutableOrderedSet = [NSMutableOrderedSet orderedSetWithOrderedSet:orderedSet];
    [mutableOrderedSet addObject:@"Fourth"];
    
    // ======================================
    // 4. NESTED COLLECTIONS - Recursive Testing
    // ======================================
    
    // Array of arrays
    NSArray *arrayOfArrays = @[
        @[@"A1", @"A2", @"A3"],
        @[@"B1", @"B2", @"B3"],
        @[@"C1", @"C2", @"C3"]
    ];
    
    // Dictionary with array values
    NSDictionary *dictWithArrays = @{
        @"fruits": @[@"Apple", @"Banana", @"Cherry"],
        @"colors": @[@"Red", @"Green", @"Blue"],
        @"numbers": @[@1, @2, @3]
    };
    
    // Array of dictionaries
    NSArray *arrayOfDicts = @[
        @{@"name": @"Alice", @"age": @25},
        @{@"name": @"Bob", @"age": @30},
        @{@"name": @"Charlie", @"age": @35}
    ];
    
    // Deep nesting - 3+ levels
    NSDictionary *deeplyNested = @{
        @"level1": @{
            @"level2": @{
                @"level3": @{
                    @"data": @[@"Deep", @"Nested", @"Value"],
                    @"count": @3
                },
                @"items": @[@"Item1", @"Item2"]
            },
            @"array": @[
                @{@"id": @1, @"value": @"First"},
                @{@"id": @2, @"value": @"Second"}
            ]
        },
        @"metadata": @{
            @"version": @"1.0",
            @"timestamp": @123456789
        }
    };
    
    // Set containing collections
    NSSet *setOfCollections = [NSSet setWithObjects:
        @[@"Array1", @"Array2"],
        @{@"key": @"value"},
        [NSSet setWithObjects:@"Set1", @"Set2", nil],
        nil
    ];
    
    // ======================================
    // 5. FOUNDATION TYPES
    // ======================================
    
    // Date and Calendar
    NSDate *currentDate = [NSDate date];
    NSDate *pastDate = [NSDate dateWithTimeIntervalSince1970:0];
    NSDate *futureDate = [NSDate dateWithTimeIntervalSinceNow:86400];
    
    NSCalendar *calendar = [NSCalendar currentCalendar];
    NSDateComponents *dateComponents = [[NSDateComponents alloc] init];
    [dateComponents setYear:2025];
    [dateComponents setMonth:8];
    [dateComponents setDay:11];
    
    // URL and UUID
    NSURL *httpUrl = [NSURL URLWithString:@"https://www.example.com/path?query=value"];
    NSURL *fileUrl = [NSURL fileURLWithPath:@"/home/user/document.txt"];
    NSURL *malformedUrl = [NSURL URLWithString:@"not a valid url"];
    
    NSUUID *uuid1 = [NSUUID UUID];
    NSUUID *uuid2 = [[NSUUID alloc] initWithUUIDString:@"550e8400-e29b-41d4-a716-446655440000"];
    
    // Data
    NSData *emptyData = [NSData data];
    NSData *smallData = [@"Hello" dataUsingEncoding:NSUTF8StringEncoding];
    NSData *binaryData = [NSData dataWithBytes:"\x01\x02\x03\x04" length:4];
    
    NSMutableData *mutableData = [NSMutableData dataWithData:smallData];
    [mutableData appendBytes:" World" length:6];
    
    // Error and Exception
    NSError *simpleError = [NSError errorWithDomain:@"TestDomain" 
                                               code:404 
                                           userInfo:nil];
    
    NSError *detailedError = [NSError errorWithDomain:NSURLErrorDomain 
                                                 code:NSURLErrorNotConnectedToInternet
                                             userInfo:@{
        NSLocalizedDescriptionKey: @"Network connection failed",
        NSLocalizedFailureReasonErrorKey: @"No internet connection",
        NSURLErrorFailingURLErrorKey: httpUrl
    }];
    
    NSException *exception = [NSException exceptionWithName:@"TestException"
                                                     reason:@"This is a test exception"
                                                   userInfo:@{@"detail": @"Additional info"}];
    
    // Null object
    NSNull *nullObject = [NSNull null];
    
    // ======================================
    // 6. SPECIALIZED TYPES
    // ======================================
    
    // AttributedString
    NSMutableAttributedString *attrString = [[NSMutableAttributedString alloc] 
        initWithString:@"Formatted Text"];
    // Note: NSFontAttributeName not available in GNUstep base
    // Just use the string as-is for testing
    
    // IndexPath
    NSIndexPath *simpleIndexPath = [NSIndexPath indexPathWithIndex:0];
    NSIndexPath *complexIndexPath = [NSIndexPath indexPathWithIndex:1];
    complexIndexPath = [complexIndexPath indexPathByAddingIndex:2];
    complexIndexPath = [complexIndexPath indexPathByAddingIndex:3];
    
    // IndexSet
    NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];
    [indexSet addIndex:1];
    [indexSet addIndex:5];
    [indexSet addIndex:10];
    [indexSet addIndexesInRange:NSMakeRange(20, 5)];
    
    // Notification
    NSNotification *notification = [NSNotification notificationWithName:@"TestNotification"
                                                                 object:nil
                                                               userInfo:@{
        @"timestamp": currentDate,
        @"priority": @"high"
    }];
    
    // ProcessInfo
    NSProcessInfo *processInfo = [NSProcessInfo processInfo];
    
    // Locale
    NSLocale *currentLocale = [NSLocale currentLocale];
    NSLocale *usLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
    
    // Scanner
    NSScanner *scanner = [NSScanner scannerWithString:@"123 456 789"];
    
    // CharacterSet
    NSCharacterSet *alphaSet = [NSCharacterSet letterCharacterSet];
    NSCharacterSet *customSet = [NSCharacterSet characterSetWithCharactersInString:@"aeiou"];
    
    // UserDefaults
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:@"TestValue" forKey:@"TestKey"];
    [defaults setInteger:42 forKey:@"TestInt"];
    [defaults setBool:YES forKey:@"TestBool"];
    
    // ======================================
    // 7. CUSTOM OBJECTS
    // ======================================
    
    TestObject *customObj = [[TestObject alloc] init];
    customObj.name = @"TestName";
    customObj.value = 100;
    
    NSArray *customArray = @[customObj];
    NSDictionary *customDict = @{@"object": customObj};
    
    // ======================================
    // 8. EDGE CASES AND NIL VALUES
    // ======================================
    
    NSString *nilString = nil;
    NSArray *arrayWithNil = @[@"Valid", [NSNull null], @"Another"];
    NSDictionary *dictWithNil = @{
        @"valid": @"Value",
        @"null": [NSNull null]
    };
    
    // Circular reference (careful!)
    NSMutableArray *circularArray = [NSMutableArray arrayWithObject:@"Start"];
    NSMutableDictionary *circularDict = [NSMutableDictionary dictionaryWithObject:circularArray 
                                                                           forKey:@"array"];
    [circularArray addObject:circularDict];
    
    // ======================================
    // BREAKPOINT HERE FOR TESTING
    // ======================================
    
    printf("All test objects created successfully\n");
    printf("Set breakpoint on line %d to test formatters\n", __LINE__ + 1);
    printf("Ready for formatter testing...\n"); // BREAKPOINT LINE
    
    // Cleanup
    [customObj release];
    [attrString release];
    [dateComponents release];
    [uuid2 release];
    [usLocale release];
    
    [pool drain];
    return 0;
}