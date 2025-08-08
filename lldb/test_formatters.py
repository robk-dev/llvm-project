#!/usr/bin/env python3
"""
Automated test framework for GNUstep LLDB formatters.

This script builds test programs, runs LLDB with each test, verifies formatter
output matches expected values, and reports pass/fail for each formatter.

Usage:
    python3 test_formatters.py [--verbose] [--filter FORMATTER_NAME]
"""

import os
import sys
import subprocess
import re
import json
import argparse
from pathlib import Path
from typing import Dict, List, Tuple, Optional, Any
from dataclasses import dataclass
from enum import Enum

# ANSI color codes for output
class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

class TestResult(Enum):
    PASS = "PASS"
    FAIL = "FAIL"
    SKIP = "SKIP"
    ERROR = "ERROR"

@dataclass
class TestCase:
    """Represents a single test case."""
    name: str
    variable: str
    expected_output: str
    regex_pattern: Optional[str] = None
    command: str = "po"
    description: str = ""

@dataclass
class FormatterTest:
    """Represents a formatter test suite."""
    name: str
    source_file: str
    executable: str
    breakpoint_line: int
    test_cases: List[TestCase]
    setup_commands: List[str] = None
    cleanup_commands: List[str] = None

class LLDBTestRunner:
    """Runs LLDB tests and captures output."""
    
    def __init__(self, lldb_path: str, lldb_server_path: str, verbose: bool = False):
        self.lldb_path = lldb_path
        self.lldb_server_path = lldb_server_path
        self.verbose = verbose
        
    def run_lldb_commands(self, executable: str, commands: List[str], timeout: int = 30) -> str:
        """Run LLDB with a list of commands and return output."""
        # Create temporary command file
        import tempfile
        with tempfile.NamedTemporaryFile(mode='w', suffix='.lldb', delete=False) as f:
            for cmd in commands:
                f.write(f"{cmd}\n")
            cmd_file = f.name
        
        try:
            # Set up environment
            env = os.environ.copy()
            env['LLDB_DEBUGSERVER_PATH'] = self.lldb_server_path
            
            # Run LLDB with command file
            cmd = [self.lldb_path, '-b', '-s', cmd_file, executable]
            
            if self.verbose:
                print(f"Running: {' '.join(cmd)}")
            
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout,
                env=env
            )
            
            return result.stdout + result.stderr
        finally:
            # Clean up temporary file
            os.unlink(cmd_file)
    
    def extract_command_output(self, full_output: str, command: str, variable: str) -> Optional[str]:
        """Extract the output of a specific command from LLDB output."""
        # Look for pattern: (lldb) <command> <variable>
        # The output follows on the next line(s)
        pattern = rf'\(lldb\)\s+{re.escape(command)}\s+{re.escape(variable)}\s*\n(.*?)(?=\(lldb\)|$)'
        match = re.search(pattern, full_output, re.DOTALL | re.MULTILINE)
        
        if match:
            output = match.group(1).strip()
            # Remove any trailing prompts or commands
            output = re.sub(r'\(lldb\).*$', '', output, flags=re.DOTALL).strip()
            return output
        
        return None

