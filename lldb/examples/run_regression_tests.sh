#!/bin/bash
# run_regression_tests.sh - Automated regression test suite for GNUstep LLDB formatters
# Runs all test programs and validates formatter output

set -e  # Exit on first error

# Configuration
LLDB=${LLDB:-/home/robk/code/llvm-project/build/bin/lldb}
TEST_DIR=$(dirname "$0")
RESULTS_DIR="${TEST_DIR}/test_results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="${RESULTS_DIR}/regression_${TIMESTAMP}.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Create results directory
mkdir -p "$RESULTS_DIR"

# Logging functions
log() {
    echo "$1" | tee -a "$LOG_FILE"
}

log_success() {
    echo -e "${GREEN}✓${NC} $1" | tee -a "$LOG_FILE"
}

log_failure() {
    echo -e "${RED}✗${NC} $1" | tee -a "$LOG_FILE"
}

log_warning() {
    echo -e "${YELLOW}⚠${NC} $1" | tee -a "$LOG_FILE"
}

# Test execution function
run_test() {
    local test_name=$1
    local test_program=$2
    local test_script=$3
    local expected_patterns=("${@:4}")
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    log "Running test: $test_name"
    
    # Check if test program exists
    if [ ! -f "$test_program" ]; then
        log_warning "Test program $test_program not found, skipping"
        SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        return 1
    fi
    
    # Create test-specific LLDB script if not provided
    if [ -z "$test_script" ]; then
        test_script="${test_program}.lldb"
        cat > "$test_script" << EOF
file $test_program
b main
run
po nil
po emptyString
po simpleString
quit
EOF
    fi
    
    # Run LLDB with test script
    local output_file="${RESULTS_DIR}/${test_name}_output.txt"
    if ! $LLDB -b -s "$test_script" "$test_program" > "$output_file" 2>&1; then
        log_failure "$test_name: LLDB crashed or returned error"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
    
    # Check for expected patterns
    local all_patterns_found=true
    for pattern in "${expected_patterns[@]}"; do
        if ! grep -q "$pattern" "$output_file"; then
            log_failure "$test_name: Pattern not found: $pattern"
            all_patterns_found=false
            FAILED_TESTS=$((FAILED_TESTS + 1))
            return 1
        fi
    done
    
    if [ "$all_patterns_found" = true ]; then
        log_success "$test_name: All patterns found"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    fi
}

# Performance benchmark function
benchmark_formatter() {
    local test_name=$1
    local test_program=$2
    local max_time_ms=$3
    
    log "Benchmarking: $test_name (max ${max_time_ms}ms)"
    
    # Create benchmark script
    local bench_script="${test_program}_bench.lldb"
    cat > "$bench_script" << EOF
file $test_program
b main
run
script import time
script start = time.time()
po largeCollection
script elapsed = (time.time() - start) * 1000
script print(f"Elapsed: {elapsed:.2f}ms")
quit
EOF
    
    local output=$($LLDB -b -s "$bench_script" "$test_program" 2>&1)
    local elapsed=$(echo "$output" | grep "Elapsed:" | sed 's/.*Elapsed: \([0-9.]*\)ms.*/\1/')
    
    if [ -n "$elapsed" ]; then
        if (( $(echo "$elapsed < $max_time_ms" | bc -l) )); then
            log_success "$test_name: Performance OK (${elapsed}ms < ${max_time_ms}ms)"
        else
            log_failure "$test_name: Performance FAIL (${elapsed}ms >= ${max_time_ms}ms)"
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
    else
        log_warning "$test_name: Could not measure performance"
    fi
}

# Main test execution
log "=== GNUstep LLDB Formatter Regression Tests ==="
log "Started at: $(date)"
log "LLDB: $LLDB"
log ""

# Build all test programs first
log "Building test programs..."
if ! make -C "$TEST_DIR" all > /dev/null 2>&1; then
    log_failure "Failed to build test programs"
    exit 1
