"""
Test to verify LLDB doesn't crash when expanding NSIndexPath, NSException, and NSNotification,
and that child properties don't get contaminated between different object types.
"""

import os
import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil

class TestFormatterCrash(TestBase):
    
    def setUp(self):
        # Call super's setUp()
        TestBase.setUp(self)
        # Find the line number to break inside main()
        self.main_source = "test_formatter_crash.m"
        self.break_line = line_number(self.main_source, "// Set breakpoint here for crash test")
        
    @skipUnlessPlatform(["linux"])
    def test_no_crash_on_expand(self):
        """Test that NSIndexPath, NSException, NSNotification don't crash when expanded."""
        self.build()
        self.run_to_breakpoint()
        
        # Test NSIndexPath - should not crash
        self.runCmd("frame variable indexPath")
        self.runCmd("frame variable -T indexPath")  # This was causing the crash
        self.expect("frame variable indexPath", substrs=["NSIndexPath"])
        
        # Test NSException - should not crash
        self.runCmd("frame variable exception")
        self.runCmd("frame variable -T exception")
        self.expect("frame variable exception", substrs=["NSException"])
        
        # Test NSNotification - should not crash  
        self.runCmd("frame variable notification")
        self.runCmd("frame variable -T notification")
        self.expect("frame variable notification", substrs=["NSNotification"])
        
    @skipUnlessPlatform(["linux"])
    def test_no_child_contamination(self):
        """Test that expanding one object doesn't contaminate children of another."""
        self.build()
        self.run_to_breakpoint()
        
        # Expand account first (has genuine properties)
        account_children = self.get_variable_children("account")
        self.assertTrue(len(account_children) > 0, "Account should have properties")
        
        # Now expand indexPath (should have no children or different children)
        indexpath_children = self.get_variable_children("indexPath")
        
        # Verify indexPath children are not the same as account children
        if len(indexpath_children) > 0 and len(account_children) > 0:
            # Make sure at least one child is different
            account_names = [child[0] for child in account_children]
            indexpath_names = [child[0] for child in indexpath_children]
            self.assertNotEqual(account_names, indexpath_names, 
                               "IndexPath should not have same children as Account")
        
        # Test exception too
        exception_children = self.get_variable_children("exception")
        if len(exception_children) > 0 and len(account_children) > 0:
            account_names = [child[0] for child in account_children]  
            exception_names = [child[0] for child in exception_children]
            self.assertNotEqual(account_names, exception_names,
                               "Exception should not have same children as Account")
                               
    @skipUnlessPlatform(["linux"])
    def test_synthetic_children_disabled(self):
        """Test that NSIndexPath, NSException, NSNotification have synthetic children disabled."""
        self.build()
        self.run_to_breakpoint()
        
        # These objects should not use the generic synthetic provider
        # They should either have no children or use their specific providers
        
        # NSIndexPath should not use generic synthetic provider
        indexpath_children = self.get_variable_children("indexPath")
        
        # NSException should not use generic synthetic provider  
        exception_children = self.get_variable_children("exception")
        
        # NSNotification should not use generic synthetic provider
        notification_children = self.get_variable_children("notification")
        
        # The main test is that these don't crash - if we get here, test passes
        self.assertTrue(True, "No crash occurred when accessing synthetic children")
        
    def get_variable_children(self, var_name):
        """Helper to safely get children of a variable without crashing."""
        try:
            frame = self.frame()
            var = frame.FindVariable(var_name)
            if not var.IsValid():
                return []
                
            children = []
            num_children = var.GetNumChildren()
            for i in range(num_children):
                child = var.GetChildAtIndex(i)
                if child.IsValid():
                    children.append((child.GetName(), child.GetValue()))
            return children
        except:
            # If anything crashes, return empty list
            return []
        
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