class FormatterTestFramework:
    """Main test framework for GNUstep LLDB formatters."""
    
    def __init__(self, build_dir: str, examples_dir: str, verbose: bool = False):
        self.build_dir = Path(build_dir)
        self.examples_dir = Path(examples_dir)
        self.verbose = verbose
        self.lldb_path = self.build_dir / "bin" / "lldb"
        self.lldb_server_path = self.build_dir / "bin" / "lldb-server"
        self.runner = LLDBTestRunner(str(self.lldb_path), str(self.lldb_server_path), verbose)
        self.results: Dict[str, List[Tuple[TestCase, TestResult, str]]] = {}
        
    def build_test_program(self, source_file: str) -> bool:
        """Build a test program using the Makefile."""
        target = Path(source_file).stem
        cmd = ["make", "-C", str(self.examples_dir), target]
        
        if self.verbose:
            print(f"Building: {' '.join(cmd)}")
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"{Colors.RED}Build failed for {source_file}:{Colors.RESET}")
            print(result.stderr)
            return False
        
        return True
    
    def run_formatter_test(self, test: FormatterTest) -> List[Tuple[TestCase, TestResult, str]]:
        """Run all test cases for a formatter."""
        results = []
        
        # Build test program
        if not self.build_test_program(test.source_file):
            return [(tc, TestResult.ERROR, "Build failed") for tc in test.test_cases]
        
        # Prepare LLDB commands
        commands = []
        
        # Setup commands
        if test.setup_commands:
            commands.extend(test.setup_commands)
        
        # Set breakpoint and run
        commands.append(f"breakpoint set --file {test.source_file} --line {test.breakpoint_line}")
        commands.append("run")
        
        # Execute test commands
        for tc in test.test_cases:
            commands.append(f"{tc.command} {tc.variable}")
        
        # Cleanup commands
        if test.cleanup_commands:
            commands.extend(test.cleanup_commands)
        
        commands.append("quit")
        
        # Run LLDB
        executable = str(self.examples_dir / test.executable)
        output = self.runner.run_lldb_commands(executable, commands)
        
        if self.verbose:
            print(f"\n--- LLDB Output ---\n{output}\n--- End Output ---\n")
        
        # Check each test case
        for tc in test.test_cases:
            actual_output = self.runner.extract_command_output(output, tc.command, tc.variable)
            
            if actual_output is None:
                results.append((tc, TestResult.ERROR, "No output captured"))
                continue
            
            # Check against expected output
            if tc.regex_pattern:
                # Use regex matching
                if re.search(tc.regex_pattern, actual_output):
                    results.append((tc, TestResult.PASS, actual_output))
                else:
                    results.append((tc, TestResult.FAIL, 
                                  f"Expected pattern: {tc.regex_pattern}\nActual: {actual_output}"))
            else:
                # Exact match (with normalization)
                normalized_expected = tc.expected_output.strip()
                normalized_actual = actual_output.strip()
                
                if normalized_expected == normalized_actual:
                    results.append((tc, TestResult.PASS, actual_output))
                else:
                    results.append((tc, TestResult.FAIL, 
                                  f"Expected: {normalized_expected}\nActual: {normalized_actual}"))
        
        return results
    
    def run_all_tests(self, filter_name: Optional[str] = None) -> None:
        """Run all formatter tests."""
        tests = self.get_all_tests()
        
        if filter_name:
            tests = [t for t in tests if filter_name.lower() in t.name.lower()]
        
        if not tests:
            print(f"{Colors.YELLOW}No tests found matching filter: {filter_name}{Colors.RESET}")
            return
        
        print(f"{Colors.BOLD}Running GNUstep LLDB Formatter Tests{Colors.RESET}")
        print("=" * 60)
        
        for test in tests:
            print(f"\n{Colors.BLUE}Testing {test.name}...{Colors.RESET}")
            
            results = self.run_formatter_test(test)
            self.results[test.name] = results
            
            # Print results for this formatter
            passed = sum(1 for _, r, _ in results if r == TestResult.PASS)
            failed = sum(1 for _, r, _ in results if r == TestResult.FAIL)
            errors = sum(1 for _, r, _ in results if r == TestResult.ERROR)
            
            if failed == 0 and errors == 0:
                print(f"{Colors.GREEN}✓ {test.name}: All {passed} tests passed{Colors.RESET}")
            else:
                print(f"{Colors.RED}✗ {test.name}: {passed} passed, {failed} failed, {errors} errors{Colors.RESET}")
            
            # Show failed tests
            for tc, result, msg in results:
                if result in [TestResult.FAIL, TestResult.ERROR]:
                    print(f"  {Colors.RED}✗ {tc.name}: {msg}{Colors.RESET}")
                elif self.verbose and result == TestResult.PASS:
                    print(f"  {Colors.GREEN}✓ {tc.name}{Colors.RESET}")
    
    def print_summary(self) -> None:
        """Print test summary."""
        print("\n" + "=" * 60)
        print(f"{Colors.BOLD}Test Summary{Colors.RESET}")
        print("=" * 60)
        
        total_passed = 0
        total_failed = 0
        total_errors = 0
        
        for formatter_name, results in self.results.items():
            passed = sum(1 for _, r, _ in results if r == TestResult.PASS)
            failed = sum(1 for _, r, _ in results if r == TestResult.FAIL)
            errors = sum(1 for _, r, _ in results if r == TestResult.ERROR)
            
            total_passed += passed
            total_failed += failed
            total_errors += errors
            
            status_color = Colors.GREEN if (failed == 0 and errors == 0) else Colors.RED
            print(f"{status_color}{formatter_name:30} {passed:3} passed, {failed:3} failed, {errors:3} errors{Colors.RESET}")
        
        print("-" * 60)
        print(f"{'TOTAL':30} {total_passed:3} passed, {total_failed:3} failed, {total_errors:3} errors")
        
        if total_failed == 0 and total_errors == 0:
            print(f"\n{Colors.GREEN}{Colors.BOLD}All tests passed! 🎉{Colors.RESET}")
            return 0
        else:
            print(f"\n{Colors.RED}{Colors.BOLD}Some tests failed or had errors{Colors.RESET}")
            return 1
    
    def get_all_tests(self) -> List[FormatterTest]:
        """Define all formatter tests."""
        tests = []
        
        # NSString Formatter Tests
        tests.append(FormatterTest(
            name="NSString Formatter",
            source_file="test_nsstring_formatter.m",
            executable="test_nsstring_formatter",
            breakpoint_line=72,
            test_cases=[
                # Basic strings
                TestCase(
                    name="Constant String",
                    variable="constantString1",
                    expected_output='@"Hello, World!"',
                    description="Basic constant NSString"
                ),
                TestCase(
                    name="Empty String",
                    variable="emptyConstant",
                    expected_output='@""',
                    description="Empty NSString"
                ),
                
                # Mutable strings
                TestCase(
                    name="Mutable String",
                    variable="mutableString1",
                    expected_output='@"Initial content"',
                    description="NSMutableString with appended content"
                ),
                
                # UTF-8 and Unicode
                TestCase(
                    name="UTF-8 String",
                    variable="utf8String",
                    regex_pattern=r'@"UTF-8 encoded string:.*"',
                    description="String with UTF-8 characters"
                ),
                TestCase(
                    name="Unicode String",
                    variable="unicode",
                    regex_pattern=r'@"Unicode:.*🌍.*😊"',
                    description="String with Unicode emoji"
                ),
                
                # Special characters
                TestCase(
                    name="Special Characters",
                    variable="specialChars",
                    regex_pattern=r'@"Special:.*\\n.*\\t.*"',
                    description="String with escape sequences"
                ),
                
                # Format strings
                TestCase(
                    name="Format String",
                    variable="formatString",
                    expected_output='@"Formatted: 42, test"',
                    description="String created with format"
                ),
                
                # Substrings
                TestCase(
                    name="Substring To Index",
                    variable="substring1",
                    expected_output='@"This is"',
                    description="Substring created with substringToIndex"
                ),
                TestCase(
                    name="Substring With Range",
                    variable="substring3",
                    expected_output='@"the"',
                    description="Substring created with substringWithRange"
                ),
                
                # nil handling
                TestCase(
                    name="Nil String",
                    variable="nilString",
                    expected_output="nil",
                    description="nil NSString pointer"
                ),
            ]
        ))
        
        # NSNumber Formatter Tests
        tests.append(FormatterTest(
            name="NSNumber Formatter",
            source_file="test_nsnumber_formatter.m",
            executable="test_nsnumber_formatter",
            breakpoint_line=53,
            test_cases=[
                # Integer numbers
                TestCase(
                    name="Positive Integer",
                    variable="intNumber",
                    expected_output="42",
                    regex_pattern=r"(\(int\))?\s*42",
                    description="Positive integer NSNumber"
                ),
                TestCase(
                    name="Negative Integer",
                    variable="negativeInt",
                    expected_output="-100",
                    regex_pattern=r"(\(int\))?\s*-100",
                    description="Negative integer NSNumber"
                ),
                TestCase(
                    name="Zero Integer",
                    variable="zeroInt",
                    expected_output="0",
                    regex_pattern=r"(\(int\))?\s*0",
                    description="Zero integer NSNumber"
                ),
                
                # Boolean values
                TestCase(
                    name="Boolean YES",
                    variable="boolYes",
                    regex_pattern=r"(YES|true|1)",
                    description="Boolean YES NSNumber"
                ),
                TestCase(
                    name="Boolean NO",
                    variable="boolNo",
                    regex_pattern=r"(NO|false|0)",
                    description="Boolean NO NSNumber"
                ),
                
                # Floating point
                TestCase(
                    name="Float Number",
                    variable="floatNumber",
                    regex_pattern=r"3\.14\d*",
                    description="Float NSNumber"
                ),
                TestCase(
                    name="Double Number",
                    variable="doubleNumber",
                    regex_pattern=r"2\.718\d*",
                    description="Double NSNumber"
                ),
                
                # Special values
                TestCase(
                    name="NaN Double",
                    variable="nanDouble",
                    regex_pattern=r"(NaN|nan)",
                    description="NaN NSNumber"
                ),
                TestCase(
                    name="Infinity",
                    variable="infDouble",
                    regex_pattern=r"(inf|Inf|INFINITY)",
                    description="Infinity NSNumber"
                ),
                
                # Large numbers
                TestCase(
                    name="Long Long",
                    variable="longLongNumber",
                    expected_output="9223372036854775807",
                    regex_pattern=r"9223372036854775807",
                    description="Maximum long long NSNumber"
                ),
                TestCase(
                    name="Unsigned Long Long",
                    variable="unsignedLongLong",
                    expected_output="18446744073709551615",
                    regex_pattern=r"18446744073709551615",
                    description="Maximum unsigned long long NSNumber"
                ),
            ]
        ))
        
        # NSArray Formatter Tests
        tests.append(FormatterTest(
            name="NSArray Formatter",
            source_file="simple_array_test.m",
            executable="simple_array_test",
            breakpoint_line=25,
            test_cases=[
                # Basic arrays
                TestCase(
                    name="String Array",
                    variable="stringArray",
                    regex_pattern=r"\(\s*@?\"?First\"?,\s*@?\"?Second\"?,\s*@?\"?Third\"?\s*\)",
                    description="NSArray with string elements"
                ),
                TestCase(
                    name="Empty Array",
                    variable="emptyArray",
                    regex_pattern=r"\(\s*\)",
                    description="Empty NSArray"
                ),
                
                # Mutable arrays
                TestCase(
                    name="Mutable Array",
                    variable="mutableArray",
                    regex_pattern=r"\(\s*@?\"?Apple\"?,\s*@?\"?Banana\"?,\s*@?\"?Cherry\"?,\s*@?\"?Date\"?,\s*@?\"?Elderberry\"?\s*\)",
                    description="NSMutableArray with multiple elements"
                ),
                
                # Frame variable tests (different command)
                TestCase(
                    name="Array via frame variable",
                    variable="stringArray",
                    command="frame variable -O",
                    regex_pattern=r"First.*Second.*Third",
                    description="NSArray displayed via frame variable"
                ),
            ]
        ))
        
        return tests

