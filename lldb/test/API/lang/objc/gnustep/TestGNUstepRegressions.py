"""
Test GNUstep formatter regressions and validate fixes.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil
import time


class TestGNUstepRegressions(TestBase):
    """Comprehensive regression tests for GNUstep formatters."""

    def setUp(self):
        """Build the test program."""
        TestBase.setUp(self)
        self.build()
        self.target = self.dbg.CreateTarget(self.getBuildArtifact("a.out"))
        self.assertTrue(self.target, VALID_TARGET)

    @skipUnlessDarwin
    def test_array_string_elements(self):
        """Test that array elements show actual string values, not placeholders."""
        self.runCmd("file " + self.getBuildArtifact("a.out"))
        self.runCmd("breakpoint set --line 105")
        self.runCmd("run")

        # Test simple array with strings
        result = self.frame().FindVariable("simpleArray")
        self.assertTrue(result.IsValid())
        
        # Check summary doesn't contain <string> placeholder
        summary = result.GetSummary()
        self.assertIsNotNone(summary)
        self.assertNotIn("<string>", summary, 
                         "Array should show actual string values, not <string> placeholders")
        
        # Verify we can see the actual values
        self.assertIn("Apple", summary)
        self.assertIn("Banana", summary)
        self.assertIn("Cherry", summary)

        # Test element access
        self.expect("expression simpleArray[0]",
                   substrs=["Apple"],
                   error=False)
        self.expect("expression simpleArray[1]",
                   substrs=["Banana"],
                   error=False)

    @skipUnlessDarwin
    def test_dictionary_key_format(self):
        """Test that dictionaries show clean key=value format."""
        self.runCmd("file " + self.getBuildArtifact("a.out"))
        self.runCmd("breakpoint set --line 105")
        self.runCmd("run")

        result = self.frame().FindVariable("simpleDict")
        self.assertTrue(result.IsValid())
        
        summary = result.GetSummary()
        self.assertIsNotNone(summary)
        
        # Should NOT contain verbose [0].key format
        self.assertNotIn("[0].key", summary,
                        "Dictionary should use clean key=value format")
        self.assertNotIn("[0].value", summary,
                        "Dictionary should use clean key=value format")
        
        # Should contain actual key-value pairs
        self.assertIn("name", summary)
        self.assertIn("John Doe", summary)

        # Test key extraction
        self.expect("expression [simpleDict objectForKey:@\"name\"]",
                   substrs=["John Doe"],
                   error=False)

    @skipUnlessDarwin
    def test_nested_collections(self):
        """Test nested collections display correctly."""
        self.runCmd("file " + self.getBuildArtifact("a.out"))
        self.runCmd("breakpoint set --line 301")  # After all objects created
        self.runCmd("run")

        # Test dictionary with array values
        result = self.frame().FindVariable("dictWithArrays")
        self.assertTrue(result.IsValid())
        
        # Expand to see nested arrays
        num_children = result.GetNumChildren()
        self.assertGreater(num_children, 0)
        
        # Find the fruits array
        for i in range(num_children):
            child = result.GetChildAtIndex(i)
            if "fruits" in str(child.GetName()):
                # Verify the array shows actual values
                child_summary = child.GetSummary()
                if child_summary:
                    self.assertNotIn("<string>", child_summary,
                                   "Nested array should show actual values")

    @skipUnlessDarwin  
    def test_tagged_pointer_numbers(self):
        """Test tagged pointer numbers in various contexts."""
        self.runCmd("file " + self.getBuildArtifact("a.out"))
        self.runCmd("breakpoint set --line 105")
        self.runCmd("run")

        # Test standalone tagged numbers
        result = self.frame().FindVariable("intNumber")
        self.assertTrue(result.IsValid())
        summary = result.GetSummary()
        self.assertIn("42", summary)

        result = self.frame().FindVariable("boolNumber")
        self.assertTrue(result.IsValid())
        summary = result.GetSummary()
        self.assertIn("YES", summary)

        # Test tagged numbers in collections
        self.expect("expression @[@1, @2, @3]",
                   substrs=["@1", "@2", "@3"],
                   error=False)
        
        self.expect("expression @{@\"key\": @42}",
                   substrs=["42"],
                   error=False)

    @skipUnlessDarwin
    def test_custom_class_properties(self):
        """Test custom class property introspection."""
        self.runCmd("file " + self.getBuildArtifact("a.out"))
        self.runCmd("breakpoint set --line 301")
        self.runCmd("run")

        result = self.frame().FindVariable("customObj")
        self.assertTrue(result.IsValid())
        
        # Should be able to see properties
        self.expect("expression customObj.name",
                   substrs=["TestName"],
                   error=False)
        self.expect("expression customObj.value",
                   substrs=["100"],
                   error=False)

        # po should show description
        self.expect("po customObj",
                   substrs=["TestObject", "TestName", "100"],
                   error=False)

    @skipUnlessDarwin
    def test_formatter_performance(self):
        """Test that formatters complete within 50ms."""
        self.runCmd("file " + self.getBuildArtifact("a.out"))
        self.runCmd("breakpoint set --line 301")
        self.runCmd("run")

        test_vars = ["longString", "deeplyNested", "arrayOfArrays"]
        
        for var_name in test_vars:
            start = time.time()
            result = self.frame().FindVariable(var_name)
            self.assertTrue(result.IsValid())
            _ = result.GetSummary()  # Force formatting
            elapsed = (time.time() - start) * 1000
            
            self.assertLess(elapsed, 50,
                          f"{var_name} formatting took {elapsed:.2f}ms, expected <50ms")

    @skipUnlessDarwin
    def test_edge_cases(self):
        """Test edge cases don't crash or hang."""
        self.runCmd("file " + self.getBuildArtifact("a.out"))
        self.runCmd("breakpoint set --line 301")
        self.runCmd("run")

        # Test empty collections
        self.expect("frame variable emptyArray",
                   substrs=["@[]"],
                   error=False)
        self.expect("frame variable emptyDict",
                   substrs=["@{}"],
                   error=False)

        # Test nil handling
        self.expect("frame variable nilString",
                   substrs=["nil"],
                   error=False)

        # Test NSNull in collections
        result = self.frame().FindVariable("arrayWithNil")
        self.assertTrue(result.IsValid())
        summary = result.GetSummary()
        self.assertIsNotNone(summary)
        
        # Test circular references (should not hang)
        start = time.time()
        result = self.frame().FindVariable("circularArray")
        self.assertTrue(result.IsValid())
        _ = result.GetSummary()
        elapsed = time.time() - start
        self.assertLess(elapsed, 1.0, "Circular reference handling took too long")

    def test_comprehensive_coverage(self):
        """Verify comprehensive test coverage of formatters."""
        # This test validates that our test suite covers all major formatter code paths
        
        formatter_types = [
            ("NSString", ["taggedString", "constantString", "mutableString", "emptyString"]),
            ("NSNumber", ["taggedInt", "taggedBool", "taggedFloat", "largeInt"]),
            ("NSArray", ["emptyArray", "simpleArray", "numberArray", "mixedArray"]),
            ("NSDictionary", ["emptyDict", "simpleDict", "mutableDict"]),
            ("NSSet", ["emptySet", "simpleSet", "numberSet"]),
            ("NSDate", ["currentDate", "pastDate"]),
            ("NSURL", ["httpUrl", "fileUrl"]),
            ("NSUUID", ["uuid1", "uuid2"]),
            ("NSData", ["emptyData", "smallData"]),
            ("NSError", ["simpleError", "detailedError"]),
        ]
        
        self.runCmd("file " + self.getBuildArtifact("test_comprehensive_formatters"))
        self.runCmd("breakpoint set --line 301")
        self.runCmd("run")
        
        coverage = {}
        for formatter_type, test_vars in formatter_types:
            tested = 0
            for var in test_vars:
                result = self.frame().FindVariable(var)
                if result.IsValid() and result.GetSummary():
                    tested += 1
            coverage[formatter_type] = (tested / len(test_vars)) * 100
        
        # Verify 90%+ coverage for core formatters
        core_formatters = ["NSString", "NSNumber", "NSArray", "NSDictionary", "NSSet"]
        for formatter in core_formatters:
            self.assertGreaterEqual(coverage.get(formatter, 0), 90,
                                  f"{formatter} coverage is {coverage.get(formatter, 0):.1f}%, expected >=90%")
        
        # Overall coverage
        total_coverage = sum(coverage.values()) / len(coverage)
        print(f"\nOverall formatter coverage: {total_coverage:.1f}%")
        self.assertGreaterEqual(total_coverage, 85,
                              f"Overall coverage is {total_coverage:.1f}%, expected >=85%")