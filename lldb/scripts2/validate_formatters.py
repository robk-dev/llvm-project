#!/usr/bin/env python3
"""
validate_formatters.py - Validate GNUstep formatter output

Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
See https://llvm.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

This script validates that GNUstep formatters produce correct output for
various object types.
"""

import os
import sys
import subprocess
import re
import json
import argparse
from dataclasses import dataclass
from typing import List, Dict, Optional, Tuple
from pathlib import Path

# Add LLDB Python path
sys.path.append('../build/lib/python3/dist-packages')

try:
    import lldb
except ImportError:
    print("Error: Could not import lldb module. Make sure LLDB is built and PYTHONPATH is set.")
    sys.exit(1)

# Color codes for terminal output
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'  # No Color

@dataclass
class TestCase:
    """Represents a formatter test case"""
    name: str
    variable: str
    expected_patterns: List[str]
    should_contain: bool = True
    is_regex: bool = False

@dataclass
class TestResult:
    """Result of a single test"""
    test_case: TestCase
    passed: bool
    actual_output: str
    error_message: Optional[str] = None

class FormatterValidator:
    """Validates GNUstep formatter output"""
    
    def __init__(self, lldb_path: str, test_program: str):
        self.lldb_path = lldb_path
        self.test_program = test_program
        self.debugger = None
        self.target = None
        self.process = None
        self.results: List[TestResult] = []
        
    def setup_debugger(self) -> bool:
        """Initialize LLDB debugger"""
        try:
            self.debugger = lldb.SBDebugger.Create()
            self.debugger.SetAsync(False)
            
            # Create target
            self.target = self.debugger.CreateTarget(self.test_program)
            if not self.target:
                print(f"{Colors.RED}Error: Failed to create target for {self.test_program}{Colors.NC}")
                return False
                
            # Set breakpoint at end of main
            main_bp = self.target.BreakpointCreateByName("main")
            if not main_bp.IsValid():
                print(f"{Colors.RED}Error: Failed to set breakpoint in main{Colors.NC}")
                return False
                
            return True
            
        except Exception as e:
            print(f"{Colors.RED}Error setting up debugger: {e}{Colors.NC}")
            return False
            
    def run_to_breakpoint(self) -> bool:
        """Run program to breakpoint"""
        try:
            # Launch process
            error = lldb.SBError()
            self.process = self.target.Launch(
                self.debugger.GetListener(),
                None,  # argv
                None,  # envp
                None,  # stdin_path
                None,  # stdout_path
                None,  # stderr_path
                None,  # working_directory
                0,     # launch_flags
                False, # stop_at_entry
                error
            )
            
            if not error.Success():
                print(f"{Colors.RED}Error launching process: {error}{Colors.NC}")
                return False
                
            # Wait for breakpoint
            state = self.process.GetState()
            if state != lldb.eStateStopped:
                print(f"{Colors.RED}Process not stopped at breakpoint{Colors.NC}")
                return False
                
            # Step a few times to ensure objects are initialized
            thread = self.process.GetSelectedThread()
            for _ in range(5):
                thread.StepOver()
                
            return True
            
        except Exception as e:
            print(f"{Colors.RED}Error running to breakpoint: {e}{Colors.NC}")
            return False
            
    def evaluate_expression(self, expr: str) -> str:
        """Evaluate an expression and return formatted output"""
        try:
            thread = self.process.GetSelectedThread()
            frame = thread.GetSelectedFrame()
            
            # Evaluate expression
            result = frame.EvaluateExpression(expr)
            
            if result.GetError().Success():
                # Get formatted summary
                summary = result.GetSummary()
                if summary:
                    return summary
                    
                # Get value if no summary
                value = result.GetValue()
                if value:
                    return value
                    
                # Get object description
                desc = result.GetObjectDescription()
                if desc:
                    return desc
                    
                return str(result)
            else:
                return f"Error: {result.GetError()}"
                
        except Exception as e:
            return f"Exception: {e}"
            
    def validate_test_case(self, test_case: TestCase) -> TestResult:
        """Validate a single test case"""
        actual_output = self.evaluate_expression(f"po {test_case.variable}")
        
        passed = True
        error_message = None
        
        for pattern in test_case.expected_patterns:
            if test_case.is_regex:
                if test_case.should_contain:
                    if not re.search(pattern, actual_output):
                        passed = False
                        error_message = f"Pattern '{pattern}' not found"
                else:
                    if re.search(pattern, actual_output):
                        passed = False
                        error_message = f"Pattern '{pattern}' found (should not be present)"
            else:
                if test_case.should_contain:
                    if pattern not in actual_output:
                        passed = False
                        error_message = f"Expected '{pattern}' not found"
                else:
                    if pattern in actual_output:
                        passed = False
                        error_message = f"Unexpected '{pattern}' found"
                        
        return TestResult(test_case, passed, actual_output, error_message)
        
    def run_tests(self, test_cases: List[TestCase]) -> None:
        """Run all test cases"""
        print(f"\n{Colors.BLUE}Running formatter validation tests...{Colors.NC}\n")
        
        if not self.setup_debugger():
            print(f"{Colors.RED}Failed to setup debugger{Colors.NC}")
            return
            
        if not self.run_to_breakpoint():
            print(f"{Colors.RED}Failed to run to breakpoint{Colors.NC}")
            return
            
        for test_case in test_cases:
            print(f"Testing {test_case.name}... ", end='')
            result = self.validate_test_case(test_case)
            self.results.append(result)
            
            if result.passed:
                print(f"{Colors.GREEN}PASSED{Colors.NC}")
            else:
                print(f"{Colors.RED}FAILED{Colors.NC}")
                if result.error_message:
                    print(f"  {Colors.YELLOW}{result.error_message}{Colors.NC}")
                print(f"  Actual: {result.actual_output[:100]}...")
                
        # Cleanup
        if self.process:
            self.process.Kill()
        if self.debugger:
            lldb.SBDebugger.Destroy(self.debugger)
            
    def print_summary(self) -> None:
        """Print test summary"""
        passed = sum(1 for r in self.results if r.passed)
        total = len(self.results)
        percentage = (passed / total * 100) if total > 0 else 0
        
        print(f"\n{'='*50}")
        print(f"Test Summary")
        print(f"{'='*50}")
        print(f"Tests run:    {total}")
        print(f"Tests passed: {Colors.GREEN}{passed}{Colors.NC}")
        print(f"Tests failed: {Colors.RED}{total - passed}{Colors.NC}")
        print(f"Pass rate:    {percentage:.1f}%")
        
        if percentage >= 80:
            print(f"\n{Colors.GREEN}✓ Formatters meet 80% validation criteria{Colors.NC}")
        else:
            print(f"\n{Colors.RED}✗ Formatters below 80% validation criteria{Colors.NC}")
            
        # Show failed tests
        if passed < total:
            print(f"\n{Colors.YELLOW}Failed tests:{Colors.NC}")
            for result in self.results:
                if not result.passed:
                    print(f"  - {result.test_case.name}")
                    if result.error_message:
                        print(f"    {result.error_message}")

