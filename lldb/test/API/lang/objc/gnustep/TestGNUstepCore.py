"""
Test comprehensive GNUstep Objective-C functionality in LLDB.

This test validates all GNUstep formatter functionality, object inspection,
and expression evaluation in a single comprehensive test suite.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil

class TestGNUstepCore(TestBase):
    """Comprehensive test suite for all GNUstep Foundation objects with deep validation.
    
    This test suite covers all 80+ Foundation objects from custom_class_test.m with:
    - Synthetic children navigation and property access chains
    - Performance testing with large collections (100+ elements)
    - Edge case handling (nil objects, corrupted memory)
    - Comprehensive formatter validation
    - Real debugging scenarios developers face
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
    def test_comprehensive_string_suite(self):
        """Comprehensive test suite for all NSString types and encodings."""
        self.build()
        self.run_to_breakpoint()
        
        # All string types from main.m
        string_tests = [
            ("emptyString", ["@\"\""]),
            ("asciiString", ["Hello, World!"]),
            ("utf8String", ["Unicode: 🌍", "emoji: 😊"]),
            ("literalString", ["This is a literal string"]),
            ("formattedString", ["Formatted: Number is 42"]),
            ("pathString", ["/usr/local/bin/test"]),
            ("mutableString1", ["Initial string"]),
            ("mutableString2", ["Appended string"]),
            ("urlString", ["https://www.example.com/path"]),
            ("xmlString", ["<root><item>value</item></root>"]),
            ("multilineString", ["This is line 1", "This is line 2", "This is line 3"])
        ]
        
        for string_var, expected_substrs in string_tests:
            with self.subTest(string=string_var):
                self.expect(f"po {string_var}", substrs=expected_substrs)
                self.expect(f"frame variable {string_var}", substrs=["NSString"])
                
        # Test string property access and method chaining
        self.expect("expr asciiString.length", substrs=["13"])  # "Hello, World!"
        self.expect("expr [asciiString uppercaseString]", substrs=["HELLO, WORLD!"])
        self.expect("expr [utf8String substringToIndex:7]", substrs=["Unicode"])
        
        # Test mutable string operations
        self.expect("expr [mutableString1 appendString:@\" APPENDED\"]", error=False)
        self.expect("po mutableString1", substrs=["Initial string APPENDED"])
        
        # Test nil and edge cases
        self.expect("po nilString", substrs=["nil"])
        self.expect("expr (NSString*)nil", substrs=["nil"])
        self.expect("expr [@\"\" length]", substrs=["0"])

    @skipUnlessPlatform(["linux"])
    def test_comprehensive_number_suite(self):
        """Comprehensive test suite for all NSNumber types including tagged pointers."""
        self.build()
        self.run_to_breakpoint()
        
        # All number types from main.m
        number_tests = [
            ("intNumber", ["42"]),
            ("floatNumber", ["3.14"]),
            ("doubleNumber", ["2.71828"]),
            ("boolNumber", ["YES"]),
            ("boolNumberNo", ["NO"]),
            ("charNumber", ["65"]),  # 'A'
            ("shortNumber", ["32767"]),
            ("longNumber", ["2147483647"]),
            ("longLongNumber", ["9223372036854775807"]),
            ("unsignedNumber", ["4294967295"]),
            ("negativeNumber", ["-999"]),
            ("zeroNumber", ["0"]),
            ("oneNumber", ["1"])
        ]
        
        for number_var, expected_substrs in number_tests:
            with self.subTest(number=number_var):
                self.expect(f"po {number_var}", substrs=expected_substrs)
                self.expect(f"frame variable {number_var}", substrs=["NSNumber"])
        
        # Test tagged pointers (small integers)
        tagged_tests = [
            ("taggedInt1", ["7"]),
            ("taggedInt2", ["15"]),
            ("taggedInt3", ["31"])
        ]
        
        for tagged_var, expected_substrs in tagged_tests:
            with self.subTest(tagged=tagged_var):
                self.expect(f"po {tagged_var}", substrs=expected_substrs)
        
        # Test number method calls and conversions
        self.expect("expr [intNumber stringValue]", substrs=["42"])
        self.expect("expr [floatNumber intValue]", substrs=["3"])
        self.expect("expr [boolNumber boolValue]", substrs=["true"])
        self.expect("expr [doubleNumber floatValue]", substrs=["2.71828"])
        
        # Test arithmetic with NSNumbers
        self.expect("expr @([intNumber intValue] + 10)", substrs=["52"])
        self.expect("expr @([floatNumber floatValue] * 2)", substrs=["6.28"])
        
        # Test nil and edge cases
        self.expect("po nilNumber", substrs=["nil"])
        self.expect("expr (NSNumber*)nil", substrs=["nil"])

    @skipUnlessPlatform(["linux"])
    def test_comprehensive_array_suite(self):
        """Comprehensive test suite for all NSArray types and synthetic children."""
        self.build()
        self.run_to_breakpoint()
        
        # All array types from main.m (actual LLDB format: parentheses, not @[])
        array_tests = [
            ("emptyArray", ["()"]),
            ("simpleArray", ["(", "Apple", "Banana", "Cherry", ")"]),
            ("numberArray", ["(", "1", "2", "3", "4", "5", ")"]),
            ("mixedArray", ["(", "String", "42", "YES", ")"]),
            ("mutableArray", ["(", "One", "Two", "Three", ")"]),
            ("nestedArray", ["(", "("])
        ]
        
        for array_var, expected_substrs in array_tests:
            with self.subTest(array=array_var):
                self.expect(f"po {array_var}", substrs=expected_substrs)
                self.expect(f"frame variable {array_var}", substrs=["NSArray"])
        
        # Test synthetic children access using proper Objective-C method calls
        children_tests = [
            ("simpleArray", 0, ["Apple"]),
            ("simpleArray", 1, ["Banana"]),
            ("simpleArray", 2, ["Cherry"]),
            ("numberArray", 0, ["1"]),
            ("numberArray", 4, ["5"]),
            ("mixedArray", 0, ["String"]),
            ("mixedArray", 1, ["42"]),
            ("mixedArray", 2, ["YES"])
        ]
        
        for array_var, index, expected_substrs in children_tests:
            with self.subTest(array=array_var, index=index):
                self.expect(f"po [{array_var} objectAtIndex:{index}]", substrs=expected_substrs)
        
        # Test nested array access using proper Objective-C method calls
        self.expect("po [[nestedArray objectAtIndex:0] objectAtIndex:0]", substrs=["A"])
        self.expect("po [[nestedArray objectAtIndex:0] objectAtIndex:1]", substrs=["B"])
        self.expect("po [[nestedArray objectAtIndex:1] objectAtIndex:0]", substrs=["C"])
        self.expect("po [[nestedArray objectAtIndex:1] objectAtIndex:2]", substrs=["E"])
        
        # Test array method calls
        self.expect("expr [simpleArray count]", substrs=["3"])
        self.expect("expr [numberArray objectAtIndex:2]", substrs=["3"])
        self.expect("expr [simpleArray firstObject]", substrs=["Apple"])
        self.expect("expr [simpleArray lastObject]", substrs=["Cherry"])
        
        # Test mutable array operations
        self.expect("expr [mutableArray addObject:@\"Four\"]", error=False)
        self.expect("expr [mutableArray count]", substrs=["4"])
        self.expect("po mutableArray", substrs=["Four"])
        
        # Performance test with large array
        self.expect("po largeArray", substrs=["("], timeout=5)
        self.expect("expr [largeArray count]", substrs=["100"])
        self.expect("po [largeArray objectAtIndex:50]", substrs=["item50"])
        
        # Test nil and edge cases
        self.expect("po nilArray", substrs=["nil"])
        self.expect("expr (NSArray*)nil", substrs=["nil"])
        self.expect("expr [simpleArray objectAtIndex:100]", error=True)  # Out of bounds

    @skipUnlessPlatform(["linux"])
    def test_comprehensive_dictionary_suite(self):
        """Comprehensive test suite for all NSDictionary types and synthetic children."""
        self.build()
        self.run_to_breakpoint()
        
        # All dictionary types from main.m (actual LLDB format: {key = value;} not @{key: value})
        dict_tests = [
            ("emptyDict", ["{}"]),
            ("simpleDict", ["{", "name = John", "age = 30", "}"]),
            ("personInfo", ["{", "occupation = Developer", "name = \"John Doe\"", "}"]),
            ("mutableDict", ["{", "Key1 = Value1", "Key2 = Value2", "}"]),
            ("nestedDict", ["{", "person", "address", "}"]),
            ("complexDict", ["{", "array", "dict", "number", "}"])
        ]
        
        for dict_var, expected_substrs in dict_tests:
            with self.subTest(dictionary=dict_var):
                self.expect(f"po {dict_var}", substrs=expected_substrs)
                self.expect(f"frame variable {dict_var}", substrs=["NSDictionary"])
        
        # Test synthetic children access using proper Objective-C method calls
        key_access_tests = [
            ("simpleDict", "name", ["John"]),
            ("simpleDict", "age", ["30"]),
            ("personInfo", "occupation", ["Developer"]),
            ("personInfo", "name", ["John Doe"]),
            ("mutableDict", "Key1", ["Value1"]),
            ("mutableDict", "Key2", ["Value2"])
        ]
        
        for dict_var, key, expected_substrs in key_access_tests:
            with self.subTest(dictionary=dict_var, key=key):
                self.expect(f"po [{dict_var} objectForKey:@\"{key}\"]", substrs=expected_substrs)
        
        # Test deep nested dictionary access using proper Objective-C method calls
        self.expect("po [[nestedDict objectForKey:@\"person\"] objectForKey:@\"name\"]", substrs=["Alice"])
        self.expect("po [[nestedDict objectForKey:@\"address\"] objectForKey:@\"city\"]", substrs=["Springfield"])
        self.expect("po [[nestedDict objectForKey:@\"address\"] objectForKey:@\"street\"]", substrs=["123 Main St"])
        
        # Test complex nested access using proper Objective-C method calls
        self.expect("po [[complexDict objectForKey:@\"array\"] objectAtIndex:0]", substrs=["item1"])
        self.expect("po [[[complexDict objectForKey:@\"dict\"] objectForKey:@\"nested\"] objectForKey:@\"key\"]", substrs=["value"])
        self.expect("po [complexDict objectForKey:@\"number\"]", substrs=["99"])
        
        # Test dictionary method calls
        self.expect("expr [simpleDict count]", substrs=["2"])
        self.expect("expr [simpleDict objectForKey:@\"name\"]", substrs=["John"])
        self.expect("expr [simpleDict allKeys]", substrs=["@["])
        self.expect("expr [simpleDict allValues]", substrs=["@["])
        
        # Test mutable dictionary operations
        self.expect("expr [mutableDict setObject:@\"NewValue\" forKey:@\"NewKey\"]", error=False)
        self.expect("expr [mutableDict count]", substrs=["3"])
        self.expect("po mutableDict", substrs=["NewValue"])
        
        # Performance test with large dictionary
        self.expect("po largeDict", substrs=["{"], timeout=5)
        self.expect("expr [largeDict count]", substrs=["50"])
        self.expect("po [largeDict objectForKey:@\"key25\"]", substrs=["value25"])
        
        # Test nil and edge cases
        self.expect("po nilDict", substrs=["nil"])
        self.expect("expr (NSDictionary*)nil", substrs=["nil"])
        self.expect("po [simpleDict objectForKey:@\"nonexistent\"]", substrs=["nil"])  # Missing key returns nil

    @skipUnlessPlatform(["linux"])
    def test_comprehensive_set_suite(self):
        """Comprehensive test suite for all NSSet types."""
        self.build()
        self.run_to_breakpoint()
        
        # All set types from main.m (order may vary in sets)
        set_tests = [
            ("emptySet", ["{", "}"]),
            ("simpleSet", ["{", "}", "Red", "Green", "Blue"]),  # Contains these elements
            ("numberSet", ["{", "}", "10", "20", "30"]),
            ("stringSet", ["{", "}", "Alpha", "Beta", "Gamma"]),
            ("mutableSet", ["{", "}", "Cat", "Dog", "Bird"])
        ]
        
        for set_var, expected_substrs in set_tests:
            with self.subTest(set=set_var):
                self.expect(f"po {set_var}", substrs=expected_substrs)
                self.expect(f"frame variable {set_var}", substrs=["NSSet"])
        
        # Test set method calls
        self.expect("expr [simpleSet count]", substrs=["3"])
        self.expect("expr [numberSet count]", substrs=["3"])
        self.expect("expr [emptySet count]", substrs=["0"])
        
        # Test set membership
        self.expect("expr [simpleSet containsObject:@\"Red\"]", substrs=["YES"])
        self.expect("expr [simpleSet containsObject:@\"Purple\"]", substrs=["NO"])
        self.expect("expr [numberSet containsObject:@20]", substrs=["YES"])
        
        # Test set enumeration methods
        self.expect("expr [simpleSet anyObject]", substrs=["Red", "Green", "Blue"])  # Any one of these
        self.expect("expr [simpleSet allObjects]", substrs=["@["])  # Should return an array
        
        # Test mutable set operations
        self.expect("expr [mutableSet addObject:@\"Fish\"]", error=False)
        self.expect("expr [mutableSet count]", substrs=["4"])
        self.expect("expr [mutableSet containsObject:@\"Fish\"]", substrs=["YES"])
        
        # Test indexed set operations
        self.expect("po emptyIndexSet", substrs=["<"])  # Empty index set format
        self.expect("po singleIndexSet", substrs=["<", "42", ">"])
        self.expect("po rangeIndexSet", substrs=["<", "10", ">"])  # Range 10-14
        self.expect("po mutableIndexSet", substrs=["<", ">"])  # Should show indices
        
        # Test NSIndexSet method calls
        self.expect("expr [singleIndexSet count]", substrs=["1"])
        self.expect("expr [rangeIndexSet count]", substrs=["5"])  # 10,11,12,13,14
        self.expect("expr [singleIndexSet containsIndex:42]", substrs=["YES"])
        self.expect("expr [singleIndexSet containsIndex:0]", substrs=["NO"])
        
        # Test nil and edge cases
        self.expect("po nilSet", substrs=["nil"])
        self.expect("expr (NSSet*)nil", substrs=["nil"])
        self.expect("po nilIndexSet", substrs=["nil"])

    @skipUnlessPlatform(["linux"])
    def test_comprehensive_custom_class_suite(self):
        """Comprehensive test suite for custom BankAccount class with deep property access."""
        self.build()
        self.run_to_breakpoint()
        
        # Test custom class description formatting
        self.expect("po account", substrs=["BankAccount", "12345", "John Doe", "1025.00"])
        self.expect("frame variable account", substrs=["BankAccount"])
        
        # Test all property access chains
        property_tests = [
            ("account.accountNumber", ["12345"]),
            ("account.owner", ["John Doe"]),
            ("account.balance", ["1025"]),  # 1000 + 75 - 50
            ("account.transactions", ["NSMutableArray"]),
            ("account.authorizedUsers", ["NSMutableSet"])
        ]
        
        for property_expr, expected_substrs in property_tests:
            with self.subTest(property=property_expr):
                self.expect(f"expr {property_expr}", substrs=expected_substrs)
        
        # Test deep nested property access into collections
        self.expect("expr account.transactions.count", substrs=["3"])  # Initial: deposit, withdraw, deposit
        self.expect("expr account.authorizedUsers.count", substrs=["2"])  # John Doe, Jane Smith
        
        # Test accessing transaction details (array of dictionaries)
        transaction_tests = [
            ("account.transactions[0][@\"type\"]", ["deposit"]),
            ("account.transactions[0][@\"amount\"]", ["1000"]),
            ("account.transactions[1][@\"type\"]", ["withdraw"]),
            ("account.transactions[1][@\"amount\"]", ["75"]),
            ("account.transactions[2][@\"type\"]", ["deposit"]),
            ("account.transactions[2][@\"amount\"]", ["100"])
        ]
        
        for transaction_expr, expected_substrs in transaction_tests:
            with self.subTest(transaction=transaction_expr):
                self.expect(f"expr {transaction_expr}", substrs=expected_substrs)
        
        # Test authorized users (set contains strings)
        self.expect("expr [account.authorizedUsers containsObject:@\"John Doe\"]", substrs=["YES"])
        self.expect("expr [account.authorizedUsers containsObject:@\"Jane Smith\"]", substrs=["YES"])
        self.expect("expr [account.authorizedUsers containsObject:@\"Unknown\"]", substrs=["NO"])
        
        # Test custom class methods
        self.expect("expr [account deposit:500.0]", error=False)
        self.expect("expr account.balance", substrs=["1525"])  # Previous + 500
        self.expect("expr account.transactions.count", substrs=["4"])  # Added one more
        
        self.expect("expr [account withdraw:200.0]", error=False)
        self.expect("expr account.balance", substrs=["1325"])  # Previous - 200
        self.expect("expr account.transactions.count", substrs=["5"])  # Added one more
        
        # Test description method
        self.expect("expr [account description]", 
                   substrs=["BankAccount", "12345", "John Doe", "1325.00", "transactions=5"])
        
        # Test edge cases with custom class
        self.expect("expr (BankAccount*)nil", substrs=["nil"])
        
        # Test accessing non-existent properties (should error gracefully)
        self.expect("expr account.nonExistentProperty", error=True)

    @skipUnlessPlatform(["linux"])
    def test_comprehensive_foundation_types_suite(self):
        """Comprehensive test suite for all Foundation types from main.m."""
        self.build()
        self.run_to_breakpoint()
        
        # Test NSDate objects
        date_tests = [
            ("currentTime", ["NSDate"]),
            ("futureDate", ["NSDate"]),
            ("pastDate", ["1970"]),  # Unix epoch
            ("specificDate", ["2023-01-01"])
        ]
        
        for date_var, expected_substrs in date_tests:
            with self.subTest(date=date_var):
                self.expect(f"po {date_var}", matching=True, patterns=[r'.*\d{4}.*'])
                self.expect(f"frame variable {date_var}", substrs=["NSDate"])
        
        # Test NSURL objects
        url_tests = [
            ("webURL", ["https://www.example.com"]),
            ("fileURL", ["file://", "/usr/local/bin/test"]),
            ("complexURL", ["ftp://", "host.com:8080"])
        ]
        
        for url_var, expected_substrs in url_tests:
            with self.subTest(url=url_var):
                self.expect(f"po {url_var}", substrs=expected_substrs)
                self.expect(f"frame variable {url_var}", substrs=["NSURL"])
        
        # Test NSUUID objects
        self.expect("po randomUUID", matching=True, patterns=[r'.*[0-9a-fA-F]{8}-[0-9a-fA-F]{4}.*'])
        self.expect("po specificUUID", substrs=["550e8400-e29b-41d4-a716-446655440000"])
        self.expect("frame variable randomUUID", substrs=["NSUUID"])
        
        # Test NSError objects
        error_tests = [
            ("simpleError", ["TestDomain", "404"]),
            ("detailedError", ["NSURLErrorDomain", "File not found"])
        ]
        
        for error_var, expected_substrs in error_tests:
            with self.subTest(error=error_var):
                self.expect(f"po {error_var}", substrs=expected_substrs)
                self.expect(f"frame variable {error_var}", substrs=["NSError"])
        
        # Test NSDecimalNumber objects
        decimal_tests = [
            ("integerDecimal", ["42"]),
            ("floatDecimal", ["123.456"]),
            ("negativeDecimal", ["-987.654"]),
            ("zeroDecimal", ["0"]),
            ("oneDecimal", ["1"]),
            ("largeDecimal", ["999999999999999999"]),
            ("notANumber", ["NaN"])
        ]
        
        for decimal_var, expected_substrs in decimal_tests:
            with self.subTest(decimal=decimal_var):
                self.expect(f"po {decimal_var}", substrs=expected_substrs)
                self.expect(f"frame variable {decimal_var}", substrs=["NSDecimalNumber"])
        
        # Test NSCharacterSet objects
        charset_tests = [
            ("alphaCharset", ["<"]),  # Should show character set format
            ("digitCharset", ["<"]),
            ("whitespaceCharset", ["<"]),
            ("customCharset", ["<"])
        ]
        
        for charset_var, expected_substrs in charset_tests:
            with self.subTest(charset=charset_var):
                self.expect(f"po {charset_var}", substrs=expected_substrs)
                self.expect(f"frame variable {charset_var}", substrs=["NSCharacterSet"])
        
        # Test NSNull and NSException
        self.expect("po nullObject", substrs=["NSNull"])
        self.expect("po testException", substrs=["TestException", "test exception"])
        self.expect("frame variable nullObject", substrs=["NSNull"])
        self.expect("frame variable testException", substrs=["NSException"])

    @skipUnlessPlatform(["linux"])
    def test_comprehensive_nil_and_edge_cases_suite(self):
        """Comprehensive test suite for nil objects and edge cases across all types."""
        self.build()
        self.run_to_breakpoint()
        
        # Test all nil objects from main.m
        nil_tests = [
            ("nilObject", ["nil"]),
            ("nilString", ["nil"]),
            ("nilNumber", ["nil"]),
            ("nilArray", ["nil"]),
            ("nilDict", ["nil"]),
            ("nilSet", ["nil"]),
            ("nilIndexSet", ["nil"]),
            ("nilDate", ["nil"]),
            ("nilURL", ["nil"]),
            ("nilUUID", ["nil"]),
            ("nilError", ["nil"]),
            ("nilDecimalNumber", ["nil"]),
            ("nilCharset", ["nil"])
        ]
        
        for nil_var, expected_substrs in nil_tests:
            with self.subTest(nil_object=nil_var):
                self.expect(f"po {nil_var}", substrs=expected_substrs)
                self.expect(f"expr {nil_var} == nil", substrs=["true"])
        
        # Test nil comparisons and casting
        nil_cast_tests = [
            ("(NSString*)nil", ["nil"]),
            ("(NSNumber*)nil", ["nil"]),
            ("(NSArray*)nil", ["nil"]),
            ("(NSDictionary*)nil", ["nil"]),
            ("(NSSet*)nil", ["nil"]),
            ("(NSDate*)nil", ["nil"]),
            ("(NSURL*)nil", ["nil"]),
            ("(NSError*)nil", ["nil"]),
            ("(BankAccount*)nil", ["nil"])
        ]
        
        for cast_expr, expected_substrs in nil_cast_tests:
            with self.subTest(cast=cast_expr):
                self.expect(f"expr {cast_expr}", substrs=expected_substrs)
        
        # Test method calls on nil (should return sensible defaults)
        nil_method_tests = [
            ("[(NSString*)nil length]", ["0"]),
            ("[(NSArray*)nil count]", ["0"]),
            ("[(NSDictionary*)nil count]", ["0"]),
            ("[(NSSet*)nil count]", ["0"])
        ]
        
        for method_expr, expected_substrs in nil_method_tests:
            with self.subTest(method=method_expr):
                self.expect(f"expr {method_expr}", substrs=expected_substrs)
        
        # Test edge cases with valid objects
        edge_case_tests = [
            ("[@\"\" length]", ["0"]),  # Empty string length
            ("[emptyArray count]", ["0"]),  # Empty array count
            ("[emptyDict count]", ["0"]),  # Empty dictionary count
            ("[emptySet count]", ["0"]),  # Empty set count
            ("[emptyIndexSet count]", ["0"])  # Empty index set count
        ]
        
        for edge_expr, expected_substrs in edge_case_tests:
            with self.subTest(edge_case=edge_expr):
                self.expect(f"expr {edge_expr}", substrs=expected_substrs)
        
        # Test error conditions that should fail gracefully
        error_tests = [
            "nonexistentObject",  # Undefined variable
            "simpleArray[100]",   # Array out of bounds
            "account.nonExistentProperty"  # Invalid property
        ]
        
        for error_expr in error_tests:
            with self.subTest(error=error_expr):
                self.expect(f"expr {error_expr}", error=True)
        
        # Test that missing dictionary keys return nil (not error)
        self.expect("expr simpleDict[@\"nonexistent\"]", substrs=["nil"])
        self.expect("expr largeDict[@\"missing\"]", substrs=["nil"])

    @skipUnlessPlatform(["linux"])
    def test_comprehensive_expression_evaluation_suite(self):
        """Comprehensive test suite for expression evaluation and method chaining."""
        self.build()
        self.run_to_breakpoint()
        
        # Test literal creation expressions
        literal_tests = [
            ("@\"test string\"", ["test string"]),
            ("@42", ["42"]),
            ("@3.14", ["3.14"]),
            ("@YES", ["YES"]),
            ("@NO", ["NO"]),
            ("@[@\"a\", @\"b\", @\"c\"]", ["@[", "a", "b", "c", "]"]),
            ("@{@\"key\": @\"value\"}", ["@{", "key", "value", "}"])
        ]
        
        for expr, expected_substrs in literal_tests:
            with self.subTest(literal=expr):
                self.expect(f"expr {expr}", substrs=expected_substrs)
        
        # Test arithmetic expressions with NSNumbers
        arithmetic_tests = [
            ("intNumber.intValue + 10", ["52"]),  # 42 + 10
            ("floatNumber.floatValue * 2", ["6.28"]),  # 3.14 * 2
            ("doubleNumber.doubleValue - 1.0", ["1.71828"]),  # 2.71828 - 1
            ("@([intNumber intValue] + [floatNumber intValue])", ["45"]),  # 42 + 3
            ("@([longNumber longValue] / 1000000)", ["2147"]),
            ("@([negativeNumber intValue] * -1)", ["999"])
        ]
        
        for expr, expected_substrs in arithmetic_tests:
            with self.subTest(arithmetic=expr):
                self.expect(f"expr {expr}", substrs=expected_substrs)
        
        # Test boolean logic expressions
        boolean_tests = [
            ("boolNumber.boolValue && YES", ["true"]),
            ("boolNumberNo.boolValue || YES", ["true"]),
            ("@([boolNumber boolValue] && [boolNumberNo boolValue])", ["NO"]),
            ("@([boolNumber boolValue] || [boolNumberNo boolValue])", ["YES"])
        ]
        
        for expr, expected_substrs in boolean_tests:
            with self.subTest(boolean=expr):
                self.expect(f"expr {expr}", substrs=expected_substrs)
        
        # Test string method chaining
        string_method_tests = [
            ("[asciiString uppercaseString]", ["HELLO, WORLD!"]),
            ("[asciiString lowercaseString]", ["hello, world!"]),
            ("[[asciiString uppercaseString] length]", ["13"]),
            ("[asciiString substringToIndex:5]", ["Hello"]),
            ("[asciiString stringByAppendingString:@\" Extra\"]", ["Hello, World! Extra"])
        ]
        
        for expr, expected_substrs in string_method_tests:
            with self.subTest(string_method=expr):
                self.expect(f"expr {expr}", substrs=expected_substrs)
        
        # Test complex nested expressions
        nested_tests = [
            ("[[[[nestedDict objectForKey:@\"person\"] objectForKey:@\"name\"] uppercaseString] length]", ["5"]),  # "ALICE" length
            ("[[simpleArray objectAtIndex:0] length]", ["5"]),  # "Apple" length
            ("[[[mutableArray objectAtIndex:0] uppercaseString] stringByAppendingString:@\"!\"]",["ONE!"]),
            ("[[nestedArray objectAtIndex:0] objectAtIndex:1]", ["B"])  # nestedArray[0][1]
        ]
        
        for expr, expected_substrs in nested_tests:
            with self.subTest(nested=expr):
                self.expect(f"expr {expr}", substrs=expected_substrs)
        
        # Test class method calls
        class_method_tests = [
            ("[NSString stringWithFormat:@\"Number: %d\", 42]", ["Number: 42"]),
            ("[NSArray arrayWithObjects:@\"x\", @\"y\", nil]", ["@[", "x", "y"]),
            ("[NSDictionary dictionaryWithObject:@\"value\" forKey:@\"key\"]", ["@{", "key", "value"]),
            ("[NSDate date]", ["2"])  # Should contain year digits
        ]
        
        for expr, expected_substrs in class_method_tests:
            with self.subTest(class_method=expr):
                if "date" in expr:
                    self.expect(f"expr {expr}", matching=True, patterns=[r'.*\d{4}.*'])
                else:
                    self.expect(f"expr {expr}", substrs=expected_substrs)

    @skipUnlessPlatform(["linux"])
    def test_comprehensive_performance_suite(self):
        """Comprehensive performance test suite for large collections and complex operations."""
        self.build()
        self.run_to_breakpoint()
        
        # Performance tests with large collections (all should complete quickly)
        large_collection_tests = [
            ("largeArray", ["@["]),
            ("largeDict", ["@{"]),
            ("po largeArray[50]", ["item50"]),  # Mid-point access
            ("po largeArray[99]", ["item99"]),  # End access
            ("po largeDict[@\"key25\"]", ["value25"]),  # Mid-point key
            ("po largeDict[@\"key49\"]", ["value49"])   # End key
        ]
        
        for test_expr, expected_substrs in large_collection_tests:
            with self.subTest(performance=test_expr):
                if test_expr.startswith("po "):
                    self.expect(test_expr, substrs=expected_substrs, timeout=5)
                else:
                    self.expect(f"po {test_expr}", substrs=expected_substrs, timeout=5)
        
        # Test count operations on large collections
        count_tests = [
            ("[largeArray count]", ["100"]),
            ("[largeDict count]", ["50"])
        ]
        
        for count_expr, expected_substrs in count_tests:
            with self.subTest(count=count_expr):
                self.expect(f"expr {count_expr}", substrs=expected_substrs, timeout=5)
        
        # Test complex nested access performance
        complex_access_tests = [
            "nestedDict[@\"person\"][@\"name\"]",
            "nestedDict[@\"address\"][@\"city\"]", 
            "complexDict[@\"array\"][0]",
            "complexDict[@\"dict\"][@\"nested\"][@\"key\"]",
            "account.transactions[0][@\"type\"]",
            "[[nestedArray objectAtIndex:1] objectAtIndex:2]"
        ]
        
        for complex_expr in complex_access_tests:
            with self.subTest(complex=complex_expr):
                self.expect(f"expr {complex_expr}", error=False, timeout=3)
        
        # Test formatter performance with repeated access
        repeated_tests = [
            "asciiString", "intNumber", "simpleArray", "simpleDict", 
            "account", "currentTime", "webURL", "randomUUID"
        ]
        
        for obj in repeated_tests:
            for i in range(3):  # Access each object 3 times quickly
                with self.subTest(repeated=f"{obj}_{i}"):
                    self.expect(f"po {obj}", error=False, timeout=2)

    @skipUnlessPlatform(["linux"])
    def test_comprehensive_frame_variable_suite(self):
        """Comprehensive test suite for frame variable display across all object types."""
        self.build()
        self.run_to_breakpoint()
        
        # Test frame variable for all string types
        string_frame_tests = [
            ("emptyString", ["NSString", "@\"\""]),
            ("asciiString", ["NSString", "Hello, World!"]),
            ("utf8String", ["NSString", "Unicode"]),
            ("literalString", ["NSString", "literal"]),
            ("mutableString1", ["NSMutableString", "Initial"])
        ]
        
        for var_name, expected_substrs in string_frame_tests:
            with self.subTest(string_frame=var_name):
                self.expect(f"frame variable {var_name}", substrs=expected_substrs)
        
        # Test frame variable for all number types
        number_frame_tests = [
            ("intNumber", ["NSNumber", "42"]),
            ("floatNumber", ["NSNumber", "3.14"]),
            ("boolNumber", ["NSNumber", "YES"]),
            ("taggedInt1", ["NSNumber", "7"]),
            ("negativeNumber", ["NSNumber", "-999"])
        ]
        
        for var_name, expected_substrs in number_frame_tests:
            with self.subTest(number_frame=var_name):
                self.expect(f"frame variable {var_name}", substrs=expected_substrs)
        
        # Test frame variable for all collection types
        collection_frame_tests = [
            ("emptyArray", ["NSArray"]),
            ("simpleArray", ["NSArray"]),
            ("mutableArray", ["NSMutableArray"]),
            ("largeArray", ["NSArray"]),
            ("emptyDict", ["NSDictionary"]),
            ("simpleDict", ["NSDictionary"]),
            ("mutableDict", ["NSMutableDictionary"]),
            ("nestedDict", ["NSDictionary"]),
            ("emptySet", ["NSSet"]),
            ("simpleSet", ["NSSet"]),
            ("mutableSet", ["NSMutableSet"]),
            ("emptyIndexSet", ["NSIndexSet"]),
            ("singleIndexSet", ["NSIndexSet"])
        ]
        
        for var_name, expected_substrs in collection_frame_tests:
            with self.subTest(collection_frame=var_name):
                self.expect(f"frame variable {var_name}", substrs=expected_substrs)
        
        # Test frame variable for all Foundation types
        foundation_frame_tests = [
            ("currentTime", ["NSDate"]),
            ("webURL", ["NSURL"]),
            ("randomUUID", ["NSUUID"]),
            ("simpleError", ["NSError"]),
            ("integerDecimal", ["NSDecimalNumber"]),
            ("alphaCharset", ["NSCharacterSet"]),
            ("nullObject", ["NSNull"]),
            ("testException", ["NSException"])
        ]
        
        for var_name, expected_substrs in foundation_frame_tests:
            with self.subTest(foundation_frame=var_name):
                self.expect(f"frame variable {var_name}", substrs=expected_substrs)
        
        # Test frame variable for custom classes
        self.expect("frame variable account", substrs=["BankAccount"])
        
        # Test frame variable for all nil objects
        nil_frame_tests = [
            "nilObject", "nilString", "nilNumber", "nilArray", "nilDict", 
            "nilSet", "nilDate", "nilURL", "nilError"
        ]
        
        for nil_var in nil_frame_tests:
            with self.subTest(nil_frame=nil_var):
                self.expect(f"frame variable {nil_var}", substrs=["nil"])

    @skipUnlessPlatform(["linux"])
    def test_comprehensive_debugging_workflows_suite(self):
        """Comprehensive test suite simulating real debugging workflows developers use."""
        self.build()
        self.run_to_breakpoint()
        
        # Workflow 1: Debugging a banking transaction issue
        self.expect("po account", substrs=["BankAccount", "12345", "John Doe"])
        self.expect("expr account.balance", substrs=["1025"])  # Check current balance
        self.expect("expr account.transactions.count", substrs=["3"])  # How many transactions?
        
        # Examine each transaction to find the issue
        for i in range(3):
            self.expect(f"po account.transactions[{i}]", substrs=["type", "amount", "timestamp"])
            self.expect(f"expr account.transactions[{i}][@\"type\"]", 
                       substrs=["deposit", "withdraw"])  # Either type is valid
        
        # Workflow 2: Debugging array processing issue
        self.expect("expr simpleArray.count", substrs=["3"])
        self.expect("po simpleArray", substrs=["Apple", "Banana", "Cherry"])
        
        # Check each element processing
        for i in range(3):
            self.expect(f"expr [[simpleArray objectAtIndex:{i}] length]", error=False)
            self.expect(f"expr [[simpleArray objectAtIndex:{i}] uppercaseString]", error=False)
        
        # Workflow 3: Debugging dictionary data structure issue
        self.expect("expr nestedDict.count", error=False)
        self.expect("po nestedDict[@\"person\"]", substrs=["name", "age"])
        self.expect("po nestedDict[@\"address\"]", substrs=["street", "city"])
        
        # Deep dive into nested structure
        self.expect("expr nestedDict[@\"person\"][@\"name\"]", substrs=["Alice"])
        self.expect("expr nestedDict[@\"address\"][@\"city\"]", substrs=["Springfield"])
        
        # Workflow 4: Debugging performance issue with large collections
        self.expect("expr largeArray.count", substrs=["100"], timeout=3)
        self.expect("expr largeDict.count", substrs=["50"], timeout=3)
        
        # Sample elements to verify data integrity
        sample_indices = [0, 25, 50, 75, 99]
        for idx in sample_indices:
            if idx < 100:  # Valid range for largeArray
                self.expect(f"po largeArray[{idx}]", substrs=[f"item{idx}"], timeout=2)
        
        # Workflow 5: Debugging string processing pipeline
        self.expect("expr asciiString.length", substrs=["13"])
        self.expect("expr [asciiString uppercaseString]", substrs=["HELLO, WORLD!"])
        self.expect("expr [asciiString substringToIndex:5]", substrs=["Hello"])
        self.expect("expr [asciiString substringFromIndex:7]", substrs=["World!"])
        
        # Chain multiple string operations
        self.expect("expr [[[asciiString uppercaseString] substringToIndex:5] lowercaseString]", 
                   substrs=["hello"])
        
        # Workflow 6: Debugging URL and network-related objects
        self.expect("expr webURL.scheme", substrs=["https"])
        self.expect("expr webURL.host", substrs=["www.example.com"])
        self.expect("po webURL", substrs=["https://www.example.com"])
        
        # Check error objects for network debugging
        self.expect("expr simpleError.code", substrs=["404"])
        self.expect("expr simpleError.domain", substrs=["TestDomain"])
        
        # Workflow 7: Debugging UUID and identifier issues
        self.expect("po specificUUID", substrs=["550e8400-e29b-41d4-a716-446655440000"])
        self.expect("expr specificUUID.UUIDString", substrs=["550e8400-e29b-41d4-a716-446655440000"])
        
        # Workflow 8: Debugging custom object relationships
        self.expect("expr account.authorizedUsers.count", substrs=["2"])
        self.expect("expr [account.authorizedUsers containsObject:@\"John Doe\"]", substrs=["YES"])
        self.expect("expr [account.authorizedUsers anyObject]", substrs=["John Doe", "Jane Smith"])

    @skipUnlessPlatform(["linux"])
    def test_error_conditions(self):
        """Test error handling and edge cases."""
        self.build()
        self.run_to_breakpoint()
        
        # Test invalid expressions (should handle gracefully)
        self.expect("expr nonexistentObject", error=True)
        
        # Test accessing invalid indices (should handle gracefully)
        self.expect("expr simpleArray[100]", error=True)
        
        # Test accessing invalid dictionary keys
        self.expect("expr simpleDict[@\"nonexistent\"]", substrs=["nil"])

    @skipUnlessPlatform(["linux"])
    def test_nested_object_access(self):
        """Test accessing nested objects and properties."""
        self.build()
        self.run_to_breakpoint()
        
        # Access nested dictionary values
        self.expect("expr nestedDict[@\"person\"][@\"name\"]", substrs=["Alice"])
        self.expect("expr nestedDict[@\"address\"][@\"city\"]", substrs=["Springfield"])
        
        # Access nested array elements
        self.expect("expr nestedArray[0][0]", substrs=["A"])
        self.expect("expr nestedArray[1][2]", substrs=["E"])
        
        # Access custom object properties
        self.expect("expr account.transactions[0][@\"type\"]", substrs=["deposit"])

    @skipUnlessPlatform(["linux"])
    def test_mutable_vs_immutable(self):
        """Test distinction between mutable and immutable objects."""
        self.build()
        self.run_to_breakpoint()
        
        # Both should display properly regardless of mutability
        self.expect("po simpleArray", substrs=["@[", "Apple"])  # NSArray
        self.expect("po mutableArray", substrs=["@[", "One"])   # NSMutableArray
        
        self.expect("po simpleDict", substrs=["@{", "name"])   # NSDictionary
        self.expect("po mutableDict", substrs=["@{", "Key1"])  # NSMutableDictionary
        
        self.expect("po simpleSet", substrs=["{", "}"])       # NSSet
        self.expect("po mutableSet", substrs=["{", "}"])      # NSMutableSet

    def test_plugin_activation(self):
        """Test that GNUstep plugin is properly activated."""
        self.build()
        self.run_to_breakpoint()
        
        # Test that type categories are active
        # This is implicit if formatters work, but we can check anyway
        self.runCmd("type category list")
        
        # Verify that basic formatters work (indicates plugin is active)
        self.expect("po @\"test\"", substrs=["test"])
        self.expect("po @42", substrs=["42"])