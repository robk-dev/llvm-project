#!/bin/bash

# Final Solution: Build custom GNUstep from source for Windows compatibility
# This addresses the dispatch.h header conflicts and creates a pure GNUstep environment

set -e

echo "=== Final Solution: Custom GNUstep Build ==="
echo "Building GNUstep from source to eliminate Apple Foundation header conflicts"

# Create build directory
BUILD_ROOT="/c/Users/vagrant/code/llvm-project/lldb/gnustep-custom-build"
mkdir -p "$BUILD_ROOT"
cd "$BUILD_ROOT"

# Set installation prefix
INSTALL_PREFIX="$BUILD_ROOT/install"
mkdir -p "$INSTALL_PREFIX"

echo "Build root: $BUILD_ROOT"
echo "Install prefix: $INSTALL_PREFIX"

# Download and build libobjc2 from source
echo "=== Building libobjc2 from source ==="
if [ ! -d "libobjc2" ]; then
    git clone --depth 1 https://github.com/gnustep/libobjc2.git
fi
cd libobjc2
mkdir -p build
cd build

# Configure libobjc2 with proper Windows settings
cmake .. \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_OBJC_COMPILER=clang \
    -DGNUSTEP_INSTALL_TYPE=NONE \
    -DTESTS=OFF

# Build and install libobjc2
make -j$(nproc)
make install

cd "$BUILD_ROOT"

# Download and build GNUstep-make
echo "=== Building GNUstep-make from source ==="
if [ ! -d "tools-make" ]; then
    git clone --depth 1 https://github.com/gnustep/tools-make.git
fi
cd tools-make

# Configure GNUstep-make for Windows
export CC=clang
export CXX=clang++
export OBJC=clang
export LDFLAGS="-L$INSTALL_PREFIX/lib"
export CPPFLAGS="-I$INSTALL_PREFIX/include"

./configure \
    --prefix="$INSTALL_PREFIX" \
    --with-layout=fhs \
    --disable-importing-config-file \
    --enable-native-objc-exceptions \
    --with-library-combo=ng-gnu-gnu \
    --with-objc-lib-flag=-lobjc

make install

# Set up GNUstep environment
source "$INSTALL_PREFIX/share/GNUstep/Makefiles/GNUstep.sh"

cd "$BUILD_ROOT"

# Download and build GNUstep-base
echo "=== Building GNUstep-base from source ==="
if [ ! -d "libs-base" ]; then
    git clone --depth 1 https://github.com/gnustep/libs-base.git
fi
cd libs-base

# Configure GNUstep-base without Apple-specific features
./configure \
    --prefix="$INSTALL_PREFIX" \
    --with-installation-domain=SYSTEM \
    --disable-mixedabi \
    --enable-native-objc-exceptions \
    --disable-libdispatch

make -j$(nproc)
make install

echo "=== Custom GNUstep Build Complete ==="

# Create environment script for the custom build
ENV_SCRIPT="/c/Users/vagrant/code/llvm-project/lldb/gnustep-custom-env.sh"
cat > "$ENV_SCRIPT" << EOF
#!/bin/bash
# Custom GNUstep environment without dispatch conflicts
export GNUSTEP_ROOT="$INSTALL_PREFIX"
source "\$GNUSTEP_ROOT/share/GNUstep/Makefiles/GNUstep.sh"

# Custom compilation flags for Windows compatibility
export GNUSTEP_CFLAGS="-I\$GNUSTEP_ROOT/include -fobjc-runtime=gnustep-2.1 -fobjc-exceptions -fconstant-string-class=NSConstantString -fblocks"
export GNUSTEP_LIBS="-L\$GNUSTEP_ROOT/lib -lgnustep-base -lobjc"

echo "Custom GNUstep environment loaded:"
echo "GNUSTEP_ROOT: \$GNUSTEP_ROOT"
echo "Headers: \$GNUSTEP_SYSTEM_HEADERS"  
echo "Libraries: \$GNUSTEP_SYSTEM_LIBRARIES"
EOF

chmod +x "$ENV_SCRIPT"

echo "Custom GNUstep build installed to: $INSTALL_PREFIX"
echo "Environment script created: $ENV_SCRIPT"
echo
echo "Next steps:"
echo "1. Source the custom environment: source $ENV_SCRIPT"
echo "2. Test compilation with custom GNUstep build"
