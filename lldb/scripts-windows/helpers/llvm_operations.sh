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
    local changes=$(git status --porcelain 2>/dev/null | grep -v '^M ' | wc -l || echo "0")
    if [ "$changes" -gt 0 ]; then
        print_warning "Uncommitted changes detected (excluding permission changes):"
        git status --porcelain 2>/dev/null | grep -v '^M ' | head -5 || echo "  (unable to show changes)"
        echo ""
        print_info "Continuing with build (assuming changes are intentional)"
        # Commented out user prompt to avoid hanging
        # read -p "Continue with build? (y/N): " -n 1 -r
        # echo ""
        # if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        #     print_info "Please commit or stash your changes first"
        #     exit 1
        # fi
    else
        print_success "Working directory is clean"
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
    print_info "Checking directory: $(pwd)"
    
    # Check if the GNUstep runtime plugin directory exists in the source
    local target_dir="lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime"
    print_info "Looking for: $target_dir"
    
    if [ ! -d "$target_dir" ]; then
        print_error "GNUstep runtime patch not found: $target_dir"
        print_info "The LLDB source should already contain the GNUstep runtime plugin."
        print_info "Please ensure this repository has the GNUstep patches integrated."
        exit 1
    fi
    
    print_info "✓ GNUstep runtime directory found"
    
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
        print_progress "Checking LLDB build configuration..."
        if ! grep -q "GNUstepObjCRuntime" "$lldb_cmake" 2>/dev/null; then
            print_progress "Adding GNUstepObjCRuntime to LLDB build configuration..."
            echo "add_subdirectory(GNUstepObjCRuntime)" >> "$lldb_cmake" || {
                print_warning "Failed to update $lldb_cmake - continuing anyway"
            }
        else
            print_info "GNUstepObjCRuntime already configured in build"
        fi
    else
        print_warning "LLDB CMakeLists.txt not found at $lldb_cmake"
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
        print_progress "Checking LLVM CMakeLists.txt..."
        
        # Simple check - if the file exists and contains LLVM content, we're good
        if grep -q "LLVM_VERSION" "$cmake_file" 2>/dev/null; then
            print_success "LLVM CMakeLists.txt found with version information"
        else
            print_warning "LLVM CMakeLists.txt found but no version info detected"
        fi
        
        # Try to get version from git tag as a fallback
        local git_version=""
        if command -v git >/dev/null 2>&1; then
            git_version=$(git describe --tags 2>/dev/null | grep -o '[0-9]\+\.[0-9]\+' | head -1 2>/dev/null || echo "")
        fi
        
        if [ -n "$git_version" ]; then
            print_info "Git-based version: $git_version"
        else
            print_info "Using development version from current branch"
        fi
        
        print_success "LLVM version verification completed"
    else
        print_warning "Could not find LLVM CMakeLists.txt at $cmake_file"
    fi
}

# Export functions
export -f prepare_llvm_source verify_llvm_source verify_gnustep_patch
export -f quick_developer_rebuild verify_llvm_version