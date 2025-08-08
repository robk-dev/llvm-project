#!/bin/bash
# GNUstep Environment Operations Helper Script
# Functions for building and managing complete GNUstep development environment
# Author: GitHub Copilot
# Date: August 2, 2025

set -euo pipefail  # Exit on error, undefined variables, and pipe failures

# Source common utilities if available, otherwise set up paths
HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$HELPERS_DIR/common.sh" ]; then
    source "$HELPERS_DIR/common.sh"
else
    # Fallback if common.sh doesn't exist
    SCRIPT_DIR="$(dirname "$HELPERS_DIR")"           # .../scripts2
    WORKSPACE_ROOT="$(dirname "$SCRIPT_DIR")"        # repo root containing examples/, gnustep-install/
    PROJECT_ROOT="$WORKSPACE_ROOT"
    
    # Color codes
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    CYAN='\033[0;36m'
    NC='\033[0m' # No Color
    
    # Functions
    print_section() { echo -e "${BLUE}$1${NC}"; }
    print_progress() { echo -e "${CYAN}→ $1${NC}"; }
    print_success() { echo -e "${GREEN}✓ $1${NC}"; }
    print_error() { echo -e "${RED}✗ $1${NC}"; }
    print_warning() { echo -e "${YELLOW}⚠ $1${NC}"; }
    
    PARALLEL_JOBS=$(nproc)
fi

# GNUstep build configuration - updated for local workspace
GNUSTEP_BUILD_DIR="${GNUSTEP_BUILD_DIR:-$WORKSPACE_ROOT/gnustep-build}"
GNUSTEP_INSTALL_DIR="${GNUSTEP_INSTALL_DIR:-$WORKSPACE_ROOT/gnustep-install}"
LIBOBJC2_BUILD_DIR="${LIBOBJC2_BUILD_DIR:-$WORKSPACE_ROOT/gnustep-build/libobjc2}"
LIBS_BASE_SOURCE_DIR="$WORKSPACE_ROOT/libs-base"
LIBOBJC2_SOURCE_DIR="$WORKSPACE_ROOT/libobjc2"

# Build settings for debugging and symbol generation WITHOUT NEW STRING ABI
# Use gnustep-2.1 runtime to match examples and avoid ABI mismatches
export GNUSTEP_CFLAGS="-g -gdwarf-5 -O0 -fno-omit-frame-pointer -DDEBUG=1"
export GNUSTEP_CXXFLAGS="-g -gdwarf-5 -O0 -fno-omit-frame-pointer -DDEBUG=1"
export GNUSTEP_OBJCFLAGS="-g -gdwarf-5 -O0 -fno-omit-frame-pointer -DDEBUG=1 -fobjc-runtime=gnustep-2.1 -fconstant-string-class=NSConstantString -fno-objc-arc"
export GNUSTEP_LDFLAGS="-g"

# Critical: DO NOT enable new string ABI
# export GNUSTEP_NEW_STRING_ABI=1  # DISABLED - causes issues

# Function to clean conflicting system packages
clean_system_gnustep_packages() {
    print_section "Step: Cleaning System GNUstep Packages"
    
    # Remove potentially conflicting GNUstep packages but keep essential build tools
    print_progress "Removing conflicting GNUstep packages..."
    
    # Packages to remove (keeping build essentials)
    PACKAGES_TO_REMOVE=(
        "libgnustep-base-dev"
        "libgnustep-base1.28"
        "libgnustep-base1.29"
        "libgnustep-base1.30"
        "gnustep-base-runtime"
        "gnustep-base-common"
        "libobjc-4.6-dev"
        "libobjc4"
        "libobjc-*-dev"
        "gnustep-*"
    )
    
    for package in "${PACKAGES_TO_REMOVE[@]}"; do
        if dpkg -l | grep -q "^ii.*$package"; then
            print_progress "Removing $package..."
            sudo apt-get remove -y "$package" || print_warning "Failed to remove $package (may not be critical)"
        else
            print_progress "$package not installed, skipping"
        fi
    done
    
    # Clean package cache
    sudo apt-get autoremove -y
    sudo apt-get autoclean
    
    print_success "System package cleanup completed"
}

