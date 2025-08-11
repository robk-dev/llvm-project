#!/bin/bash
# Complete End-to-End LLVM/LLDB Build with GNUstep Runtime Patch for Windows MSYS2/UCRT64
# This script automates the entire process from download to verification on Windows
# Author: LLDB GNUstep Development Team
# Date: August 2025

set -euo pipefail  # Exit on error, undefined variables, and pipe failures

# Dynamic path configuration - works for any user
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$WORKSPACE_ROOT")"

# Convert Windows paths to MSYS2 paths if needed
if [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
    PROJECT_ROOT=$(cygpath -u "$PROJECT_ROOT" 2>/dev/null || echo "$PROJECT_ROOT")
    WORKSPACE_ROOT=$(cygpath -u "$WORKSPACE_ROOT" 2>/dev/null || echo "$WORKSPACE_ROOT")
fi

# Build configuration
LLVM_BUILD_DIR="${LLVM_BUILD_DIR:-$PROJECT_ROOT/build-windows}"
LLVM_REPO="https://github.com/llvm/llvm-project.git"
LLVM_BRANCH="llvmorg-20.1.8"
BUILD_TYPE="RelWithDebInfo"

# Patch location - relative to workspace
PATCH_DIR="$WORKSPACE_ROOT/llvm_patch/GNUstepObjCRuntime"

# Windows-specific: Use fewer parallel jobs by default
if [[ "$OSTYPE" == "msys" ]]; then
    # Windows builds are memory intensive
    PARALLEL_JOBS=${PARALLEL_JOBS:-$(($(nproc) / 2))}
    if [ "$PARALLEL_JOBS" -lt 2 ]; then
        PARALLEL_JOBS=2
    fi
else
    PARALLEL_JOBS=${PARALLEL_JOBS:-$(nproc)}
fi

# GNUstep install directory (for workspace builds)
GNUSTEP_INSTALL_DIR="${WORKSPACE_ROOT}/gnustep-install-windows"

# Export variables for use in helper scripts
export SCRIPT_DIR WORKSPACE_ROOT PROJECT_ROOT LLVM_BUILD_DIR LLVM_REPO LLVM_BRANCH 
export BUILD_TYPE PATCH_DIR PARALLEL_JOBS GNUSTEP_INSTALL_DIR

# Source helper modules
source "$SCRIPT_DIR/helpers/common.sh"
source "$SCRIPT_DIR/helpers/system_checks.sh"
source "$SCRIPT_DIR/helpers/llvm_operations.sh"
source "$SCRIPT_DIR/helpers/build_operations.sh"
source "$SCRIPT_DIR/helpers/gnustep_operations.sh"
source "$SCRIPT_DIR/helpers/path_replacer.sh"
source "$SCRIPT_DIR/helpers/helper_scripts.sh"

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}  LLVM/LLDB Build with GNUstep Runtime - Windows MSYS2/UCRT64${NC}"
echo -e "${GREEN}================================================================${NC}"
echo -e "${CYAN}This script will:${NC}"
echo -e "${CYAN}  1. Check system requirements (50GB disk, 8GB+ RAM)${NC}"
echo -e "${CYAN}  2. Install all build dependencies via pacman${NC}"
echo -e "${CYAN}  3. Verify LLVM source and GNUstep integration${NC}"
echo -e "${CYAN}  4. Configure LLDB build with GNUstep runtime support${NC}"
echo -e "${CYAN}  5. Build LLDB with integrated patches (2-3 hours on Windows)${NC}"
echo -e "${CYAN}  6. Build lldb-server for debugging support${NC}"
echo -e "${CYAN}  7. Build complete GNUstep environment (libobjc2, gnustep-base)${NC}"
echo -e "${CYAN}  8. Replace hardcoded paths with current workspace paths${NC}"
echo -e "${CYAN}  9. Verify installation and create test programs${NC}"
echo ""
echo -e "${YELLOW}Build directory: ${LLVM_BUILD_DIR}${NC}"
echo -e "${YELLOW}Parallel jobs: ${PARALLEL_JOBS} (optimized for Windows)${NC}"
echo -e "${YELLOW}Platform: MSYS2/UCRT64 on Windows${NC}"
echo ""

