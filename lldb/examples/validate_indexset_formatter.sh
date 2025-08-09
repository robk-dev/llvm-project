#!/bin/bash

echo "=== NSIndexSet Formatter Validation Script ==="
echo "Testing all NSIndexSet formatting scenarios..."

# Create LLDB script for testing
cat > /tmp/indexset_test.lldb << 'EOF'
# Load the test program
target create test_indexset_validation
b test_indexset_validation.m:69
run

# Test all the IndexSet objects
echo "=== FORMATTER VALIDATION RESULTS ==="
echo ""

echo "Test 1 - Empty IndexSet:"
frame variable emptySet
echo "Expected: '0 indexes'"
echo ""

echo "Test 2 - Single Index:"
frame variable singleIndex  
echo "Expected: '1 index: 42'"
echo ""

echo "Test 3 - Small Range:"
frame variable smallRange
echo "Expected: '5 indexes in [10-14]'"
echo ""

echo "Test 4 - Large Range:"
frame variable largeRange
echo "Expected: '1000 indexes in [0-999]'"
echo ""

echo "Test 5 - Scattered Indexes:"
frame variable scatteredSet
echo "Expected: '5 indexes' (scattered)"
echo ""

echo "Test 6 - Multiple Ranges:"
frame variable multiRangeSet
echo "Expected: '9 indexes' (multiple ranges)"
echo ""

echo "Test 7 - Zero Index:"
frame variable zeroIndex
echo "Expected: '1 index: 0'"
echo ""

echo "Test 8 - Null Object:"
frame variable nullSet
echo "Expected: '(null)' or similar"
echo ""

echo "Test 9 - Large Sparse Set:"
frame variable largeSet
echo "Expected: '100 indexes' (large sparse)"
echo ""

echo "=== FORMATTER VALIDATION COMPLETE ==="
echo "All tests completed. Review output above."

quit
EOF

# Run the test
echo "Running LLDB validation..."
/home/robk/code/llvm-project/build/bin/lldb -s /tmp/indexset_test.lldb

echo "Validation script completed."