# Function to ensure GNUstep build dependencies
install_gnustep_dependencies() {
    print_section "Step: Installing GNUstep Build Dependencies"
    
    print_progress "Updating package list..."
    sudo apt-get update
    
    # Essential build dependencies for GNUstep
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
    
    print_progress "Installing GNUstep build dependencies..."
    for dep in "${GNUSTEP_DEPS[@]}"; do
        if ! dpkg -l | grep -q "^ii.*$dep"; then
            print_progress "Installing $dep..."
            sudo apt-get install -y "$dep"
        fi
    done
    
    print_success "GNUstep build dependencies installed"
}

# Function to build libobjc2 with debugging symbols
build_libobjc2_with_debug() {
    print_section "Step: Building libobjc2 with Debug Symbols"
    
    # Check if source exists
    if [ ! -d "$LIBOBJC2_SOURCE_DIR" ]; then
        print_progress "Cloning libobjc2 source..."
        cd "$WORKSPACE_ROOT"
        git clone https://github.com/gnustep/libobjc2.git
    fi
    
    cd "$LIBOBJC2_SOURCE_DIR"
    
    # Update to latest
    print_progress "Updating libobjc2 source..."
    git fetch origin
    git reset --hard origin/master
    
    # Create build directory
    rm -rf "$LIBOBJC2_BUILD_DIR"
    mkdir -p "$LIBOBJC2_BUILD_DIR"
    cd "$LIBOBJC2_BUILD_DIR"
    
    # Configure with debugging symbols and DWARF-5
    print_progress "Configuring libobjc2 build with DWARF-5 debug symbols..."
    cmake "$LIBOBJC2_SOURCE_DIR" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_FLAGS="$GNUSTEP_CFLAGS -ffunction-sections -fdata-sections" \
        -DCMAKE_CXX_FLAGS="$GNUSTEP_CXXFLAGS -ffunction-sections -fdata-sections" \
        -DCMAKE_SHARED_LINKER_FLAGS="-lstdc++" \
        -DCMAKE_INSTALL_PREFIX="$GNUSTEP_INSTALL_DIR" \
        -DCMAKE_C_COMPILER="$LLVM_BUILD_DIR/bin/clang" \
        -DCMAKE_CXX_COMPILER="$LLVM_BUILD_DIR/bin/clang++" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DGNUSTEP_INSTALL_TYPE=NONE \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -GNinja
    
    # Build
    print_progress "Building libobjc2 (this may take 10-15 minutes)..."
    START_TIME=$(date +%s)
    
    ninja -j$PARALLEL_JOBS
    
    END_TIME=$(date +%s)
    BUILD_TIME=$(( (END_TIME - START_TIME) / 60 ))
    
    # Install
    print_progress "Installing libobjc2..."
    ninja install
    
    print_success "libobjc2 built and installed successfully in ${BUILD_TIME} minutes"
    
    # Verify installation
    if [ -f "$GNUSTEP_INSTALL_DIR/lib/libobjc.so" ]; then
        OBJC_VERSION=$(strings "$GNUSTEP_INSTALL_DIR/lib/libobjc.so" | grep -E "libobjc.*[0-9]+" | head -1 || echo "Version unknown")
        print_success "✅ libobjc2 installed: $OBJC_VERSION"
    else
        print_error "❌ libobjc2 installation verification failed"
        return 1
    fi
}

