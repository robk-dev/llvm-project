#!/bin/bash
# Complete End-to-End LLVM/LLDB Build with GNUstep Runtime Patch
# This script automates the entire process from download to verification
# Author: GitHub Copilot
# Date: July 30, 2025

set -euo pipefail  # Exit on error, undefined variables, and pipe failures

# Dynamic path configuration - works for any user
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$WORKSPACE_ROOT")"
LLVM_BUILD_DIR="${LLVM_BUILD_DIR:-$PROJECT_ROOT/build}"  # Use our existing build directory
LLVM_REPO="https://github.com/llvm/llvm-project.git"
LLVM_BRANCH="llvmorg-20.1.8"
BUILD_TYPE="RelWithDebInfo"

# Patch location - now relative to workspace
PATCH_DIR="$WORKSPACE_ROOT/llvm_patch/GNUstepObjCRuntime"

# Calculate safe parallel jobs (will be defined in common.sh functions)
PARALLEL_JOBS=${PARALLEL_JOBS:-$(nproc)}

# GNUstep install directory (for workspace builds)
GNUSTEP_INSTALL_DIR="${WORKSPACE_ROOT}/gnustep-install"

# Export variables for use in helper scripts
export SCRIPT_DIR WORKSPACE_ROOT PROJECT_ROOT LLVM_BUILD_DIR LLVM_REPO LLVM_BRANCH BUILD_TYPE PATCH_DIR PARALLEL_JOBS GNUSTEP_INSTALL_DIR

# Source helper modules
source "$SCRIPT_DIR/helpers/common.sh"
source "$SCRIPT_DIR/helpers/system_checks.sh"
source "$SCRIPT_DIR/helpers/llvm_operations.sh"
source "$SCRIPT_DIR/helpers/build_operations.sh"
source "$SCRIPT_DIR/helpers/gnustep_operations.sh"
source "$SCRIPT_DIR/helpers/helper_scripts.sh"

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}  Complete LLVM/LLDB Build with GNUstep Runtime Patch${NC}"
echo -e "${GREEN}================================================================${NC}"
echo -e "${CYAN}This script will:${NC}"
echo -e "${CYAN}  1. Check system requirements (50GB disk, 8GB+ RAM)${NC}"
echo -e "${CYAN}  2. Install all build dependencies${NC}"
echo -e "${CYAN}  3. Download LLVM 20.1.8 source code (~2GB) [skippable with --skip-download]${NC}"
echo -e "${CYAN}  4. Apply GNUstep runtime patch for dynamic class discovery${NC}"
echo -e "${CYAN}  5. Verify LLVM 20+ API compatibility${NC}"
echo -e "${CYAN}  6. Build LLDB with the patch (1-2 hours)${NC}"
echo -e "${CYAN}  7. Build lldb-server (required for VS Code debugging)${NC}"
echo -e "${CYAN}  8. [OPTIONAL] Build complete GNUstep debug environment${NC}"
echo -e "${CYAN}  9. Verify installation and create test programs${NC}"
echo ""
echo -e "${YELLOW}Build directory: ${LLVM_BUILD_DIR}${NC}"
echo -e "${YELLOW}Parallel jobs: ${PARALLEL_JOBS}${NC}"
echo ""
echo -e "${CYAN}💡 New Options:${NC}"
echo -e "${CYAN}  --with-gnustep    Build complete GNUstep environment with debug symbols${NC}"
echo -e "${CYAN}  --gnustep-only    Build only GNUstep environment (skip LLVM build)${NC}"
echo ""

