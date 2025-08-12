#!/bin/bash
# Build operations for LLVM/LLDB on Windows MSYS2/UCRT64

set -euo pipefail

# Source common utilities
HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HELPERS_DIR/common.sh"

# Initialize build variables if not already set
if [ -z "${LLVM_BUILD_DIR:-}" ]; then
    SCRIPT_DIR="$(dirname "$HELPERS_DIR")"
    WORKSPACE_ROOT="$(dirname "$SCRIPT_DIR")"
    PROJECT_ROOT="$(dirname "$WORKSPACE_ROOT")"
    LLVM_BUILD_DIR="${PROJECT_ROOT}/build"
fi

if [ -z "${BUILD_TYPE:-}" ]; then
    BUILD_TYPE="RelWithDebInfo"
fi

if [ -z "${PARALLEL_JOBS:-}" ]; then
    PARALLEL_JOBS=$(nproc)
fi

# Function to clean CMake cache and build artifacts
clean_cmake_cache() {
    print_progress "Cleaning CMake cache and build artifacts..."
    
    if [ -d "$LLVM_BUILD_DIR/build" ]; then
        cd "$LLVM_BUILD_DIR/build"
        
        # Remove CMake cache files
        rm -f CMakeCache.txt
        rm -rf CMakeFiles/
        rm -f cmake_install.cmake
        rm -f build.ninja
        rm -f rules.ninja
        
        print_success "CMake cache cleaned"
    else
        print_info "No build directory found, nothing to clean"
    fi
}

# Function to fix CMake path issues for Python scripts
fix_cmake_python_paths() {
    print_progress "Fixing CMake Python path configuration..."
    
    if [ ! -d "$LLVM_BUILD_DIR/build" ]; then
        print_error "Build directory not found"
        return 1
    fi
    
    cd "$LLVM_BUILD_DIR/build"
    
    # If CMakeCache.txt exists and has path issues, clean and reconfigure
    if [ -f "CMakeCache.txt" ]; then
        # Check if there are corrupted paths in the cache
        if grep -q "msys2-ucrt64-toolchain.*home.*kardjali" CMakeCache.txt 2>/dev/null; then
            print_warning "Detected old MSYS2 installation paths in CMake cache"
            print_info "Cleaning cache to fix path issues..."
            clean_cmake_cache
            return 0
        fi
        
        # Check for the old MSYS2 installation path
        if grep -q "C:/Users/kardjali/Downloads/msys2-ucrt64-toolchain" CMakeCache.txt 2>/dev/null; then
            print_warning "Detected old MSYS2 installation path in CMake cache"
            print_info "Cleaning cache to fix path issues..."
            clean_cmake_cache
            return 0
        fi
        
        # Check for mixed path formats that cause issues
        if grep -q "/c/.*C:/" CMakeCache.txt 2>/dev/null; then
            print_warning "Detected corrupted mixed path formats in CMake cache"
            print_info "Cleaning cache to fix path issues..."
            clean_cmake_cache
            return 0
        fi
    fi
    
    print_success "CMake paths look clean"
    return 0
}

# Function to completely clean all build artifacts and caches
deep_clean_build() {
    print_progress "Performing deep clean of build directory and caches..."
    
    if [ -d "$LLVM_BUILD_DIR" ]; then
        cd "$LLVM_BUILD_DIR"
        
        # Remove entire build directory
        rm -rf build/
        
        # Also clean any CMake generated files in parent directories
        find . -name "CMakeCache.txt" -delete 2>/dev/null || true
        find . -name "CMakeFiles" -type d -exec rm -rf {} + 2>/dev/null || true
        find . -name "cmake_install.cmake" -delete 2>/dev/null || true
        find . -name "build.ninja" -delete 2>/dev/null || true
        find . -name "rules.ninja" -delete 2>/dev/null || true
        
        print_success "Deep clean completed"
    else
        print_info "No build directory found, nothing to clean"
    fi
    
    # Also clean any leftover cache files in the LLDB directory
    local lldb_cache="$WORKSPACE_ROOT/build/CMakeCache.txt"
    if [ -f "$lldb_cache" ]; then
        print_info "Removing LLDB cache file: $lldb_cache"
        rm -f "$lldb_cache"
    fi
    
    # Clean any files with old MSYS2 installation paths
    cd "$PROJECT_ROOT"
    local old_path_files=$(find . -name "*.cmake" -exec grep -l "msys2-ucrt64-toolchain.*Downloads" {} \; 2>/dev/null || true)
    if [ -n "$old_path_files" ]; then
        print_warning "Found files with old MSYS2 paths, removing them..."
        echo "$old_path_files" | while read -r file; do
            print_info "Removing: $file"
            rm -f "$file"
        done
    fi
}

