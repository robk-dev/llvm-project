#import <Foundation/Foundation.h>
#include <stdio.h>

// Custom exception for testing
@interface TestException : NSException
+ (instancetype)exceptionWithReason:(NSString *)reason;
@end

@implementation TestException
+ (instancetype)exceptionWithReason:(NSString *)reason {
    return [TestException exceptionWithName:@"TestException" 
                                    reason:reason 
                                  userInfo:nil];
}
@end

// Test class that throws exceptions
@interface ExceptionTestClass : NSObject
- (void)methodThatThrows;
- (void)methodThatThrowsCustom;
- (void)methodThatCatches;
- (void)nestedExceptionMethod:(int)level;
@end

@implementation ExceptionTestClass

- (void)methodThatThrows {
    printf("ExceptionTestClass: About to throw NSException\n");
    @throw [NSException exceptionWithName:@"TestException" 
                                   reason:@"This is a test exception" 
                                 userInfo:nil];
}

- (void)methodThatThrowsCustom {
    printf("ExceptionTestClass: About to throw custom exception\n");
    @throw [TestException exceptionWithReason:@"Custom test exception"];
}

- (void)methodThatCatches {
    printf("ExceptionTestClass: Method with try-catch block\n");
    @try {
        [self methodThatThrows];
    } @catch (NSException *exception) {
        printf("Caught exception: %s - %s\n", 
               [exception.name UTF8String], 
               [exception.reason UTF8String]);
    }
    printf("ExceptionTestClass: After try-catch block\n");
}

- (void)nestedExceptionMethod:(int)level {
    printf("ExceptionTestClass: nestedExceptionMethod level %d\n", level);
    if (level <= 0) {
        @throw [NSException exceptionWithName:@"NestedTestException" 
                                       reason:@"Nested exception test" 
                                     userInfo:@{@"level": @(level)}];
    }
    [self nestedExceptionMethod:(level - 1)];
}

@end

// C function that throws (for testing C++ exception interop if any)
void cFunctionThatThrows() {
    printf("C function: About to trigger objc_exception_throw\n");
    @throw [NSException exceptionWithName:@"CException" 
                                   reason:@"Exception from C function" 
                                 userInfo:nil];
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printf("=== Exception Resolver Validation Test ===\n");
        
        ExceptionTestClass *testObj = [[ExceptionTestClass alloc] init];
        
        // Test Case 1: Basic exception throw (should trigger exception resolver)
        printf("\n--- Test Case 1: Basic Exception Throw ---\n");
        @try {
            [testObj methodThatThrows];  // EXCEPTION BREAKPOINT 1: Should trigger on throw
        } @catch (NSException *exception) {
            printf("Main: Caught basic exception: %s\n", [exception.reason UTF8String]);
        }
        
        // Test Case 2: Custom exception throw
        printf("\n--- Test Case 2: Custom Exception Throw ---\n");
        @try {
            [testObj methodThatThrowsCustom];  // EXCEPTION BREAKPOINT 2: Should trigger on custom throw
        } @catch (TestException *exception) {
            printf("Main: Caught custom exception: %s\n", [exception.reason UTF8String]);
        } @catch (NSException *exception) {
            printf("Main: Caught as NSException: %s\n", [exception.reason UTF8String]);
        }
        
        // Test Case 3: Method with internal try-catch (catch breakpoints)
        printf("\n--- Test Case 3: Internal Try-Catch ---\n");
        [testObj methodThatCatches];  // EXCEPTION BREAKPOINT 3: Should trigger on throw, may trigger on catch
        
        // Test Case 4: Exception from C function
        printf("\n--- Test Case 4: Exception from C Function ---\n");
        @try {
            cFunctionThatThrows();  // EXCEPTION BREAKPOINT 4: Should trigger objc_exception_throw
        } @catch (NSException *exception) {
            printf("Main: Caught C function exception: %s\n", [exception.reason UTF8String]);
        }
        
        // Test Case 5: Nested exception (deep call stack)
        printf("\n--- Test Case 5: Nested Exception ---\n");
        @try {
            [testObj nestedExceptionMethod:3];  // EXCEPTION BREAKPOINT 5: Deep stack exception
        } @catch (NSException *exception) {
            printf("Main: Caught nested exception: %s\n", [exception.reason UTF8String]);
            printf("Exception info: %s\n", [exception.userInfo[@"level"] stringValue].UTF8String);
        }
        
        // Test Case 6: Foundation exceptions
        printf("\n--- Test Case 6: Foundation Exceptions ---\n");
        @try {
            NSArray *array = @[@"item"];
            id obj = [array objectAtIndex:10];  // EXCEPTION BREAKPOINT 6: NSRangeException
            printf("Should not reach here: %p\n", obj);
        } @catch (NSException *exception) {
            printf("Main: Caught Foundation exception: %s - %s\n", 
                   [exception.name UTF8String], [exception.reason UTF8String]);
        }
        
        // Test Case 7: Multiple exceptions in sequence
        printf("\n--- Test Case 7: Multiple Sequential Exceptions ---\n");
        for (int i = 0; i < 3; i++) {
            @try {
                printf("Throwing exception %d\n", i);
                @throw [NSException exceptionWithName:[NSString stringWithFormat:@"Exception%d", i]
                                               reason:[NSString stringWithFormat:@"Test exception %d", i]
                                             userInfo:@{@"index": @(i)}];
            } @catch (NSException *exception) {
                printf("Caught sequential exception %d: %s\n", i, [exception.reason UTF8String]);
            }
        }
        
        printf("\n=== Exception Resolver Test Complete ===\n");
        printf("All exceptions were handled. Exception breakpoints should have triggered.\n");
    }
    
    return 0;
}