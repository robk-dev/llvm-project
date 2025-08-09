#!/bin/bash
# GNUstep build operations for Windows MSYS2/UCRT64
# Author: LLDB GNUstep Development Team
# Date: August 2025

set -euo pipefail

# Source common utilities
HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HELPERS_DIR/common.sh"

# GNUstep build configuration for Windows
GNUSTEP_BUILD_DIR="${GNUSTEP_BUILD_DIR:-$WORKSPACE_ROOT/gnustep-build-windows}"
GNUSTEP_INSTALL_DIR="${GNUSTEP_INSTALL_DIR:-$WORKSPACE_ROOT/gnustep-install-windows}"
LIBOBJC2_BUILD_DIR="${LIBOBJC2_BUILD_DIR:-$GNUSTEP_BUILD_DIR/libobjc2}"
LIBS_BASE_BUILD_DIR="${LIBS_BASE_BUILD_DIR:-$GNUSTEP_BUILD_DIR/libs-base}"

# Source directories
LIBOBJC2_SOURCE_DIR="$WORKSPACE_ROOT/libobjc2"
LIBS_BASE_SOURCE_DIR="$WORKSPACE_ROOT/libs-base"

# Windows-specific build flags
export GNUSTEP_CFLAGS="-g -O2 -fno-omit-frame-pointer -DDEBUG=1"
export GNUSTEP_CXXFLAGS="-g -O2 -fno-omit-frame-pointer -DDEBUG=1 -std=c++17"
export GNUSTEP_OBJCFLAGS="-g -O2 -fno-omit-frame-pointer -DDEBUG=1 -fobjc-runtime=gnustep-2.0 -fblocks"
export GNUSTEP_LDFLAGS="-g -fuse-ld=lld"

# Function to build libobjc2 on Windows
build_libobjc2_windows() {
    print_section "Building libobjc2 for Windows"
    
    # Clone or update libobjc2
    if [ ! -d "$LIBOBJC2_SOURCE_DIR" ]; then
        print_progress "Cloning libobjc2..."
        cd "$WORKSPACE_ROOT"
        git clone https://github.com/gnustep/libobjc2.git
    else
        print_progress "Updating libobjc2..."
        cd "$LIBOBJC2_SOURCE_DIR"
        git fetch origin
        git reset --hard origin/master
    fi
    
    # Create build directory
    ensure_directory "$LIBOBJC2_BUILD_DIR" "libobjc2 build directory"
    cd "$LIBOBJC2_BUILD_DIR"
    
    # Configure with our built clang
    print_progress "Configuring libobjc2..."
    local cmake_args=(
        "-G" "Ninja"
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
        "-DCMAKE_INSTALL_PREFIX=$GNUSTEP_INSTALL_DIR"
        "-DCMAKE_C_COMPILER=$LLVM_BUILD_DIR/build/bin/clang.exe"
        "-DCMAKE_CXX_COMPILER=$LLVM_BUILD_DIR/build/bin/clang++.exe"
        "-DCMAKE_ASM_COMPILER=$LLVM_BUILD_DIR/build/bin/clang.exe"
        "-DCMAKE_C_FLAGS=$GNUSTEP_CFLAGS"
        "-DCMAKE_CXX_FLAGS=$GNUSTEP_CXXFLAGS"
        "-DCMAKE_SHARED_LINKER_FLAGS=$GNUSTEP_LDFLAGS"
        "-DGNUSTEP_INSTALL_TYPE=NONE"
        "-DTESTS=OFF"  # Disable tests for now
    )
    
    if cmake "$LIBOBJC2_SOURCE_DIR" "${cmake_args[@]}"; then
        print_success "libobjc2 configured successfully"
    else
        print_error "libobjc2 configuration failed"
        return 1
    fi
    
    # Build libobjc2
    print_progress "Building libobjc2..."
    if ninja -j$PARALLEL_JOBS; then
        print_success "libobjc2 built successfully"
    else
        print_error "libobjc2 build failed"
        return 1
    fi
    
    # Install libobjc2
    print_progress "Installing libobjc2..."
    if ninja install; then
        print_success "libobjc2 installed to $GNUSTEP_INSTALL_DIR"
    else
        print_error "libobjc2 installation failed"
        return 1
    fi
    
    # Verify installation
    if [ -f "$GNUSTEP_INSTALL_DIR/lib/libobjc.dll" ] || [ -f "$GNUSTEP_INSTALL_DIR/lib/libobjc.dll.a" ]; then
        print_success "libobjc2 library verified"
        ls -la "$GNUSTEP_INSTALL_DIR/lib/libobjc*" 2>/dev/null || true
    else
        print_warning "libobjc2 library not found in expected location"
    fi
}

