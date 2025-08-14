#!/bin/bash

echo "==============================================="
echo " ��� LLDB Build with Clang (avoiding GCC issues)"
echo "==============================================="

set -e

BUILD_DIR="/c/code/llvm-project/build"
SOURCE_DIR="/c/code/llvm-project"

cd "$BUILD_DIR"

echo "ℹ️  Configuring LLDB build with Clang compiler..."

cmake \
    -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DLLVM_ENABLE_PROJECTS="clang;lldb" \
    -DLLVM_TARGETS_TO_BUILD="X86" \
    -DLLDB_PYTHON_HOME="/c/tools/msys64/ucrt64" \
    -DLLDB_PYTHON_RELATIVE_PATH="lib/python3.12" \
    -DPYTHON_EXECUTABLE="/c/tools/msys64/ucrt64/bin/python3.exe" \
    -DLLDB_ENABLE_PYTHON=ON \
    -DLLDB_BUILD_FRAMEWORK=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DLLVM_ENABLE_DUMP=ON \
    -DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=ON \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,--stack,16777216" \
    -DCMAKE_SHARED_LINKER_FLAGS="-Wl,--stack,16777216" \
    "$SOURCE_DIR/llvm"

echo "ℹ️  Building LLVM Support libraries..."
ninja llvm-tblgen clang-tblgen

echo "ℹ️  Building Clang tablegen targets (required for LLDB)..."
ninja clang-tablegen-targets || true

echo "ℹ️  Building LLDB server with Clang..."
ninja -j1 lldb-server

echo "ℹ️  Building main LLDB executable..."
ninja -j1 lldb

echo "✅ Build completed successfully!"
echo "��� LLDB executable: $BUILD_DIR/bin/lldb.exe"
echo "��� LLDB server: $BUILD_DIR/bin/lldb-server.exe"
