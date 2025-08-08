#!/bin/bash

# Test the formatter with a simple program and minimal debugging output
echo "Testing formatter improvements..."

cd /home/robk/code/llvm-project/lldb/examples

# Create a temporary LLDB command file to avoid verbose output
cat > test_commands.lldb << 'EOF'
settings set target.process.stop-on-sharedlibrary-events false
target create simple_test
b simple_test.m:9
run
frame variable greeting
frame variable fruits
frame variable fruits[0]
quit
EOF

# Run LLDB with the command file, filtering to show only variable outputs
echo "=== Testing NSString and NSArray formatters ==="
/home/robk/code/llvm-project/build/bin/lldb -s test_commands.lldb 2>/dev/null | \
  grep -E "(greeting|fruits)" | \
  grep -v "Breakpoint\|Process\|frame #0"

# Clean up
rm -f test_commands.lldb

echo "Test complete."