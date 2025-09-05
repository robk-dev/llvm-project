#!/bin/bash
#===-- run_gnustep_tests.sh - Run all GNUstep LLDB tests ----------------===//
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===//
#
# This script runs all GNUstep-related LLDB tests including unit tests and
# integration tests.
#
#===----------------------------------------------------------------------===//

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(dirname "$SCRIPT_DIR")"           # repo root containing examples/, gnustep-install/
PROJECT_ROOT="$(dirname "$WORKSPACE_ROOT")"   

BUILD_DIR="$PROJECT_ROOT/build"
LLDB_BIN="$BUILD_DIR/bin/lldb"
TEST_PROGRAMS_DIR="$PROJECT_ROOT/lldb/examples"

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

echo "=========================================="
echo "GNUstep LLDB Test Suite"
echo "=========================================="
echo ""

# Function to run a test and report result
run_test() {
    local test_name="$1"
    local test_command="$2"
    
    echo -n "Running $test_name... "
    TESTS_RUN=$((TESTS_RUN + 1))
    
    if eval "$test_command" > /tmp/test_output.log 2>&1; then
        echo -e "${GREEN}PASSED${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}FAILED${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        echo "  Output:"
        tail -20 /tmp/test_output.log | sed 's/^/    /'
    fi
}

# Check prerequisites
echo "Checking prerequisites..."
if [ ! -f "$LLDB_BIN" ]; then
    echo -e "${RED}Error: LLDB not found at $LLDB_BIN${NC}"
    echo "Please build LLDB first with: cd $BUILD_DIR && ninja lldb"
    exit 1
fi

if [ ! -d "/usr/local/lib" ] || [ ! -f "/usr/local/lib/libobjc.so" ]; then
    echo -e "${YELLOW}Warning: GNUstep runtime not found in /usr/local/lib${NC}"
    echo "Some tests may fail. Install GNUstep runtime first."
fi

echo -e "${GREEN}Prerequisites OK${NC}"
echo ""

# Build the GNUstep plugin if needed
echo "Building GNUstep plugin..."
cd "$BUILD_DIR"
if ninja lldbPluginGNUstepObjCRuntime > /tmp/build.log 2>&1; then
    echo -e "${GREEN}Plugin built successfully${NC}"
else
    echo -e "${RED}Plugin build failed${NC}"
    tail -20 /tmp/build.log
    exit 1
fi
echo ""

# Run unit tests
echo "=========================================="
echo "Unit Tests"
echo "=========================================="

if [ -f "$BUILD_DIR/tools/lldb/unittests/Language/ObjC/GNUstep/LanguageObjCGNUstepTests" ]; then
    run_test "Introspector Unit Tests" \
        "$BUILD_DIR/tools/lldb/unittests/Language/ObjC/GNUstep/LanguageObjCGNUstepTests --gtest_filter=GNUstepIntrospectorTest.*"
    
    run_test "Formatters Unit Tests" \
        "$BUILD_DIR/tools/lldb/unittests/Language/ObjC/GNUstep/LanguageObjCGNUstepTests --gtest_filter=GNUstepFormattersTest.*"
    
    run_test "Tagged Pointer Unit Tests" \
        "$BUILD_DIR/tools/lldb/unittests/Language/ObjC/GNUstep/LanguageObjCGNUstepTests --gtest_filter=GNUstepTaggedPointerTest.*"
else
    echo -e "${YELLOW}Unit tests not built yet. Run: cd $BUILD_DIR && ninja LanguageObjCGNUstepTests${NC}"
fi
echo ""

# Build test programs
echo "=========================================="
echo "Building Test Programs"
echo "=========================================="

cd "$TEST_PROGRAMS_DIR"
if make all > /tmp/make.log 2>&1; then
    echo -e "${GREEN}Test programs built successfully${NC}"
else
    echo -e "${YELLOW}Some test programs failed to build${NC}"
    tail -10 /tmp/make.log
