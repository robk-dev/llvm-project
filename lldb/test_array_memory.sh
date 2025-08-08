#!/bin/bash

# Test script to debug NSArray memory layout

LLDB="/home/robk/code/llvm-project/build/bin/lldb"
TEST_PROG="/home/robk/code/llvm-project/lldb/examples/custom_class_test"

echo "Analyzing NSArray memory layout..."

$LLDB -b -o "b 128" \
      -o "run" \
      -o "p/x (void*)fruits" \
      -o "p/x *(long*)((char*)fruits+8)" \
      -o "p/x *(int*)((char*)fruits+16)" \
      -o "memory read -s8 -c8 -fx \`*(void**)((char*)fruits+8)\`" \
      -o "quit" \
      $TEST_PROG 2>&1 | tail -20