# Function to configure LLVM build for Windows
configure_windows_build() {
    print_progress "Configuring LLVM build for Windows MSYS2/UCRT64..."
    
    ensure_directory "$LLVM_BUILD_DIR/build" "build directory"
    cd "$LLVM_BUILD_DIR/build"
    
    # Set up environment for finding MSYS2 libraries
    export PKG_CONFIG_PATH="/usr/lib/pkgconfig:/ucrt64/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    export CMAKE_PREFIX_PATH="/usr:/ucrt64:${CMAKE_PREFIX_PATH:-}"
    print_info "PKG_CONFIG_PATH: $PKG_CONFIG_PATH"
    print_info "CMAKE_PREFIX_PATH: $CMAKE_PREFIX_PATH"
    
    # Python configuration for MSYS2 - now enabled with PYTHONHOME fix
    print_info "Python support enabled with PYTHONHOME=/ucrt64"
    
    # Combined Windows + Ubuntu22 CMake configuration with tested working flags
    local cmake_args=(
        "-G" "Ninja"
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo"  # Use RelWithDebInfo from Ubuntu22
        "-DLLVM_ENABLE_PROJECTS=clang;lldb"  # Focus on clang;lldb as tested
        "-DLLVM_TARGETS_TO_BUILD=X86"  # Focus on X86 as tested  
        "-DLLVM_ENABLE_RTTI=ON"  # Ubuntu22 working flag - needed for exceptions
        "-DLLVM_ENABLE_EH=ON"  # Ubuntu22 working flag - exception handling
        "-DLLVM_ENABLE_THREADS=ON"  # Ubuntu22 working flag - threading support
        "-DLLDB_ENABLE_PYTHON=ON"  # Ubuntu22 working flag - Python bindings
        "-DLLDB_USE_SYSTEM_DEBUGSERVER=ON"  # Ubuntu22 working flag
        "-DLLVM_BUILD_LLVM_DYLIB=OFF"  # Static linking on Windows
        "-DLLVM_LINK_LLVM_DYLIB=OFF"
        "-DLLVM_INCLUDE_TESTS=OFF"  # Disable tests to avoid path issues
        "-DLLVM_INCLUDE_BENCHMARKS=OFF"  # Disable benchmarks
        "-DLLVM_INCLUDE_EXAMPLES=OFF"  # Disable examples
        "-DLLVM_INCLUDE_DOCS=OFF"  # Disable documentation generation
        "-DLLVM_BUILD_TOOLS=ON"  # Keep essential tools
        "-DLLDB_INCLUDE_TESTS=OFF"  # Disable LLDB tests
        "-DLLDB_ENABLE_LIBEDIT=OFF"  # Disable libedit to avoid compatibility issues
        "-DCLANG_INCLUDE_TESTS=OFF"  # Disable Clang tests
        "-DCLANG_BUILD_TOOLS=ON"  # Keep Clang tools
        "-DCMAKE_MSYS2_ARG_CONV=STRICT"  # Strict path conversion
        "-DCMAKE_MSYS_RUNTIME_PATH_RESOLUTION=1"  # Help CMake resolve MSYS paths
        "-DMSYS=1"  # Tell CMake we're in MSYS2 environment
        "-DCMAKE_SYSTEM_NAME=Windows"  # Explicitly set system name
        "-DCMAKE_HOST_SYSTEM_NAME=Windows"  # Set host system
        "-DLLVM_ENABLE_ASSERTIONS=ON"
        "-DBUILD_SHARED_LIBS=ON"
        "-DLLVM_ENABLE_ZLIB=ON"
        "-DLLVM_ENABLE_ZSTD=ON"
        "-DLLVM_ENABLE_LIBXML2=ON"
        "-DLLVM_PARALLEL_COMPILE_JOBS=$PARALLEL_JOBS"
        "-DLLVM_PARALLEL_LINK_JOBS=2"  # Limit link jobs on Windows
        "-DCMAKE_C_COMPILER=clang"  # Use system clang 20.1.8 (compatible)
        "-DCMAKE_CXX_COMPILER=clang++"  # Use system clang++ 20.1.8 (compatible)
        "-DCMAKE_LINKER=lld"
        "-DLLVM_USE_LINKER=lld"
        "-DCMAKE_RC_COMPILER=windres"
        "-DLLVM_HOST_TRIPLE=x86_64-w64-windows-gnu"  # Explicit host triple
        "-DLLVM_TARGET_TRIPLE=x86_64-w64-windows-gnu"  # Explicit target triple
    )
    
    # Add ccache if available
    if command_exists ccache; then
        cmake_args+=(
            "-DCMAKE_C_COMPILER_LAUNCHER=ccache"
            "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
        )
        print_info "ccache enabled for build"
    fi
    
    # Windows-specific optimizations
    cmake_args+=(
        "-DCMAKE_C_FLAGS=-O2 -g -fuse-ld=lld"
        "-DCMAKE_CXX_FLAGS=-O2 -g -fuse-ld=lld -std=c++17"
        "-DCMAKE_EXE_LINKER_FLAGS=-Wl,--large-address-aware"
    )
    
    print_progress "Running CMake configuration..."
    print_info "This may take 5-10 minutes on first run..."
    
    if cmake "$PROJECT_ROOT/llvm" "${cmake_args[@]}"; then
        print_success "CMake configuration completed successfully"
    else
        print_error "CMake configuration failed"
        print_info "Check the error messages above for details"
        exit 1
    fi
    
    # Generate compile_commands.json for IDE support
    print_info "Build system: Ninja"
    print_info "Build type: $BUILD_TYPE"
    print_info "Parallel compile jobs: $PARALLEL_JOBS"
    print_info "Parallel link jobs: 2"
}

