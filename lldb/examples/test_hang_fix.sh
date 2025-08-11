#!/bin/bash

# Test script to validate that the LLDB hanging issue is fixed
# This script tests po commands with timeout to ensure they complete

set -e

LLDB_BIN="/home/robk/code/llvm-project/build/bin/lldb"
TEST_PROGRAM="/home/robk/code/llvm-project/lldb/examples/simple_hang_test"

echo "=== Testing LLDB Hang Fix ==="
echo "LLDB: $LLDB_BIN"
echo "Test program: $TEST_PROGRAM"

# Create an LLDB command script
cat > /tmp/test_hang_fix.lldb << 'EOF'
# Set a breakpoint and run
b simple_hang_test.m:14
run
# Test po commands that previously caused hanging
po testString
po testNumber  
po testArray
po testDict
po dummy
# Test edge cases
po nil
po (void*)0x0
# Print variables directly (should work without hanging)
print testString
print testNumber
# Continue and exit
continue
quit
EOF

echo "=== Running LLDB with timeout (should complete within 30 seconds) ==="

# Use timeout to ensure the test completes
if timeout 30s $LLDB_BIN -s /tmp/test_hang_fix.lldb $TEST_PROGRAM; then
    echo "SUCCESS: LLDB completed without hanging!"
    echo "The hanging issue has been FIXED."
    exit 0
else
    exit_code=$?
    if [ $exit_code -eq 124 ]; then
        echo "FAILURE: LLDB timed out after 30 seconds - hanging issue persists"
        exit 1
    else
        echo "LLDB exited with code $exit_code (non-timeout error)"
        exit $exit_code
    fi
fi

# Clean up
rm -f /tmp/test_hang_fix.lldb