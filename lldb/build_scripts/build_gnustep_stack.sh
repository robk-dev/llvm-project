
#!/bin/bash

# GNUstep Stack Build Script
# Builds the complete GNUstep stack (libobjc2 + libs-base) using our LLVM toolchain
# This ensures compatibility with our LLDB GNUstep formatter plugin

set -e  # Exit on any error

# Configuration
PROJECT_ROOT="/home/robk/code/llvm-project"
BUILD_DIR="${PROJECT_ROOT}/build"
LLVM_BUILD_DIR="${BUILD_DIR}"
GNUSTEP_BUILD_DIR="${PROJECT_ROOT}/lldb/gnustep-build"
GNUSTEP_INSTALL_PREFIX="${PROJECT_ROOT}/lldb/gnustep-install"

# Source directories (these should exist in the LLDB workspace)
LIBOBJC2_SOURCE="${PROJECT_ROOT}/lldb/libobjc2"
LIBSBASE_SOURCE="${PROJECT_ROOT}/lldb/libs-base"

# Build type
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"
JOBS="${JOBS:-$(nproc)}"

echo "=========================================="
echo "GNUstep Stack Build Script"
echo "=========================================="
echo "Project Root: ${PROJECT_ROOT}"
echo "LLVM Build Dir: ${LLVM_BUILD_DIR}"
echo "GNUstep Build Dir: ${GNUSTEP_BUILD_DIR}"
echo "GNUstep Install Prefix: ${GNUSTEP_INSTALL_PREFIX}"
echo "Build Type: ${BUILD_TYPE}"
echo "Parallel Jobs: ${JOBS}"
echo "=========================================="

# Verify LLVM is built
if [[ ! -f "${LLVM_BUILD_DIR}/bin/clang" ]]; then
    echo "❌ LLVM/Clang not found at ${LLVM_BUILD_DIR}/bin/clang"
    echo "Please build LLVM first: ./build_scripts/build_lldb.sh"
    exit 1
fi

if [[ ! -f "${LLVM_BUILD_DIR}/bin/clang++" ]]; then
    echo "❌ Clang++ not found at ${LLVM_BUILD_DIR}/bin/clang++"
    exit 1
fi

echo "✓ LLVM toolchain found"

# Verify source directories exist
if [[ ! -d "${LIBOBJC2_SOURCE}" ]]; then
    echo "❌ libobjc2 source not found at ${LIBOBJC2_SOURCE}"
    echo "Please clone libobjc2 to lldb/libobjc2"
    echo "  git clone https://github.com/gnustep/libobjc2.git ${LIBOBJC2_SOURCE}"
    exit 1
fi

if [[ ! -d "${LIBSBASE_SOURCE}" ]]; then
    echo "❌ libs-base source not found at ${LIBSBASE_SOURCE}"
    echo "Please clone libs-base to lldb/libs-base"
    echo "  git clone https://github.com/gnustep/libs-base.git ${LIBSBASE_SOURCE}"
    exit 1
fi

echo "✓ GNUstep source directories found"

# Clean build directory if requested
if [[ "$1" == "clean" ]]; then
    echo "Cleaning GNUstep build directory..."
    rm -rf "${GNUSTEP_BUILD_DIR}"
    rm -rf "${GNUSTEP_INSTALL_PREFIX}"
fi

# Create build directories
mkdir -p "${GNUSTEP_BUILD_DIR}"
mkdir -p "${GNUSTEP_INSTALL_PREFIX}"

# Set up environment to use our LLVM toolchain
export CC="${LLVM_BUILD_DIR}/bin/clang"
export CXX="${LLVM_BUILD_DIR}/bin/clang++"
export CMAKE_C_COMPILER="${CC}"
export CMAKE_CXX_COMPILER="${CXX}"

# Add our install directory to paths
export PATH="${GNUSTEP_INSTALL_PREFIX}/bin:${LLVM_BUILD_DIR}/bin:$PATH"
export LD_LIBRARY_PATH="${GNUSTEP_INSTALL_PREFIX}/lib:${LLVM_BUILD_DIR}/lib:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="${GNUSTEP_INSTALL_PREFIX}/lib/pkgconfig:$PKG_CONFIG_PATH"

echo "Environment configured:"
echo "  CC: $CC"
echo "  CXX: $CXX"
echo "  PATH: $PATH"

echo "=========================================="
echo "Building libobjc2 (Objective-C Runtime)"
echo "=========================================="

cd "${GNUSTEP_BUILD_DIR}"
mkdir -p libobjc2
cd libobjc2

cmake \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_INSTALL_PREFIX="${GNUSTEP_INSTALL_PREFIX}" \
    -DCMAKE_C_FLAGS="-fblocks" \
    -DCMAKE_CXX_FLAGS="-fblocks" \
    -DBUILD_SHARED_LIBS=ON \
    -DENABLE_OBJCXX=ON \
    -DLLVM_OPTS=ON \
    -DGNUSTEP_INSTALL_TYPE=NONE \
    -DCMAKE_INSTALL_LIBDIR=lib \
    "${LIBOBJC2_SOURCE}"