def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Test framework for GNUstep LLDB formatters"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable verbose output"
    )
    parser.add_argument(
        "--filter", "-f",
        type=str,
        help="Filter tests by formatter name"
    )
    parser.add_argument(
        "--build-dir",
        type=str,
        default="/home/robk/code/llvm-project/build",
        help="LLVM build directory"
    )
    parser.add_argument(
        "--examples-dir",
        type=str,
        default="/home/robk/code/llvm-project/lldb/examples",
        help="LLDB examples directory"
    )
    parser.add_argument(
        "--json-output",
        type=str,
        help="Write results to JSON file"
    )
    
    args = parser.parse_args()
    
    # Check that required paths exist
    build_dir = Path(args.build_dir)
    examples_dir = Path(args.examples_dir)
    
    if not build_dir.exists():
        print(f"{Colors.RED}Error: Build directory does not exist: {build_dir}{Colors.RESET}")
        return 1
    
    if not examples_dir.exists():
        print(f"{Colors.RED}Error: Examples directory does not exist: {examples_dir}{Colors.RESET}")
        return 1
    
    # Create and run test framework
    framework = FormatterTestFramework(
        str(build_dir),
        str(examples_dir),
        verbose=args.verbose
    )
    
    # Run tests
    framework.run_all_tests(filter_name=args.filter)
    
    # Print summary
    exit_code = framework.print_summary()
    
    # Write JSON output if requested
    if args.json_output:
        json_results = {}
        for formatter_name, results in framework.results.items():
            json_results[formatter_name] = [
                {
                    "test_name": tc.name,
                    "result": result.value,
                    "message": msg
                }
                for tc, result, msg in results
            ]
        
        with open(args.json_output, 'w') as f:
            json.dump(json_results, f, indent=2)
        print(f"\nResults written to: {args.json_output}")
    
    return exit_code

if __name__ == "__main__":
    sys.exit(main())