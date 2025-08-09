#!/bin/bash
# Quick test to verify GNUstep plugin is working

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LLDB="$PROJECT_ROOT/build/bin/lldb"
TEST_PROG="$PROJECT_ROOT/lldb/examples/custom_class_test"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "Quick GNUstep LLDB Plugin Test"
echo "==============================="
echo ""

# Check if test program exists
if [ ! -f "$TEST_PROG" ]; then
    echo -e "${RED}Error: Test program not found${NC}"
    echo "Building test programs..."
    "$SCRIPT_DIR/build_test_programs.sh"
fi

# Create simple LLDB script
cat > /tmp/quick_test.lldb << 'EOF'
b custom_class_test.m:125
run
frame variable greeting
frame variable magicNumber
frame variable fruits
quit
EOF

echo "Running quick formatter test..."
if timeout 10 "$LLDB" -s /tmp/quick_test.lldb "$TEST_PROG" > /tmp/quick_test_output.txt 2>&1; then
    echo -e "${GREEN}Test completed successfully${NC}"
    echo ""
    echo "Sample output:"
    grep -E "(greeting|magicNumber|fruits)" /tmp/quick_test_output.txt | head -10
else
    echo -e "${RED}Test failed or timed out${NC}"
    echo "Output:"
    tail -20 /tmp/quick_test_output.txt
    exit 1
fi

echo ""
echo "To run full test suite: $SCRIPT_DIR/run_gnustep_tests.sh"