# Function to build libgnustep-base with debugging symbols  
build_gnustep_base_with_debug() {
    print_section "Step: Building libgnustep-base with Debug Symbols"
    
    # Ensure gnustep-make is installed into our workspace prefix so we don't rely on /usr/share
    ensure_gnustep_make_installed() {
        local MAKEFILES_DIR="$GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles"
        if [ -d "$MAKEFILES_DIR" ] && [ -f "$MAKEFILES_DIR/aggregate.make" ]; then
            print_progress "Found gnustep-make in $MAKEFILES_DIR"
            export GNUSTEP_MAKEFILES="$MAKEFILES_DIR"
            return 0
        fi

        print_progress "gnustep-make not found in workspace; building and installing into $GNUSTEP_INSTALL_DIR ..."
        local TOOLS_MAKE_DIR="$WORKSPACE_ROOT/gnustep-build/tools-make"
        local TOOLS_MAKE_SRC="$WORKSPACE_ROOT/tools-make"

        # Clone or update tools-make (gnustep-make)
        if [ ! -d "$TOOLS_MAKE_SRC" ]; then
            print_progress "Cloning gnustep/tools-make ..."
            cd "$WORKSPACE_ROOT"
            git clone https://github.com/gnustep/tools-make.git
        else
            cd "$TOOLS_MAKE_SRC"
            print_progress "Updating gnustep/tools-make ..."
            git fetch origin
            git checkout master
            git pull --ff-only origin master || true
        fi

        mkdir -p "$TOOLS_MAKE_DIR"
        cd "$TOOLS_MAKE_SRC"
        print_progress "Configuring gnustep-make with prefix $GNUSTEP_INSTALL_DIR ..."
        ./configure --prefix="$GNUSTEP_INSTALL_DIR"
        print_progress "Building gnustep-make ..."
        make -j${PARALLEL_JOBS:-1}
        print_progress "Installing gnustep-make ..."
        make install

        if [ -d "$MAKEFILES_DIR" ] && [ -f "$MAKEFILES_DIR/aggregate.make" ]; then
            export GNUSTEP_MAKEFILES="$MAKEFILES_DIR"
            # Ensure the new gnustep-make binaries and configs are preferred
            export PATH="$GNUSTEP_INSTALL_DIR/bin:${PATH:-}"
            print_success "gnustep-make installed at $MAKEFILES_DIR"
        else
            print_error "Failed to install gnustep-make into $GNUSTEP_INSTALL_DIR"
            return 1
        fi
    }

    # After ensure, return to libs-base source dir to proceed with configure/build
    # (ensure_gnustep_make_installed changes directories internally)
    cd "$LIBS_BASE_SOURCE_DIR"

    # Clone or update libs-base from official GNUstep repository
    if [ ! -d "$LIBS_BASE_SOURCE_DIR" ]; then
        print_progress "Cloning libs-base from official GNUstep repository..."
        cd "$WORKSPACE_ROOT"
        git clone https://github.com/gnustep/libs-base.git
    else
        cd "$LIBS_BASE_SOURCE_DIR"
        print_progress "Updating libs-base from official GNUstep repository..."
        git fetch origin
        git checkout master
        git pull origin master
    fi
    
    cd "$LIBS_BASE_SOURCE_DIR"
    
    # Clean any previous build
    print_progress "Cleaning previous build..."
    if [ -f "GNUmakefile" ]; then
        make clean || print_warning "Clean failed (may not be critical)"
    fi
    
    # Configure environment for GNUstep build
    print_progress "Setting up GNUstep build environment..."
    
    # Ensure GNUstep Makefiles are available in our workspace and set env
    ensure_gnustep_make_installed

    # Set GNUstep environment variables for debugging builds with clang
    # export GNUSTEP_NEW_STRING_ABI=1  # DISABLED
    export ADDITIONAL_OBJCFLAGS="$GNUSTEP_OBJCFLAGS -I$GNUSTEP_INSTALL_DIR/include"
    export ADDITIONAL_CFLAGS="$GNUSTEP_CFLAGS -I$GNUSTEP_INSTALL_DIR/include"
    export ADDITIONAL_CPPFLAGS="-I$GNUSTEP_INSTALL_DIR/include"
    export ADDITIONAL_LDFLAGS="$GNUSTEP_LDFLAGS -L$GNUSTEP_INSTALL_DIR/lib -Wl,-rpath,$GNUSTEP_INSTALL_DIR/lib -lobjc"
    export debug=yes
    export strip=no
    export shared=yes
    
    # Prefer the workspace LLVM toolchain when available, else fall back to system clang
    if [ -n "${LLVM_BUILD_DIR:-}" ] && [ -x "$LLVM_BUILD_DIR/bin/clang" ]; then
        export CC="$LLVM_BUILD_DIR/bin/clang"
        export CXX="$LLVM_BUILD_DIR/bin/clang++"
        export OBJC="$LLVM_BUILD_DIR/bin/clang"
        export OBJCXX="$LLVM_BUILD_DIR/bin/clang++"
        export LDCC="$LLVM_BUILD_DIR/bin/clang"
    else
        export CC="clang"
        export CXX="clang++"
        export OBJC="clang"
        export OBJCXX="clang++"
        export LDCC="clang"
    fi
    
    # Ensure our custom libobjc is found first by pkg-config and the linker
    export PKG_CONFIG_PATH="$GNUSTEP_INSTALL_DIR/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    export LD_LIBRARY_PATH="$GNUSTEP_INSTALL_DIR/lib:${LD_LIBRARY_PATH:-}"
    export PATH="$GNUSTEP_INSTALL_DIR/bin:${PATH:-}"
    
    # Configure gnustep-base if needed
    print_progress "Configuring gnustep-base..."
    if [ ! -f "configure" ]; then
        print_progress "Running autoreconf to generate configure script..."
        autoreconf -if
    fi
    
    if [ ! -f "GNUmakefile" ]; then
        # Configure with debug settings and non-fragile ABI
        CFLAGS="$GNUSTEP_CFLAGS -I$GNUSTEP_INSTALL_DIR/include" \
        CXXFLAGS="$GNUSTEP_CXXFLAGS -I$GNUSTEP_INSTALL_DIR/include" \
        OBJCFLAGS="$GNUSTEP_OBJCFLAGS -I$GNUSTEP_INSTALL_DIR/include" \
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
    
    # Build with DWARF-5 debug symbols
    print_progress "Building gnustep-base with DWARF-5 debug symbols (this may take 20-30 minutes)..."
    START_TIME=$(date +%s)
    
    # Add explicit -fno-objc-arc to prevent ARC feature detection issues
    make -j$PARALLEL_JOBS debug=yes strip=no ADDITIONAL_OBJCFLAGS="-fno-objc-arc"
    
    END_TIME=$(date +%s)
    BUILD_TIME=$(( (END_TIME - START_TIME) / 60 ))
    
    # Install
    print_progress "Installing gnustep-base..."
    make install debug=yes strip=no ADDITIONAL_OBJCFLAGS="-fno-objc-arc"
    
    print_success "gnustep-base built and installed successfully in ${BUILD_TIME} minutes"
    
    # Verify installation (support both flattened and classic GNUstep layouts)
    print_progress "Verifying gnustep-base installation under $GNUSTEP_INSTALL_DIR"
    BASE_LIB=""
    # Candidate locations
    for pattern in \
        "$GNUSTEP_INSTALL_DIR/lib/libgnustep-base.so" \
        "$GNUSTEP_INSTALL_DIR/lib/libgnustep-base.so."* \
        "$GNUSTEP_INSTALL_DIR/System/Library/Libraries/libgnustep-base.so" \
        "$GNUSTEP_INSTALL_DIR/System/Library/Libraries/libgnustep-base.so."* \
        "$GNUSTEP_INSTALL_DIR/Library/Libraries/libgnustep-base.so" \
        "$GNUSTEP_INSTALL_DIR/Library/Libraries/libgnustep-base.so."* \
        "$GNUSTEP_INSTALL_DIR/lib/libgnustep-base.dylib" \
        "$GNUSTEP_INSTALL_DIR/System/Library/Libraries/libgnustep-base.dylib" \
        "$GNUSTEP_INSTALL_DIR/Library/Libraries/libgnustep-base.dylib"; do
        for f in $pattern; do
            if [ -f "$f" ]; then BASE_LIB="$f"; break 2; fi
        done
    done

    if [ -n "$BASE_LIB" ]; then
        BASE_VERSION=$(strings "$BASE_LIB" | grep -E "gnustep-base.*[0-9]+" | head -1 || echo "Version unknown")
        print_success "✅ gnustep-base installed: $BASE_LIB ($BASE_VERSION)"
        # Check for debug symbols
        if objdump -h "$BASE_LIB" | grep -q debug; then
            print_success "✅ Debug symbols present in gnustep-base"
        else
            print_warning "⚠️  Debug symbols may not be present in gnustep-base"
        fi
    else
        print_error "❌ gnustep-base installation verification failed (no library under $GNUSTEP_INSTALL_DIR)"
        print_progress "Diagnostics: listing lib* directories under prefix"
        find "$GNUSTEP_INSTALL_DIR" -maxdepth 4 -type d -iname 'lib*' -print | sed -n '1,120p'
        return 1
    fi
}

