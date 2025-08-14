#!/bin/bash
# Minimal LLDB build script for MSYS2 with memory constraints
# This script builds only the essential LLDB components to avoid linker memory issues

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/helpers/common.sh"

BUILD_DIR="/c/code/llvm-project/build"
LLVM_SRC="/c/code/llvm-project"

print_section "🚀 Minimal LLDB Build for MSYS2"

cd "$BUILD_DIR"

# Configure with minimal static build using GCC (more compatible with MSYS2)
print_info "Configuring minimal LLDB build with GCC (for MSYS2 compatibility)..."
cmake "$LLVM_SRC/llvm" \
    -G "Ninja" \
    -DCMAKE_C_COMPILER="C:/tools/msys64/ucrt64/bin/gcc.exe" \
    -DCMAKE_CXX_COMPILER="C:/tools/msys64/ucrt64/bin/g++.exe" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DLLVM_ENABLE_PROJECTS="clang;lldb" \
    -DLLVM_TARGETS_TO_BUILD="X86" \
    -DLLVM_ENABLE_RTTI=ON \
    -DLLVM_ENABLE_EH=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DLLVM_BUILD_LLVM_DYLIB=OFF \
    -DLLVM_LINK_LLVM_DYLIB=OFF \
    -DLLDB_BUILD_FRAMEWORK=OFF \
    -DLLDB_ENABLE_SHARED=OFF \
    -DLLDB_ENABLE_LIBXML2=OFF \
    -DLLDB_ENABLE_CURSES=OFF \
    -DLLDB_ENABLE_LIBEDIT=OFF \
    -DLLDB_ENABLE_LZMA=OFF \
    -DLLVM_PARALLEL_COMPILE_JOBS=8 \
    -DLLVM_PARALLEL_LINK_JOBS=1 \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLDB_INCLUDE_TESTS=OFF \
    -DCLANG_INCLUDE_TESTS=OFF \
    -DLLDB_ENABLE_PYTHON=ON \
    -DLLDB_PYTHON_HOME="C:/tools/msys64/ucrt64" \
    -DLLDB_EMBED_PYTHON_HOME=ON \
    -DLLDB_PYTHON_RELATIVE_PATH="python3.12" \
    -DCMAKE_CXX_FLAGS="-static-libgcc -static-libstdc++" \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++"

print_info "Building LLVM Support libraries..."
ninja -j8 LLVMSupport LLVMCore LLVMTargetParser LLVMBinaryFormat LLVMObject

print_info "Building Clang tablegen targets (required for LLDB)..."
ninja -j8 clang-tablegen-targets

print_info "Building LLDB server (simpler target without shared library)..."
export LDFLAGS="-Wl,--no-keep-memory -Wl,--reduce-memory-overheads -Wl,--as-needed"
# Try building lldb-server first as it's simpler
ninja -j1 lldb-server.exe

if [ -f "bin/lldb-server.exe" ]; then
    print_success "✅ LLDB server built successfully!"
    print_info "Now attempting to build main LLDB executable..."
    
    # Try to build the main LLDB executable
    ninja -j1 lldb.exe
    
    if [ -f "bin/lldb.exe" ]; then
        print_success "✅ LLDB built successfully!"
        print_info "Location: $BUILD_DIR/bin/lldb.exe"
        
        # Test basic functionality
        print_info "Testing LLDB..."
        winpty ./bin/lldb.exe --version || print_warning "LLDB version check failed"
    else
        print_warning "⚠️ Main LLDB failed, but lldb-server is available"
        print_info "You can still use lldb-server for remote debugging"
    fi
else
    print_error "❌ LLDB server build failed"
    exit 1
fi