fi
log_success "Test programs built successfully"
log ""

# Test NSString formatters
run_test "NSString_Basic" \
    "${TEST_DIR}/test_nsstring_formatter" \
    "" \
    '@"Hello World"' \
    'nil'

# Test NSNumber formatters
run_test "NSNumber_Comprehensive" \
    "${TEST_DIR}/test_nsnumber_comprehensive" \
    "" \
    '42' \
    '3.14' \
    'YES' \
    'NO'

# Test NSArray formatters
run_test "NSArray_Display" \
    "${TEST_DIR}/test_nsarray_comprehensive" \
    "" \
    '3 objects' \
    'Apple' \
    'Banana'

# Test NSDictionary formatters
run_test "NSDictionary_KeyValue" \
    "${TEST_DIR}/dictionary_test" \
    "" \
    'key/value pairs' \
    'name' \
    'age'

# Test NSSet formatters
run_test "NSSet_Objects" \
    "${TEST_DIR}/nsset_test" \
    "" \
    'objects'

# Test NSIndexSet formatters
run_test "NSIndexSet_Ranges" \
    "${TEST_DIR}/test_indexset" \
    "" \
    'index' \
    'range'

# Test NSDecimalNumber formatters
run_test "NSDecimalNumber_Values" \
    "${TEST_DIR}/test_decimalnumber" \
    "" \
    'decimal'

# Test NSCharacterSet formatters
run_test "NSCharacterSet_Bitmap" \
    "${TEST_DIR}/test_characterset" \
    "" \
    'character'

# Test NSUserDefaults formatters
run_test "NSUserDefaults_Settings" \
    "${TEST_DIR}/test_userdefaults" \
    "" \
    'defaults'

# Test custom class introspection
run_test "CustomClass_Properties" \
    "${TEST_DIR}/custom_class_test" \
    "" \
    'BankAccount' \
    'accountNumber' \
    'owner'

# Performance benchmarks
log ""
log "=== Performance Benchmarks ==="
benchmark_formatter "NSString" "${TEST_DIR}/test_nsstring_formatter" 10
benchmark_formatter "NSNumber" "${TEST_DIR}/test_nsnumber_comprehensive" 5
benchmark_formatter "NSArray" "${TEST_DIR}/test_nsarray_comprehensive" 20
benchmark_formatter "NSDictionary" "${TEST_DIR}/dictionary_test" 30
benchmark_formatter "NSIndexSet" "${TEST_DIR}/test_indexset" 15

# Memory leak check (using valgrind if available)
if command -v valgrind > /dev/null 2>&1; then
    log ""
    log "=== Memory Leak Detection ==="
    
    for test_prog in test_nsstring_formatter test_nsnumber_comprehensive; do
        if [ -f "${TEST_DIR}/$test_prog" ]; then
            log "Checking $test_prog for memory leaks..."
            if valgrind --leak-check=full --error-exitcode=1 \
                       "${TEST_DIR}/$test_prog" > /dev/null 2>&1; then
                log_success "$test_prog: No memory leaks detected"
            else
                log_failure "$test_prog: Memory leaks detected"
                FAILED_TESTS=$((FAILED_TESTS + 1))
            fi
        fi
    done
else
    log_warning "valgrind not found, skipping memory leak detection"
fi

# Summary
log ""
log "=== Test Summary ==="
log "Total tests: $TOTAL_TESTS"
log_success "Passed: $PASSED_TESTS"
if [ $FAILED_TESTS -gt 0 ]; then
    log_failure "Failed: $FAILED_TESTS"
else
    log "Failed: 0"
fi
if [ $SKIPPED_TESTS -gt 0 ]; then
    log_warning "Skipped: $SKIPPED_TESTS"
fi

log ""
log "Results saved to: $LOG_FILE"
log "Completed at: $(date)"

# Exit with appropriate code
if [ $FAILED_TESTS -gt 0 ]; then
    exit 1
else
    exit 0
fi