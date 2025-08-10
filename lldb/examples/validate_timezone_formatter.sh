#!/bin/bash

# NSTimeZone Enhanced Formatter Validation Script
# Tests the enhanced timezone debugging capabilities

set -e

LLDB_PATH="/home/robk/code/llvm-project/build/bin/lldb"
TEST_PROGRAM="test_timezone_basic"
SCRIPT_DIR=$(dirname "$0")

echo "=== NSTimeZone Enhanced Formatter Validation ==="
echo "LLDB Path: $LLDB_PATH"
echo "Test Program: $TEST_PROGRAM"
echo "Working Directory: $(pwd)"
echo ""

# Build test program if needed
if [[ ! -f "$TEST_PROGRAM" || "test_timezone_basic.m" -nt "$TEST_PROGRAM" ]]; then
    echo "Building test program..."
    make test_timezone_basic
    echo ""
fi

# Check if LLDB binary exists
if [[ ! -f "$LLDB_PATH" ]]; then
    echo "❌ ERROR: LLDB not found at $LLDB_PATH"
    echo "Build may still be in progress. Please run:"
    echo "cd /home/robk/code/llvm-project/build/NATIVE && ninja lldbPluginGNUstepObjCRuntime"
    exit 1
fi

echo "🔍 Testing Current Timezone Formatter State..."
echo ""

# Create comprehensive test script
cat > timezone_test_session.lldb << 'EOF'
b test_timezone_basic.m:19
run

echo "=== Testing NSTimeZone Formatter Enhancement ==="
echo ""

echo "1. NSLocalTimeZone (should show enhanced info):"
po localTimeZone
echo ""

echo "2. GSAbsTimeZone - GMT (should show clean name):"  
po gmtTimeZone
echo ""

echo "3. GSAbsTimeZone - UTC (should show clean name):"
po utcTimeZone  
echo ""

echo "4. GSAbsTimeZone - Fixed Offset (already working well):"
po offsetTimeZone
echo ""

echo "5. GSTimeZone - America/New_York (MAIN TEST - should show timezone name and DST info):"
po nyTimeZone
echo ""

echo "6. GSTimeZone - Asia/Tokyo (MAIN TEST - should show timezone name):"
po tokyoTimeZone
echo ""

echo "7. Nil timezone (should show nil):"
po nilTimeZone
echo ""

echo "=== Raw Memory Inspection ==="
p localTimeZone
p gmtTimeZone  
p nyTimeZone
p tokyoTimeZone
echo ""

echo "=== Frame Variable Inspection ==="
frame variable -O
echo ""

echo "=== Test Completed ==="
quit
EOF

echo "🚀 Running LLDB timezone formatter test session..."
echo ""

# Run the test session
timeout 60 "$LLDB_PATH" -s timezone_test_session.lldb "$TEST_PROGRAM" 2>&1 | tee timezone_test_results.txt

echo ""
echo "=== Validation Results Analysis ==="

# Check for key improvements
echo "🔍 Analyzing results for formatter improvements..."

if grep -q "GSTimeZone(name=" timezone_test_results.txt; then
    echo "✅ SUCCESS: Enhanced GSTimeZone formatter working!"
    echo "   Found proper GSTimeZone(name=...) format"
else
    echo "❌ ISSUE: GSTimeZone still showing as raw memory pointer"
    echo "   Expected: GSTimeZone(name=\"America/New_York\", offset=...)"
    echo "   Check build status of GNUstep plugin"
fi

if grep -q "NSLocalTimeZone(name=" timezone_test_results.txt; then
    echo "✅ SUCCESS: Enhanced NSLocalTimeZone formatter working!"
else
    echo "⚠️  PARTIAL: NSLocalTimeZone may need additional work"
fi

if grep -q "GSAbsTimeZone(name=\"GMT\"" timezone_test_results.txt; then
    echo "✅ SUCCESS: GSAbsTimeZone name extraction improved!"
else
    echo "⚠️  INFO: GSAbsTimeZone inline string still showing as pointer"
fi

echo ""
echo "📊 Summary of timezone objects tested:"
grep -c "po .*TimeZone" timezone_test_session.lldb | xargs echo "Total timezone objects tested:"

echo ""
echo "📁 Full test results saved to: timezone_test_results.txt"

# Check for any formatter crashes or errors
if grep -qi "error\|crash\|exception" timezone_test_results.txt; then
    echo "⚠️  WARNING: Potential issues detected in output"
    echo "Review timezone_test_results.txt for error details"
fi

echo ""
echo "=== Validation Complete ==="

# Clean up test script
rm -f timezone_test_session.lldb