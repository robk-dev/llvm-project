#!/usr/bin/env python3
"""
Comprehensive formatter validation script.
Tests all formatters and provides detailed coverage report.
"""

import subprocess
import re
import sys
from typing import Dict, List, Tuple

class FormatterValidator:
    def __init__(self):
        self.lldb = "../../../../../../../build/bin/lldb"
        self.test_program = "test_comprehensive_formatters"
        self.results = {}
        self.coverage = {}
        
    def run_lldb_test(self, variable: str) -> str:
        """Run LLDB and get formatter output for a variable."""
        lldb_script = f"""
target create {self.test_program}
breakpoint set --line 301
run
frame variable {variable}
quit
"""
        try:
            result = subprocess.run(
                [self.lldb, "-b"],
                input=lldb_script,
                capture_output=True,
                text=True,
                timeout=10
            )
            # Extract the variable output
            for line in result.stdout.split('\n'):
                if f") {variable} =" in line:
                    return line.strip()
            return "NOT FOUND"
        except subprocess.TimeoutExpired:
            return "TIMEOUT"
        except Exception as e:
            return f"ERROR: {e}"
    
    def validate_string_formatters(self):
        """Test NSString formatter variants."""
        print("\n=== STRING FORMATTER VALIDATION ===")
        test_cases = [
            ("taggedString", r'@"Hello"', "Tagged pointer string"),
            ("constantString", r'@"This is a constant string literal"', "Constant string"),
            ("mutableString", r'@"Mutable String"', "Mutable string"),
            ("emptyString", r'@""', "Empty string"),
            ("unicodeString", r'@"Hello.*世界.*🌍"', "Unicode string"),
        ]
        
        passed = 0
        for var, expected_pattern, desc in test_cases:
            output = self.run_lldb_test(var)
            if re.search(expected_pattern, output):
                print(f"✓ {desc}: PASS")
                passed += 1
            else:
                print(f"✗ {desc}: FAIL")
                print(f"  Got: {output}")
        
        self.coverage["NSString"] = (passed / len(test_cases)) * 100
        return passed == len(test_cases)
    
    def validate_number_formatters(self):
        """Test NSNumber formatter with tagged pointers."""
        print("\n=== NUMBER FORMATTER VALIDATION ===")
        test_cases = [
            ("taggedInt", "42", "Tagged integer"),
            ("taggedBool", "YES", "Tagged boolean"),
            ("taggedFloat", "3.14", "Tagged float"),
            ("negativeNum", "-100", "Negative number"),
            ("zeroNum", "0", "Zero value"),
        ]
        
        passed = 0
        for var, expected, desc in test_cases:
            output = self.run_lldb_test(var)
            if expected in output:
                print(f"✓ {desc}: PASS")
                passed += 1
            else:
                print(f"✗ {desc}: FAIL")
                print(f"  Got: {output}")
        
        self.coverage["NSNumber"] = (passed / len(test_cases)) * 100
        return passed == len(test_cases)
    
    def validate_array_formatters(self):
        """Test NSArray formatter - Critical for regression."""
        print("\n=== ARRAY FORMATTER VALIDATION ===")
        test_cases = [
            ("emptyArray", "@[]", "Empty array"),
            ("simpleArray", '@["Apple", "Banana", "Cherry"]', "String array (NO PLACEHOLDERS)"),
            ("numberArray", "@[1, 2, 3, 4, 5]", "Number array"),
        ]
        
        passed = 0
        for var, expected, desc in test_cases:
            output = self.run_lldb_test(var)
            
            # Critical check: No <string> placeholders
            if "<string>" in output:
                print(f"✗ {desc}: CRITICAL FAIL - Contains <string> placeholder!")
                print(f"  Got: {output}")
                continue
                
            if expected in output or re.search(re.escape(expected).replace(r'\[', r'\[').replace(r'\]', r'\]'), output):
                print(f"✓ {desc}: PASS")
                passed += 1
            else:
                print(f"✗ {desc}: FAIL")
                print(f"  Got: {output}")
        
        self.coverage["NSArray"] = (passed / len(test_cases)) * 100
        return passed == len(test_cases)
    
    def validate_dictionary_formatters(self):
        """Test NSDictionary formatter - Critical for key display."""
        print("\n=== DICTIONARY FORMATTER VALIDATION ===")
        
        output = self.run_lldb_test("simpleDict")
        
        # Critical checks
        passed = True
        
        # Should NOT contain verbose format
        if "[0].key" in output or "[0].value" in output:
            print("✗ Dictionary: CRITICAL FAIL - Using verbose [0].key format!")
            print(f"  Got: {output}")
            passed = False
        
        # Should contain actual keys and values
        if '"name"' in output and '"John Doe"' in output:
            print("✓ Dictionary key extraction: PASS")
        else:
            print("✗ Dictionary key extraction: FAIL")
            print(f"  Got: {output}")
            passed = False
        
        self.coverage["NSDictionary"] = 100 if passed else 0
        return passed
    
    def validate_custom_classes(self):
        """Test custom class introspection."""
        print("\n=== CUSTOM CLASS VALIDATION ===")
        
        output = self.run_lldb_test("customObj")
        
        # Should show properties
        if "TestName" in output and "100" in output:
            print("✓ Custom class properties: PASS")
            self.coverage["CustomClass"] = 100
            return True
        else:
            print("✗ Custom class properties: FAIL")
            print(f"  Got: {output}")
            self.coverage["CustomClass"] = 0
            return False
    
    def validate_performance(self):
        """Test formatter performance."""
        print("\n=== PERFORMANCE VALIDATION ===")
        
        import time
        
        test_vars = ["longString", "deeplyNested", "arrayOfArrays"]
        all_passed = True
        
        for var in test_vars:
            start = time.time()
            output = self.run_lldb_test(var)
            elapsed = (time.time() - start) * 1000
            
            if elapsed < 50:
                print(f"✓ {var}: {elapsed:.2f}ms (PASS)")
            else:
                print(f"✗ {var}: {elapsed:.2f}ms (FAIL - exceeds 50ms)")
                all_passed = False
        
        self.coverage["Performance"] = 100 if all_passed else 50
        return all_passed
    
    def run_all_validations(self):
        """Run all validation tests."""
        print("=" * 60)
        print("GNUSTEP FORMATTER VALIDATION SUITE")
        print("=" * 60)
        
        results = {
            "Strings": self.validate_string_formatters(),
            "Numbers": self.validate_number_formatters(),
            "Arrays": self.validate_array_formatters(),
            "Dictionaries": self.validate_dictionary_formatters(),
            "Custom Classes": self.validate_custom_classes(),
            "Performance": self.validate_performance(),
        }
        
        print("\n" + "=" * 60)
        print("VALIDATION SUMMARY")
        print("=" * 60)
        
        for category, passed in results.items():
            status = "✓ PASS" if passed else "✗ FAIL"
            print(f"{category:20s}: {status}")
        
        print("\n" + "=" * 60)
        print("COVERAGE REPORT")
        print("=" * 60)
        
        total_coverage = 0
        for formatter, coverage in self.coverage.items():
            print(f"{formatter:20s}: {coverage:.1f}%")
            total_coverage += coverage
        
        avg_coverage = total_coverage / len(self.coverage) if self.coverage else 0
        print(f"\nOverall Coverage: {avg_coverage:.1f}%")
        
        # Check critical requirements
        print("\n" + "=" * 60)
        print("CRITICAL REQUIREMENTS")
        print("=" * 60)
        
        critical_passed = True
        
        # No <string> placeholders
        if results["Arrays"]:
            print("✓ Arrays show actual values (no <string> placeholders)")
        else:
            print("✗ Arrays contain placeholders - CRITICAL ISSUE")
            critical_passed = False
        
        # Clean dictionary format
        if results["Dictionaries"]:
            print("✓ Dictionaries use clean key=value format")
        else:
            print("✗ Dictionaries use verbose format - CRITICAL ISSUE")
            critical_passed = False
        
        # Performance requirement
        if results["Performance"]:
            print("✓ All formatters meet <50ms requirement")
        else:
            print("✗ Performance requirement not met")
            critical_passed = False
        
        # Coverage requirement
        if avg_coverage >= 90:
            print(f"✓ Coverage {avg_coverage:.1f}% meets 90% requirement")
        else:
            print(f"✗ Coverage {avg_coverage:.1f}% below 90% requirement")
            critical_passed = False
        
        print("\n" + "=" * 60)
        if all(results.values()) and critical_passed:
            print("✅ ALL TESTS PASSED - FORMATTERS ARE PRODUCTION READY!")
            return 0
        else:
            print("❌ TESTS FAILED - FIXES REQUIRED")
            return 1

if __name__ == "__main__":
    validator = FormatterValidator()
    sys.exit(validator.run_all_validations())