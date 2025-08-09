"""
Test GNUstep collection formatters (NSArray, NSDictionary, NSSet) in LLDB.
"""

import os
import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil

class TestGNUstepCollections(TestBase):
    
    def setUp(self):
        # Call super's setUp()
        TestBase.setUp(self)
        # Find the line numbers to break at
        self.main_source = "test_collections.m"
        self.array_line = line_number(self.main_source, "// Break here for array testing")
        self.dict_line = line_number(self.main_source, "// Break here for dictionary testing")
        self.set_line = line_number(self.main_source, "// Break here for set testing")
        
    @skipUnlessPlatform(["linux"])
    @skipUnlessDarwin  # Skip on non-GNUstep platforms
    def test_array_formatters(self):
        """Test NSArray and NSMutableArray formatters."""
        self.build()
        self.runCmd("file " + self.getBuildArtifact("a.out"), CURRENT_EXECUTABLE_SET)
        
        # Set breakpoint for array testing
        lldbutil.run_break_set_by_file_and_line(
            self, self.main_source, self.array_line, num_expected_locations=1
        )
        
        self.runCmd("run", RUN_SUCCEEDED)
        
        # Test empty array
        self.expect("po emptyArray", substrs=["@[]"])
        
        # Test single element array
        self.expect("po singleArray", substrs=["1 object"])
        self.expect("expr singleArray[0]", substrs=["Hello"])
        
        # Test multiple element array
        self.expect("po multiArray", substrs=["3 objects"])
        self.expect("expr multiArray[0]", substrs=["Apple"])
        self.expect("expr multiArray[1]", substrs=["Banana"])
        self.expect("expr multiArray[2]", substrs=["Cherry"])
        
        # Test large array (should show truncated display)
        self.expect("po largeArray", substrs=["100 objects"])
        
        # Test nested array
        self.expect("po nestedArray", substrs=["2 objects"])
        self.expect("expr nestedArray[0]", substrs=["NSArray"])
        
        # Test mutable array
        self.expect("po mutableArray", substrs=["objects"])
        
        # Test array performance - should complete quickly
        self.expect("expr (NSUInteger)[largeArray count]", substrs=["100"])
        
        # Test array with different object types
        self.expect("po mixedArray", substrs=["objects"])
        
    @skipUnlessPlatform(["linux"])
    @skipUnlessDarwin
    def test_dictionary_formatters(self):
        """Test NSDictionary and NSMutableDictionary formatters."""
        self.build()
        self.runCmd("file " + self.getBuildArtifact("a.out"), CURRENT_EXECUTABLE_SET)
        
        # Set breakpoint for dictionary testing
        lldbutil.run_break_set_by_file_and_line(
            self, self.main_source, self.dict_line, num_expected_locations=1
        )
        
        self.runCmd("run", RUN_SUCCEEDED)
        
        # Test empty dictionary
        self.expect("po emptyDict", substrs=["@{}"])
        
        # Test single key-value pair
        self.expect("po singleDict", substrs=["1 key/value pair"])
        
        # Test multiple key-value pairs
        self.expect("po multiDict", substrs=["3 key/value pairs"])
        
        # Test key-value access
        self.expect("expr multiDict[@\"name\"]", substrs=["John"])
        self.expect("expr multiDict[@\"age\"]", substrs=["30"])
        
        # Test dictionary display format
        # Should show "key = value" not "[0].key" and "[0].value"
        self.expect("frame variable multiDict", 
                   patterns=["name.*=.*John", "age.*=.*30"],
                   matching=False,  # Not expecting [0].key format
                   substrs=["[0].key", "[0].value"])
        
        # Test large dictionary
        self.expect("po largeDict", substrs=["100 key/value pairs"])
        
        # Test nested dictionary
        self.expect("po nestedDict", substrs=["key/value pair"])
        
        # Test mutable dictionary
        self.expect("po mutableDict", substrs=["key/value pair"])
        
        # Test dictionary with different key types
        self.expect("po mixedKeysDict", substrs=["key/value pair"])
        
    @skipUnlessPlatform(["linux"])
    @skipUnlessDarwin
    def test_set_formatters(self):
        """Test NSSet and NSMutableSet formatters."""
        self.build()
        self.runCmd("file " + self.getBuildArtifact("a.out"), CURRENT_EXECUTABLE_SET)
        
        # Set breakpoint for set testing
        lldbutil.run_break_set_by_file_and_line(
            self, self.main_source, self.set_line, num_expected_locations=1
        )
        
        self.runCmd("run", RUN_SUCCEEDED)
        
        # Test empty set
        self.expect("po emptySet", substrs=["0 objects"])
        
        # Test single element set
        self.expect("po singleSet", substrs=["1 object"])
        
        # Test multiple element set
        self.expect("po multiSet", substrs=["3 objects"])
        
        # Test set enumeration
        self.expect("expr [multiSet count]", substrs=["3"])
        
        # Test large set
        self.expect("po largeSet", substrs=["100 objects"])
        
        # Test mutable set
        self.expect("po mutableSet", substrs=["objects"])
        
        # Test set with different object types
        self.expect("po mixedSet", substrs=["objects"])
        
        # Test that set doesn't allow duplicates
        self.expect("po uniqueSet", substrs=["3 objects"])  # Added 5 but 2 were duplicates
        
    @skipUnlessPlatform(["linux"])
    @skipUnlessDarwin
    def test_collection_memory_safety(self):
        """Test formatters handle corrupted collections gracefully."""
        self.build()
        self.runCmd("file " + self.getBuildArtifact("a.out"), CURRENT_EXECUTABLE_SET)
        
        # Set breakpoint at end of main
        self.runCmd("breakpoint set -n test_corrupted_collections")
        self.runCmd("run", RUN_SUCCEEDED)
        
        # Test nil collections
        self.expect("po nilArray", substrs=["nil"])
        self.expect("po nilDict", substrs=["nil"])
        self.expect("po nilSet", substrs=["nil"])
        
        # Test collections with corrupted internal state
        # Formatters should not crash, but may show error or partial data
        self.expect("po corruptedArray", error=True)
        self.expect("po corruptedDict", error=True)
        self.expect("po corruptedSet", error=True)
        
    @skipUnlessPlatform(["linux"])
    @skipUnlessDarwin
    def test_collection_performance(self):
        """Test that formatters complete in reasonable time for large collections."""
        self.build()
        self.runCmd("file " + self.getBuildArtifact("a.out"), CURRENT_EXECUTABLE_SET)
        
        self.runCmd("breakpoint set -n test_large_collections")
        self.runCmd("run", RUN_SUCCEEDED)
        
        import time
        
        # Test large array (10000 elements)
        start = time.time()
        self.expect("po veryLargeArray", substrs=["10000 objects"])
        elapsed = time.time() - start
        self.assertLess(elapsed, 1.0, "Array formatter took too long")
        
        # Test large dictionary (10000 pairs)
        start = time.time()
        self.expect("po veryLargeDict", substrs=["10000 key/value pairs"])
        elapsed = time.time() - start
        self.assertLess(elapsed, 1.0, "Dictionary formatter took too long")
        
        # Test large set (10000 objects)
        start = time.time()
        self.expect("po veryLargeSet", substrs=["10000 objects"])
        elapsed = time.time() - start
        self.assertLess(elapsed, 1.0, "Set formatter took too long")
        
    @skipUnlessPlatform(["linux"])
    @skipUnlessDarwin
    def test_collection_child_access(self):
        """Test synthetic children providers for collections."""
        self.build()
        self.runCmd("file " + self.getBuildArtifact("a.out"), CURRENT_EXECUTABLE_SET)
        
        self.runCmd("breakpoint set -n test_child_access")
        self.runCmd("run", RUN_SUCCEEDED)
        
        # Test array child access
        self.expect("expr testArray[0]", substrs=["First"])
        self.expect("expr testArray[1]", substrs=["Second"])
        self.expect("expr testArray[2]", substrs=["Third"])
        
        # Test out of bounds access
        self.expect("expr testArray[100]", error=True)
        
        # Test dictionary child access via frame variable
        self.expect("frame variable testDict",
                   substrs=["name", "John", "age", "30", "city", "NYC"])
        
        # Test set child enumeration
        self.expect("frame variable testSet",
                   substrs=["[0]", "[1]", "[2]"])
        
        # Test nested collection access
        self.expect("expr nestedArray[0][0]", substrs=["Inner"])
        self.expect("expr nestedDict[@\"user\"][@\"name\"]", substrs=["Alice"])
        
    def create_test_source(self):
        """Create the test source file if it doesn't exist."""
        test_source = """
#import <Foundation/Foundation.h>

void test_corrupted_collections() {
    NSArray *nilArray = nil;
    NSDictionary *nilDict = nil;
    NSSet *nilSet = nil;
    
    // Create corrupted collections (simulated)
    NSArray *corruptedArray = (NSArray *)0xDEADBEEF;
    NSDictionary *corruptedDict = (NSDictionary *)0xDEADBEEF;
    NSSet *corruptedSet = (NSSet *)0xDEADBEEF;
    
    printf("Testing corrupted collections\\n");
}

void test_large_collections() {
    NSMutableArray *veryLargeArray = [NSMutableArray array];
    NSMutableDictionary *veryLargeDict = [NSMutableDictionary dictionary];
    NSMutableSet *veryLargeSet = [NSMutableSet set];
    
    for (int i = 0; i < 10000; i++) {
        [veryLargeArray addObject:@(i)];
        [veryLargeDict setObject:@(i) forKey:[NSString stringWithFormat:@"key%d", i]];
        [veryLargeSet addObject:@(i)];
    }
    
    printf("Testing large collections\\n");
}

void test_child_access() {
    NSArray *testArray = @[@"First", @"Second", @"Third"];
    NSDictionary *testDict = @{@"name": @"John", @"age": @30, @"city": @"NYC"};
    NSSet *testSet = [NSSet setWithObjects:@"A", @"B", @"C", nil];
    
    NSArray *nestedArray = @[@[@"Inner"]];
    NSDictionary *nestedDict = @{@"user": @{@"name": @"Alice"}};
    
    printf("Testing child access\\n");
}

int main() {
    @autoreleasepool {
        // Array testing
        NSArray *emptyArray = @[];
        NSArray *singleArray = @[@"Hello"];
        NSArray *multiArray = @[@"Apple", @"Banana", @"Cherry"];
        
        NSMutableArray *largeArray = [NSMutableArray array];
        for (int i = 0; i < 100; i++) {
            [largeArray addObject:@(i)];
        }
        
        NSArray *nestedArray = @[@[@1, @2], @[@3, @4]];
        NSMutableArray *mutableArray = [NSMutableArray arrayWithArray:multiArray];
        
        NSArray *mixedArray = @[@"String", @42, @{@"key": @"value"}, [NSNull null]];
        
        printf("Array testing\\n"); // Break here for array testing
        
        // Dictionary testing
        NSDictionary *emptyDict = @{};
        NSDictionary *singleDict = @{@"key": @"value"};
        NSDictionary *multiDict = @{@"name": @"John", @"age": @30, @"city": @"NYC"};
        
        NSMutableDictionary *largeDict = [NSMutableDictionary dictionary];
        for (int i = 0; i < 100; i++) {
            largeDict[[NSString stringWithFormat:@"key%d", i]] = @(i);
        }
        
        NSDictionary *nestedDict = @{@"user": @{@"name": @"Alice", @"age": @25}};
        NSMutableDictionary *mutableDict = [NSMutableDictionary dictionaryWithDictionary:multiDict];
        
        NSDictionary *mixedKeysDict = @{@"string": @1, @42: @"number key"};
        
        printf("Dictionary testing\\n"); // Break here for dictionary testing
        
        // Set testing
        NSSet *emptySet = [NSSet set];
        NSSet *singleSet = [NSSet setWithObject:@"One"];
        NSSet *multiSet = [NSSet setWithObjects:@"A", @"B", @"C", nil];
        
        NSMutableSet *largeSet = [NSMutableSet set];
        for (int i = 0; i < 100; i++) {
            [largeSet addObject:@(i)];
        }
        
        NSMutableSet *mutableSet = [NSMutableSet setWithSet:multiSet];
        
        NSSet *mixedSet = [NSSet setWithObjects:@"String", @42, [NSNull null], nil];
        
        // Test uniqueness
        NSSet *uniqueSet = [NSSet setWithObjects:@"A", @"B", @"C", @"A", @"B", nil];
        
        printf("Set testing\\n"); // Break here for set testing
        
        // Additional tests
        test_corrupted_collections();
        test_large_collections();
        test_child_access();
    }
    
    return 0;
}
"""
        with open(self.main_source, 'w') as f:
            f.write(test_source)