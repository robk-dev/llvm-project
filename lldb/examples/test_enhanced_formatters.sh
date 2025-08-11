#!/bin/bash
cd /home/robk/code/llvm-project/lldb/examples

echo "Testing enhanced GNUstep formatters with inline preview..."
echo "=================================================="

# Test NSArray formatter
echo -e "Testing NSArray formatter:"
echo -e "b simple_array_test.m:25\nrun\nframe variable stringArray\nframe variable mutableArray\nframe variable emptyArray\nquit" | \
/home/robk/code/llvm-project/build/bin/lldb simple_array_test 2>/dev/null | \
grep -E "stringArray|mutableArray|emptyArray" | \
grep -E "= \(" | \
sed 's/^.*= //'

echo ""

# Test custom_class_test array
echo -e "Testing with custom_class_test:"
echo -e "b custom_class_test.m:228\nrun\nframe variable fruits\nquit" | \
/home/robk/code/llvm-project/build/bin/lldb custom_class_test 2>/dev/null | \
grep -E "fruits.*= " | \
sed 's/^.*= //'

echo ""
echo "Test complete."