# Function to show final instructions
show_final_instructions() {
    print_section "🎉 Build Complete!"
    
    echo -e "${GREEN}Your patched LLDB with lldb-server is ready to use on Windows!${NC}"
    echo ""
    echo -e "${YELLOW}Quick Start:${NC}"
    echo "  1. Source environment: source $LLVM_BUILD_DIR/setup_environment.sh"
    echo "  2. Build test program: cd $LLVM_BUILD_DIR/examples && make"
    echo "  3. Debug with LLDB: $LLVM_BUILD_DIR/bin/lldb.exe ./test_custom_class.exe"
    echo ""
    echo -e "${YELLOW}VS Code Setup (Windows):${NC}"
    echo "  1. Copy settings from: $LLVM_BUILD_DIR/vscode_settings.json"
    echo "  2. Use lldb-mi or CodeLLDB extension for debugging"
    echo ""
    echo -e "${YELLOW}Verification:${NC}"
    echo "  Run: $LLVM_BUILD_DIR/verify_gnustep_patch.sh"
    echo ""
    echo -e "${YELLOW}Key Features of Your Patched LLDB:${NC}"
    echo "  ✓ Dynamic class discovery via objc_copyClassList()"
    echo "  ✓ Automatic support for custom Objective-C classes"
    echo "  ✓ No hardcoded class lists needed"
    echo "  ✓ Works with libobjc2 on Windows"
    echo "  ✓ Native Windows performance"
    echo "  ✓ lldb-server for debugging support"
    echo "  ✓ Complete GNUstep environment with debug symbols"
    echo "  ✓ All paths updated for your workspace"
    echo ""
    echo -e "${YELLOW}Location Summary:${NC}"
    echo "  LLDB binary:    $LLVM_BUILD_DIR/bin/lldb.exe"
    echo "  lldb-server:    $LLVM_BUILD_DIR/bin/lldb-server.exe"
    echo "  Clang compiler: $LLVM_BUILD_DIR/bin/clang.exe"
    echo "  GNUstep libs:   $GNUSTEP_INSTALL_DIR"
    echo "  Examples:       $LLVM_BUILD_DIR/examples/"
    echo ""
    echo -e "${YELLOW}Build Performance:${NC}"
    echo "  ✓ ccache enabled for faster rebuilds"
    echo "  ✓ Parallel compilation with $PARALLEL_JOBS jobs"
    echo "  ✓ Optimized for Windows MSYS2 environment"
    echo ""
    echo -e "${BLUE}🔥 Windows LLDB/GNUstep setup complete!${NC}"
}

