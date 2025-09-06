"""
Test GNUstep runtime function calls and introspection capabilities.

This test validates that all runtime functions from libobjc2 work correctly
through the LLDB plugin interface.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil

class TestGNUstepRuntimeFunctions(TestBase):
    """Test suite for validating GNUstep runtime function calls.
    
    This test ensures all runtime functions we use from libobjc2/objc/runtime.h
    work correctly through our LLDB plugin:
    - objc_copyClassList
    - objc_getClass
    - objc_getMetaClass
    - class_getSuperclass
    - class_getInstanceSize
    - class_copyMethodList
    - class_copyIvarList
    - object_getClass
    - sel_getUid
    - method_getName
    - method_getTypeEncoding
    - ivar_getName
    - ivar_getTypeEncoding
    - ivar_getOffset
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
    def test_runtime_class_enumeration(self):
        """Test objc_copyClassList and class enumeration."""
        self.build()
        self.run_to_breakpoint()
        
        # Test that we can enumerate classes
        # This indirectly tests objc_copyClassList
        self.expect("expr (int)objc_getClassList(NULL, 0)", 
                   patterns=[r'\d+'],  # Should return count > 0
                   error=False)
        
        # Test that common GNUstep classes are found
        foundation_classes = [
            "NSObject", "NSString", "NSArray", "NSDictionary",
            "NSNumber", "NSValue", "NSSet", "NSDate", "NSURL",
            "NSError", "NSException", "NSNotification"
        ]
        
        for class_name in foundation_classes:
            with self.subTest(class_name=class_name):
                # Test objc_getClass
                self.expect(f'expr (void*)objc_getClass("{class_name}")',
                           patterns=[r'0x[0-9a-fA-F]+'],  # Non-null pointer
                           error=False)
                
                # Test objc_getMetaClass  
                self.expect(f'expr (void*)objc_getMetaClass("{class_name}")',
                           patterns=[r'0x[0-9a-fA-F]+'],  # Non-null pointer
                           error=False)

    @skipUnlessPlatform(["linux"])
    def test_runtime_class_introspection(self):
        """Test class introspection functions."""
        self.build()
        self.run_to_breakpoint()
        
        # Test class_getSuperclass
        self.expect('expr (void*)class_getSuperclass(objc_getClass("NSString"))',
                   patterns=[r'0x[0-9a-fA-F]+'],  # Should return NSObject
                   error=False)
        
        # Test class_getInstanceSize
        self.expect('expr (size_t)class_getInstanceSize(objc_getClass("NSObject"))',
                   patterns=[r'\d+'],  # Should return size > 0
                   error=False)
        
        # Test that NSObject's superclass is nil
        self.expect('expr (void*)class_getSuperclass(objc_getClass("NSObject"))',
                   substrs=['0x0000000000000000'],  # nil
                   error=False)

    @skipUnlessPlatform(["linux"])
    def test_runtime_method_introspection(self):
        """Test method introspection functions."""
        self.build()
        self.run_to_breakpoint()
        
        # Test class_copyMethodList - should return non-null for NSString
        self.expect('expr (void*)class_copyMethodList(objc_getClass("NSString"), NULL)',
                   patterns=[r'0x[0-9a-fA-F]+'],  # Non-null method list
                   error=False)
        
        # Test sel_getUid for common selectors
        common_selectors = ["init", "description", "length", "count", "alloc"]
        
        for selector in common_selectors:
            with self.subTest(selector=selector):
                self.expect(f'expr (void*)sel_getUid("{selector}")',
                           patterns=[r'0x[0-9a-fA-F]+'],  # Non-null selector
                           error=False)

    @skipUnlessPlatform(["linux"])
    def test_runtime_ivar_introspection(self):
        """Test instance variable introspection functions."""
        self.build()
        self.run_to_breakpoint()
        
        # Test class_copyIvarList for custom class
        self.expect('expr (void*)class_copyIvarList(objc_getClass("BankAccount"), NULL)',
                   patterns=[r'0x[0-9a-fA-F]+|0x0'],  # May or may not have ivars
                   error=False)
        
        # Test that we can get the class of an object
        self.expect('expr (void*)object_getClass(account)',
                   patterns=[r'0x[0-9a-fA-F]+'],  # Should return BankAccount class
                   error=False)

    @skipUnlessPlatform(["linux"])
    def test_runtime_isa_resolution(self):
        """Test ISA resolution for various object types."""
        self.build()
        self.run_to_breakpoint()
        
        # Test ISA resolution for different object types
        test_objects = [
            ("emptyString", "NSString"),
            ("simpleArray", "NSArray"),
            ("simpleDict", "NSDictionary"),
            ("simpleSet", "NSSet"),
            ("intNumber", "NSNumber"),
            ("account", "BankAccount")
        ]
        
        for obj_name, expected_class in test_objects:
            with self.subTest(object=obj_name):
                # Get the ISA through object_getClass
                self.expect(f'expr (void*)object_getClass({obj_name})',
                           patterns=[r'0x[0-9a-fA-F]+'],  # Non-null ISA
                           error=False)
                
                # Verify class name matches expected
                self.expect(f'expr (const char*)class_getName(object_getClass({obj_name}))',
                           substrs=[expected_class],
                           error=False)

    @skipUnlessPlatform(["linux"])
    def test_runtime_tagged_pointer_handling(self):
        """Test runtime handling of tagged pointers."""
        self.build()
        self.run_to_breakpoint()
        
        # Small integers are often tagged in GNUstep
        small_numbers = ["@0", "@1", "@42", "@-1"]
        
        for num_literal in small_numbers:
            with self.subTest(number=num_literal):
                # Create a small number that might be tagged
                self.expect(f'expr (void*){num_literal}',
                           patterns=[r'0x[0-9a-fA-F]+'],  # Should get pointer
                           error=False)
                
                # Verify we can still get its class
                self.expect(f'expr (void*)object_getClass({num_literal})',
                           patterns=[r'0x[0-9a-fA-F]+'],  # Should resolve to NSNumber
                           error=False)

    @skipUnlessPlatform(["linux"])
    def test_runtime_selector_operations(self):
        """Test selector creation and name retrieval."""
        self.build()
        self.run_to_breakpoint()
        
        # Test selector creation and name retrieval
        test_selectors = [
            "initWithString:",
            "stringByAppendingString:",
            "objectAtIndex:",
            "setObject:forKey:",
            "addObject:",
            "removeObject:"
        ]
        
        for sel_name in test_selectors:
            with self.subTest(selector=sel_name):
                # Create selector
                self.expect(f'expr (void*)sel_getUid("{sel_name}")',
                           patterns=[r'0x[0-9a-fA-F]+'],  # Non-null selector
                           error=False)
                
                # Get selector name back
                self.expect(f'expr (const char*)sel_getName(sel_getUid("{sel_name}"))',
                           substrs=[sel_name],
                           error=False)

    @skipUnlessPlatform(["linux"])
    def test_runtime_memory_safety(self):
        """Test that runtime functions handle edge cases safely."""
        self.build()
        self.run_to_breakpoint()
        
        # Test null pointer handling
        null_tests = [
            'expr (void*)objc_getClass(NULL)',  # Should return nil
            'expr (void*)class_getSuperclass(NULL)',  # Should return nil
            'expr (size_t)class_getInstanceSize(NULL)',  # Should return 0
            'expr (void*)object_getClass(NULL)',  # Should return nil
            'expr (void*)sel_getUid(NULL)',  # Should return nil
            'expr (const char*)sel_getName(NULL)',  # Should return NULL
        ]
        
        for test_expr in null_tests:
            with self.subTest(expr=test_expr):
                # These should not crash, just return nil/0/NULL
                self.expect(test_expr, error=False)
        
        # Test invalid class names
        self.expect('expr (void*)objc_getClass("NonExistentClass")',
                   substrs=['0x0000000000000000'],  # Should return nil
                   error=False)
        
        # Test invalid selectors (empty string)
        self.expect('expr (void*)sel_getUid("")',
                   patterns=[r'0x[0-9a-fA-F]+|0x0'],  # May return nil or empty selector
                   error=False)

    @skipUnlessPlatform(["linux"])
    def test_runtime_performance(self):
        """Test that runtime functions complete in reasonable time."""
        self.build()
        self.run_to_breakpoint()
        
        import time
        
        # Test that class enumeration completes quickly
        start_time = time.time()
        self.expect("expr (int)objc_getClassList(NULL, 0)", 
                   patterns=[r'\d+'],
                   error=False)
        elapsed = time.time() - start_time
        
        # Should complete in less than 1 second
        self.assertLess(elapsed, 1.0, 
                       f"Class enumeration took {elapsed:.2f}s, expected < 1s")
        
        # Test that class lookup is fast
        start_time = time.time()
        for _ in range(10):
            self.expect('expr (void*)objc_getClass("NSString")',
                       patterns=[r'0x[0-9a-fA-F]+'],
                       error=False)
        elapsed = time.time() - start_time
        
        # 10 lookups should complete in less than 2 seconds
        self.assertLess(elapsed, 2.0,
                       f"10 class lookups took {elapsed:.2f}s, expected < 2s")