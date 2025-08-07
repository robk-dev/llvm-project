#!/bin/bash

# LLDB Build Script
# Builds both LLDB and lldb-server with our GNUstep ObjC Runtime V2 plugin

set -e  # Exit on any error

# Configuration
PROJECT_ROOT="/home/robk/code/llvm-project"
BUILD_DIR="${PROJECT_ROOT}/build"
LLVM_SOURCE="${PROJECT_ROOT}"
INSTALL_PREFIX="/usr/local"

# Build type (Debug, Release, RelWithDebInfo)
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"

# Number of parallel jobs
JOBS="${JOBS:-$(nproc)}"

echo "=========================================="
echo "LLDB Build Script"
echo "=========================================="
echo "Project Root: ${PROJECT_ROOT}"
echo "Build Directory: ${BUILD_DIR}"
echo "Build Type: ${BUILD_TYPE}"
echo "Parallel Jobs: ${JOBS}"
echo "=========================================="

# Clean build directory if requested
if [[ "$1" == "clean" ]]; then
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
fi

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "Configuring LLVM/LLDB with CMake..."

# Configure with CMake
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DLLVM_ENABLE_PROJECTS="clang;lldb" \
    -DLLVM_ENABLE_RUNTIMES="compiler-rt" \
    -DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64" \
    -DLLVM_ENABLE_ASSERTIONS=ON \
    -DLLVM_OPTIMIZED_TABLEGEN=ON \
    -DLLVM_USE_LINKER=lld \
    -DLLVM_PARALLEL_LINK_JOBS=2 \
    -DLLDB_ENABLE_PYTHON=ON \
    -DLLDB_ENABLE_LUA=OFF \
    -DLLDB_ENABLE_CURSES=ON \
    -DLLDB_ENABLE_LIBEDIT=ON \
    -DLLDB_ENABLE_LIBXML2=ON \
    -DLLDB_BUILD_FRAMEWORK=OFF \
    -DLLDB_INCLUDE_TESTS=ON \
    -DCLANG_ENABLE_STATIC_ANALYZER=ON \
    -DCLANG_ENABLE_ARCMT=ON \
    -DCLANG_ENABLE_FORMAT=ON \
    "${LLVM_SOURCE}/llvm"

echo "Building LLVM/LLDB..."

# Build specific targets
echo "Building core LLVM components..."
ninja llvm-config llvm-tblgen clang-tblgen

echo "Building Clang..."
ninja clang

echo "Building LLDB components..."
ninja lldb
ninja lldb-server
ninja lldb-vscode

# Build additional LLDB tools
echo "Building additional LLDB tools..."
ninja lldb-instr
ninja lldb-mi 2>/dev/null || echo "lldb-mi not available, skipping..."

echo "Building LLDB Python bindings..."
ninja lldb-python-scripts 2>/dev/null || echo "Python scripts target not available, skipping..."

echo "=========================================="
echo "Build completed successfully!"
echo "=========================================="

# Show what was built
echo "Built binaries:"
echo "LLDB: ${BUILD_DIR}/bin/lldb"
echo "lldb-server: ${BUILD_DIR}/bin/lldb-server"
echo "lldb-vscode: ${BUILD_DIR}/bin/lldb-vscode"

# Test that the binaries exist
if [[ -f "${BUILD_DIR}/bin/lldb" ]]; then
    echo "✓ LLDB binary exists"
    echo "Version: $(${BUILD_DIR}/bin/lldb --version | head -n1)"
else
    echo "✗ LLDB binary not found!"
    exit 1
fi

if [[ -f "${BUILD_DIR}/bin/lldb-server" ]]; then
    echo "✓ lldb-server binary exists"
else
    echo "✗ lldb-server binary not found!"
    exit 1
fi

# Check if our GNUstep plugin was built
PLUGIN_PATH="${BUILD_DIR}/lib/liblldbPluginGNUstepObjCRuntime.so"
if [[ -f "${PLUGIN_PATH}" ]]; then
    echo "✓ GNUstep ObjC Runtime plugin built: ${PLUGIN_PATH}"
else
    echo "⚠ GNUstep ObjC Runtime plugin not found at: ${PLUGIN_PATH}"
    echo "Checking for alternative locations..."
    find "${BUILD_DIR}" -name "*GNUstep*" -type f 2>/dev/null || echo "No GNUstep plugins found"
fi

echo "=========================================="
echo "Build script completed!"
echo ""
echo "To test LLDB with our plugin:"
echo "  export PATH=\"${BUILD_DIR}/bin:\$PATH\""
echo "  export LLDB_DEBUGSERVER_PATH=\"${BUILD_DIR}/bin/lldb-server\""
echo "  cd /home/robk/code/llvm-project/lldb/examples"
echo "  make clean && make"
echo "  ${BUILD_DIR}/bin/lldb ./custom_class_test"
echo "=========================================="