fi
echo ""

# Run integration tests with actual programs
echo "=========================================="
echo "Integration Tests"
echo "=========================================="

# Test custom_class_test
if [ -f "$TEST_PROGRAMS_DIR/custom_class_test" ]; then
    # Create LLDB command file for testing
    cat > /tmp/test_custom_class.lldb << EOF
b custom_class_test.m:228
run
po account
po personInfo
po fruits
po magicNumber
po greeting
quit
EOF
    
    run_test "Custom Class Formatter" \
        "$LLDB_BIN -s /tmp/test_custom_class.lldb $TEST_PROGRAMS_DIR/custom_class_test"
fi

# Test dictionary display
if [ -f "$TEST_PROGRAMS_DIR/test_dictionary_display" ]; then
    cat > /tmp/test_dict.lldb << EOF
b main
run
n 20
po simpleDict
po complexDict
po emptyDict
quit
EOF
    
    run_test "Dictionary Formatter" \
        "$LLDB_BIN -s /tmp/test_dict.lldb $TEST_PROGRAMS_DIR/test_dictionary_display"
fi

# Test set display
if [ -f "$TEST_PROGRAMS_DIR/test_set_display" ]; then
    cat > /tmp/test_set.lldb << EOF
b main
run
n 15
po stringSet
po numberSet
po emptySet
quit
EOF
    
    run_test "Set Formatter" \
        "$LLDB_BIN -s /tmp/test_set.lldb $TEST_PROGRAMS_DIR/test_set_display"
fi

# Test mutable collections
if [ -f "$TEST_PROGRAMS_DIR/test_mutable_set" ]; then
    cat > /tmp/test_mutable.lldb << EOF
b main
run
n 10
po mutableSet
quit
EOF
    
    run_test "Mutable Set Formatter" \
        "$LLDB_BIN -s /tmp/test_mutable.lldb $TEST_PROGRAMS_DIR/test_mutable_set"
fi

echo ""

# Run Python API tests
echo "=========================================="
echo "Python API Tests"
echo "=========================================="

if [ -d "$PROJECT_ROOT/lldb/test/API/lang/objc/gnustep" ]; then
    cd "$PROJECT_ROOT/lldb/test"
    
    # Run specific GNUstep tests
    run_test "GNUstep Formatter API Tests" \
        "python -m lldb.dotest -p TestGNUstepFormatters.py --executable $LLDB_BIN"
else
    echo -e "${YELLOW}API tests not found${NC}"
fi
echo ""

# Test formatter performance
echo "=========================================="
echo "Performance Tests"
echo "=========================================="

if [ -f "$TEST_PROGRAMS_DIR/test_complete" ]; then
    cat > /tmp/test_perf.lldb << EOF
b test_complete.m:200
run
script import time
script start = time.time()
po largeArray
script print(f"Large array format time: {time.time() - start:.3f}s")
script start = time.time()
po largeDict
script print(f"Large dict format time: {time.time() - start:.3f}s")
quit
EOF
    
    run_test "Formatter Performance" \
        "$LLDB_BIN -s /tmp/test_perf.lldb $TEST_PROGRAMS_DIR/test_complete"
fi
echo ""

# Summary
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo "Tests run:    $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
else
    echo -e "Tests failed: $TESTS_FAILED"
fi
echo ""

# Calculate pass percentage
if [ $TESTS_RUN -gt 0 ]; then
    PASS_PERCENT=$((TESTS_PASSED * 100 / TESTS_RUN))
    echo "Pass rate: ${PASS_PERCENT}%"
    
    if [ $PASS_PERCENT -ge 80 ]; then
        echo -e "${GREEN}✓ Tests meet 80% pass criteria${NC}"
        exit 0
    else
        echo -e "${RED}✗ Tests below 80% pass criteria${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}No tests were run${NC}"
    exit 1
fi