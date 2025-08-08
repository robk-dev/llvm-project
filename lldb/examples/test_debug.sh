#!/bin/bash

echo "Debug test..."

cat > debug_commands.lldb << 'EOF'
target create simple_test
b simple_test.m:9
run
frame variable greeting
frame variable fruits
frame variable fruits[0]
frame variable fruits[1]
quit
EOF

echo "Running with all output:"
/home/robk/code/llvm-project/build/bin/lldb -s debug_commands.lldb 2>&1

rm -f debug_commands.lldb