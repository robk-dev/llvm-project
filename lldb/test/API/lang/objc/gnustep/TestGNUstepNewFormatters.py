"""
Test newly added GNUstep Foundation class formatters in LLDB.
"""

import os
import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil

class TestGNUstepNewFormatters(TestBase):
    
    def setUp(self):
        # Call super's setUp()
        TestBase.setUp(self)
        # Find the line number to break inside main()
        self.main_source = "test_new_formatters.m"
        self.break_line = line_number(self.main_source, "// Break here for testing")
        
    @skipUnlessPlatform(["linux"])
    def test_nsnull_formatter(self):
        """Test NSNull formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # NSNull is a singleton - should show [NSNull null]
        self.expect("po nullObject", substrs=["[NSNull null]"])
        
        # Test in collections
        self.expect("po arrayWithNull", substrs=["@[", "[NSNull null]", "]"])
        
    @skipUnlessPlatform(["linux"])
    def test_nsexception_formatter(self):
        """Test NSException formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Should show exception name and reason
        self.expect("po testException", 
                   substrs=["TestException", "Something went wrong"])
        
        # Test with userInfo
        self.expect("po exceptionWithInfo", 
                   substrs=["InvalidArgumentException", "userInfo"])
        
    @skipUnlessPlatform(["linux"])
    def test_nsattributedstring_formatter(self):
        """Test NSAttributedString formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Should show the underlying string content
        self.expect("po simpleAttributedString", 
                   substrs=["Hello, World!"])
        
        # Test with attributes
        self.expect("po attributedStringWithAttrs", 
                   substrs=["Formatted Text"])
        
    @skipUnlessPlatform(["linux"])
    def test_nsindexpath_formatter(self):
        """Test NSIndexPath formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Should show indexes in dotted notation
        self.expect("po simpleIndexPath", substrs=["0.1"])
        
        # Test longer path
        self.expect("po longIndexPath", substrs=["1.2.3.4"])
        
        # Test empty index path
        self.expect("po emptyIndexPath", substrs=["NSIndexPath"])
        
    @skipUnlessPlatform(["linux"])
    def test_nsnotification_formatter(self):
        """Test NSNotification formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Should show notification name
        self.expect("po simpleNotification", 
                   substrs=["TestNotification"])
        
        # Test with object and userInfo
        self.expect("po fullNotification", 
                   substrs=["DataChangedNotification", "object", "userInfo"])
        
    @skipUnlessPlatform(["linux"])
    def test_nsdate_formatter(self):
        """Test NSDate formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Should show human-readable date
        self.expect("po currentDate", substrs=["20"])  # Year starts with 20
        
        # Test specific date
        self.expect("po specificDate", substrs=["2024"])
        
        # Test distant past/future
        self.expect("po distantPast", substrs=["NSDate"])
        self.expect("po distantFuture", substrs=["NSDate"])
        
    @skipUnlessPlatform(["linux"])
    def test_nsurl_formatter(self):
        """Test NSURL formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Should show URL string
        self.expect("po httpURL", substrs=["https://www.example.com"])
        
        # Test file URL
        self.expect("po fileURL", substrs=["file://", "/tmp/test.txt"])
        
        # Test complex URL with parameters
        self.expect("po complexURL", 
                   substrs=["https://api.example.com/data?key=value"])
        
    @skipUnlessPlatform(["linux"])
    def test_nsdata_formatter(self):
        """Test NSData formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Should show byte count
        self.expect("po smallData", substrs=["bytes"])
        
        # Test empty data
        self.expect("po emptyData", substrs=["0 bytes"])
        
        # Test large data
        self.expect("po largeData", substrs=["1024 bytes"])
        
    @skipUnlessPlatform(["linux"])
    def test_nsuuid_formatter(self):
        """Test NSUUID formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Should show UUID string (format: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX)
        self.expect("po randomUUID", patterns=[r"[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}"])
        
        # Test specific UUID
        self.expect("po specificUUID", 
                   substrs=["550E8400-E29B-41D4-A716-446655440000"])
        
    @skipUnlessPlatform(["linux"])
    def test_nserror_formatter(self):
        """Test NSError formatter for GNUstep."""
        self.build()
        self.run_to_breakpoint()
        
        # Should show domain, code, and description
        self.expect("po simpleError", 
                   substrs=["NSCocoaErrorDomain", "404"])
        
        # Test with userInfo
        self.expect("po errorWithInfo", 
                   substrs=["CustomDomain", "1001", "userInfo"])
        
        # Test nested errors (underlying error)
        self.expect("po nestedError", 
                   substrs=["NetworkError", "underlying"])
        
    @skipUnlessPlatform(["linux"])
    def test_formatter_performance(self):
        """Test that new formatters complete quickly."""
        self.build()
        self.run_to_breakpoint()
        
        # All formatters should complete within reasonable time
        import time
        
        formatters_to_test = [
            "nullObject",
            "testException",
            "simpleAttributedString",
            "simpleIndexPath",
            "simpleNotification",
            "currentDate",
            "httpURL",
            "smallData",
            "randomUUID",
            "simpleError"
        ]
        
        for var in formatters_to_test:
            start = time.time()
            self.expect(f"po {var}", substrs=["NS"])
            elapsed = time.time() - start
            self.assertLess(elapsed, 0.5, f"Formatter for {var} took too long")
        
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