# Function to show final instructions
show_final_instructions() {
    print_section "🎉 Build Complete!"
    
    echo -e "${GREEN}Your patched LLDB with lldb-server is ready to use!${NC}"
    echo ""
    echo -e "${YELLOW}Quick Start:${NC}"
    echo "  1. Source environment: source $LLVM_BUILD_DIR/setup_environment.sh"
    echo "  2. Build test program: cd $LLVM_BUILD_DIR/examples && make"
    echo "  3. Debug with LLDB: $LLVM_BUILD_DIR/build/bin/lldb ./test_custom_class"
    echo ""
    echo -e "${YELLOW}VS Code Setup:${NC}"
    echo "  1. Copy settings from: $LLVM_BUILD_DIR/vscode_settings.json"
    echo "  2. Or run troubleshooting: $LLVM_BUILD_DIR/troubleshoot_lldb_server.sh"
    echo ""
    echo -e "${YELLOW}Verification:${NC}"
    echo "  Run: $LLVM_BUILD_DIR/verify_gnustep_patch.sh"
    echo ""
    echo -e "${YELLOW}Key Features of Your Patched LLDB:${NC}"
    echo "  ✓ Dynamic class discovery via objc_copyClassList()"
    echo "  ✓ Automatic support for custom Objective-C classes"
    echo "  ✓ No hardcoded class lists needed"
    echo "  ✓ Works with libobjc2"
    echo "  ✓ Native C++ performance"
    echo "  ✓ lldb-server for VS Code debugging support"
    if $BUILD_GNUSTEP; then
        echo "  ✓ Complete GNUstep debug environment with symbols"
        echo "  ✓ libobjc2 and gnustep-base built with debug symbols"
        echo "  ✓ Ivar offset symbol generation enabled"
    fi
    echo ""
    echo -e "${YELLOW}Location Summary:${NC}"
    echo "  LLDB binary:    $LLVM_BUILD_DIR/build/bin/lldb"
    echo "  lldb-server:    $LLVM_BUILD_DIR/build/bin/lldb-server"
    echo "  Clang compiler: $LLVM_BUILD_DIR/build/bin/clang"
    echo "  Examples:       $LLVM_BUILD_DIR/examples/"
    echo "  Verification:   $LLVM_BUILD_DIR/verify_gnustep_patch.sh"
    echo ""
    echo -e "${YELLOW}Build Performance Features:${NC}"
    echo "  ✓ ccache enabled for faster rebuilds"
    echo "  ✓ Parallel compilation with $PARALLEL_JOBS jobs"
    echo "  ✓ Optimized build configuration ($BUILD_TYPE)"
    echo ""
    echo -e "${YELLOW}For subsequent builds:${NC}"
    echo "  cd $LLVM_BUILD_DIR/build && ninja lldb lldb-server  # Much faster with ccache!"
    echo ""
    echo -e "${YELLOW}For incremental rebuilds with patch changes:${NC}"
    echo "  $SCRIPT_DIR/setup.sh --skip-deps --skip-download  # Skip download, just apply patch and rebuild"
    echo ""
    echo -e "${BLUE}🔥 You now have the most advanced Objective-C debugging setup for Linux!${NC}"
}