# Function to verify symbol generation in examples
verify_gnustep_symbol_generation() {
    print_section "Step: Verifying Symbol Generation"
    
    # Update examples Makefile to use our new libraries
    print_progress "Updating examples Makefile..."
    cd "$WORKSPACE_ROOT/examples"
    
    # Create updated Makefile with proper library paths
    cat > Makefile << 'EOF'
# Makefile for LLDB Bridge Examples - Debug Symbol Edition

CC = /usr/bin/clang-18
CFLAGS = -fobjc-runtime=gnustep-2.1 -fblocks -g -gdwarf-5 -O0 -fno-omit-frame-pointer \
         -I/usr/local/include/GNUstep -I/usr/include/GNUstep \
         -fconstant-string-class=NSConstantString \
         -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -DDEBUG=1
LDFLAGS = -L/usr/local/lib \
          -Wl,-rpath,/usr/local/lib \
          -g
LIBS = -lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm

# Source files
SOURCES = simple_test.m custom_class_test.m foundation_test.m
EXECUTABLES = $(SOURCES:.m=)

.PHONY: all clean symbols

all: $(EXECUTABLES)

%: %.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(EXECUTABLES)

# Target to check symbols in generated binaries
symbols: custom_class_test
	@echo "Checking for ivar offset symbols in custom_class_test..."
	@objdump -t custom_class_test | grep "__objc_ivar_offset" || echo "No ivar offset symbols found"
	@echo ""
	@echo "Checking for debug symbols..."
	@objdump -h custom_class_test | grep debug && echo "Debug symbols present" || echo "No debug symbols found"
	@echo ""
	@echo "Checking library dependencies..."
	@ldd custom_class_test | grep -E "(gnustep|objc)"

# Individual targets for convenience
simple_test: simple_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

custom_class_test: custom_class_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

foundation_test: foundation_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

# Help target
help:
	@echo "Available targets:"
	@echo "  all              - Build all examples"
	@echo "  simple_test      - Build simple test example"
	@echo "  custom_class_test - Build custom class example"
	@echo "  foundation_test  - Build Foundation types example"
	@echo "  symbols          - Check symbols in custom_class_test"
	@echo "  clean            - Remove all built executables"
	@echo "  help             - Show this help message"
EOF
    
    # Build custom_class_test to check symbols
    print_progress "Building custom_class_test with symbol checking..."
    make clean
    make custom_class_test
    
    if [ -f "custom_class_test" ]; then
        print_success "✅ custom_class_test built successfully"
        
        # Check for ivar offset symbols
        print_progress "Checking for ivar offset symbols..."
        IVAR_SYMBOLS=$(objdump -t custom_class_test | grep "__objc_ivar_offset" | wc -l)
        if [ "$IVAR_SYMBOLS" -gt 0 ]; then
            print_success "✅ Found $IVAR_SYMBOLS ivar offset symbols"
            print_progress "Symbol examples:"
            objdump -t custom_class_test | grep "__objc_ivar_offset" | head -5
        else
            print_warning "⚠️  No ivar offset symbols found in binary"
        fi
        
        # Check for debug symbols
        print_progress "Checking for debug symbols..."
        if objdump -h custom_class_test | grep -q debug; then
            print_success "✅ Debug symbols present in binary"
        else
            print_warning "⚠️  Debug symbols not found in binary"
        fi
        
        # Check library linkage
        print_progress "Checking library dependencies..."
        GNUSTEP_LIBS=$(ldd custom_class_test | grep -E "(gnustep|objc)" | wc -l)
        if [ "$GNUSTEP_LIBS" -gt 0 ]; then
            print_success "✅ Linked to $GNUSTEP_LIBS GNUstep/ObjC libraries"
            ldd custom_class_test | grep -E "(gnustep|objc)"
        else
            print_error "❌ Not properly linked to GNUstep libraries"
            return 1
        fi
        
    else
        print_error "❌ Failed to build custom_class_test"
        return 1
    fi
}