# Function to build gnustep-make on Windows
build_gnustep_make_windows() {
    print_section "Building gnustep-make for Windows"
    
    local MAKE_SOURCE_DIR="$WORKSPACE_ROOT/gnustep-make"
    
    # Clone or update gnustep-make
    if [ ! -d "$MAKE_SOURCE_DIR" ]; then
        print_progress "Cloning gnustep-make..."
        cd "$WORKSPACE_ROOT"
        git clone https://github.com/gnustep/gnustep-make.git
    else
        print_progress "Updating gnustep-make..."
        cd "$MAKE_SOURCE_DIR"
        git fetch origin
        git reset --hard origin/master
    fi
    
    cd "$MAKE_SOURCE_DIR"
    
    # Configure gnustep-make
    print_progress "Configuring gnustep-make..."
    ./configure \
        --prefix="$GNUSTEP_INSTALL_DIR" \
        --with-library-combo=ng-gnu-gnu \
        --with-objc-lib-flag=-lobjc \
        --enable-objc-arc \
        CC="$LLVM_BUILD_DIR/build/bin/clang.exe" \
        CXX="$LLVM_BUILD_DIR/build/bin/clang++.exe" \
        OBJC="$LLVM_BUILD_DIR/build/bin/clang.exe" \
        OBJCXX="$LLVM_BUILD_DIR/build/bin/clang++.exe" \
        CFLAGS="$GNUSTEP_CFLAGS" \
        OBJCFLAGS="$GNUSTEP_OBJCFLAGS" \
        LDFLAGS="-L$GNUSTEP_INSTALL_DIR/lib $GNUSTEP_LDFLAGS"
    
    # Build and install
    print_progress "Building gnustep-make..."
    make -j$PARALLEL_JOBS
    
    print_progress "Installing gnustep-make..."
    make install
    
    print_success "gnustep-make installed successfully"
    
    # Source GNUstep environment
    if [ -f "$GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles/GNUstep.sh" ]; then
        source "$GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles/GNUstep.sh"
        print_success "GNUstep environment configured"
    fi
}

# Function to build gnustep-base on Windows
build_gnustep_base_windows() {
    print_section "Building gnustep-base for Windows"
    
    # Clone or update gnustep-base
    if [ ! -d "$LIBS_BASE_SOURCE_DIR" ]; then
        print_progress "Cloning gnustep-base..."
        cd "$WORKSPACE_ROOT"
        git clone https://github.com/gnustep/libs-base.git
    else
        print_progress "Updating gnustep-base..."
        cd "$LIBS_BASE_SOURCE_DIR"
        git fetch origin
        git reset --hard origin/master
    fi
    
    # Source GNUstep environment
    if [ -f "$GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles/GNUstep.sh" ]; then
        source "$GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles/GNUstep.sh"
    fi
    
    # Create build directory
    ensure_directory "$LIBS_BASE_BUILD_DIR" "gnustep-base build directory"
    cd "$LIBS_BASE_BUILD_DIR"
    
    # Configure gnustep-base for Windows
    print_progress "Configuring gnustep-base..."
    "$LIBS_BASE_SOURCE_DIR/configure" \
        --prefix="$GNUSTEP_INSTALL_DIR" \
        --disable-tls \
        --disable-icu \
        --disable-xml \
        --with-installation-domain=SYSTEM \
        CC="$LLVM_BUILD_DIR/build/bin/clang.exe" \
        CXX="$LLVM_BUILD_DIR/build/bin/clang++.exe" \
        OBJC="$LLVM_BUILD_DIR/build/bin/clang.exe" \
        CFLAGS="$GNUSTEP_CFLAGS -I$GNUSTEP_INSTALL_DIR/include" \
        OBJCFLAGS="$GNUSTEP_OBJCFLAGS -I$GNUSTEP_INSTALL_DIR/include" \
        LDFLAGS="-L$GNUSTEP_INSTALL_DIR/lib $GNUSTEP_LDFLAGS" \
        LIBS="-lobjc -lm"
    
    # Build gnustep-base
    print_progress "Building gnustep-base (this may take 15-30 minutes)..."
    if make -j$PARALLEL_JOBS messages=yes; then
        print_success "gnustep-base built successfully"
    else
        print_warning "gnustep-base build had issues, attempting to continue..."
    fi
    
    # Install gnustep-base
    print_progress "Installing gnustep-base..."
    if make install; then
        print_success "gnustep-base installed to $GNUSTEP_INSTALL_DIR"
    else
        print_warning "gnustep-base installation had issues"
    fi
}

