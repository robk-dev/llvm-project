//===-- test_jsonserialization.m ----------------------------------------===//
//
// Test program for NSJSONSerialization formatter validation
// Demonstrates various JSON serialization scenarios for LLDB testing
//
//===----------------------------------------------------------------------===//

#import <Foundation/Foundation.h>
#import <stdio.h>
#import <stdlib.h>

@interface JSONTestHelper : NSObject
+ (void)testBasicSerialization;
+ (void)testOptionsHandling;
+ (void)testErrorHandling;
+ (void)testLargeJSON;
+ (void)testFragments;
@end

@implementation JSONTestHelper

+ (void)testBasicSerialization {
    printf("\n=== Basic JSON Serialization Tests ===\n");
    
    // Test 1: Simple dictionary to JSON
    NSDictionary *dict = @{
        @"name": @"Test User",
        @"age": @25,
        @"active": @YES,
        @"score": @98.5
    };
    
    NSError *error = nil;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:dict 
                                                       options:0 
                                                         error:&error];
    
    printf("Dictionary JSON data: %p (length: %lu)\n", 
           (void*)jsonData, [jsonData length]);
    
    // Test 2: Array to JSON
    NSArray *array = @[@"apple", @"banana", @"cherry", @42, @YES];
    
    NSData *arrayJsonData = [NSJSONSerialization dataWithJSONObject:array 
                                                            options:NSJSONWritingPrettyPrinted 
                                                              error:&error];
    
    printf("Array JSON data: %p (length: %lu)\n", 
           (void*)arrayJsonData, [arrayJsonData length]);
    
    // Breakpoint here to test NSData containing JSON
    printf("Set breakpoint here to examine jsonData and arrayJsonData\n"); // BREAKPOINT 1
}

+ (void)testOptionsHandling {
    printf("\n=== JSON Options Tests ===\n");
    
    NSDictionary *testDict = @{
        @"zebra": @"last",
        @"apple": @"first",
        @"banana": @"middle"
    };
    
    // Test different writing options
    NSJSONWritingOptions options1 = 0; // No options
    NSJSONWritingOptions options2 = NSJSONWritingPrettyPrinted;
    NSJSONWritingOptions options3 = NSJSONWritingSortedKeys;
    NSJSONWritingOptions options4 = NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys;
    NSJSONWritingOptions options5 = 4; // NSJSONWritingFragmentsAllowed (may not be available in GNUstep)
    
    NSError *error = nil;
    
    NSData *json1 = [NSJSONSerialization dataWithJSONObject:testDict options:options1 error:&error];
    NSData *json2 = [NSJSONSerialization dataWithJSONObject:testDict options:options2 error:&error];
    NSData *json3 = [NSJSONSerialization dataWithJSONObject:testDict options:options3 error:&error];
    NSData *json4 = [NSJSONSerialization dataWithJSONObject:testDict options:options4 error:&error];
    NSData *json5 = [NSJSONSerialization dataWithJSONObject:testDict options:options5 error:&error];
    
    printf("Testing different options: %lu, %lu, %lu, %lu, %lu\n", 
           options1, options2, options3, options4, options5);
    
    // Test reading options
    NSJSONReadingOptions readOptions1 = 0; // No options  
    NSJSONReadingOptions readOptions2 = NSJSONReadingMutableContainers;
    NSJSONReadingOptions readOptions3 = NSJSONReadingMutableLeaves;
    NSJSONReadingOptions readOptions4 = 4; // NSJSONReadingFragmentsAllowed (may not be available in GNUstep)
    NSJSONReadingOptions readOptions5 = NSJSONReadingMutableContainers | NSJSONReadingMutableLeaves;
    
    printf("Testing read options: %lu, %lu, %lu, %lu, %lu\n",
           readOptions1, readOptions2, readOptions3, readOptions4, readOptions5);
    
    // Breakpoint here to test option values
    printf("Set breakpoint here to examine options values\n"); // BREAKPOINT 2
}

+ (void)testErrorHandling {
    printf("\n=== JSON Error Handling Tests ===\n");
    
    // Test 1: Invalid object for serialization
    NSObject *invalidObject = [[NSObject alloc] init];
    
    NSError *error1 = nil;
    NSData *result1 = [NSJSONSerialization dataWithJSONObject:invalidObject 
                                                      options:0 
                                                        error:&error1];
    
    printf("Invalid object serialization - result: %p, error: %p\n", 
           (void*)result1, (void*)error1);
    
    if (error1) {
        printf("Error domain: %s, code: %ld\n", 
               [[error1 domain] UTF8String], [error1 code]);
    }
    
    // Test 2: Invalid JSON data for parsing
    const char *invalidJSON = "{\"key\": invalid_value}";
    NSData *invalidData = [NSData dataWithBytes:invalidJSON length:strlen(invalidJSON)];
    
    NSError *error2 = nil;
    id result2 = [NSJSONSerialization JSONObjectWithData:invalidData 
                                                 options:0 
                                                   error:&error2];
    
    printf("Invalid JSON parsing - result: %p, error: %p\n", 
           (void*)result2, (void*)error2);
    
    if (error2) {
        printf("Parse error domain: %s, code: %ld\n", 
               [[error2 domain] UTF8String], [error2 code]);
    }
    
    // Breakpoint here to test error objects
    printf("Set breakpoint here to examine error1 and error2\n"); // BREAKPOINT 3
}

