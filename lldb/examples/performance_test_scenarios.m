//===-- performance_test_scenarios.m ------------------------------------===//
// Performance test scenarios for GNUstep formatters
// Designed to stress test formatter performance with various collection sizes
//===----------------------------------------------------------------------===//

#import <Foundation/Foundation.h>

// Test scenario constants
#define SMALL_COLLECTION_SIZE 5
#define MEDIUM_COLLECTION_SIZE 50
#define LARGE_COLLECTION_SIZE 500
#define VERY_LARGE_COLLECTION_SIZE 5000

void performance_test_array_scenarios() {
    printf("=== Array Performance Test Scenarios ===\n");
    
    // Small array with strings
    NSMutableArray *smallStringArray = [NSMutableArray array];
    for (int i = 0; i < SMALL_COLLECTION_SIZE; i++) {
        [smallStringArray addObject:[NSString stringWithFormat:@"String_%d", i]];
    }
    
    // Medium array with mixed types
    NSMutableArray *mediumMixedArray = [NSMutableArray array];
    for (int i = 0; i < MEDIUM_COLLECTION_SIZE; i++) {
        if (i % 3 == 0) {
            [mediumMixedArray addObject:@(i)];
        } else if (i % 3 == 1) {
            [mediumMixedArray addObject:[NSString stringWithFormat:@"Item_%d", i]];
        } else {
            [mediumMixedArray addObject:@[@(i), @(i*2)]];
        }
    }
    
    // Large array with numbers (heavy on tagged pointer processing)
    NSMutableArray *largeNumberArray = [NSMutableArray array];
    for (int i = 0; i < LARGE_COLLECTION_SIZE; i++) {
        [largeNumberArray addObject:@(i)];
    }
    
    // Nested array scenario (stress test recursion handling)
    NSMutableArray *nestedArray = [NSMutableArray array];
    for (int i = 0; i < 10; i++) {
        NSMutableArray *subArray = [NSMutableArray array];
        for (int j = 0; j < 20; j++) {
            [subArray addObject:[NSString stringWithFormat:@"Nested_%d_%d", i, j]];
        }
        [nestedArray addObject:subArray];
    }
    
    // Very large array (test performance limits)
    NSMutableArray *veryLargeArray = [NSMutableArray arrayWithCapacity:VERY_LARGE_COLLECTION_SIZE];
    for (int i = 0; i < VERY_LARGE_COLLECTION_SIZE; i++) {
        [veryLargeArray addObject:@(i)];
    }
    
    printf("Arrays created for performance testing\n");
    // Breakpoint marker for performance testing
    printf("BREAKPOINT: Array performance test ready\n");
}

void performance_test_dictionary_scenarios() {
    printf("=== Dictionary Performance Test Scenarios ===\n");
    
    // Small dictionary with string keys
    NSMutableDictionary *smallDict = [NSMutableDictionary dictionary];
    for (int i = 0; i < SMALL_COLLECTION_SIZE; i++) {
        NSString *key = [NSString stringWithFormat:@"key_%d", i];
        NSString *value = [NSString stringWithFormat:@"value_%d", i];
        [smallDict setObject:value forKey:key];
    }
    
    // Medium dictionary with mixed key-value types
    NSMutableDictionary *mediumDict = [NSMutableDictionary dictionary];
    for (int i = 0; i < MEDIUM_COLLECTION_SIZE; i++) {
        NSString *key = [NSString stringWithFormat:@"key_%d", i];
        id value;
        if (i % 4 == 0) {
            value = @(i);
        } else if (i % 4 == 1) {
            value = [NSString stringWithFormat:@"value_%d", i];
        } else if (i % 4 == 2) {
            value = @[@(i), @(i*2), @(i*3)];
        } else {
            value = @{@"nested_key": @(i)};
        }
        [mediumDict setObject:value forKey:key];
    }
    
    // Large dictionary with numeric values
    NSMutableDictionary *largeDict = [NSMutableDictionary dictionaryWithCapacity:LARGE_COLLECTION_SIZE];
    for (int i = 0; i < LARGE_COLLECTION_SIZE; i++) {
        NSString *key = [NSString stringWithFormat:@"item_%d", i];
        [largeDict setObject:@(i * 1.5) forKey:key];
    }
    
    // Deeply nested dictionary scenario
    NSMutableDictionary *nestedDict = [NSMutableDictionary dictionary];
    for (int i = 0; i < 5; i++) {
        NSMutableDictionary *subDict = [NSMutableDictionary dictionary];
        for (int j = 0; j < 10; j++) {
            NSString *subKey = [NSString stringWithFormat:@"sub_%d", j];
            NSArray *subValue = @[@(i), @(j), [NSString stringWithFormat:@"nested_%d_%d", i, j]];
            [subDict setObject:subValue forKey:subKey];
        }
        NSString *mainKey = [NSString stringWithFormat:@"level_%d", i];
        [nestedDict setObject:subDict forKey:mainKey];
    }
    
    printf("Dictionaries created for performance testing\n");
    printf("BREAKPOINT: Dictionary performance test ready\n");
}

