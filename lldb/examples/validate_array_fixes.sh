#!/bin/bash

# Array Corruption Fix Validation Script

echo "=== NSArray Element Corruption Fix Validation ==="
echo "Building test program..."

# Build the test
make test_array_corruption_fixes 2>/dev/null
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to build test program"
    exit 1
fi

echo "Running validation tests..."

# Create LLDB test script
cat > validate_arrays.lldb << 'EOF'
target create test_array_corruption_fixes
run

print fruits
print fruits[0]
print fruits[1] 
print fruits[2]
print fruits[3]

print mixed
print mixed[0]
print mixed[1]
print mixed[2]
print mixed[3]

print nested
print nested[0]
print nested[1]
print nested[2]

print objects
print objects[0]
print objects[1]

print empty

print large
print large[0]
print large[1]
print large[9]

print stringTypes
print stringTypes[0]
print stringTypes[1]

quit
EOF

echo "Running LLDB validation..."
/home/robk/code/llvm-project/build/bin/lldb -b -s validate_arrays.lldb > validation_output.txt 2>&1

echo "Analyzing results..."
PASSED=0
FAILED=0

# Check for corruption patterns
if grep -q "comma, 4, 4" validation_output.txt; then
    echo "FAILED: Still shows 'comma, 4, 4' corruption"
    FAILED=$((FAILED + 1))
else
    echo "PASSED: No 'comma, 4, 4' corruption found"
    PASSED=$((PASSED + 1))
fi

# Check for tagged string placeholders  
if grep -q "<tagged_string>" validation_output.txt; then
    echo "FAILED: Still shows <tagged_string> placeholders"
    FAILED=$((FAILED + 1))
else
    echo "PASSED: No <tagged_string> placeholders found"
    PASSED=$((PASSED + 1))
fi

# Check if fruits array shows correctly
if grep -q '"apple"' validation_output.txt && grep -q '"banana"' validation_output.txt; then
    echo "PASSED: String elements display correctly"
    PASSED=$((PASSED + 1))
else
    echo "FAILED: String elements not displaying correctly"
    FAILED=$((FAILED + 1))
fi

# Check if mixed array shows numbers correctly
if grep -q "42" validation_output.txt && grep -q "3.14" validation_output.txt; then
    echo "PASSED: Number elements display correctly"
    PASSED=$((PASSED + 1))
else
    echo "FAILED: Number elements not displaying correctly"
    FAILED=$((FAILED + 1))
fi

echo ""
echo "=== VALIDATION SUMMARY ==="
echo "Tests Passed: $PASSED"
echo "Tests Failed: $FAILED"

if [ $FAILED -eq 0 ]; then
    echo "✅ ALL TESTS PASSED - Array corruption fixes successful!"
else
    echo "❌ Some tests failed - Review validation_output.txt for details"
fi

echo ""
echo "Full output saved to: validation_output.txt"
echo "LLDB script saved to: validate_arrays.lldb"