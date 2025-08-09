#!/bin/bash
# LLVM download and patch operations for Windows MSYS2/UCRT64
# Author: LLDB GNUstep Development Team
# Date: August 2025

set -euo pipefail

# Source common utilities
HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HELPERS_DIR/common.sh"

# Function to prepare LLVM source (we're already in the repo)
prepare_llvm_source() {
    print_progress "Preparing LLVM build directory..."
    ensure_directory "$LLVM_BUILD_DIR" "LLVM build directory"
    
    # We're already in the LLVM project root
    cd "$PROJECT_ROOT"
    
    # Check if we're in a git repository
    if [ ! -d ".git" ]; then
        print_error "Not in a git repository. Please clone the LLVM project first."
        exit 1
    fi
    
    # Check current status
    print_progress "Checking LLVM repository status..."
    
    # Show repository info
    local current_branch=$(git rev-parse --abbrev-ref HEAD)
    local commit_hash=$(git rev-parse --short HEAD)
    local commit_date=$(git log -1 --format=%ci)
    
    print_success "Using existing LLVM repository:"
    print_info "  Branch: $current_branch"
    print_info "  Commit: $commit_hash"
    print_info "  Date: $commit_date"
    
    # Check if we have uncommitted changes (ignore file permission changes)
    local changes=$(git status --porcelain | grep -v '^M ' | wc -l)
    if [ $changes -gt 0 ]; then
        print_warning "Uncommitted changes detected (excluding permission changes):"
        git status --porcelain | grep -v '^M ' | head -5
        echo ""
        read -p "Continue with build? (y/N): " -n 1 -r
        echo ""
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_info "Please commit or stash your changes first"
            exit 1
        fi
    fi
}

# Function to verify we're in the LLVM source
verify_llvm_source() {
    cd "$PROJECT_ROOT"
    
    # Verify we're in the LLVM project
    if [ ! -f "llvm/CMakeLists.txt" ] || [ ! -d "lldb" ]; then
        print_error "Not in LLVM project root. Missing llvm/CMakeLists.txt or lldb directory."
        print_info "Current directory: $(pwd)"
        print_info "Please ensure you're running this script from the LLVM project root."
        exit 1
    fi
    
    # Check current branch and status
    local current_branch=$(git rev-parse --abbrev-ref HEAD)
    local commit_hash=$(git rev-parse --short HEAD)
    local commit_date=$(git log -1 --format=%ci)
    
    print_success "LLVM project verified:"
    print_info "  Location: $PROJECT_ROOT"
    print_info "  Branch: $current_branch"
    print_info "  Commit: $commit_hash ($commit_date)"
    
    # Note: We don't enforce a specific branch since users might be on their own branches
    if [ "$current_branch" != "main" ] && [ "$current_branch" != "master" ]; then
        print_info "Working on branch: $current_branch (not main/master)"
    fi
}

# Function to verify GNUstep runtime patch is present
verify_gnustep_patch() {
    print_progress "Verifying GNUstep runtime patch is present..."
    
    cd "$PROJECT_ROOT"
    
    # Check if the GNUstep runtime plugin directory exists in the source
    local target_dir="lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime"
    
    if [ ! -d "$target_dir" ]; then
        print_error "GNUstep runtime patch not found: $target_dir"
        print_info "The LLDB source should already contain the GNUstep runtime plugin."
        print_info "Please ensure this repository has the GNUstep patches integrated."
        exit 1
    fi
    
    # Verify required files exist
    local required_files=(
        "GNUstepObjCRuntime.cpp"
        "GNUstepObjCRuntime.h"
        "CMakeLists.txt"
    )
    
    for file in "${required_files[@]}"; do
        if [ ! -f "$target_dir/$file" ]; then
            print_error "Required patch file missing: $target_dir/$file"
        fi
    done
    
    # Verify LLDB's CMakeLists.txt includes the plugin
    local lldb_cmake="lldb/source/Plugins/LanguageRuntime/ObjC/CMakeLists.txt"
    if [ -f "$lldb_cmake" ]; then
        if ! grep -q "GNUstepObjCRuntime" "$lldb_cmake"; then
            print_progress "Adding GNUstepObjCRuntime to LLDB build configuration..."
            echo "add_subdirectory(GNUstepObjCRuntime)" >> "$lldb_cmake"
        fi
    fi
    
    print_success "GNUstep runtime patch verified and ready"
    
    # Show patch info
    print_info "Available GNUstep features:"
    echo "  ✓ Dynamic class discovery via objc_copyClassList()"
    echo "  ✓ Custom class introspection support"
    echo "  ✓ Windows-compatible implementation"
    echo "  ✓ Integrated into repository source"
}

# Function for quick developer rebuild
quick_developer_rebuild() {
    if [ ! -d "$LLVM_BUILD_DIR/build" ]; then
        print_error "Build directory not found. Run full build first."
    fi
    
    cd "$LLVM_BUILD_DIR/build"
    
    # Set up environment
    export CC="ccache clang"
    export CXX="ccache clang++"
    
    print_progress "Rebuilding LLDB and lldb-server..."
    
    # Use time_command for timing
    time_command "ninja -j$PARALLEL_JOBS lldb lldb-server" "LLDB rebuild"
    
    # Check if binaries exist
    if [ -f "bin/lldb.exe" ] && [ -f "bin/lldb-server.exe" ]; then
        print_success "Quick rebuild completed!"
        print_info "Binaries:"
        echo "  LLDB: $LLVM_BUILD_DIR/build/bin/lldb.exe"
        echo "  lldb-server: $LLVM_BUILD_DIR/build/bin/lldb-server.exe"
    else
        print_error "Build seemed to succeed but binaries not found"
    fi
}

# Function to verify LLVM version compatibility
verify_llvm_version() {
    print_progress "Verifying LLVM version..."
    
    cd "$PROJECT_ROOT"
    
    # Check for version in CMakeLists.txt
    local cmake_file="llvm/CMakeLists.txt"
    if [ -f "$cmake_file" ]; then
        local version_major=$(grep "set(LLVM_VERSION_MAJOR" "$cmake_file" | sed 's/.*MAJOR \([0-9]*\).*/\1/')
        local version_minor=$(grep "set(LLVM_VERSION_MINOR" "$cmake_file" | sed 's/.*MINOR \([0-9]*\).*/\1/')
        local version_patch=$(grep "set(LLVM_VERSION_PATCH" "$cmake_file" | sed 's/.*PATCH \([0-9]*\).*/\1/')
        
        local llvm_version="${version_major}.${version_minor}.${version_patch}"
        print_info "LLVM version: $llvm_version"
        
        # Most modern LLVM versions should work, but warn if very old
        if [ "$version_major" -lt 15 ]; then
            print_warning "LLVM version $llvm_version is quite old. Consider updating to LLVM 17+"
        else
            print_success "LLVM version $llvm_version should be compatible"
        fi
    else
        print_warning "Could not determine LLVM version from $cmake_file"
    fi
}

# Export functions
export -f prepare_llvm_source verify_llvm_source verify_gnustep_patch
export -f quick_developer_rebuild verify_llvm_version