# Function to build LLDB on Windows
build_lldb_windows() {
    print_progress "Building LLDB (this will take 1-3 hours on Windows)..."
    
    cd "$LLVM_BUILD_DIR/build"
    
    # Check if we're resuming a build
    if [ -f "CMakeCache.txt" ]; then
        print_info "Found existing CMake cache, resuming build..."
    else
        print_error "No CMake cache found. Run configuration first."
    fi
    
    # Set memory limit for linker to prevent OOM on Windows
    export LDFLAGS="-Wl,--no-keep-memory"
    
    # Build LLDB
    print_progress "Starting LLDB build..."
    print_info "Building with $PARALLEL_JOBS parallel jobs"
    print_info "This is the longest step - please be patient"
    
    local start_time=$(date +%s)
    
    # Build in stages to manage memory on Windows
    print_progress "Stage 1: Building LLVM core libraries..."
    ninja -j$PARALLEL_JOBS LLVMCore LLVMSupport || {
        print_error "Failed to build LLVM core libraries"
        exit 1
    }
    
    print_progress "Stage 2: Building Clang..."
    ninja -j$PARALLEL_JOBS clang || {
        print_error "Failed to build Clang"
        exit 1
    }
    
    print_progress "Stage 3: Building LLDB..."
    ninja -j$PARALLEL_JOBS lldb || {
        print_error "Failed to build LLDB"
        exit 1
    }
    
    local end_time=$(date +%s)
    local build_time=$(( (end_time - start_time) / 60 ))
    
    print_success "LLDB built successfully in ${build_time} minutes"
    
    # Verify LLDB executable
    if [ -f "bin/lldb.exe" ]; then
        local lldb_size=$(du -h "bin/lldb.exe" | cut -f1)
        print_success "LLDB executable created: bin/lldb.exe (${lldb_size})"
        
        # Test LLDB version
        ./bin/lldb.exe --version || print_warning "Could not get LLDB version"
    else
        print_error "LLDB executable not found after build"
    fi
}

