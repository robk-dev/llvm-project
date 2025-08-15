#!/bin/bash
# Minimal LLDB build setup for MSYS2 UCRT64
# Assumes toolchain, gnustep, and libobjc2 are already installed
# No assumptions about paths - everything auto-detected

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Output functions
print_section() {
    echo ""
    echo -e "${BLUE}===============================================${NC}"
    echo -e "${BLUE} $1${NC}"
    echo -e "${BLUE}===============================================${NC}"
    echo ""
}

print_info() {
    echo -e "${CYAN}ℹ️  $1${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
    exit 1
}

print_progress() {
    echo -e "${CYAN}➤ $1${NC}"
}

# Auto-detect paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LLDB_ROOT="$(dirname "$SCRIPT_DIR")"
LLVM_PROJECT_ROOT="$(dirname "$LLDB_ROOT")"
BUILD_DIR="$LLVM_PROJECT_ROOT/build"

print_section "🚀 Minimal LLDB Setup for MSYS2 UCRT64"

# Verify we're in MSYS2 UCRT64
if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
    print_error "This script must be run in MSYS2 UCRT64 environment"
    exit 1
fi

print_info "Auto-detected paths:"
print_info "  LLVM Project: $LLVM_PROJECT_ROOT"
print_info "  LLDB Source:  $LLDB_ROOT" 
print_info "  Build Dir:    $BUILD_DIR"

# Verify required directories exist
if [[ ! -d "$LLVM_PROJECT_ROOT/llvm" ]]; then
    print_error "LLVM source not found at $LLVM_PROJECT_ROOT/llvm"
fi

if [[ ! -d "$LLDB_ROOT/source" ]]; then
    print_error "LLDB source not found at $LLDB_ROOT/source"
fi

# Check for required tools
print_progress "Checking required tools..."
required_tools=("cmake" "ninja" "clang.exe" "clang++.exe" "python3")
missing_tools=()

for tool in "${required_tools[@]}"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        missing_tools+=("$tool")
    fi
done