# Main execution flow
main() {
    # Parse command line options
    SKIP_DEPS=false
    SKIP_DOWNLOAD=false
    FORCE_CLEAN=false
    BUILD_GNUSTEP=false
    GNUSTEP_ONLY=false
    DEV_MODE=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --dev)
                # Developer mode: quick patch & rebuild
                DEV_MODE=true
                SKIP_DEPS=true
                SKIP_DOWNLOAD=true
                shift
                ;;
            --skip-deps)
                SKIP_DEPS=true
                shift
                ;;
            --skip-download)
                SKIP_DOWNLOAD=true
                shift
                ;;
            --force-clean)
                FORCE_CLEAN=true
                shift
                ;;
            --with-gnustep)
                BUILD_GNUSTEP=true
                shift
                ;;
            --gnustep-only)
                GNUSTEP_ONLY=true
                BUILD_GNUSTEP=true
                shift
                ;;
            --status)
                # Just show build status and exit
                if [ -d "$LLVM_BUILD_DIR" ]; then
                    show_build_status
                    exit 0
                else
                    print_error "LLVM build directory not found: $LLVM_BUILD_DIR"
                fi
                ;;
            --test)
                # Run quick tests and exit
                if [ -d "$LLVM_BUILD_DIR" ]; then
                    run_quick_tests
                    exit 0
                else
                    print_error "LLVM build directory not found: $LLVM_BUILD_DIR"
                fi
                ;;
            --help)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --dev             Quick developer mode: copy patch files and rebuild"
                echo "                    (implies --skip-deps --skip-download)"
                echo "  --skip-deps       Skip dependency installation"
                echo "  --skip-download   Skip LLVM download (use existing)"
                echo "  --force-clean     Clean existing build directory"
                echo "  --with-gnustep    Build complete GNUstep debug environment"
                echo "  --gnustep-only    Build only GNUstep environment (skip LLVM)"
                echo "  --status          Show build status and exit"
                echo "  --test            Run quick tests and exit"
                echo "  --help            Show this help"
                echo ""
                echo "Environment variables:"
                echo "  LLVM_BUILD_DIR    Build directory (default: ~/llvm-build)"
                echo "  PARALLEL_JOBS     Number of parallel jobs (default: auto-calculated based on memory)"
                echo ""
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                ;;
        esac
    done
    
    # Clean existing build if requested
    if $FORCE_CLEAN && [ -d "$LLVM_BUILD_DIR" ]; then
        print_warning "Cleaning existing build directory: $LLVM_BUILD_DIR"
        rm -rf "$LLVM_BUILD_DIR"
    fi
    
    # Developer mode: minimal output, quick rebuild
    if $DEV_MODE; then
        echo -e "${CYAN}🚀 Developer Mode: Quick patch & rebuild${NC}"
        
        # Set environment variables for build
        export LD_LIBRARY_PATH="$LLVM_BUILD_DIR/build/lib"
        export PATH="$LLVM_BUILD_DIR/build/bin:$PATH"
                
        # Check if build directory exists
        if [ ! -d "$LLVM_BUILD_DIR/build" ]; then
            print_error "Build directory not found. Run full build first with: $0"
        fi

        # Quick rebuild
        echo -n "Building LLDB... "
        cd "$LLVM_BUILD_DIR/build"
        if ninja lldb lldb-server >/dev/null 2>&1; then
            echo -e "${GREEN}✓${NC}"
            echo -e "${GREEN}Build complete!${NC} LLDB: $LLVM_BUILD_DIR/build/bin/lldb"
            echo -e "${CYAN}Environment variables set:${NC}"
            echo "  export LD_LIBRARY_PATH=\"$LLVM_BUILD_DIR/build/lib\""
            echo "  export PATH=\"$LLVM_BUILD_DIR/build/bin:\$PATH\""
        else
            echo -e "${RED}✗${NC}"
            echo "Build failed. Running with full output:"
            ninja lldb lldb-server
        fi
        exit $?
    fi
    
    # Execute normal build steps
    check_disk_space
    
    # Handle GNUstep-only mode
    if $GNUSTEP_ONLY; then
        print_section "🔧 GNUstep-Only Build Mode"
        print_progress "Building complete GNUstep debug environment..."
        build_complete_gnustep_environment
        print_success "GNUstep-only build completed successfully!"
        exit 0
    fi
    
    if ! $SKIP_DEPS; then
        install_dependencies
    fi
    
    if ! $SKIP_DOWNLOAD; then
        download_llvm
    else
        print_section "Step 3: Skipping LLVM Download (using existing)"
        if [ ! -d "$LLVM_BUILD_DIR/llvm-project" ]; then
            print_error "LLVM project not found at $LLVM_BUILD_DIR/llvm-project"
            print_error "Please run without --skip-download first to download LLVM"
        fi
        
        cd "$LLVM_BUILD_DIR/llvm-project"
        COMMIT_HASH=$(git rev-parse --short HEAD)
        COMMIT_DATE=$(git log -1 --format=%ci)
        print_success "Using existing LLVM: $COMMIT_HASH ($COMMIT_DATE)"
    fi
    
    verify_llvm_version
    configure_build
    build_lldb
    build_lldb_server
    verify_installation
    
    # Build GNUstep environment if requested
    if $BUILD_GNUSTEP; then
        print_section "🔧 Building Complete GNUstep Debug Environment"
        print_progress "This will ensure optimal symbol generation for debugging..."
        build_complete_gnustep_environment
    fi
    
    update_vscode_settings
    create_helper_scripts
    show_final_instructions
    
    print_success "Complete LLVM/LLDB build with GNUstep patch finished successfully!"
}

# Handle Ctrl+C gracefully
trap 'echo -e "\n${YELLOW}Build interrupted by user${NC}"; exit 1' INT

# Run main function
main "$@"
