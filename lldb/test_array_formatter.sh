#!/bin/bash

# Test script to debug NSArray formatter issues

LLDB="/home/robk/code/llvm-project/build/bin/lldb"
TEST_PROG="/home/robk/code/llvm-project/lldb/examples/custom_class_test"

echo "Testing NSArray formatter..."
echo "Expected: 4 objects @[\"apple\", \"banana\", \"cherry\", \"date\"]"
echo "Running LLDB..."

$LLDB -b -o "b 128" \
      -o "run" \
      -o "po fruits" \
      -o "p fruits" \
      -o "frame variable fruits" \
      -o "memory read -s8 -c8 -fx fruits" \
      -o "quit" \
      $TEST_PROG 2>&1 | grep -A5 -B5 "fruits"