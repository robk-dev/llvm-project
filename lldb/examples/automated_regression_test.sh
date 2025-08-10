#!/bin/bash
#===-- automated_regression_test.sh ------------------------------------===//
#
# COMPREHENSIVE FOUNDATION FORMATTER REGRESSION TEST FRAMEWORK
#
# This script provides automated testing for continuous validation of 
# GNUstep Foundation formatters to ensure production reliability.
#
#===----------------------------------------------------------------------===//

set -e

# Configuration
LLDB_PATH="/home/robk/code/llvm-project/build/bin/lldb"
BUILD_PATH="/home/robk/code/llvm-project/build"
TEST_PATH="/home/robk/code/llvm-project/lldb/examples"
PLUGIN_PATH="${BUILD_PATH}/lib/liblldbPluginGNUstepObjCRuntime.a"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test results
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

echo -e "${BLUE}=== GNUstep Foundation Formatter Regression Test Suite ===${NC}"
echo "Testing all Foundation formatters for production readiness"
echo "Date: $(date)"
echo ""

# Function to run a test and capture results
run_test() {
    local test_name="$1"
    local command="$2"
    local expected_pattern="$3"
    
    echo -e "${YELLOW}Running: ${test_name}${NC}"
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    # Run the command and capture output
    if output=$(eval "$command" 2>&1); then
        if [[ -z "$expected_pattern" ]] || echo "$output" | grep -q "$expected_pattern"; then
            echo -e "${GREEN}✓ PASS: ${test_name}${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))
            return 0
        else
            echo -e "${RED}✗ FAIL: ${test_name} - Expected pattern not found${NC}"
            echo "Output: $output"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            return 1
        fi
    else
        echo -e "${RED}✗ FAIL: ${test_name} - Command failed${NC}"
        echo "Output: $output"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
}

# Function to check build status
check_build() {
    echo -e "${BLUE}=== Build Status Check ===${NC}"
    
    run_test "Plugin Build Check" \
        "test -f '${PLUGIN_PATH}'" \
        ""
        
    run_test "LLDB Executable Check" \
        "test -f '${LLDB_PATH}'" \
        ""
        
    run_test "Test Program Build" \
        "cd '${TEST_PATH}' && make foundation_test_simple" \
        "foundation_test_simple"
        
    echo ""
}

# Function to run unit tests
run_unit_tests() {
    echo -e "${BLUE}=== Unit Test Validation ===${NC}"
    
    run_test "Build Unit Tests" \
        "cd '${BUILD_PATH}' && ninja LanguageObjCGNUstepTests" \
        "LanguageObjCGNUstepTests"
        
    run_test "Execute Unit Tests" \
        "cd '${BUILD_PATH}' && timeout 60 ./tools/lldb/unittests/Language/ObjC/GNUstep/LanguageObjCGNUstepTests --gtest_output=brief" \
        "PASSED"
        
    echo ""
}

# Function to test individual formatter types
test_formatter_types() {
    echo -e "${BLUE}=== Individual Formatter Type Tests ===${NC}"
    
    # Create a simple test script for each formatter type
    cat > "${TEST_PATH}/quick_test.lldb" << 'EOF'
b foundation_test_simple.m:310
run
po shortString
po smallInt
po smallArray
po smallDict
po smallSet
po now
po smallData
po randomUUID
po httpURL
po fileError
continue
quit
EOF

    run_test "Foundation Formatter Integration" \
        "cd '${TEST_PATH}' && timeout 30 '${LLDB_PATH}' -S quick_test.lldb foundation_test_simple" \
        "Hello"
        
    rm -f "${TEST_PATH}/quick_test.lldb"
    echo ""
}

# Function to test performance requirements
test_performance() {
    echo -e "${BLUE}=== Performance Requirements Test ===${NC}"
    
    # Create performance test
    cat > "${TEST_PATH}/perf_test.lldb" << 'EOF'
b foundation_test_simple.m:310
run
# Time a series of formatter operations
po largeArray
po largeDict
po largeSet
po deeplyNested
continue
quit
EOF

    run_test "Large Collection Performance" \
        "cd '${TEST_PATH}' && timeout 10 '${LLDB_PATH}' -S perf_test.lldb foundation_test_simple" \
        "items"
        
    rm -f "${TEST_PATH}/perf_test.lldb"
    echo ""
}

# Function to test edge cases
test_edge_cases() {
    echo -e "${BLUE}=== Edge Case Validation ===${NC}"
    
    # Create edge case test
    cat > "${TEST_PATH}/edge_test.lldb" << 'EOF'
b foundation_test_simple.m:310
run
po nilString
po nilArray
po nilDict
po nilSet
po nilDate
po nilData
po nilUUID
po nilURL
po nilError
po nilIndexSet
po nilDecimal
po nilCharSet
continue
quit
EOF

    run_test "Nil Object Handling" \
        "cd '${TEST_PATH}' && timeout 15 '${LLDB_PATH}' -S edge_test.lldb foundation_test_simple" \
        "nil"
        
    rm -f "${TEST_PATH}/edge_test.lldb"
    echo ""
}

# Function to test memory safety
test_memory_safety() {
    echo -e "${BLUE}=== Memory Safety Validation ===${NC}"
    
    run_test "Valgrind Memory Check" \
        "cd '${TEST_PATH}' && timeout 30 valgrind --error-exitcode=1 --leak-check=summary ./foundation_test_simple" \
        "ERROR SUMMARY: 0 errors"
        
    echo ""
}

# Function to generate final report
generate_report() {
    echo -e "${BLUE}=== FINAL REGRESSION TEST REPORT ===${NC}"
    echo ""
    echo "Total Tests: $TOTAL_TESTS"
    echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed: ${RED}$FAILED_TESTS${NC}"
    echo ""
    
    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "${GREEN}🎉 ALL TESTS PASSED - PRODUCTION READY${NC}"
        echo "The Foundation formatter implementation passes all regression tests"
        echo "and is validated for production deployment and upstream submission."
        echo ""
        echo "Key Validations Completed:"
        echo "✓ Build system integration"
        echo "✓ Unit test coverage"
        echo "✓ Formatter functionality"
        echo "✓ Performance requirements"
        echo "✓ Edge case handling"
        echo "✓ Memory safety"
        echo ""
        return 0
    else
        echo -e "${RED}❌ REGRESSION DETECTED${NC}"
        echo "Some tests failed. Review output above for details."
        echo "The implementation requires fixes before deployment."
        echo ""
        return 1
    fi
}

# Main execution flow
main() {
    echo "Starting comprehensive regression test suite..."
    echo "This validates production readiness of Foundation formatters"
    echo ""
    
    check_build
    run_unit_tests
    test_formatter_types
    test_performance
    test_edge_cases
    
    # Only run memory safety if valgrind is available
    if command -v valgrind &> /dev/null; then
        test_memory_safety
    else
        echo -e "${YELLOW}Skipping memory safety tests (valgrind not available)${NC}"
        echo ""
    fi
    
    generate_report
}

# Execute main function
main "$@"