#!/bin/bash
# Emergency LLDB rebuild script with workarounds for Windows GCC linker issues

set -euo pipefail

BUILD_DIR="/c/code/llvm-project/build"
LLVM_SRC="/c/code/llvm-project"

echo "🔧 Emergency LLDB rebuild with Windows GCC workarounds..."
cd "$BUILD_DIR"

# Step 1: Reconfigure with more conservative settings
echo "Step 1: Configuring with minimal dependencies..."
cmake "$LLVM_SRC/llvm" \
    -G "Ninja" \
    -DCMAKE_C_COMPILER="C:/tools/msys64/ucrt64/bin/gcc.exe" \
    -DCMAKE_CXX_COMPILER="C:/tools/msys64/ucrt64/bin/g++.exe" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DLLVM_ENABLE_PROJECTS="clang;lldb" \
    -DLLVM_TARGETS_TO_BUILD="X86" \
    -DLLVM_ENABLE_RTTI=ON \
    -DLLVM_ENABLE_EH=ON \
    -DBUILD_SHARED_LIBS=ON \
    -DLLVM_BUILD_LLVM_DYLIB=ON \
    -DLLVM_LINK_LLVM_DYLIB=ON \
    -DLLDB_BUILD_FRAMEWORK=OFF \
    -DLLDB_ENABLE_SHARED=ON \
    -DLLDB_ENABLE_LIBXML2=OFF \
    -DLLDB_ENABLE_CURSES=OFF \
    -DLLDB_ENABLE_LIBEDIT=OFF \
    -DLLDB_ENABLE_LZMA=OFF \
    -DLLDB_ENABLE_PYTHON=OFF \
    -DLLVM_PARALLEL_COMPILE_JOBS=4 \
    -DLLVM_PARALLEL_LINK_JOBS=1 \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLDB_INCLUDE_TESTS=OFF \
    -DCLANG_INCLUDE_TESTS=OFF \
    -DCMAKE_CXX_FLAGS="-static-libgcc -static-libstdc++ -Wa,-mbig-obj -O1" \
    -DCMAKE_C_FLAGS="-static-libgcc -O1" \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++ -Wl,--no-keep-memory -Wl,--reduce-memory-overheads -Wl,--as-needed -Wl,--gc-sections"

# Step 2: Build core LLVM libraries first
echo "Step 2: Building core LLVM libraries..."
ninja -j4 LLVMSupport LLVMCore LLVMTargetParser LLVMBinaryFormat LLVMObject LLVM

# Step 3: Build the shared LLVM library
echo "Step 3: Building LLVM shared library..."
ninja -j1 LLVM-15

# Step 4: Try building lldb-server with extreme linker constraints
echo "Step 4: Building lldb-server with memory constraints..."
export LDFLAGS="-Wl,--no-keep-memory -Wl,--reduce-memory-overheads -Wl,--as-needed -Wl,--gc-sections"
ninja -j1 lldb-server

echo "✅ Build attempt completed!"