if [[ ${#missing_tools[@]} -gt 0 ]]; then
    print_error "Missing required tools: ${missing_tools[*]}"
fi

print_success "All required tools found"

# Install SWIG for Python bindings if not present
print_progress "Checking for SWIG (needed for Python support)..."
if ! command -v swig >/dev/null 2>&1; then
    print_info "Installing SWIG for Python bindings..."
    if ! pacman -S --noconfirm --needed mingw-w64-ucrt-x86_64-swig; then
        print_warning "Failed to install SWIG - Python support will be disabled"
        ENABLE_PYTHON=OFF
    else
        ENABLE_PYTHON=ON
    fi
else
    print_success "SWIG found - Python support will be enabled"
    ENABLE_PYTHON=ON
fi

# Detect system resources for optimal build settings
print_progress "Detecting system resources..."
CPU_CORES=$(nproc)
TOTAL_RAM_KB=$(grep MemTotal /proc/meminfo | awk '{print $2}')
TOTAL_RAM_GB=$((TOTAL_RAM_KB / 1024 / 1024))

# Conservative parallel job calculation for LLDB builds
# LLDB linking can be very memory intensive
MEMORY_JOBS=$((TOTAL_RAM_GB / 3))  # ~3GB per linker job
CPU_JOBS=$((CPU_CORES / 2))        # Don't max out CPU
PARALLEL_COMPILE_JOBS=$((CPU_CORES))
PARALLEL_LINK_JOBS=$((MEMORY_JOBS < 2 ? 1 : (MEMORY_JOBS > 2 ? 2 : MEMORY_JOBS)))

print_info "System: ${CPU_CORES} cores, ${TOTAL_RAM_GB}GB RAM"
print_info "Build settings: ${PARALLEL_COMPILE_JOBS} compile jobs, ${PARALLEL_LINK_JOBS} link jobs"

# Create build directory
print_progress "Setting up build directory..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Clear any existing CMake cache to avoid configuration conflicts
if [[ -f "CMakeCache.txt" ]]; then
    print_info "Clearing existing CMake cache..."
    rm -f CMakeCache.txt
    rm -rf CMakeFiles/
fi

# Auto-detect compiler paths
CLANG_PATH=$(which clang.exe)
CLANGXX_PATH=$(which clang++.exe)

if [[ "$ENABLE_PYTHON" == "ON" ]]; then
    PYTHON_HOME=$(python3 -c "import sys; print(sys.prefix)")
    print_info "Using compilers:"
    print_info "  Clang: $CLANG_PATH"
    print_info "  Clang++: $CLANGXX_PATH"
    print_info "  Python: $PYTHON_HOME"
else
    print_info "Using compilers:"
    print_info "  Clang: $CLANG_PATH"
    print_info "  Clang++: $CLANGXX_PATH"
    print_warning "Python support disabled (no SWIG)"
fi

# Configure CMake with optimal settings based on available features
print_progress "Configuring LLDB build..."

# Verify editline components exist
# Note: MSYS2 editline is incompatible with LLDB's BSD editline expectations
EDITLINE_FOUND=false
print_info "Disabling editline - MSYS2 implementation incompatible with LLDB"

# Build CMake arguments dynamically
CMAKE_ARGS=(
    "$LLVM_PROJECT_ROOT/llvm"
    -G "Ninja"
    -DCMAKE_C_COMPILER="$CLANG_PATH"
    -DCMAKE_CXX_COMPILER="$CLANGXX_PATH"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
    -DLLVM_ENABLE_PROJECTS="clang;lldb"
    -DLLVM_TARGETS_TO_BUILD="X86"
    -DBUILD_SHARED_LIBS=OFF
    -DLLDB_ENABLE_CURSES=OFF
    # -DLLDB_DISABLE_PYTHON=OFF
    # -DLLDB_BUILD_FRAMEWORK=OFF
    # -DLLDB_ENABLE_LIBXML2=OFF
    # -DLLDB_ENABLE_LZMA=OFF
    -DLLVM_PARALLEL_COMPILE_JOBS="$PARALLEL_COMPILE_JOBS"
    -DLLVM_PARALLEL_LINK_JOBS="$PARALLEL_LINK_JOBS"
    -DLLVM_INCLUDE_TESTS=OFF
    -DLLDB_INCLUDE_TESTS=OFF
    -DCLANG_INCLUDE_TESTS=OFF
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,--no-keep-memory -Wl,--reduce-memory-overheads"
)

# Add editline support if available
if [[ "$EDITLINE_FOUND" == "true" ]]; then
    CMAKE_ARGS+=(
        -DLLDB_ENABLE_LIBEDIT=ON
        -DLibEdit_INCLUDE_DIRS="/ucrt64/include"
        -DLibEdit_LIBRARIES="/ucrt64/lib/libedit.a"
    )
    print_info "Editline support: ENABLED"
else
    CMAKE_ARGS+=(-DLLDB_ENABLE_LIBEDIT=OFF)
    print_info "Editline support: DISABLED"
fi

# Add Python support if SWIG is available
if [[ "$ENABLE_PYTHON" == "ON" ]]; then
    CMAKE_ARGS+=(
        -DLLDB_ENABLE_PYTHON=ON
        -DLLDB_PYTHON_HOME="$PYTHON_HOME"
        -DLLDB_EMBED_PYTHON_HOME=ON
    )
    print_info "Python scripting: ENABLED"
else
    CMAKE_ARGS+=(-DLLDB_ENABLE_PYTHON=OFF)
    print_info "Python scripting: DISABLED"
fi

cmake "${CMAKE_ARGS[@]}"

print_success "CMake configuration completed!"

# Build essential targets in order
print_progress "Building LLVM support libraries..."
ninja -j"$PARALLEL_COMPILE_JOBS" LLVMSupport LLVMCore LLVMTargetParser LLVMBinaryFormat LLVMObject

print_progress "Building Clang dependencies..."
ninja -j"$PARALLEL_COMPILE_JOBS" clang-tablegen-targets

print_progress "Building LLDB server (memory-conservative)..."
export LDFLAGS="-Wl,--no-keep-memory -Wl,--reduce-memory-overheads -Wl,--as-needed"
ninja -j8 lldb-server

if [[ -f "bin/lldb-server.exe" ]]; then
    print_success "LLDB server built successfully!"
    
    print_progress "Building main LLDB executable..."
    ninja -j8 lldb
    
    if [[ -f "bin/lldb.exe" ]]; then
        print_success "LLDB built successfully!"
        print_info "Binaries location: $BUILD_DIR/bin/"
        
        # Quick verification
        print_progress "Verifying LLDB installation..."
        if ./bin/lldb.exe --version >/dev/null 2>&1; then
            print_success "LLDB verification passed!"
            echo ""
            print_section "🎉 Build Complete!"
            print_info "LLDB executable: $BUILD_DIR/bin/lldb.exe"
            print_info "LLDB server:     $BUILD_DIR/bin/lldb-server.exe"
            echo ""
            print_info "You can now use LLDB with:"
            print_info "  $BUILD_DIR/bin/lldb.exe [options] [program]"
            echo ""
        else
            print_warning "LLDB built but failed verification - may still be usable"
        fi
    else
        print_warning "Main LLDB executable failed to build, but lldb-server is available"
        print_info "LLDB server can be used for remote debugging"
    fi
else
    print_error "LLDB server build failed"
fi

print_section "✨ Setup Complete"
print_info "Build artifacts are in: $BUILD_DIR/bin/"
print_info "To rebuild individual components:"
print_info "  cd $BUILD_DIR"  
print_info "  ninja lldb          # Rebuild main LLDB"
print_info "  ninja lldb-server   # Rebuild LLDB server"
print_info "  ninja clean         # Clean build artifacts"