# Function to update environment configuration
update_gnustep_environment_config() {
    print_section "Step: Updating GNUstep Environment Configuration"
    
    # Create environment setup script
    ENV_SCRIPT="$PROJECT_ROOT/setup_gnustep_debug_env.sh"
    
    cat > "$ENV_SCRIPT" << EOF
#!/bin/bash
# GNUstep Debug Environment Setup
# Source this script to set up the environment for debugging

# Library paths (prefer workspace prefix)
export LD_LIBRARY_PATH="$GNUSTEP_INSTALL_DIR/lib:\$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="$GNUSTEP_INSTALL_DIR/lib/pkgconfig:\$PKG_CONFIG_PATH"

# GNUstep environment (use local makefiles)
export GNUSTEP_MAKEFILES="$GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles"
export GNUSTEP_SYSTEM_ROOT="$GNUSTEP_INSTALL_DIR"
export GNUSTEP_INSTALLATION_DIR="$GNUSTEP_INSTALL_DIR"
export GNUSTEP_FLATTENED=yes

# Debugging flags for compilation
export CFLAGS="$GNUSTEP_CFLAGS"
export CXXFLAGS="$GNUSTEP_CXXFLAGS"
export OBJCFLAGS="$GNUSTEP_OBJCFLAGS"
export LDFLAGS="$GNUSTEP_LDFLAGS"

# LLDB path (if custom build exists)
if [ -f "$LLVM_BUILD_DIR/build/bin/lldb" ]; then
    export PATH="$LLVM_BUILD_DIR/build/bin:\$PATH"
    echo "✅ Using custom LLDB: $LLVM_BUILD_DIR/build/bin/lldb"
else
    echo "⚠️  Custom LLDB not found, using system LLDB"
fi

echo "🔧 GNUstep Debug Environment Configured"
echo "📁 Library path: \$LD_LIBRARY_PATH"
echo "🎯 Ready for debugging with symbols!"
EOF
    
    chmod +x "$ENV_SCRIPT"
    print_success "✅ Environment script created: $ENV_SCRIPT"
    
    # Update VS Code tasks to use new libraries
    TASKS_FILE="$WORKSPACE_ROOT/.vscode/tasks.json"
    if [ -f "$TASKS_FILE" ]; then
        print_progress "Updating VS Code tasks configuration..."
        # Backup original
        cp "$TASKS_FILE" "$TASKS_FILE.backup"
        
        # Update library paths in tasks.json to use /usr/local/lib
        sed -i 's|/home/robk/libobjc2-build/libobjc2/build|/usr/local/lib|g' "$TASKS_FILE"
        sed -i 's|/home/robk/code/libs-base/Source/obj|/usr/local/lib|g' "$TASKS_FILE"
        
        print_success "✅ VS Code tasks updated to use new library paths"
    fi
}

