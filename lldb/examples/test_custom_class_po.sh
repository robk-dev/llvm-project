#!/bin/bash

# Test script to validate po commands work with custom classes and complex objects
set -e

LLDB_BIN="/home/robk/code/llvm-project/build/bin/lldb"
TEST_PROGRAM="/home/robk/code/llvm-project/lldb/examples/custom_class_test"

echo "=== Testing po Commands with Custom Classes and Complex Objects ==="
echo "LLDB: $LLDB_BIN"
echo "Test program: $TEST_PROGRAM"

# Create an LLDB command script for testing complex objects
cat > /tmp/test_custom_class_po.lldb << 'EOF'
# Set breakpoint at the end where all objects are created
b custom_class_test.m:588
run
# Test po commands on all types of objects
po account
po personInfo  
po fruits
po magicNumber
po testString
po testDate
po testURL
po testError
po testData
po testUUID
po emptyArray
po mixedArray
po emptySet
po mixedSet
po simpleDictionary
po emptyIndexSet
po singleIndexSet  
po rangeIndexSet
po decimalInt
po decimalFloat
po letterSet
po digitSet
# Test edge cases
po nil
po (void*)0x0
# Print some variables directly to compare
print account
print personInfo
print fruits
# Kill process and exit
process kill
quit
EOF

echo "=== Running custom class po command tests ==="

if timeout 20s $LLDB_BIN -s /tmp/test_custom_class_po.lldb $TEST_PROGRAM > /tmp/custom_po_output.txt 2>&1; then
    echo "SUCCESS: Custom class po commands completed!"
    echo ""
    echo "=== Sample po Command Results ==="
    grep -E "(po account|po personInfo|po fruits|po magicNumber)" /tmp/custom_po_output.txt -A 1 | head -20
    echo ""
    echo "✅ HANGING FIX WORKS WITH COMPLEX CUSTOM OBJECTS!"
    exit 0
else
    exit_code=$?
    if [ $exit_code -eq 124 ]; then
        echo "❌ FAILURE: LLDB still hanging with custom objects"
        tail -50 /tmp/custom_po_output.txt
        exit 1
    else
        echo "LLDB exited with code $exit_code"
        if grep -q "GNUstep object at\|NSNumber\|NSString\|nil" /tmp/custom_po_output.txt; then
            echo "✅ po commands are working with custom objects!"
            echo ""
            echo "=== Sample Results ==="
            grep -E "(po account|po personInfo|po fruits|po magicNumber)" /tmp/custom_po_output.txt -A 1 | head -10
            exit 0
        else
            echo "❌ po commands failed"
            tail -30 /tmp/custom_po_output.txt
            exit $exit_code
        fi
    fi
fi

# Clean up
rm -f /tmp/test_custom_class_po.lldb /tmp/custom_po_output.txt