echo "Building libobjc2..."
ninja -j ${JOBS}

echo "Installing libobjc2..."
ninja install

echo "✓ libobjc2 built and installed"

# Verify libobjc2 installation
if [[ ! -f "${GNUSTEP_INSTALL_PREFIX}/lib/libobjc.so" ]]; then
    echo "❌ libobjc2 installation failed - library not found"
    exit 1
fi

echo "✓ libobjc2 library verified: ${GNUSTEP_INSTALL_PREFIX}/lib/libobjc.so"

echo "=========================================="
echo "Building libs-base (Foundation Library)"
echo "=========================================="

# For now, let's skip libs-base and use the system one
# libs-base has complex dependencies on gnustep-make configuration
echo "⚠ Skipping libs-base build for now"
echo "=========================================="
echo "Building libs-base (Foundation Library)"
echo "=========================================="

cd "${GNUSTEP_BUILD_DIR}"
mkdir -p libs-base
cd libs-base

# libs-base uses autotools, not CMake
# We need to configure it properly for our custom environment

# Set up additional environment for libs-base
export OBJC="${CC}"
export CPPFLAGS="-I${GNUSTEP_INSTALL_PREFIX}/include"
export LDFLAGS="-L${GNUSTEP_INSTALL_PREFIX}/lib -Wl,-rpath,${GNUSTEP_INSTALL_PREFIX}/lib"

# Configure libs-base with autotools
echo "Configuring libs-base..."
cd "${LIBSBASE_SOURCE}"

# Run autoreconf if needed
if [[ ! -f configure ]]; then
    echo "Running autoreconf..."
    autoreconf -fiv
fi

./configure \
    --prefix="${GNUSTEP_INSTALL_PREFIX}" \
    --with-objc-runtime="${GNUSTEP_INSTALL_PREFIX}" \
    --enable-debug \
    --disable-mixedabi \
    --with-installation-domain=SYSTEM \
    CC="${CC}" \
    OBJC="${CC}" \
    CPPFLAGS="${CPPFLAGS}" \
    LDFLAGS="${LDFLAGS}" \
    RUNTIME_FLAG="-fobjc-runtime=gnustep-2.1"

echo "Building libs-base..."
make -j${JOBS} VERBOSE=1

echo "Installing libs-base..."
make install

echo "✓ libs-base built and installed"

# Verify libs-base installation
if [[ ! -f "${GNUSTEP_INSTALL_PREFIX}/lib/libgnustep-base.so" ]]; then
    echo "❌ libs-base installation failed - library not found"
    exit 1
fi

echo "✓ libs-base library verified: ${GNUSTEP_INSTALL_PREFIX}/lib/libgnustep-base.so"

echo "=========================================="
echo "GNUstep Stack Build Complete!"
echo "=========================================="

echo "Installation Summary:"
echo "  Install Prefix: ${GNUSTEP_INSTALL_PREFIX}"
echo "  libobjc2: ${GNUSTEP_INSTALL_PREFIX}/lib/libobjc.so"
echo "  libs-base: ${GNUSTEP_INSTALL_PREFIX}/lib/libgnustep-base.so"
echo "  Headers: ${GNUSTEP_INSTALL_PREFIX}/include/"

echo ""
echo "Environment Setup:"
echo "  export GNUSTEP_ROOT=\"${GNUSTEP_INSTALL_PREFIX}\""
echo "  export PATH=\"${GNUSTEP_INSTALL_PREFIX}/bin:\$PATH\""
echo "  export LD_LIBRARY_PATH=\"${GNUSTEP_INSTALL_PREFIX}/lib:\$LD_LIBRARY_PATH\""
echo "  export PKG_CONFIG_PATH=\"${GNUSTEP_INSTALL_PREFIX}/lib/pkgconfig:\$PKG_CONFIG_PATH\""

echo ""
echo "Compilation Flags for Examples:"
echo "  CC=\"${CC}\""
echo "  CFLAGS=\"-fobjc-runtime=gnustep-2.1 -fblocks -I${GNUSTEP_INSTALL_PREFIX}/include\""
echo "  LDFLAGS=\"-L${GNUSTEP_INSTALL_PREFIX}/lib -Wl,-rpath,${GNUSTEP_INSTALL_PREFIX}/lib\""
echo "  LIBS=\"-lgnustep-base -lobjc -lBlocksRuntime\""

echo ""
echo "Test the installation:"
echo "  cd ${PROJECT_ROOT}/lldb/examples"
echo "  ./build_scripts/test_gnustep_stack.sh"

echo "=========================================="