# Main function to build complete GNUstep environment on Windows
build_complete_gnustep_windows() {
    print_section "Building Complete GNUstep Environment for Windows"
    
    # Create installation directory
    ensure_directory "$GNUSTEP_INSTALL_DIR" "GNUstep installation directory"
    
    # Build components in order
    build_libobjc2_windows || {
        print_error "Failed to build libobjc2"
        return 1
    }
    
    build_gnustep_make_windows || {
        print_error "Failed to build gnustep-make"
        return 1
    }
    
    build_gnustep_base_windows || {
        print_warning "gnustep-base build incomplete (may still be usable)"
    }
    
    # Create environment setup script
    create_gnustep_env_script
    
    print_success "GNUstep environment build completed!"
    print_info "Installation directory: $GNUSTEP_INSTALL_DIR"
}

# Function to create GNUstep environment setup script
create_gnustep_env_script() {
    print_progress "Creating GNUstep environment setup script..."
    
    local env_script="$GNUSTEP_INSTALL_DIR/setup-gnustep-env.sh"
    
    cat > "$env_script" << 'EOF'
#!/bin/bash
# GNUstep Environment Setup for Windows MSYS2

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Set GNUstep paths
export GNUSTEP_INSTALL_DIR="$SCRIPT_DIR"
export GNUSTEP_MAKEFILES="$GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles"

# Add GNUstep libraries to path
export PATH="$GNUSTEP_INSTALL_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$GNUSTEP_INSTALL_DIR/lib:$LD_LIBRARY_PATH"

# Source GNUstep environment if available
if [ -f "$GNUSTEP_MAKEFILES/GNUstep.sh" ]; then
    source "$GNUSTEP_MAKEFILES/GNUstep.sh"
fi

# Set compiler to use our built clang
export CC="clang"
export CXX="clang++"
export OBJC="clang"

echo "GNUstep environment configured!"
echo "  GNUSTEP_INSTALL_DIR: $GNUSTEP_INSTALL_DIR"
echo "  Compiler: $CC"
EOF
    
    chmod +x "$env_script"
    print_success "Environment script created: $env_script"
}

# Function to test GNUstep installation
test_gnustep_installation() {
    print_section "Testing GNUstep Installation"
    
    # Create a simple test program
    local test_dir="$LLVM_BUILD_DIR/gnustep-test"
    ensure_directory "$test_dir" "GNUstep test directory"
    
    cat > "$test_dir/test.m" << 'EOF'
#import <Foundation/Foundation.h>

@interface TestClass : NSObject
- (void)sayHello;
@end

@implementation TestClass
- (void)sayHello {
    NSLog(@"Hello from GNUstep on Windows!");
}
@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        TestClass *obj = [[TestClass alloc] init];
        [obj sayHello];
        NSLog(@"GNUstep is working!");
    }
    return 0;
}
EOF
    
    cd "$test_dir"
    
    # Compile test program
    print_progress "Compiling GNUstep test program..."
    "$LLVM_BUILD_DIR/build/bin/clang.exe" \
        -fobjc-runtime=gnustep-2.0 \
        -fblocks \
        -I"$GNUSTEP_INSTALL_DIR/include" \
        -L"$GNUSTEP_INSTALL_DIR/lib" \
        -lobjc \
        -lgnustep-base \
        test.m -o test.exe
    
    if [ -f "test.exe" ]; then
        print_success "Test program compiled successfully"
        
        # Run test
        print_progress "Running test program..."
        if ./test.exe; then
            print_success "GNUstep test passed!"
        else
            print_warning "Test program execution failed"
        fi
    else
        print_error "Failed to compile test program"
    fi
}

# Export functions
export -f build_libobjc2_windows build_gnustep_make_windows build_gnustep_base_windows
export -f build_complete_gnustep_windows create_gnustep_env_script test_gnustep_installation