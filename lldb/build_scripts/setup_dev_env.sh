#!/bin/bash

# Complete Development Environment Setup Script
# Sets up the entire LLVM + GNUstep + LLDB development environment

set -e

PROJECT_ROOT="/home/robk/code/llvm-project"
LLDB_DIR="${PROJECT_ROOT}/lldb"

echo "=========================================="
echo "LLVM + GNUstep + LLDB Setup Script"
echo "=========================================="
echo "This script will set up the complete development environment:"
echo "1. Clone libobjc2 and libs-base if needed"
echo "2. Build LLVM/LLDB with our plugin"
echo "3. Build custom GNUstep stack"
echo "4. Test everything"
echo "=========================================="

cd "${LLDB_DIR}"

# Step 1: Clone GNUstep sources if needed
echo "Step 1: Checking GNUstep source repositories..."

if [[ ! -d "libobjc2" ]]; then
    echo "Cloning libobjc2..."
    git clone https://github.com/gnustep/libobjc2.git
else
    echo "✓ libobjc2 already exists"
fi

if [[ ! -d "libs-base" ]]; then
    echo "Cloning libs-base..."
    git clone https://github.com/gnustep/libs-base.git
else
    echo "✓ libs-base already exists"
fi

echo "✓ GNUstep sources ready"

# Step 2: Build LLVM/LLDB
echo ""
echo "Step 2: Building LLVM/LLDB with GNUstep plugin..."
if [[ ! -f "${PROJECT_ROOT}/build/bin/lldb" ]]; then
    echo "Building LLVM/LLDB..."
    ./build_scripts/build_lldb.sh
else
    echo "✓ LLVM/LLDB already built"
fi

# Step 3: Build GNUstep stack
echo ""
echo "Step 3: Building custom GNUstep stack..."
if [[ ! -f "gnustep-install/lib/libobjc.so" ]] || [[ ! -f "gnustep-install/lib/libgnustep-base.so" ]]; then
    echo "Building GNUstep stack..."
    ./build_scripts/build_gnustep_stack.sh
else
    echo "✓ GNUstep stack already built"
fi

# Step 4: Test everything
echo ""
echo "Step 4: Testing the complete setup..."
./build_scripts/test_gnustep_stack.sh

echo ""
echo "=========================================="
echo "Development Environment Setup Complete!"
echo "=========================================="

echo ""
echo "What you now have:"
echo "✓ LLVM/Clang built from source with our modifications"
echo "✓ LLDB with GNUstep formatter plugin"
echo "✓ Custom libobjc2 runtime compatible with LLVM"
echo "✓ Custom libs-base Foundation library"
echo "✓ VSCode tasks configured for both system and custom GNUstep"

echo ""
echo "Quick Start Guide:"
echo ""
echo "1. Build and test with custom GNUstep stack:"
echo "   Ctrl+Shift+P → 'Tasks: Run Task' → 'build-custom-class-test-with-custom-gnustep'"
echo ""
echo "2. Debug with LLDB + our formatters:"
echo "   cd examples"
echo "   ${PROJECT_ROOT}/build/bin/lldb ./custom_class_test_custom"
echo ""
echo "3. Environment variables for manual compilation:"
echo "   export GNUSTEP_ROOT=\"${LLDB_DIR}/gnustep-install\""
echo "   export PATH=\"${LLDB_DIR}/gnustep-install/bin:${PROJECT_ROOT}/build/bin:\$PATH\""
echo "   export LD_LIBRARY_PATH=\"${LLDB_DIR}/gnustep-install/lib:${PROJECT_ROOT}/build/lib:\$LD_LIBRARY_PATH\""

echo ""
echo "Available build scripts:"
echo "  ./build_scripts/build_lldb.sh          - Build LLVM/LLDB"
echo "  ./build_scripts/build_gnustep_stack.sh - Build GNUstep stack"
echo "  ./build_scripts/test_lldb.sh           - Test LLDB"
echo "  ./build_scripts/test_gnustep_stack.sh  - Test GNUstep stack"
echo "  ./build_scripts/setup_dev_env.sh       - This script"

echo ""
echo "Available VSCode tasks:"
echo "  build-simple-test                              - System GNUstep"
echo "  build-custom-class-test                        - System GNUstep"
echo "  build-with-custom-gnustep                      - Custom GNUstep (current file)"
echo "  build-custom-class-test-with-custom-gnustep    - Custom GNUstep (custom_class_test)"

echo ""
echo "=========================================="
