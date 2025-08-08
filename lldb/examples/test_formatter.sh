#!/bin/bash

# Test the GNUstep formatter improvements
echo "Testing GNUstep formatters..."

LLDB_BIN="/home/robk/code/llvm-project/build/bin/lldb"
TEST_PROGRAM="custom_class_test"

# Test 1: NSArray formatting
echo "=== Test 1: NSArray formatting ==="
$LLDB_BIN -b "$TEST_PROGRAM" << 'EOF' 2>/dev/null | grep -A5 -B2 'fruits ='
b custom_class_test.m:82
run
frame variable fruits
quit
EOF

# Test 2: Individual array element access
echo "=== Test 2: Individual array element access ==="
$LLDB_BIN -b "$TEST_PROGRAM" << 'EOF' 2>/dev/null | grep -A3 -B2 'fruits\[0\]'
b custom_class_test.m:82
run
frame variable fruits[0]
quit
EOF

# Test 3: String formatting
echo "=== Test 3: String formatting ==="
$LLDB_BIN -b "$TEST_PROGRAM" << 'EOF' 2>/dev/null | grep -A3 -B2 'greeting2'
b custom_class_test.m:82
run
frame variable greeting2
quit
EOF

echo "Testing complete."