void performance_test_string_scenarios() {
    printf("=== String Performance Test Scenarios ===\n");
    
    // Short strings (likely tagged pointers)
    NSArray *shortStrings = @[@"a", @"hello", @"world", @"test"];
    
    // Medium length strings
    NSMutableArray *mediumStrings = [NSMutableArray array];
    for (int i = 0; i < 20; i++) {
        NSString *str = [NSString stringWithFormat:@"This is a medium length string number %d with some content", i];
        [mediumStrings addObject:str];
    }
    
    // Long strings
    NSMutableArray *longStrings = [NSMutableArray array];
    for (int i = 0; i < 10; i++) {
        NSMutableString *longStr = [NSMutableString string];
        for (int j = 0; j < 100; j++) {
            [longStr appendFormat:@"Part %d of long string %d. ", j, i];
        }
        [longStrings addObject:[longStr copy]];
    }
    
    // Strings with Unicode content
    NSArray *unicodeStrings = @[
        @"Hello 世界",
        @"Café ☕️",
        @"🚀 Rocket",
        @"Ελληνικά",
        @"العربية"
    ];
    
    printf("String scenarios created for performance testing\n");
    printf("BREAKPOINT: String performance test ready\n");
}

void performance_test_mixed_collections() {
    printf("=== Mixed Collection Performance Test ===\n");
    
    // Create a complex data structure that combines all types
    NSMutableDictionary *complexData = [NSMutableDictionary dictionary];
    
    // Add array of dictionaries
    NSMutableArray *arrayOfDicts = [NSMutableArray array];
    for (int i = 0; i < 20; i++) {
        NSDictionary *dict = @{
            @"id": @(i),
            @"name": [NSString stringWithFormat:@"Item_%d", i],
            @"values": @[@(i*2), @(i*3), @(i*4)],
            @"metadata": @{@"created": [NSDate date], @"active": @(i % 2 == 0)}
        };
        [arrayOfDicts addObject:dict];
    }
    [complexData setObject:arrayOfDicts forKey:@"items"];
    
    // Add dictionary of arrays
    NSMutableDictionary *dictOfArrays = [NSMutableDictionary dictionary];
    for (int i = 0; i < 10; i++) {
        NSString *key = [NSString stringWithFormat:@"category_%d", i];
        NSMutableArray *array = [NSMutableArray array];
        for (int j = 0; j < 15; j++) {
            [array addObject:@(i * 10 + j)];
        }
        [dictOfArrays setObject:array forKey:key];
    }
    [complexData setObject:dictOfArrays forKey:@"categories"];
    
    // Add deeply nested structure
    NSMutableDictionary *deepNesting = [NSMutableDictionary dictionary];
    NSMutableDictionary *currentLevel = deepNesting;
    for (int i = 0; i < 8; i++) {
        NSString *key = [NSString stringWithFormat:@"level_%d", i];
        NSMutableDictionary *nextLevel = [NSMutableDictionary dictionary];
        [nextLevel setObject:@(i) forKey:@"value"];
        [nextLevel setObject:@[@(i), @(i*2), @(i*3)] forKey:@"array"];
        [currentLevel setObject:nextLevel forKey:key];
        currentLevel = nextLevel;
    }
    [complexData setObject:deepNesting forKey:@"deep_structure"];
    
    printf("Complex mixed collection created for performance testing\n");
    printf("BREAKPOINT: Mixed collection performance test ready\n");
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("Starting GNUstep Formatter Performance Tests\n");
        
        performance_test_array_scenarios();
        performance_test_dictionary_scenarios();
        performance_test_string_scenarios();
        performance_test_mixed_collections();
        
        printf("\nAll performance test scenarios ready\n");
        printf("Use LLDB breakpoints at each 'BREAKPOINT:' marker to test formatter performance\n");
        
        // Keep all objects alive
        while (1) {
            sleep(1);
        }
    }
    return 0;
}