# Function to build lldb-server on Windows
build_lldb_server_windows() {
    print_progress "Building lldb-server..."
    
    cd "$LLVM_BUILD_DIR/build"
    
    # Build lldb-server
    if ninja -j$PARALLEL_JOBS lldb-server; then
        print_success "lldb-server built successfully"
        
        if [ -f "bin/lldb-server.exe" ]; then
            local server_size=$(du -h "bin/lldb-server.exe" | cut -f1)
            print_success "lldb-server executable: bin/lldb-server.exe (${server_size})"
        else
            print_warning "lldb-server.exe not found, checking for alternative names..."
            # Sometimes it might be named differently
            ls -la bin/*lldb* 2>/dev/null || true
        fi
    else
        print_warning "lldb-server build failed (not critical for local debugging)"
    fi
}

# Function to build specific LLDB components
build_lldb_component() {
    local component="$1"
    print_progress "Building $component..."
    
    cd "$LLVM_BUILD_DIR/build"
    
    if ninja -j$PARALLEL_JOBS "$component"; then
        print_success "$component built successfully"
    else
        print_error "Failed to build $component"
    fi
}

# Function to create symbolic links for easier access
create_build_symlinks() {
    print_progress "Creating convenient symlinks..."
    
    cd "$LLVM_BUILD_DIR"
    
    # Create symlinks to important binaries
    if [ -f "build/bin/lldb.exe" ]; then
        ln -sf "build/bin/lldb.exe" "lldb.exe"
        print_info "Created symlink: $LLVM_BUILD_DIR/lldb.exe"
    fi
    
    if [ -f "build/bin/clang.exe" ]; then
        ln -sf "build/bin/clang.exe" "clang.exe"
        print_info "Created symlink: $LLVM_BUILD_DIR/clang.exe"
    fi
}

# Function to generate build report
generate_build_report() {
    print_progress "Generating build report..."
    
    local report_file="$LLVM_BUILD_DIR/build-report.txt"
    
    {
        echo "LLVM/LLDB Build Report - Windows MSYS2/UCRT64"
        echo "============================================="
        echo "Date: $(date)"
        echo "System: $(uname -a)"
        echo "MSYSTEM: $MSYSTEM"
        echo ""
        echo "Build Configuration:"
        echo "  Build directory: $LLVM_BUILD_DIR"
        echo "  Build type: $BUILD_TYPE"
        echo "  Parallel jobs: $PARALLEL_JOBS"
        echo "  Compiler: $(clang --version | head -1)"
        echo ""
        echo "Built Components:"
        
        if [ -f "$LLVM_BUILD_DIR/build/bin/lldb.exe" ]; then
            echo "  ✓ LLDB: $(du -h "$LLVM_BUILD_DIR/build/bin/lldb.exe" | cut -f1)"
        fi
        
        if [ -f "$LLVM_BUILD_DIR/build/bin/lldb-server.exe" ]; then
            echo "  ✓ lldb-server: $(du -h "$LLVM_BUILD_DIR/build/bin/lldb-server.exe" | cut -f1)"
        fi
        
        if [ -f "$LLVM_BUILD_DIR/build/bin/clang.exe" ]; then
            echo "  ✓ Clang: $(du -h "$LLVM_BUILD_DIR/build/bin/clang.exe" | cut -f1)"
        fi
        
        echo ""
        echo "Disk usage:"
        du -sh "$LLVM_BUILD_DIR/build" 2>/dev/null || echo "  Build directory size: unknown"
        
    } > "$report_file"
    
    print_success "Build report saved to: $report_file"
    cat "$report_file"
}

# Export functions
export -f clean_cmake_cache fix_cmake_python_paths deep_clean_build configure_windows_build build_lldb_windows build_lldb_server_windows
export -f build_lldb_component create_build_symlinks generate_build_report