+ (void)testLargeJSON {
    printf("\n=== Large JSON Tests ===\n");
    
    // Create a large dictionary
    NSMutableDictionary *largeDict = [[NSMutableDictionary alloc] init];
    
    for (int i = 0; i < 1000; ++i) {
        NSString *key = [NSString stringWithFormat:@"key_%04d", i];
        NSString *value = [NSString stringWithFormat:@"value_for_key_%04d_with_extra_text", i];
        [largeDict setObject:value forKey:key];
    }
    
    NSError *error = nil;
    NSData *largeJsonData = [NSJSONSerialization dataWithJSONObject:largeDict 
                                                            options:NSJSONWritingPrettyPrinted 
                                                              error:&error];
    
    printf("Large JSON data: %p (length: %lu)\n", 
           (void*)largeJsonData, [largeJsonData length]);
    
    // Test parsing it back
    id parsedObject = [NSJSONSerialization JSONObjectWithData:largeJsonData 
                                                      options:NSJSONReadingMutableContainers 
                                                        error:&error];
    
    printf("Parsed large JSON: %p\n", (void*)parsedObject);
    
    // Breakpoint here to test large data handling
    printf("Set breakpoint here to examine largeJsonData\n"); // BREAKPOINT 4
}

+ (void)testFragments {
    printf("\n=== JSON Fragments Tests ===\n");
    
    // Test fragment serialization (requires NSJSONWritingFragmentsAllowed)
    NSString *stringFragment = @"Hello, JSON World!";
    NSNumber *numberFragment = @42.5;
    NSNumber *boolFragment = @NO;
    NSNull *nullFragment = [NSNull null];
    
    NSError *error = nil;
    
    // These should fail without NSJSONWritingFragmentsAllowed
    NSData *stringData1 = [NSJSONSerialization dataWithJSONObject:stringFragment 
                                                          options:0 
                                                            error:&error];
    printf("String fragment (no options): %p, error: %p\n", (void*)stringData1, (void*)error);
    
    error = nil; // Reset error
    
    // These should succeed with NSJSONWritingFragmentsAllowed  
    NSData *stringData2 = [NSJSONSerialization dataWithJSONObject:stringFragment 
                                                          options:4  // NSJSONWritingFragmentsAllowed
                                                            error:&error];
    printf("String fragment (with fragments): %p, error: %p\n", (void*)stringData2, (void*)error);
    
    error = nil;
    NSData *numberData = [NSJSONSerialization dataWithJSONObject:numberFragment 
                                                         options:4  // NSJSONWritingFragmentsAllowed
                                                           error:&error];
    printf("Number fragment: %p, error: %p\n", (void*)numberData, (void*)error);
    
    error = nil;
    NSData *boolData = [NSJSONSerialization dataWithJSONObject:boolFragment 
                                                        options:4  // NSJSONWritingFragmentsAllowed
                                                          error:&error];
    printf("Bool fragment: %p, error: %p\n", (void*)boolData, (void*)error);
    
    error = nil;
    NSData *nullData = [NSJSONSerialization dataWithJSONObject:nullFragment 
                                                        options:4  // NSJSONWritingFragmentsAllowed
                                                          error:&error];
    printf("Null fragment: %p, error: %p\n", (void*)nullData, (void*)error);
    
    // Breakpoint here to test fragments
    printf("Set breakpoint here to examine fragment data\n"); // BREAKPOINT 5
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("NSJSONSerialization Formatter Test Program\n");
        printf("==========================================\n");
        
        // Test static class access
        Class jsonClass = [NSJSONSerialization class];
        printf("NSJSONSerialization class: %p\n", (void*)jsonClass);
        
        // Test if object validation works
        NSDictionary *validDict = @{@"test": @"value"};
        BOOL isValid = [NSJSONSerialization isValidJSONObject:validDict];
        printf("Valid dictionary check: %s\n", isValid ? "YES" : "NO");
        
        // Run comprehensive tests
        [JSONTestHelper testBasicSerialization];
        [JSONTestHelper testOptionsHandling];
        [JSONTestHelper testErrorHandling];
        [JSONTestHelper testLargeJSON];
        [JSONTestHelper testFragments];
        
        printf("\n=== Final Test - Class Object ===\n");
        // This gives us the class object to test our formatter
        printf("Set final breakpoint to examine jsonClass\n"); // BREAKPOINT 6
        
        printf("\nTest program completed. Check LLDB formatter output.\n");
    }
    return 0;
}