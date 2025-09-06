"""
Test GNUstep Foundation types formatters in LLDB.

This test focuses on Foundation-specific types like NSDate, NSURL, NSError, 
NSDecimalNumber, NSCharacterSet, NSIndexSet, etc.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil

class TestGNUstepFoundationTypes(TestBase):
    """Test comprehensive GNUstep Foundation types formatting and functionality.
    
    This test suite covers all Foundation types from main.m including:
    - Original types: NSDate, NSURL, NSUUID, NSError, NSDecimalNumber, NSIndexSet, NSCharacterSet, NSNull, NSException
    - Extended types: NSScanner, NSBundle, NSUserDefaults, NSLocale, NSAttributedString, NSNotification
    
    Tests include:
    - Basic object formatting with po command
    - Frame variable type display
    - Property access and method calls
    - Nil handling across all types
    - Expression evaluation and object creation
    - Edge cases and complex scenarios
    """

    def setUp(self):
        TestBase.setUp(self)
        self.main_source = "main.m"
        self.break_line = line_number(self.main_source, "// Break here for testing")
        
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

    @skipUnlessPlatform(["linux"])
    def test_nsdate_formatting(self):
        """Test NSDate formatter with various date objects."""
        self.build()
        self.run_to_breakpoint()
        
        # Test date objects - should show some representation
        self.expect("po currentTime", matching=True, patterns=[r'.*\d{4}-\d{2}-\d{2}.*'])
        self.expect("po futureDate", matching=True, patterns=[r'.*\d{4}-\d{2}-\d{2}.*'])
        self.expect("po pastDate", substrs=["1970"])  # Unix epoch
        
        # Test nil date
        self.expect("po nilDate", substrs=["nil"])
        
        # Frame variable should show NSDate type
        self.expect("frame variable currentTime", substrs=["NSDate"])

    @skipUnlessPlatform(["linux"])
    def test_nsurl_formatting(self):
        """Test NSURL formatter with various URL types."""
        self.build()
        self.run_to_breakpoint()
        
        # Test URL objects
        self.expect("po webURL", substrs=["https://www.example.com"])
        self.expect("po fileURL", substrs=["file://", "/usr/local/bin/test"])
        self.expect("po complexURL", substrs=["ftp://", "host.com:8080"])
        
        # Test nil URL
        self.expect("po nilURL", substrs=["nil"])
        
        # Frame variable should show NSURL type
        self.expect("frame variable webURL", substrs=["NSURL"])

    @skipUnlessPlatform(["linux"])
    def test_nsuuid_formatting(self):
        """Test NSUUID formatter."""
        self.build()
        self.run_to_breakpoint()
        
        # Test UUID objects - should show UUID format
        self.expect("po randomUUID", matching=True, patterns=[r'.*[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}.*'])
        self.expect("po specificUUID", substrs=["550e8400-e29b-41d4-a716-446655440000"])
        
        # Test nil UUID
        self.expect("po nilUUID", substrs=["nil"])
        
        # Frame variable should show NSUUID type
        self.expect("frame variable randomUUID", substrs=["NSUUID"])

    @skipUnlessPlatform(["linux"])
    def test_nserror_formatting(self):
        """Test NSError formatter."""
        self.build()
        self.run_to_breakpoint()
        
        # Test error objects
        self.expect("po simpleError", substrs=["TestDomain", "404"])
        self.expect("po detailedError", substrs=["NSURLErrorDomain", "File not found"])
        
        # Test nil error
        self.expect("po nilError", substrs=["nil"])
        
        # Frame variable should show NSError type
        self.expect("frame variable simpleError", substrs=["NSError"])

    @skipUnlessPlatform(["linux"])
    def test_nsdecimalnumber_formatting(self):
        """Test NSDecimalNumber formatter."""
        self.build()
        self.run_to_breakpoint()
        
        # Test decimal number objects
        self.expect("po integerDecimal", substrs=["42"])
        self.expect("po floatDecimal", substrs=["123.456"])
        self.expect("po negativeDecimal", substrs=["-987.654"])
        self.expect("po zeroDecimal", substrs=["0"])
        self.expect("po oneDecimal", substrs=["1"])
        
        # Large decimal should show properly
        self.expect("po largeDecimal", substrs=["999999999999999999"])
        
        # Special values
        self.expect("po notANumber", substrs=["NaN"])
        
        # Test nil decimal
        self.expect("po nilDecimalNumber", substrs=["nil"])
        
        # Frame variable should show NSDecimalNumber type
        self.expect("frame variable integerDecimal", substrs=["NSDecimalNumber"])

    @skipUnlessPlatform(["linux"])
    def test_nsindexset_formatting(self):
        """Test NSIndexSet formatter."""
        self.build()
        self.run_to_breakpoint()
        
        # Test index set objects
        self.expect("po emptyIndexSet", substrs=["<"])  # Empty index set
        self.expect("po singleIndexSet", substrs=["<", "42", ">"])
        self.expect("po rangeIndexSet", substrs=["<", "10", ">"])  # Range 10-14
        self.expect("po mutableIndexSet", substrs=["<", ">"])  # Should show indices
        
        # Frame variable should show NSIndexSet type
        self.expect("frame variable singleIndexSet", substrs=["NSIndexSet"])

    @skipUnlessPlatform(["linux"])
    def test_nscharacterset_formatting(self):
        """Test NSCharacterSet formatter."""
        self.build()
        self.run_to_breakpoint()
        
        # Test character set objects - should show some representation
        self.expect("po alphaCharset", substrs=["<"])  # Should show character set format
        self.expect("po digitCharset", substrs=["<"])
        self.expect("po whitespaceCharset", substrs=["<"])
        self.expect("po customCharset", substrs=["<"])
        
        # Test nil charset
        self.expect("po nilCharset", substrs=["nil"])
        
        # Frame variable should show NSCharacterSet type
        self.expect("frame variable alphaCharset", substrs=["NSCharacterSet"])

    @skipUnlessPlatform(["linux"])
    def test_nsnull_formatting(self):
        """Test NSNull formatter."""
        self.build()
        self.run_to_breakpoint()
        
        # Test NSNull singleton
        self.expect("po nullObject", substrs=["NSNull"])
        
        # Frame variable should show NSNull type
        self.expect("frame variable nullObject", substrs=["NSNull"])

    @skipUnlessPlatform(["linux"])
    def test_nsexception_formatting(self):
        """Test NSException formatter."""
        self.build()
        self.run_to_breakpoint()
        
        # Test exception object
        self.expect("po testException", substrs=["TestException", "test exception"])
        
        # Frame variable should show NSException type
        self.expect("frame variable testException", substrs=["NSException"])

    @skipUnlessPlatform(["linux"])
    def test_foundation_types_nil_handling(self):
        """Test that all Foundation types handle nil correctly."""
        self.build()
        self.run_to_breakpoint()
        
        # Test nil objects for all Foundation types
        nil_tests = [
            "nilDate", "nilURL", "nilUUID", "nilError", 
            "nilDecimalNumber", "nilCharset"
        ]
        
        for nil_obj in nil_tests:
            with self.subTest(nil_object=nil_obj):
                self.expect(f"po {nil_obj}", substrs=["nil"])

    @skipUnlessPlatform(["linux"])
    def test_foundation_expression_evaluation(self):
        """Test expression evaluation with Foundation types."""
        self.build()
        self.run_to_breakpoint()
        
        # Test creating new Foundation objects in expressions
        self.expect("expr [NSDate date]", matching=True, patterns=[r'.*\d{4}.*'])
        self.expect("expr [NSURL URLWithString:@\"http://example.com\"]", 
                   substrs=["http://example.com"])
        self.expect("expr [NSUUID UUID]", 
                   matching=True, patterns=[r'.*[0-9a-fA-F-]{36}.*'])
        
        # Test NSDecimalNumber creation
        self.expect("expr [NSDecimalNumber decimalNumberWithString:@\"123.45\"]", 
                   substrs=["123.45"])

    @skipUnlessPlatform(["linux"]) 
    def test_foundation_property_access(self):
        """Test accessing properties of Foundation objects."""
        self.build()
        self.run_to_breakpoint()
        
        # Test URL property access
        self.expect("expr webURL.scheme", substrs=["https"])
        self.expect("expr webURL.host", substrs=["www.example.com"])
        
        # Test error property access
        self.expect("expr simpleError.code", substrs=["404"])
        self.expect("expr simpleError.domain", substrs=["TestDomain"])
        
        # Test UUID string representation
        self.expect("expr specificUUID.UUIDString", 
                   substrs=["550e8400-e29b-41d4-a716-446655440000"])

    @skipUnlessPlatform(["linux"])
    def test_nsscanner_formatting(self):
        """Test NSScanner formatter and state inspection."""
        self.build()
        self.run_to_breakpoint()
        
        # Test NSScanner objects
        self.expect("po stringScanner", substrs=["NSScanner"])
        self.expect("po numberScanner", substrs=["NSScanner"])
        self.expect("po emptyScanner", substrs=["NSScanner"])
        
        # Test nil scanner
        self.expect("po nilScanner", substrs=["nil"])
        
        # Frame variable should show NSScanner type
        self.expect("frame variable stringScanner", substrs=["NSScanner"])
        
        # Test scanner properties and state
        self.expect("po stringScanner.string", substrs=["Hello 123 World 456.78 End"])
        self.expect("po numberScanner.string", substrs=["123.456 789 -42.5"])
        self.expect("po emptyScanner.string", substrs=["@\"\""])

    @skipUnlessPlatform(["linux"])
    def test_nsbundle_formatting(self):
        """Test NSBundle formatter."""
        self.build()
        self.run_to_breakpoint()
        
        # Test NSBundle objects
        self.expect("po mainBundle", substrs=["NSBundle"])
        self.expect("po foundationBundle", matching=True, patterns=[r".*NSBundle.*"])
        
        # Test nil bundle
        self.expect("po nilBundle", substrs=["nil"])
        
        # Frame variable should show NSBundle type
        self.expect("frame variable mainBundle", substrs=["NSBundle"])
        
        # Test bundle properties
        self.expect("expr mainBundle.bundlePath", error=False)
        self.expect("expr mainBundle.executablePath", error=False)

    @skipUnlessPlatform(["linux"])
    def test_nsuserdefaults_formatting(self):
        """Test NSUserDefaults formatter."""
        self.build()
        self.run_to_breakpoint()
        
        # Test NSUserDefaults objects
        self.expect("po standardDefaults", substrs=["NSUserDefaults"])
        self.expect("po customDefaults", substrs=["NSUserDefaults"])
        
        # Test nil defaults
        self.expect("po nilDefaults", substrs=["nil"])
        
        # Frame variable should show NSUserDefaults type
        self.expect("frame variable standardDefaults", substrs=["NSUserDefaults"])
        
        # Test user defaults values that were set in main.m
        self.expect("po [standardDefaults objectForKey:@\"test_key\"]", substrs=["test_value"])
        self.expect("expr [standardDefaults integerForKey:@\"test_number\"]", substrs=["42"])
        self.expect("expr [standardDefaults boolForKey:@\"test_bool\"]", substrs=["YES"])

    @skipUnlessPlatform(["linux"])
    def test_nslocale_formatting(self):
        """Test NSLocale formatter."""
        self.build()
        self.run_to_breakpoint()
        
        # Test NSLocale objects
        self.expect("po currentLocale", substrs=["NSLocale"])
        self.expect("po systemLocale", substrs=["NSLocale"])
        self.expect("po usLocale", substrs=["NSLocale", "en_US"])
        self.expect("po frenchLocale", substrs=["NSLocale", "fr_FR"])
        
        # Test nil locale
        self.expect("po nilLocale", substrs=["nil"])
        
        # Frame variable should show NSLocale type
        self.expect("frame variable currentLocale", substrs=["NSLocale"])
        
        # Test locale properties
        self.expect("po usLocale.localeIdentifier", substrs=["en_US"])
        self.expect("po frenchLocale.localeIdentifier", substrs=["fr_FR"])

    @skipUnlessPlatform(["linux"])
    def test_nsattributedstring_formatting(self):
        """Test NSAttributedString formatter."""
        self.build()
        self.run_to_breakpoint()
        
        # Test NSAttributedString objects
        self.expect("po attrString", substrs=["Attributed Text"])
        
        # Test nil attributed string
        self.expect("po nilAttrString", substrs=["nil"])
        
        # Frame variable should show NSAttributedString type
        self.expect("frame variable attrString", substrs=["NSAttributedString"])
        
        # Test attributed string properties
        self.expect("po attrString.string", substrs=["Attributed Text"])
        self.expect("expr attrString.length", substrs=["15"])  # Length of "Attributed Text"

    @skipUnlessPlatform(["linux"])
    def test_nsnotification_formatting(self):
        """Test NSNotification formatter."""
        self.build()
        self.run_to_breakpoint()
        
        # Test NSNotification objects
        self.expect("po notification", substrs=["TestNotification"])
        
        # Test nil notification
        self.expect("po nilNotification", substrs=["nil"])
        
        # Frame variable should show NSNotification type
        self.expect("frame variable notification", substrs=["NSNotification"])
        
        # Test notification properties
        self.expect("po notification.name", substrs=["TestNotification"])
        self.expect("po notification.userInfo", substrs=["{", "key = value", "number = 123", "}"])
        self.expect("po notification.object", substrs=["nil"])