def get_standard_test_cases() -> List[TestCase]:
    """Get standard test cases for GNUstep formatters"""
    return [
        # String tests
        TestCase("Empty String", "emptyString", ['@""']),
        TestCase("ASCII String", "asciiString", ["Hello, World!"]),
        TestCase("UTF-8 String", "utf8String", ["Unicode"]),
        TestCase("Tagged String", "taggedString", ["Hi"]),
        
        # Number tests
        TestCase("Integer Number", "intNumber", ["42"]),
        TestCase("Float Number", "floatNumber", ["3.14"]),
        TestCase("Bool Number", "boolNumber", ["YES", "1"]),
        TestCase("Tagged Integer", "taggedInt", ["7"]),
        
        # Array tests
        TestCase("Empty Array", "emptyArray", ["@[]", "0 objects"]),
        TestCase("Simple Array", "simpleArray", ["Apple", "Banana", "Cherry"]),
        TestCase("Mutable Array", "mutableArray", ["3 objects", "NSMutableArray"]),
        
        # Dictionary tests
        TestCase("Empty Dictionary", "emptyDict", ["@{}", "0 entries"]),
        TestCase("Simple Dictionary", "simpleDict", ["name", "John", "age", "30"]),
        TestCase("Mutable Dictionary", "mutableDict", ["2", "pairs", "NSMutableDictionary"]),
        
        # Set tests
        TestCase("Empty Set", "emptySet", ["NSSet", "0"]),
        TestCase("Simple Set", "simpleSet", ["3 objects", "NSSet"]),
        TestCase("Mutable Set", "mutableSet", ["NSMutableSet"]),
        
        # Custom class test
        TestCase("Custom Class", "account", ["BankAccount", "12345", "John Doe"]),
        
        # Nil test
        TestCase("Nil Object", "nilObject", ["nil", "0x0"]),
        
        # Performance tests (should complete quickly)
        TestCase("Large Array", "largeArray", ["1000 objects"], is_regex=False),
        TestCase("Large Dictionary", "largeDict", ["500", "pairs"], is_regex=False),
    ]

def main():
    parser = argparse.ArgumentParser(description='Validate GNUstep LLDB formatters')
    parser.add_argument('--lldb', default='../build/bin/lldb',
                       help='Path to LLDB executable')
    parser.add_argument('--program', default='../lldb/test/API/lang/objc/gnustep/a.out',
                       help='Test program to debug')
    parser.add_argument('--test-file', help='JSON file with custom test cases')
    parser.add_argument('--verbose', action='store_true', help='Verbose output')
    
    args = parser.parse_args()
    
    # Check if test program exists
    if not Path(args.program).exists():
        print(f"{Colors.RED}Error: Test program not found: {args.program}{Colors.NC}")
        print(f"Build it first with: {Colors.YELLOW}cd ../lldb/scripts2 && ./build_test_programs.sh{Colors.NC}")
        sys.exit(1)
        
    # Load test cases
    if args.test_file:
        with open(args.test_file, 'r') as f:
            test_data = json.load(f)
            test_cases = [TestCase(**tc) for tc in test_data]
    else:
        test_cases = get_standard_test_cases()
        
    # Run validation
    validator = FormatterValidator(args.lldb, args.program)
    validator.run_tests(test_cases)
    validator.print_summary()
    
    # Exit with appropriate code
    passed = sum(1 for r in validator.results if r.passed)
    total = len(validator.results)
    sys.exit(0 if passed == total else 1)

if __name__ == '__main__':
    main()