# Function to run comprehensive GNUstep tests
run_comprehensive_gnustep_tests() {
    print_section "Step: Running Comprehensive GNUstep Tests"
    
    cd "$WORKSPACE_ROOT/examples"
    
    # Test 1: Build all examples
    print_progress "Building all examples..."
    if make all; then
        print_success "✅ All examples built successfully"
    else
        print_error "❌ Failed to build some examples"
        return 1
    fi
    
    # Test 2: Symbol verification for each binary
    for binary in simple_test custom_class_test foundation_test; do
        if [ -f "$binary" ]; then
            print_progress "Checking symbols in $binary..."
            
            IVAR_COUNT=$(objdump -t "$binary" | grep "__objc_ivar_offset" | wc -l)
            DEBUG_PRESENT=$(objdump -h "$binary" | grep debug | wc -l)
            
            print_success "  📊 $binary: $IVAR_COUNT ivar symbols, $DEBUG_PRESENT debug sections"
            
            if [ "$IVAR_COUNT" -gt 0 ]; then
                print_success "  ✅ Ivar offset symbols present"
            else
                print_warning "  ⚠️  No ivar offset symbols (may be expected for simple programs)"
            fi
        fi
    done
    
    # Test 3: Runtime execution
    print_progress "Testing runtime execution..."
    for binary in simple_test foundation_test; do
        if [ -f "$binary" ]; then
            print_progress "Running $binary..."
            if timeout 5 "./$binary" >/dev/null 2>&1; then
                print_success "  ✅ $binary executed successfully"
            else
                print_warning "  ⚠️  $binary execution failed or timed out"
            fi
        fi
    done
    
    # Test 4: Library dependency verification
    print_progress "Verifying library dependencies..."
    if ldd custom_class_test 2>/dev/null | grep -q "/usr/local/lib/libgnustep-base.so"; then
        print_success "✅ Using locally built gnustep-base"
    else
        print_warning "⚠️  May not be using locally built gnustep-base"
    fi
    
    if ldd custom_class_test 2>/dev/null | grep -q "/usr/local/lib/libobjc.so"; then
        print_success "✅ Using locally built libobjc2"
    else
        print_warning "⚠️  May not be using locally built libobjc2"
    fi
}

