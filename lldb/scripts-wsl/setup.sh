#!/bin/bash
# Two-Stage LLVM/LLDB Build Script for WSL
# Stage 1: Build clang/lld with system compiler
# Stage 2: Build LLDB with stage1 clang
# Author: LLDB GNUstep Team
# Date: 2025

set -euo pipefail

# Dynamic path configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$WORKSPACE_ROOT")"

echo "=================================================================================="
echo " LLVM/LLDB Two-Stage Build for WSL"
echo "=================================================================================="
echo "Script Dir:     $SCRIPT_DIR"
echo "Workspace Root: $WORKSPACE_ROOT"
echo "Project Root:   $PROJECT_ROOT"
echo ""

# Build directories
STAGE1_BUILD_DIR="${PROJECT_ROOT}/build-stage1"
BUILD_DIR="${PROJECT_ROOT}/build"

# Build configuration
BUILD_TYPE="RelWithDebInfo"
PARALLEL_JOBS=${PARALLEL_JOBS:-$(nproc)}

# GNUstep configuration
GNUSTEP_BUILD_DIR="${PROJECT_ROOT}/gnustep-build"
GNUSTEP_INSTALL_DIR="${PROJECT_ROOT}/gnustep-install"

# GNUstep build dependencies
GNUSTEP_DEPS=(
    "build-essential"
    "cmake"
    "ninja-build"
    "clang"
    "libclang-dev"
    "libblocksruntime-dev"
    "libkqueue-dev"
    "libpthread-workqueue-dev"
    "gobjc"
    "libxml2-dev"
    "libxslt1-dev"
    "libffi-dev"
    "libicu-dev"
    "libbsd-dev"
    "libssl-dev"
    "libgnutls28-dev"
    "libunwind-dev"
    "uuid-dev"
    "git"
    "pkg-config"
    "ccache"
    "autoconf"
    "automake"
    "libtool"
    "make"
)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
print_stage() {
    echo -e "\n${BLUE}=================================================================================="
    echo -e " $1"
    echo -e "==================================================================================${NC}\n"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
    exit 1
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Python configuration for WSL (force Linux Python, not Windows)
# Auto-detect Python version from system
PYTHON3_EXEC="/usr/bin/python3"
PYTHON3_VERSION=$($PYTHON3_EXEC -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
PYTHON3_INC="/usr/include/python${PYTHON3_VERSION}"
PYTHON3_LIB="/usr/lib/x86_64-linux-gnu/libpython${PYTHON3_VERSION}.so"

# Verify Python dev files exist
if [ ! -d "$PYTHON3_INC" ]; then
    print_error "Python ${PYTHON3_VERSION} headers not found at $PYTHON3_INC"
    print_error "Install with: sudo apt-get install python${PYTHON3_VERSION}-dev"
    exit 1
fi

print_success "Found Python ${PYTHON3_VERSION} headers at $PYTHON3_INC"

# Check prerequisites
check_prerequisites() {
    print_stage "Checking Prerequisites"
    
    # Check for critical tools (these should have been installed by install_gnustep_dependencies)
    local missing_deps=""
    
    if ! command -v ninja &> /dev/null; then
        missing_deps="$missing_deps ninja-build"
    fi
    
    if ! command -v cmake &> /dev/null; then
        missing_deps="$missing_deps cmake"
    fi
    
    if ! command -v ccache &> /dev/null; then
        missing_deps="$missing_deps ccache"
    fi
    
    if [ ! -d "$PYTHON3_INC" ]; then
        missing_deps="$missing_deps python3-dev"
    fi
    
    if ! command -v swig &> /dev/null; then
        missing_deps="$missing_deps swig"
    fi
    
    if ! command -v git &> /dev/null; then
        missing_deps="$missing_deps git"
    fi
    
    if [ ! -z "$missing_deps" ]; then
        print_error "Missing critical dependencies: $missing_deps"
        echo "Install with: sudo apt-get install $missing_deps"
        echo "Or run without --skip-deps to auto-install dependencies"
        exit 1
    fi
    
    print_success "All critical prerequisites are available"
    
    # Check disk space (need at least 35GB for LLVM + GNUstep)
    local available_space=$(df "$PROJECT_ROOT" | awk 'NR==2 {print int($4/1048576)}')
    if [ "$available_space" -lt 35 ]; then
        print_error "Insufficient disk space: ${available_space}GB available, need at least 35GB"
    fi
    print_success "Disk space: ${available_space}GB available"
    
    # Check RAM
    local available_ram=$(free -g | awk '/^Mem:/{print $2}')
    if [ "$available_ram" -lt 8 ]; then
        print_warning "Low RAM: ${available_ram}GB available. Build may be slow."
        PARALLEL_JOBS=$((PARALLEL_JOBS / 2))
        print_warning "Reducing parallel jobs to $PARALLEL_JOBS"
    else
        print_success "RAM: ${available_ram}GB available"
    fi
}

# Install GNUstep build dependencies
install_gnustep_dependencies() {
    print_stage "Installing GNUstep Build Dependencies"
    
    print_warning "Updating package list..."
    sudo apt-get update
    
    print_warning "Installing GNUstep build dependencies..."
    local missing_deps=""
    
    for dep in "${GNUSTEP_DEPS[@]}"; do
        if ! dpkg -l | grep -q "^ii.*$dep"; then
            missing_deps="$missing_deps $dep"
        fi
    done
    
    if [ ! -z "$missing_deps" ]; then
        echo "Installing missing dependencies: $missing_deps"
        sudo apt-get install -y $missing_deps
    fi
    
    print_success "GNUstep build dependencies installed"
}

# Build Stage 1: Clang and LLD
build_stage1() {
    print_stage "Stage 1: Building Clang and LLD"
    
    if [ -f "$STAGE1_BUILD_DIR/bin/clang" ] && [ -f "$STAGE1_BUILD_DIR/bin/ld.lld" ]; then
        print_success "Stage 1 already built (found clang and lld)"
        echo "To rebuild, remove: $STAGE1_BUILD_DIR"
        return 0
    fi
    
    mkdir -p "$STAGE1_BUILD_DIR"
    cd "$STAGE1_BUILD_DIR"
    
    echo "Configuring Stage 1 build..."
    cmake -G Ninja ../llvm \
        -DCMAKE_BUILD_TYPE=Release \
        -DLLVM_ENABLE_PROJECTS="clang;lld" \
        -DLLVM_TARGETS_TO_BUILD="X86" \
        -DLLVM_ENABLE_ASSERTIONS=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -DLLVM_CCACHE_BUILD=ON \
        -DLLVM_INCLUDE_TESTS=OFF \
        -DLLVM_INCLUDE_EXAMPLES=OFF \
        -DLLVM_INCLUDE_DOCS=OFF
    
    echo "Building clang and lld (this may take 20-40 minutes)..."
    ninja clang lld llvm-tblgen clang-tblgen -j${PARALLEL_JOBS}
    
    if [ ! -f "$STAGE1_BUILD_DIR/bin/clang" ]; then
        print_error "Stage 1 build failed: clang not found"
    fi
    
    if [ ! -f "$STAGE1_BUILD_DIR/bin/ld.lld" ]; then
        print_error "Stage 1 build failed: lld not found"
    fi
    
    print_success "Stage 1 complete: clang and lld built successfully"
}

# Build Stage 2: LLDB with Stage 1 Clang
build_stage2() {
    print_stage "Stage 2: Building LLDB with Stage 1 Clang"
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Check if we need to reconfigure
    if [ -f "CMakeCache.txt" ]; then
        local current_compiler=$(grep "CMAKE_CXX_COMPILER:FILEPATH" CMakeCache.txt | cut -d= -f2)
        if [ "$current_compiler" != "$STAGE1_BUILD_DIR/bin/clang++" ]; then
            print_warning "Compiler changed, removing CMakeCache.txt"
            rm -f CMakeCache.txt
        fi
    fi
    
    echo "Configuring Stage 2 build..."
    echo "Using clang from: $STAGE1_BUILD_DIR/bin/clang"
    echo "Using Python from: $PYTHON3_EXEC"
    
    # Export stage1 tools for tablegen
    export LLVM_TABLEGEN="$STAGE1_BUILD_DIR/bin/llvm-tblgen"
    export CLANG_TABLEGEN="$STAGE1_BUILD_DIR/bin/clang-tblgen"
    
    cmake -G Ninja ../llvm \
        -DCMAKE_C_COMPILER="$STAGE1_BUILD_DIR/bin/clang" \
        -DCMAKE_CXX_COMPILER="$STAGE1_BUILD_DIR/bin/clang++" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DLLVM_ENABLE_PROJECTS="clang;lldb;lld" \
        -DLLVM_ENABLE_ASSERTIONS=ON \
        -DLLDB_INCLUDE_TESTS=ON \
        -DBUILD_SHARED_LIBS=ON \
        -DLLVM_CCACHE_BUILD=ON \
        -DLLDB_ENABLE_PYTHON=ON \
        -DPython3_EXECUTABLE="$PYTHON3_EXEC" \
        -DPython3_INCLUDE_DIRS="$PYTHON3_INC" \
        -DPython3_LIBRARIES="$PYTHON3_LIB" \
        -DLLDB_PYTHON_HOME="/usr" \
        -DLLDB_ENABLE_LIBEDIT=ON \
        -DLLDB_ENABLE_CURSES=ON \
        -DLLVM_TABLEGEN="$LLVM_TABLEGEN" \
        -DCLANG_TABLEGEN="$CLANG_TABLEGEN" \
        -DLLVM_USE_LINKER="$STAGE1_BUILD_DIR/bin/ld.lld" \
        -DCMAKE_INSTALL_PREFIX="/usr/local/llvm-reldeb"
    
    echo "Building LLDB and lldb-server (this may take 30-60 minutes)..."
    ninja lldb lldb-server lldbPluginGNUstepObjCRuntime -j${PARALLEL_JOBS}
    
    if [ ! -f "$BUILD_DIR/bin/lldb" ]; then
        print_error "Stage 2 build failed: lldb not found"
    fi
    
    if [ ! -f "$BUILD_DIR/bin/lldb-server" ]; then
        print_error "Stage 2 build failed: lldb-server not found"
    fi
    
    print_success "Stage 2 complete: LLDB and lldb-server built successfully"
}

# Verify the build
verify_build() {
    print_stage "Verifying Build"
    
    # Check Python support
    echo -n "Checking Python support... "
    local python_path=$("$BUILD_DIR/bin/lldb" -P 2>/dev/null || echo "")
    if [ -z "$python_path" ]; then
        print_error "Python support not working!"
    else
        print_success "Python enabled: $python_path"
    fi
    
    # Check lldb-server
    echo -n "Checking lldb-server... "
    if "$BUILD_DIR/bin/lldb-server" version >/dev/null 2>&1; then
        print_success "lldb-server working"
    else
        print_error "lldb-server not working!"
    fi
    
    # Check GNUstep plugin
    echo -n "Checking GNUstep plugin... "
    if [ -f "$BUILD_DIR/lib/liblldbPluginGNUstepObjCRuntime.so" ]; then
        print_success "GNUstep plugin built"
    else
        print_warning "GNUstep plugin not found (may be statically linked)"
    fi
    
    # Quick LLDB test
    echo -n "Testing LLDB basic functionality... "
    if echo "quit" | "$BUILD_DIR/bin/lldb" >/dev/null 2>&1; then
        print_success "LLDB starts successfully"
    else
        print_error "LLDB failed to start!"
    fi
}

# Create helper scripts
create_helpers() {
    print_stage "Creating Helper Scripts"
    
    # Create basic LLDB environment setup script (for compatibility)
    cat > "$BUILD_DIR/setup_env.sh" << 'EOF'
#!/bin/bash
# Basic LLDB environment setup
export PATH="$(dirname "${BASH_SOURCE[0]}")/bin:$PATH"
export LD_LIBRARY_PATH="$(dirname "${BASH_SOURCE[0]}")/lib:$LD_LIBRARY_PATH"
echo "LLDB environment configured:"
echo "  LLDB: $(which lldb)"
echo "  Clang: $(which clang)"
echo ""
echo "💡 For full development environment (LLDB + GNUstep), use:"
echo "   source $(dirname "${BASH_SOURCE[0]}")/setup_dev_env.sh"
EOF
    chmod +x "$BUILD_DIR/setup_env.sh"
    
    # Create simple test reference script (points to dev.sh)
    cat > "$BUILD_DIR/test_lldb.sh" << EOF
#!/bin/bash
# LLDB Testing Script - Use dev.sh for full functionality
SCRIPT_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
LLDB_DIR="\$SCRIPT_DIR/../lldb"

echo "🚀 For LLDB testing with GNUstep examples, use the dev.sh script:"
echo ""
echo "  cd \$LLDB_DIR"
echo "  ./dev.sh build-example custom_class_test"
echo "  ./dev.sh debug custom_class_test"
echo ""
echo "Or for full test suite:"
echo "  ./dev.sh test"
echo ""
echo "To manually use this LLDB build:"
echo "  source \$SCRIPT_DIR/setup_dev_env.sh"
echo "  lldb [your_program]"
EOF
    chmod +x "$BUILD_DIR/test_lldb.sh"
    
    print_success "Helper scripts created"
}

# Clone a repository with error handling
clone_repository() {
    local repo_url="$1"
    local target_dir="$2"
    
    if [ -d "$target_dir" ]; then
        print_success "Repository already exists: $target_dir"
        cd "$target_dir"
        print_warning "Updating repository..."
        git fetch origin
        git reset --hard origin/master
    else
        print_warning "Cloning repository: $repo_url"
        git clone "$repo_url" "$target_dir"
        cd "$target_dir"
    fi
}

# Build libobjc2 with debugging symbols using Stage 1 clang
build_libobjc2() {
    print_stage "Building libobjc2 with Debug Symbols"
    
    local libobjc2_source_dir="$PROJECT_ROOT/libobjc2"
    local libobjc2_build_dir="$GNUSTEP_BUILD_DIR/libobjc2"
    
    # Clone or update libobjc2
    clone_repository "https://github.com/gnustep/libobjc2.git" "$libobjc2_source_dir"
    
    # Create build directory
    rm -rf "$libobjc2_build_dir"
    mkdir -p "$libobjc2_build_dir"
    cd "$libobjc2_build_dir"
    
    # Configure with Stage 1 clang
    print_warning "Configuring libobjc2 build with Stage 1 clang..."
    cmake "$libobjc2_source_dir" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_C_FLAGS="-g -O0 -fno-omit-frame-pointer -DDEBUG=1 -ffunction-sections -fdata-sections" \
        -DCMAKE_CXX_FLAGS="-g -O0 -fno-omit-frame-pointer -DDEBUG=1 -ffunction-sections -fdata-sections" \
        -DCMAKE_SHARED_LINKER_FLAGS="-lstdc++" \
        -DCMAKE_INSTALL_PREFIX="$GNUSTEP_INSTALL_DIR" \
        -DCMAKE_C_COMPILER="$STAGE1_BUILD_DIR/bin/clang" \
        -DCMAKE_CXX_COMPILER="$STAGE1_BUILD_DIR/bin/clang++" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DGNUSTEP_INSTALL_TYPE=NONE \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -GNinja
    
    # Build
    print_warning "Building libobjc2 (this may take 10-15 minutes)..."
    ninja -j${PARALLEL_JOBS}
    
    # Install
    print_warning "Installing libobjc2..."
    ninja install
    
    # Verify installation
    if [ -f "$GNUSTEP_INSTALL_DIR/lib/libobjc.so" ]; then
        print_success "libobjc2 built and installed successfully"
    else
        print_error "libobjc2 installation verification failed"
        return 1
    fi
}

# Build GNUstep make (tools-make) with Stage 1 clang
build_gnustep_make() {
    print_stage "Building GNUstep Make"
    
    local tools_make_source_dir="$PROJECT_ROOT/tools-make"
    
    # Clone or update tools-make
    clone_repository "https://github.com/gnustep/tools-make.git" "$tools_make_source_dir"
    
    # Configure with Stage 1 clang
    print_warning "Configuring gnustep-make with Stage 1 clang..."
    CC="$STAGE1_BUILD_DIR/bin/clang" \
    CXX="$STAGE1_BUILD_DIR/bin/clang++" \
    ./configure --prefix="$GNUSTEP_INSTALL_DIR"
    
    # Build and install
    print_warning "Building gnustep-make..."
    make -j${PARALLEL_JOBS}
    
    print_warning "Installing gnustep-make..."
    make install
    
    # Verify installation
    if [ -f "$GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles/aggregate.make" ]; then
        print_success "gnustep-make built and installed successfully"
        export GNUSTEP_MAKEFILES="$GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles"
    else
        print_error "gnustep-make installation verification failed"
        return 1
    fi
}

# Build GNUstep base (libs-base) with Stage 1 clang
build_gnustep_base() {
    print_stage "Building GNUstep Base with Debug Symbols"
    
    local libs_base_source_dir="$PROJECT_ROOT/libs-base"
    
    # Clone or update libs-base
    clone_repository "https://github.com/gnustep/libs-base.git" "$libs_base_source_dir"
    
    # Clean any previous build
    print_warning "Cleaning previous build..."
    if [ -f "GNUmakefile" ]; then
        make clean || print_warning "Clean failed (may not be critical)"
    fi
    
    # Set up environment variables for debugging builds
    export GNUSTEP_MAKEFILES="$GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles"
    export ADDITIONAL_OBJCFLAGS="-g -O0 -fno-omit-frame-pointer -DDEBUG=1 -fobjc-runtime=gnustep-2.1 -fconstant-string-class=NSConstantString -fno-objc-arc -I$GNUSTEP_INSTALL_DIR/include"
    export ADDITIONAL_CFLAGS="-g -O0 -fno-omit-frame-pointer -DDEBUG=1 -I$GNUSTEP_INSTALL_DIR/include"
    export ADDITIONAL_CPPFLAGS="-I$GNUSTEP_INSTALL_DIR/include"
    export ADDITIONAL_LDFLAGS="-g -L$GNUSTEP_INSTALL_DIR/lib -Wl,-rpath,$GNUSTEP_INSTALL_DIR/lib -lobjc"
    export debug=yes
    export strip=no
    export shared=yes
    
    # Set compilers to Stage 1 clang
    export CC="$STAGE1_BUILD_DIR/bin/clang"
    export CXX="$STAGE1_BUILD_DIR/bin/clang++"
    export OBJC="$STAGE1_BUILD_DIR/bin/clang"
    export OBJCXX="$STAGE1_BUILD_DIR/bin/clang++"
    export LDCC="$STAGE1_BUILD_DIR/bin/clang"
    
    # Set up library paths
    export PKG_CONFIG_PATH="$GNUSTEP_INSTALL_DIR/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    export LD_LIBRARY_PATH="$GNUSTEP_INSTALL_DIR/lib:${LD_LIBRARY_PATH:-}"
    export PATH="$GNUSTEP_INSTALL_DIR/bin:${PATH:-}"
    
    # Configure gnustep-base
    print_warning "Configuring gnustep-base..."
    if [ ! -f "configure" ]; then
        print_warning "Running autoreconf to generate configure script..."
        autoreconf -if
    fi
    
    if [ ! -f "GNUmakefile" ]; then
        CFLAGS="-g -O0 -fno-omit-frame-pointer -DDEBUG=1 -I$GNUSTEP_INSTALL_DIR/include" \
        CXXFLAGS="-g -O0 -fno-omit-frame-pointer -DDEBUG=1 -I$GNUSTEP_INSTALL_DIR/include" \
        OBJCFLAGS="-g -O0 -fno-omit-frame-pointer -DDEBUG=1 -fobjc-runtime=gnustep-2.1 -fconstant-string-class=NSConstantString -fno-objc-arc -I$GNUSTEP_INSTALL_DIR/include" \
        CPPFLAGS="-I$GNUSTEP_INSTALL_DIR/include" \
        LDFLAGS="-L$GNUSTEP_INSTALL_DIR/lib -Wl,-rpath,$GNUSTEP_INSTALL_DIR/lib -lobjc" \
        RUNTIME_VERSION="gnustep-2.1" \
        ./configure \
            --prefix="$GNUSTEP_INSTALL_DIR" \
            --enable-debug \
            --disable-strip \
            --enable-objc-nonfragile-abi \
            --disable-mixedabi \
            --with-installation-domain=SYSTEM \
            --with-library-combo=ng-gnu-gnu \
            --enable-libffi \
            --enable-static=no \
            --enable-shared=yes
    fi
    
    # Build with debug symbols
    print_warning "Building gnustep-base with debug symbols (this may take 20-30 minutes)..."
    make -j${PARALLEL_JOBS} debug=yes strip=no ADDITIONAL_OBJCFLAGS="-fno-objc-arc"
    
    # Install
    print_warning "Installing gnustep-base..."
    make install debug=yes strip=no ADDITIONAL_OBJCFLAGS="-fno-objc-arc"
    
    # Verify installation
    local base_lib=""
    for pattern in \
        "$GNUSTEP_INSTALL_DIR/lib/libgnustep-base.so" \
        "$GNUSTEP_INSTALL_DIR/lib/libgnustep-base.so."* \
        "$GNUSTEP_INSTALL_DIR/System/Library/Libraries/libgnustep-base.so" \
        "$GNUSTEP_INSTALL_DIR/System/Library/Libraries/libgnustep-base.so."*; do
        for f in $pattern; do
            if [ -f "$f" ]; then 
                base_lib="$f"
                break 2
            fi
        done
    done
    
    if [ -n "$base_lib" ]; then
        print_success "gnustep-base built and installed successfully"
    else
        print_error "gnustep-base installation verification failed"
        return 1
    fi
}

# Build GNUstep with Stage 1 Clang
build_gnustep_with_stage1_clang() {
    print_stage "Building GNUstep Environment with Stage 1 Clang"
    
    # Verify Stage 1 clang exists
    if [ ! -f "$STAGE1_BUILD_DIR/bin/clang" ]; then
        print_error "Stage 1 clang not found at $STAGE1_BUILD_DIR/bin/clang"
        print_error "Stage 1 must be built before building GNUstep"
        return 1
    fi
    
    # Set LLVM_BUILD_DIR for GNUstep functions
    export LLVM_BUILD_DIR="$STAGE1_BUILD_DIR"
    
    # Create GNUstep build and install directories
    mkdir -p "$GNUSTEP_BUILD_DIR"
    mkdir -p "$GNUSTEP_INSTALL_DIR"
    
    # Build components in order
    build_libobjc2
    build_gnustep_make  
    build_gnustep_base
    
    # Create GNUstep environment setup script
    create_gnustep_environment_script
    
    print_success "GNUstep environment built successfully with Stage 1 clang"
}

# Create combined environment setup script
create_gnustep_environment_script() {
    print_stage "Creating Combined Development Environment Script"
    
    local env_script="$BUILD_DIR/setup_dev_env.sh"
    
    cat > "$env_script" << EOF
#!/bin/bash
# Combined LLDB + GNUstep Development Environment Setup
# Source this script to set up the complete environment for LLDB and GNUstep development

echo "🚀 Setting up LLDB + GNUstep Development Environment..."

# LLDB environment
export PATH="$BUILD_DIR/bin:\${PATH:-}"
export LD_LIBRARY_PATH="$BUILD_DIR/lib:\${LD_LIBRARY_PATH:-}"

# GNUstep environment
export LD_LIBRARY_PATH="$GNUSTEP_INSTALL_DIR/lib:\${LD_LIBRARY_PATH:-}"
export PKG_CONFIG_PATH="$GNUSTEP_INSTALL_DIR/lib/pkgconfig:\${PKG_CONFIG_PATH:-}"
export GNUSTEP_MAKEFILES="$GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles"
export GNUSTEP_SYSTEM_ROOT="$GNUSTEP_INSTALL_DIR"
export GNUSTEP_INSTALLATION_DIR="$GNUSTEP_INSTALL_DIR"
export GNUSTEP_FLATTENED=yes

# Prefer Stage 1 clang if available, otherwise use LLDB build clang
if [ -f "$STAGE1_BUILD_DIR/bin/clang" ]; then
    export PATH="$STAGE1_BUILD_DIR/bin:\${PATH:-}"
    echo "✅ Using Stage 1 clang: $STAGE1_BUILD_DIR/bin/clang"
elif [ -f "$BUILD_DIR/bin/clang" ]; then
    echo "✅ Using LLDB build clang: $BUILD_DIR/bin/clang"
else
    echo "⚠️  Using system clang"
fi

# Verify tools are available
if [ -f "$BUILD_DIR/bin/lldb" ]; then
    echo "✅ Custom LLDB: $BUILD_DIR/bin/lldb"
else
    echo "⚠️  Custom LLDB not found at $BUILD_DIR/bin/lldb"
fi

if [ -f "$GNUSTEP_INSTALL_DIR/lib/libobjc.so" ]; then
    echo "✅ GNUstep Runtime: $GNUSTEP_INSTALL_DIR/lib/libobjc.so"
else
    echo "⚠️  GNUstep Runtime not found"
fi

echo "🔧 Combined Environment Configured"
echo "📁 LLDB Build: $BUILD_DIR"
echo "📁 GNUstep Install: $GNUSTEP_INSTALL_DIR"
echo "🎯 Ready for Objective-C development and debugging!"
echo ""
echo "💡 Quick commands:"
echo "   lldb/dev.sh build-example custom_class_test"
echo "   lldb/dev.sh debug custom_class_test"
EOF
    
    chmod +x "$env_script"
    print_success "Combined environment script created: $env_script"
}

# Show final instructions
show_instructions() {
    print_stage "Build Complete!"
    
    echo -e "${GREEN}Your two-stage LLDB build with GNUstep environment is ready!${NC}"
    echo ""
    echo "Build locations:"
    echo "  Stage 1 (Bootstrap): $STAGE1_BUILD_DIR"
    echo "  Final Build:         $BUILD_DIR"
    echo "  GNUstep Install:     $GNUSTEP_INSTALL_DIR"
    echo ""
    echo "Binaries:"
    echo "  LLDB:        $BUILD_DIR/bin/lldb"
    echo "  lldb-server: $BUILD_DIR/bin/lldb-server"
    echo "  Clang:       $BUILD_DIR/bin/clang"
    echo ""
    echo "GNUstep Libraries:"
    echo "  libobjc2:    $GNUSTEP_INSTALL_DIR/lib/libobjc.so"
    echo "  gnustep-base: $GNUSTEP_INSTALL_DIR/lib/libgnustep-base.so"
    echo ""
    echo "To use manually:"
    echo "  source $BUILD_DIR/setup_dev_env.sh"
    echo ""
    echo "For incremental rebuilds:"
    echo "  cd lldb"
    echo "  ./dev.sh build"
    echo ""
    echo "For development and testing:"
    echo "  cd lldb"
    echo "  ./dev.sh build-example custom_class_test"
    echo "  ./dev.sh debug custom_class_test"
    echo ""
    echo -e "${GREEN}🎉 Ready for Objective-C debugging!${NC}"
}

# Main execution
main() {
    # Parse arguments
    SKIP_STAGE1=false
    SKIP_STAGE2=false
    SKIP_DEPS=false
    CLEAN_BUILD=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --skip-stage1)
                SKIP_STAGE1=true
                shift
                ;;
            --skip-stage2)
                SKIP_STAGE2=true
                shift
                ;;
            --skip-deps)
                SKIP_DEPS=true
                shift
                ;;
            --clean)
                CLEAN_BUILD=true
                shift
                ;;
            --help)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --skip-stage1   Skip building stage 1 (use existing)"
                echo "  --skip-stage2   Skip building stage 2 (stage 1 only)"
                echo "  --skip-deps     Skip installing system dependencies"
                echo "  --clean         Clean build directories before starting"
                echo "  --help          Show this help"
                echo ""
                echo "Environment variables:"
                echo "  PARALLEL_JOBS   Number of parallel build jobs (default: nproc)"
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                ;;
        esac
    done
    
    # Clean if requested
    if $CLEAN_BUILD; then
        print_warning "Cleaning build directories..."
        rm -rf "$STAGE1_BUILD_DIR" "$BUILD_DIR" "$GNUSTEP_BUILD_DIR"
    fi
    
    # Run build stages
    if ! $SKIP_DEPS; then
        install_gnustep_dependencies
    else
        print_success "Skipping dependency installation"
    fi
    
    check_prerequisites
    
    if ! $SKIP_STAGE1; then
        build_stage1
    else
        if [ ! -f "$STAGE1_BUILD_DIR/bin/clang" ]; then
            print_error "Stage 1 not found. Cannot skip stage 1."
        fi
        print_success "Skipping Stage 1 (using existing)"
    fi
    
    if ! $SKIP_STAGE2; then
        build_stage2
        verify_build
        create_helpers
        
        # Build GNUstep after successful LLDB build
        build_gnustep_with_stage1_clang
        
    else
        print_success "Skipping Stage 2"
    fi
    
    show_instructions
    print_success "Build script completed successfully!"
}

# Run main function
main "$@"