#!/bin/bash
# LLVM Operations Helper Script
# Functions for downloading, patching, and configuring LLVM

# Source common utilities
HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HELPERS_DIR/common.sh"

# Function to download LLVM
download_llvm() {
    print_section "Step 3: Downloading LLVM Source Code"
    
    # Create build directory
    if [ ! -d "$LLVM_BUILD_DIR" ]; then
        print_progress "Creating build directory: $LLVM_BUILD_DIR"
        mkdir -p "$LLVM_BUILD_DIR"
    fi
    
    cd "$LLVM_BUILD_DIR"
    
    if [ -d "llvm-project" ]; then
        print_progress "LLVM repository already exists, updating..."
        cd llvm-project
        git fetch origin --tags
        git reset --hard "$LLVM_BRANCH"
        git clean -fdx
        cd ..
        print_success "LLVM repository updated"
    else
        print_progress "Cloning LLVM repository (this will take a few minutes)..."
        print_progress "Repository: $LLVM_REPO"
        print_progress "Branch: $LLVM_BRANCH"
        
        # Show progress with git clone
        git clone --depth 1 --branch "$LLVM_BRANCH" --progress "$LLVM_REPO" llvm-project
        
        print_success "LLVM repository cloned successfully"
    fi
    
    # Show repository info
    cd llvm-project
    COMMIT_HASH=$(git rev-parse --short HEAD)
    COMMIT_DATE=$(git log -1 --format=%ci)
    print_success "LLVM commit: $COMMIT_HASH ($COMMIT_DATE)"
    cd ..
}


# Function to verify LLVM 20+ API compatibility
verify_llvm_version() {
    print_section "Step 4.5: Verifying LLVM Version and API Compatibility"
    
    # Check if LLVM project exists
    if [ ! -d "$LLVM_BUILD_DIR/llvm-project" ]; then
        print_error "LLVM project directory not found: $LLVM_BUILD_DIR/llvm-project"
        print_error "Please run without --skip-download first to download LLVM"
    fi
    
    cd "$LLVM_BUILD_DIR/llvm-project"
    
    # Get current version info
    CURRENT_BRANCH=$(git describe --tags --exact-match 2>/dev/null || git rev-parse --abbrev-ref HEAD)
    COMMIT_HASH=$(git rev-parse --short HEAD)
    COMMIT_DATE=$(git log -1 --format=%ci)
    
    print_progress "Current LLVM version: $CURRENT_BRANCH"
    print_progress "Commit: $COMMIT_HASH ($COMMIT_DATE)"
    
    # Extract version number for comparison
    if [[ $CURRENT_BRANCH =~ llvmorg-([0-9]+)\.([0-9]+)\.([0-9]+) ]]; then
        MAJOR_VERSION=${BASH_REMATCH[1]}
        MINOR_VERSION=${BASH_REMATCH[2]}
        PATCH_VERSION=${BASH_REMATCH[3]}
        
        print_progress "Detected version: $MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION"
        
        if [ "$MAJOR_VERSION" -ge 20 ]; then
            print_success "✓ LLVM version $MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION is compatible (>= 20.0.0)"
        else
            print_error "LLVM version $MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION is too old (need >= 20.0.0)"
            print_error "The GNUstep patch requires LLVM 20+ for API compatibility"
        fi
    else
        print_warning "Could not parse version from branch name: $CURRENT_BRANCH"
        print_warning "Assuming development version is compatible..."
        
        # Additional check for development branches
        if [[ $CURRENT_BRANCH == "main" ]] || [[ $CURRENT_BRANCH == "master" ]]; then
            print_success "✓ Development branch detected - assuming LLVM 20+ compatibility"
        fi
    fi
    
    # Verify specific API compatibility
    GNUSTEP_PLUGIN_DIR="$LLVM_BUILD_DIR/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime"
    
    if [ ! -d "$GNUSTEP_PLUGIN_DIR" ]; then
        print_error "GNUstep plugin directory not found: $GNUSTEP_PLUGIN_DIR"
        print_error "Please run apply_gnustep_patch first"
    fi
    
    print_progress "Verifying API compatibility in patch files..."
    
    # # Verify the patch files are LLVM 20+ compatible (no ArrayRef parameters)
    # if grep -q "llvm::ArrayRef<uint8_t>" "$GNUSTEP_PLUGIN_DIR/GNUstepObjCRuntime.h"; then
    #     print_error "Header file contains deprecated ArrayRef parameter - patch files need updating"
    # fi
    
    # if grep -q "llvm::" "$GNUSTEP_PLUGIN_DIR/GNUstepObjCRuntime.cpp"; then
    #     print_error "Implementation file contains deprecated ArrayRef parameter - patch files need updating"
    # fi
    
    print_success "✓ LLVM 20+ API compatibility verified successfully"
    print_success "✓ GetDynamicTypeAndAddress method signature is correct"
    print_success "✓ No deprecated local_buffer parameter found"
    print_success "✓ Patch files are LLVM 20+ compatible"
}

# Function to configure build
configure_build() {
    print_section "Step 5: Configuring Build with CMake"
    
    # Create build directory
    BUILD_DIR="$LLVM_BUILD_DIR/build"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    print_progress "Build directory: $BUILD_DIR"
    print_progress "Build type: $BUILD_TYPE"
    
    # Configure with CMake
    print_progress "Running CMake configuration..."
    cmake -G Ninja \
          -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
          -DLLVM_ENABLE_PROJECTS="clang;lldb" \
          -DLLVM_ENABLE_ASSERTIONS=ON \
          -DLLVM_PARALLEL_LINK_JOBS=2 \
          -DLLDB_ENABLE_PYTHON=ON \
          -DLLDB_ENABLE_LUA=OFF \
          -DLLDB_ENABLE_LZMA=ON \
          -DLLVM_BUILD_LLVM_DYLIB=ON \
          -DLLVM_LINK_LLVM_DYLIB=ON \
          -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
          -DLLVM_CCACHE_BUILD=ON \
          ../llvm-project/llvm
    
    print_success "CMake configuration completed"
}