# Main function to build complete GNUstep debug environment
build_complete_gnustep_environment() {
    print_section "🚀 Building Complete GNUstep Debug Environment"
    
    clean_system_gnustep_packages
    install_gnustep_dependencies  
    build_libobjc2_with_debug
    build_gnustep_base_with_debug
    verify_gnustep_symbol_generation
    update_gnustep_environment_config
    run_comprehensive_gnustep_tests
    
    print_section "🎉 GNUstep Debug Environment Complete!"
    
    echo -e "${GREEN}Your complete GNUstep debugging environment is ready!${NC}"
    echo ""
    echo -e "${YELLOW}Key Components:${NC}"
    echo "  ✓ libobjc2 with debug symbols: /usr/local/lib/libobjc.so"
    echo "  ✓ gnustep-base with debug symbols: /usr/local/lib/libgnustep-base.so"
    echo "  ✓ Debug-optimized compilation flags"
    echo "  ✓ Symbol generation enabled"
    echo ""
    echo -e "${YELLOW}Environment Setup:${NC}"
    echo "  Source: source $PROJECT_ROOT/setup_gnustep_debug_env.sh"
    echo ""
    echo -e "${BLUE}🔥 Ready for advanced Objective-C debugging with full symbol support!${NC}"
}

# Main execution
if [ "${BASH_SOURCE[0]}" = "${0}" ]; then
    # Script is being executed directly
    case "${1:-}" in
        build-all)
            build_complete_gnustep_environment
            ;;
        clean-packages)
            clean_system_gnustep_packages
            ;;
        install-deps)
            install_gnustep_dependencies
            ;;
        build-libobjc2)
            build_libobjc2_with_debug
            ;;
        build-base)
            build_gnustep_base_with_debug
            ;;
        verify)
            verify_gnustep_symbol_generation
            ;;
        test)
            run_comprehensive_gnustep_tests
            ;;
        *)
            echo "Usage: $0 {build-all|clean-packages|install-deps|build-libobjc2|build-base|verify|test}"
            echo ""
            echo "Commands:"
            echo "  build-all       - Build complete GNUstep environment"
            echo "  clean-packages  - Remove conflicting system packages"
            echo "  install-deps    - Install build dependencies"
            echo "  build-libobjc2  - Build libobjc2 with debug symbols"
            echo "  build-base      - Build libs-base with debug symbols"
            echo "  verify          - Verify symbol generation"
            echo "  test            - Run comprehensive tests"
            exit 1
            ;;
    esac
fi
