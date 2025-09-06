"""
Test GNUstep expression evaluation in LLDB.

This test focuses on expression evaluation capabilities with the GNUstep runtime,
including literal syntax, method calls, and complex expressions.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil

class TestGNUstepExpressions(TestBase):
    """Test GNUstep expression evaluation functionality."""

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
    def test_string_literal_expressions(self):
        """Test string literal creation and manipulation."""
        self.build()
        self.run_to_breakpoint()
        
        # Basic string literals
        self.expect("expr @\"Hello\"", substrs=["Hello"])
        self.expect("expr @\"\"", substrs=["@\"\""])
        self.expect("expr @\"Unicode: 🌍\"", substrs=["Unicode", "🌍"])
        
        # String method calls
        self.expect("expr [@\"Hello\" uppercaseString]", substrs=["HELLO"])
        self.expect("expr [@\"WORLD\" lowercaseString]", substrs=["world"])
        self.expect("expr [@\"test\" length]", substrs=["4"])
        
        # String concatenation
        self.expect("expr [@\"Hello\" stringByAppendingString:@\" World\"]", 
                   substrs=["Hello World"])

    @skipUnlessPlatform(["linux"])
    def test_number_literal_expressions(self):
        """Test number literal creation and arithmetic."""
        self.build()
        self.run_to_breakpoint()
        
        # Basic number literals
        self.expect("expr @42", substrs=["42"])
        self.expect("expr @3.14", substrs=["3.14"])
        self.expect("expr @YES", substrs=["YES"])
        self.expect("expr @NO", substrs=["NO"])
        
        # Number method calls
        self.expect("expr [@42 stringValue]", substrs=["42"])
        self.expect("expr [@3.14 intValue]", substrs=["3"])
        self.expect("expr [@YES boolValue]", substrs=["true"])
        
        # Arithmetic expressions
        self.expect("expr @(21 + 21)", substrs=["42"])
        self.expect("expr @(6 * 7)", substrs=["42"])
        self.expect("expr @(100 - 58)", substrs=["42"])

    @skipUnlessPlatform(["linux"])
    def test_array_literal_expressions(self):
        """Test array literal creation and manipulation."""
        self.build()
        self.run_to_breakpoint()
        
        # Basic array literals
        self.expect("expr @[@\"a\", @\"b\", @\"c\"]", 
                   substrs=["@[", "a", "b", "c", "]"])
        self.expect("expr @[]", substrs=["@[]"])
        self.expect("expr @[@1, @2, @3]", substrs=["@[", "1", "2", "3"])
        
        # Array method calls
        self.expect("expr [@[@\"x\", @\"y\"] count]", substrs=["2"])
        self.expect("expr [@[@\"first\", @\"second\"] objectAtIndex:0]", 
                   substrs=["first"])
        self.expect("expr [@[@\"a\", @\"b\", @\"c\"] objectAtIndex:2]", 
                   substrs=["c"])
        
        # Array containing mixed types
        self.expect("expr @[@\"string\", @42, @YES]", 
                   substrs=["@[", "string", "42", "YES"])

    @skipUnlessPlatform(["linux"])
    def test_dictionary_literal_expressions(self):
        """Test dictionary literal creation and access."""
        self.build()
        self.run_to_breakpoint()
        
        # Basic dictionary literals
        self.expect("expr @{@\"key\": @\"value\"}", 
                   substrs=["@{", "key", "value", "}"])
        self.expect("expr @{}", substrs=["@{}"])
        self.expect("expr @{@\"num\": @42, @\"bool\": @YES}", 
                   substrs=["@{", "num", "42", "bool", "YES"])
        
        # Dictionary method calls
        self.expect("expr [@{@\"test\": @\"data\"} count]", substrs=["1"])
        self.expect("expr [@{@\"name\": @\"John\"} objectForKey:@\"name\"]", 
                   substrs=["John"])
        
        # Dictionary key access syntax
        self.expect("expr @{@\"answer\": @42}[@\"answer\"]", substrs=["42"])

    @skipUnlessPlatform(["linux"])
    def test_method_chaining_expressions(self):
        """Test complex method chaining expressions."""
        self.build()
        self.run_to_breakpoint()
        
        # String method chaining
        self.expect("expr [[@\"  hello world  \" stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] uppercaseString]", 
                   substrs=["HELLO WORLD"])
        
        # Array method chaining with existing objects
        self.expect("expr [[simpleArray objectAtIndex:0] uppercaseString]", 
                   substrs=["APPLE"])
        
        # Complex nested access
        self.expect("expr [[[nestedDict objectForKey:@\"person\"] objectForKey:@\"name\"] length]", 
                   substrs=["5"])  # "Alice" has 5 characters

    @skipUnlessPlatform(["linux"])
    def test_variable_modification_expressions(self):
        """Test modifying existing variables through expressions."""
        self.build()
        self.run_to_breakpoint()
        
        # Modify mutable array
        self.expect("expr [mutableArray addObject:@\"Four\"]", error=False)
        self.expect("expr [mutableArray count]", substrs=["4"])
        self.expect("po mutableArray", substrs=["Four"])
        
        # Modify mutable dictionary
        self.expect("expr [mutableDict setObject:@\"NewValue\" forKey:@\"NewKey\"]", error=False)
        self.expect("expr [mutableDict count]", substrs=["3"])
        self.expect("po mutableDict", substrs=["NewValue"])
        
        # Modify mutable set
        self.expect("expr [mutableSet addObject:@\"Fish\"]", error=False)
        self.expect("expr [mutableSet count]", substrs=["4"])

    @skipUnlessPlatform(["linux"])
    def test_class_method_expressions(self):
        """Test calling class methods in expressions."""
        self.build()
        self.run_to_breakpoint()
        
        # NSString class methods
        self.expect("expr [NSString stringWithFormat:@\"Number: %d\", 42]", 
                   substrs=["Number: 42"])
        
        # NSArray class methods
        self.expect("expr [NSArray arrayWithObjects:@\"x\", @\"y\", nil]", 
                   substrs=["@[", "x", "y"])
        
        # NSDictionary class methods
        self.expect("expr [NSDictionary dictionaryWithObject:@\"value\" forKey:@\"key\"]", 
                   substrs=["@{", "key", "value"])
        
        # NSDate class methods
        self.expect("expr [NSDate date]", 
                   matching=True, patterns=[r'.*\d{4}.*'])

    @skipUnlessPlatform(["linux"])
    def test_custom_class_expressions(self):
        """Test expressions with custom classes."""
        self.build()
        self.run_to_breakpoint()
        
        # Access custom class properties
        self.expect("expr account.owner", substrs=["John Doe"])
        self.expect("expr account.balance", substrs=["1025"])
        self.expect("expr account.accountNumber", substrs=["12345"])
        
        # Call custom class methods
        self.expect("expr [account deposit:100.0]", error=False)
        self.expect("expr account.balance", substrs=["1125"])
        
        # Access nested properties
        self.expect("expr [account.transactions count]", substrs=["4"])

    @skipUnlessPlatform(["linux"])
    def test_nil_and_null_expressions(self):
        """Test expressions involving nil and null values."""
        self.build()
        self.run_to_breakpoint()
        
        # Nil comparisons
        self.expect("expr nilObject == nil", substrs=["true"])
        self.expect("expr nilObject != nil", substrs=["false"])
        self.expect("expr (NSString*)nil", substrs=["nil"])
        
        # NSNull handling
        self.expect("expr [NSNull null]", substrs=["NSNull"])
        self.expect("expr nullObject", substrs=["NSNull"])

    @skipUnlessPlatform(["linux"])
    def test_type_casting_expressions(self):
        """Test type casting in expressions."""
        self.build()
        self.run_to_breakpoint()
        
        # Basic type casting
        self.expect("expr (NSString*)asciiString", substrs=["Hello, World!"])
        self.expect("expr (NSMutableArray*)mutableArray", substrs=["NSMutableArray"])
        
        # Casting with method calls
        self.expect("expr [(NSString*)asciiString length]", substrs=["13"])

    @skipUnlessPlatform(["linux"])
    def test_complex_expressions(self):
        """Test complex multi-step expressions."""
        self.build()
        self.run_to_breakpoint()
        
        # Complex dictionary/array access
        self.expect("expr [[[[nestedDict objectForKey:@\"person\"] objectForKey:@\"name\"] uppercaseString] length]", 
                   substrs=["5"])
        
        # Mathematical expressions with NSNumber
        self.expect("expr @([intNumber intValue] * [floatNumber intValue])", 
                   substrs=["126"])  # 42 * 3 = 126
        
        # Boolean logic expressions  
        self.expect("expr @([boolNumber boolValue] && YES)", substrs=["YES"])
        self.expect("expr @([boolNumber boolValue] || NO)", substrs=["YES"])

    @skipUnlessPlatform(["linux"])
    def test_expression_error_handling(self):
        """Test that expressions handle errors gracefully."""
        self.build()
        self.run_to_breakpoint()
        
        # Invalid method calls should error
        self.expect("expr [nonexistentObject someMethod]", error=True)
        
        # Invalid array access should error
        self.expect("expr simpleArray[100]", error=True)
        
        # Invalid dictionary key should return nil (not error)
        self.expect("expr simpleDict[@\"nonexistent\"]", substrs=["nil"])
        
        # Method call on nil should return nil (not crash)
        self.expect("expr [(NSString*)nil length]", substrs=["0"])

    @skipUnlessPlatform(["linux"])
    def test_performance_complex_expressions(self):
        """Test performance of complex expressions."""
        self.build()
        self.run_to_breakpoint()
        
        # Complex expressions should complete within timeout
        performance_expressions = [
            "[[largeArray objectAtIndex:500] stringValue]",
            "[largeDict objectForKey:@\"key100\"]",
            "[@\"test\" stringByAppendingString:[@100 stringValue]]"
        ]
        
        for expr in performance_expressions:
            with self.subTest(expression=expr):
                self.expect(f"expr {expr}", error=False, timeout=5)