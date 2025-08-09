"""
Test GNUstep Objective-C formatters in LLDB.
"""

import os
import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil

class TestGNUstepFormatters(TestBase):
    
    def setUp(self):
        # Call super's setUp()
        TestBase.setUp(self)
        # Find the line number to break inside main()
        self.main_source = "main.m"
        self.break_line = line_number(self.main_source, "// Break here for testing")
        
    @skipUnlessPlatform(["linux"])
    def test_string_formatter(self):
        """Test NSString formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Test various string types
        self.expect("po emptyString", substrs=["@\"\""])
        self.expect("po asciiString", substrs=["Hello, World!"])
        self.expect("po utf8String", substrs=["Unicode"])
        self.expect("po taggedString", substrs=["Hi"])  # Small tagged string
        
    @skipUnlessPlatform(["linux"])
    def test_number_formatter(self):
        """Test NSNumber formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Test various number types
        self.expect("po intNumber", substrs=["42"])
        self.expect("po floatNumber", substrs=["3.14"])
        self.expect("po boolNumber", substrs=["YES"])
        self.expect("po taggedInt", substrs=["7"])  # Tagged integer
        
    @skipUnlessPlatform(["linux"])
    def test_array_formatter(self):
        """Test NSArray formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Test array display - no longer shows "3 objects" prefix
        self.expect("po emptyArray", substrs=["@[]"])
        self.expect("po simpleArray", substrs=["@[", "Apple", "Banana", "Cherry"])
        self.expect("po mutableArray", substrs=["@[", "]"])  # Just shows content, no prefix
        
        # Test synthetic children
        self.expect("expr simpleArray[0]", substrs=["Apple"])
        self.expect("expr simpleArray[1]", substrs=["Banana"])
        
    @skipUnlessPlatform(["linux"])
    def test_dictionary_formatter(self):
        """Test NSDictionary formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Test dictionary display - no longer shows "2 key/value pairs" prefix
        self.expect("po emptyDict", substrs=["@{}"])
        self.expect("po simpleDict", substrs=["@{", "name", "John", "age", "30", "}"])
        self.expect("po mutableDict", substrs=["@{", "}"])  # Just shows content, no prefix
        
        # Test synthetic children
        self.expect("expr simpleDict[@\"name\"]", substrs=["John"])
        
    @skipUnlessPlatform(["linux"])
    def test_set_formatter(self):
        """Test NSSet formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Test set display - no longer shows "3 objects" prefix
        self.expect("po emptySet", substrs=["{", "}"])  # Empty set shows {}
        self.expect("po simpleSet", substrs=["{", "}"])  # Just shows content, no prefix
        self.expect("po mutableSet", substrs=["{", "}"])  # Just shows content, no prefix
        
    @skipUnlessPlatform(["linux"])
    def test_nil_handling(self):
        """Test handling of nil objects."""
        self.build()
        self.run_to_breakpoint()
        
        # Test nil objects
        self.expect("po nilObject", substrs=["nil"])
        self.expect("expr nilObject == nil", substrs=["true"])
        
    @skipUnlessPlatform(["linux"])
    def test_custom_classes(self):
        """Test custom class formatting."""
        self.build()
        self.run_to_breakpoint()
        
        # Test custom class (BankAccount)
        self.expect("po account", substrs=["BankAccount", "12345", "John Doe", "1000.00"])
        
        # Test accessing properties
        self.expect("expr account.balance", substrs=["1000"])
        self.expect("expr account.accountNumber", substrs=["12345"])
        
    @skipUnlessPlatform(["linux"])
    def test_nested_collections(self):
        """Test nested collection formatting."""
        self.build()
        self.run_to_breakpoint()
        
        # Test nested array
        self.expect("po nestedArray", substrs=["@[", "@["])
        
        # Test nested dictionary
        self.expect("po nestedDict", substrs=["address", "street", "city"])
        
    @skipUnlessPlatform(["linux"])
    def test_performance(self):
        """Test formatter performance with large collections."""
        self.build()
        self.run_to_breakpoint()
        
        # Large array should complete quickly - no longer shows count prefix
        self.expect("po largeArray", substrs=["@["], timeout=5)
        
        # Large dictionary should complete quickly - no longer shows count prefix
        self.expect("po largeDict", substrs=["@{"], timeout=5)
        
    def run_to_breakpoint(self):
        """Helper to run to the test breakpoint."""
        exe = self.getBuildArtifact("a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)
        
        # Set breakpoint
        lldbutil.run_break_set_by_file_and_line(
            self, self.main_source, self.break_line, num_expected_locations=1
        )
        
        # Run the program
        self.runCmd("run", RUN_SUCCEEDED)
        
        # The stop reason should be breakpoint
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
                    substrs=['stopped', 'stop reason = breakpoint'])