# Main execution flow
main() {
    # Parse command line options
    SKIP_DEPS=false
    SKIP_SOURCE_CHECK=false
    FORCE_CLEAN=false
    SKIP_GNUSTEP=false
    GNUSTEP_ONLY=false
    SKIP_PATH_REPLACE=false
    DEV_MODE=false
    FORCE_PATH_SETUP=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --dev)
                DEV_MODE=true
                SKIP_DEPS=true
                SKIP_SOURCE_CHECK=true
                shift
                ;;
            --skip-deps)
                SKIP_DEPS=true
                shift
                ;;
            --skip-source-check)
                SKIP_SOURCE_CHECK=true
                shift
                ;;
            --skip-gnustep)
                SKIP_GNUSTEP=true
                shift
                ;;
            --gnustep-only)
                GNUSTEP_ONLY=true
                shift
                ;;
            --skip-path-replace)
                SKIP_PATH_REPLACE=true
                shift
                ;;
            --force-clean)
                FORCE_CLEAN=true
                shift
                ;;
            --force-path-setup)
                FORCE_PATH_SETUP=true
                shift
                ;;
            --help)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --dev               Quick developer mode: rebuild only"
                echo "  --skip-deps         Skip dependency installation"
                echo "  --skip-source-check Skip LLVM source verification"
                echo "  --skip-gnustep      Skip GNUstep build"
                echo "  --gnustep-only      Build only GNUstep environment (skip LLVM)"
                echo "  --skip-path-replace Skip path replacement step"
                echo "  --force-clean       Clean existing build directory"
                echo "  --force-path-setup  Force PATH setup and tool verification"
                echo "  --help              Show this help"
                echo ""
                echo "Environment variables:"
                echo "  LLVM_BUILD_DIR      Build directory (default: PROJECT_ROOT/build-windows)"
                echo "  PARALLEL_JOBS       Number of parallel jobs (default: CPU cores / 2 on Windows)"
                echo ""
                echo "Example usage:"
                echo "  # Full build from scratch"
                echo "  ./setup.sh"
                echo ""
                echo "  # Quick rebuild after changes"
                echo "  ./setup.sh --dev"
                echo ""
                echo "  # Skip GNUstep if only testing LLDB"
                echo "  ./setup.sh --skip-gnustep"
                echo ""
                echo "  # Skip source check for faster rebuilds"
                echo "  ./setup.sh --skip-source-check"
                echo ""
                echo "  # Build only GNUstep environment"
                echo "  ./setup.sh --gnustep-only"
                echo ""
                echo "  # Fix PATH issues (if tools installed but not found)"
                echo "  ./setup.sh --force-path-setup"
                echo ""
                exit 0
                ;;
            *)
                print_error "Unknown option: $1. Use --help for usage."
                ;;
        esac
    done
    
    # Clean existing build if requested
    if $FORCE_CLEAN && [ -d "$LLVM_BUILD_DIR" ]; then
        print_warning "Cleaning existing build directory: $LLVM_BUILD_DIR"
        rm -rf "$LLVM_BUILD_DIR"
    fi
    
    # Developer mode: quick rebuild
    if $DEV_MODE; then
        print_section "🚀 Developer Mode: Quick rebuild"
        quick_developer_rebuild
        exit 0
    fi
    
    # Handle GNUstep-only mode
    if $GNUSTEP_ONLY; then
        print_section "🔧 GNUstep-Only Build Mode"
        print_info "Building only GNUstep environment (libobjc2, gnustep-make, gnustep-base)"
        
        # Basic system checks
        check_windows_environment
        check_disk_space
        
        if ! $SKIP_DEPS; then
            print_section "Installing Dependencies"
            install_windows_dependencies
        fi
        
        print_section "Building GNUstep Environment"
        build_complete_gnustep_windows
        
        print_section "Creating GNUstep Test Programs"
        create_gnustep_only_test_programs
        
        print_success "GNUstep-only build completed successfully!"
        print_info "GNUstep installation: $GNUSTEP_INSTALL_DIR"
        print_info "To use: source $GNUSTEP_INSTALL_DIR/setup-gnustep-env.sh"
        exit 0
    fi
    
    # Execute normal build steps
    print_section "Step 1: System Checks"
    
    # Force PATH setup if requested
    if $FORCE_PATH_SETUP; then
        print_info "Force PATH setup requested..."
        # This will be called in check_windows_environment, but we call it early
        setup_msys2_path
        
        print_info "Current PATH (first 10 entries):"
        echo "$PATH" | tr ':' '\n' | head -10 | sed 's/^/  /'
        
        print_info "Tool locations:"
        for tool in cmake ninja clang clang++ git make; do
            if command -v "$tool" >/dev/null 2>&1; then
                echo "  $tool: $(command -v "$tool")"
            else
                echo "  $tool: NOT FOUND"
            fi
        done
    fi
    
    check_windows_environment
    check_disk_space
    
    if ! $SKIP_DEPS; then
        print_section "Step 2: Installing Dependencies"
        install_windows_dependencies
    else
        print_info "Skipping dependency installation (--skip-deps)"
    fi
    
    if ! $SKIP_SOURCE_CHECK; then
        print_section "Step 3: Verifying LLVM Source"
        prepare_llvm_source
        verify_llvm_source
        verify_llvm_version
    else
        print_info "Skipping LLVM source verification (--skip-source-check)"
    fi
    
    print_section "Step 4: Verifying GNUstep Integration"
    verify_gnustep_patch
    
    print_section "Step 5: Configuring Build"
    configure_windows_build
    
    print_section "Step 6: Building LLDB"
    build_lldb_windows
    
    print_section "Step 7: Building lldb-server"
    build_lldb_server_windows
    
    if ! $SKIP_GNUSTEP; then
        print_section "Step 8: Building GNUstep Environment"
        build_complete_gnustep_windows
    else
        print_info "Skipping GNUstep build (--skip-gnustep)"
    fi
    
    if ! $SKIP_PATH_REPLACE; then
        print_section "Step 9: Updating Paths for Current Workspace"
        replace_hardcoded_paths
    else
        print_info "Skipping path replacement (--skip-path-replace)"
    fi
    
    print_section "Step 10: Verification"
    verify_windows_installation
    create_test_programs
    
    print_section "Step 11: Creating Helper Scripts"
    create_windows_helper_scripts
    
    show_final_instructions
    
    print_success "Windows LLVM/LLDB build with GNUstep patch finished successfully!"
}

# Handle Ctrl+C gracefully
trap 'echo -e "\n${YELLOW}Build interrupted by user${NC}"; exit 1' INT

# Run main function
main "$@"