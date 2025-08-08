#!/bin/bash

# Test script to isolate timeout/infinite loop issues in formatters
# This will test each formatter individually with a timeout

LLDB="/home/robk/code/llvm-project/build/bin/lldb"
TIMEOUT_SECONDS=10

echo "Testing GNUstep formatter timeout issues..."
echo "Using LLDB: $LLDB"
echo "Timeout: $TIMEOUT_SECONDS seconds"
echo

# Function to test a specific formatter
test_formatter() {
    local test_name="$1"
    local program="$2"
    local breakpoint="$3"
    local command="$4"
    
    echo "=== Testing $test_name ==="
    
    if [ ! -f "$program" ]; then
        echo "ERROR: $program not found. Run 'make $program' first."
        return 1
    fi
    
    # Run LLDB with timeout
    timeout ${TIMEOUT_SECONDS}s $LLDB -b \
        -o "target create $program" \
        -o "$breakpoint" \
        -o "run" \
        -o "$command" \
        -o "quit" 2>&1
    
    local exit_code=$?
    
    if [ $exit_code -eq 124 ]; then
        echo "TIMEOUT: $test_name hung after $TIMEOUT_SECONDS seconds!"
        echo "This indicates an infinite loop or performance issue."
        return 1
    elif [ $exit_code -ne 0 ]; then
        echo "ERROR: $test_name failed with exit code $exit_code"
        return 1
    else
        echo "SUCCESS: $test_name completed within timeout"
        return 0
    fi
    
    echo
}

# Test NSString formatter
test_formatter "NSString" "string_test" "b string_test.m:44" "frame variable constantString1"

# Test NSArray formatter (most likely culprit)
test_formatter "NSArray" "simple_array_test" "b simple_array_test.m:45" "frame variable stringArray"

# Test more complex NSArray
test_formatter "NSArray Complex" "array_test" "b array_test.m:97" "frame variable stringArray"

# Test NSDictionary formatter
test_formatter "NSDictionary" "dictionary_test" "b dictionary_test.m:53" "frame variable multiDict"

# Test NSSet formatter
test_formatter "NSSet" "nsset_test" "b nsset_test.m:115" "frame variable stringSet"

# Test comprehensive NSArray (most likely to hang)
test_formatter "NSArray Comprehensive" "test_nsarray_comprehensive" "b test_nsarray_comprehensive.m:68" "frame variable allArrays"

# Test specific array inline preview (the suspected culprit)
test_formatter "NSArray Inline Preview" "simple_array_test" "b simple_array_test.m:45" "p stringArray"

echo "=== Timeout Testing Complete ==="
echo
echo "If any tests showed TIMEOUT, that formatter has infinite loop issues."
echo "Look for:"
echo "  - Recursive calls in GetInlineElementsPreview() methods"
echo "  - Infinite loops in collection iteration"
echo "  - Tagged pointer decoding issues"
echo "  - Memory access patterns causing hangs"