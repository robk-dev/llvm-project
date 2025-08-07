#!/bin/bash

# Quick test script to verify LLDB and our GNUstep plugin

set -e

PROJECT_ROOT="/home/robk/code/llvm-project"
BUILD_DIR="${PROJECT_ROOT}/build"
EXAMPLES_DIR="${PROJECT_ROOT}/lldb/examples"

echo "=========================================="
echo "LLDB GNUstep Plugin Test"
echo "=========================================="

# Check if LLDB was built
if [[ ! -f "${BUILD_DIR}/bin/lldb" ]]; then
    echo "❌ LLDB not found at ${BUILD_DIR}/bin/lldb"
    echo "Run the build script first: ./build_scripts/build_lldb.sh"
    exit 1
fi

if [[ ! -f "${BUILD_DIR}/bin/lldb-server" ]]; then
    echo "❌ lldb-server not found at ${BUILD_DIR}/bin/lldb-server"
    exit 1
fi

echo "✓ LLDB and lldb-server found"

# Set up environment
export PATH="${BUILD_DIR}/bin:$PATH"
export LLDB_DEBUGSERVER_PATH="${BUILD_DIR}/bin/lldb-server"

echo "✓ Environment configured"
echo "LLDB Path: $(which lldb)"
echo "lldb-server Path: $LLDB_DEBUGSERVER_PATH"

# Build test program
echo "Building test program..."
cd "${EXAMPLES_DIR}"

# Clean and build
make clean
make custom_class_test

if [[ ! -f "custom_class_test" ]]; then
    echo "❌ Failed to build test program"
    exit 1
fi

echo "✓ Test program built successfully"

# Test LLDB startup and plugin loading
echo "Testing LLDB plugin loading..."

# Create a simple test script for LLDB
cat > test_plugin.lldb << 'EOF'
target create custom_class_test
script print("LLDB Python scripting works")
plugin list
quit
EOF

echo "Running LLDB test..."
if lldb -s test_plugin.lldb 2>&1 | grep -q "GNUstep\|gnu-objc"; then
    echo "✓ GNUstep plugin appears to be loaded"
else
    echo "⚠ GNUstep plugin may not be loaded (check plugin list output above)"
fi

# Clean up
rm -f test_plugin.lldb

echo "=========================================="
echo "Test completed!"
echo ""
echo "To run interactive debugging session:"
echo "  cd ${EXAMPLES_DIR}"
echo "  lldb ./custom_class_test"
echo "  (lldb) run"
echo "  (lldb) p myObject"
echo "=========================================="
