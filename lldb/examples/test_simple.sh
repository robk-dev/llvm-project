#!/bin/bash

# Simple test without grep to see all output
echo "Running simple test..."

/home/robk/code/llvm-project/build/bin/lldb -b custom_class_test << 'EOF'
b custom_class_test.m:82
run
frame variable fruits
quit
EOF