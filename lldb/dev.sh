#!/bin/bash
# GNUstep LLDB Plugin Development Script
# Production-ready development environment for LLVM/LLDB GNUstep bridge

set -e  # Exit on error
set -u  # Exit on undefined variables

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/../build"
EXAMPLES_DIR="$SCRIPT_DIR/examples"
PLUGIN_TARGET="lldbPluginGNUstepObjCRuntime"
LLDB_BIN="$BUILD_DIR/bin/lldb"
LLDB_SERVER_BIN="$BUILD_DIR/bin/lldb-server"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_section() {
    echo -e "${PURPLE}[SECTION]${NC} $1"
}

# Verify prerequisites
check_prerequisites() {
    log_section "Checking prerequisites..."
    
    # Check if we're in the right directory
    if [[ ! -f "$SCRIPT_DIR/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp" ]]; then
        log_error "Not in LLDB root directory. Expected to find GNUstepObjCRuntime plugin source."
        exit 1
    fi
    
    # Check build directory exists
    if [[ ! -d "$BUILD_DIR" ]]; then
        log_error "Build directory not found: $BUILD_DIR"
        log_info "Run initial CMake configuration first."
        exit 1
    fi
    
    # Check ninja is available
    if ! command -v ninja &> /dev/null; then
        log_error "ninja build system not found. Please install ninja-build."
        exit 1
    fi
    
    # Check if examples directory exists
    if [[ ! -d "$EXAMPLES_DIR" ]]; then
        log_error "Examples directory not found: $EXAMPLES_DIR"
        exit 1
    fi
    
    log_success "Prerequisites check passed"
}

# Clean build function - full rebuild
clean_build() {
    log_section "🧹 Cleaning and rebuilding GNUstep plugin..."
    check_prerequisites
    
    cd "$BUILD_DIR" || exit 1
    
    # Clean specific plugin artifacts
    log_info "Removing plugin build artifacts..."
    find . -name "*GNUstepObjCRuntime*" -type f -delete 2>/dev/null || true
    
    # Clean LLDB core components 
    log_info "Cleaning core LLDB components..."
    ninja -t clean lldb lldb-server lldb-argdumper "$PLUGIN_TARGET" 2>/dev/null || true
    
    # Full rebuild
    log_info "Building LLDB with GNUstep plugin..."
    local start_time=$(date +%s)
    
    if ninja lldb lldb-server lldb-argdumper "$PLUGIN_TARGET" -j$(nproc); then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        log_success "Clean build completed in ${duration}s"
        
        # Verify plugin was built
        if [[ -f "$BUILD_DIR/lib/liblldbPluginGNUstepObjCRuntime.a" ]]; then
            log_success "GNUstep plugin built successfully"
        else
            log_warning "Plugin build succeeded but library not found at expected location"
        fi
    else
        log_error "Build failed"
        exit 1
    fi
}

# Quick rebuild function - incremental
quick_build() {
    log_section "🔨 Quick rebuild..."
    check_prerequisites
    
    cd "$BUILD_DIR" || exit 1
    
    log_info "Incremental build of GNUstep plugin..."
    local start_time=$(date +%s)
    
    if ninja "$PLUGIN_TARGET" -j$(nproc); then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        log_success "Quick build completed in ${duration}s"
    else
        log_error "Quick build failed"
        exit 1
    fi
}

# Run tests function
run_tests() {
    log_section "🧪 Running tests..."
    check_prerequisites
    
    cd "$BUILD_DIR" || exit 1
    
    # Check if test targets exist
    log_info "Checking available LLDB tests..."
    if ninja -t targets | grep -q "check-lldb.*gnustep"; then
        log_info "Running GNUstep-specific LLDB tests..."
        ninja check-lldb-plugins-languageruntime-objc-gnustep
    else
        log_warning "No specific GNUstep tests found in build system"
    fi
    
    # Run unit tests if available
    if [[ -f "$BUILD_DIR/unittests/Language/ObjC/GNUstep/GNUstepTests" ]]; then
        log_info "Running GNUstep unit tests..."
        "$BUILD_DIR/unittests/Language/ObjC/GNUstep/GNUstepTests"
    else
        log_info "No unit tests found, skipping..."
    fi
    
    log_success "Test execution completed"
}

# Clean examples function
clean_examples() {
    log_section "🧹 Cleaning example binaries..."
    
    if [[ ! -d "$EXAMPLES_DIR" ]]; then
        log_error "Examples directory not found: $EXAMPLES_DIR"
        exit 1
    fi
    
    cd "$EXAMPLES_DIR" || exit 1
    
    # Count executables before cleaning
    local executable_count
    executable_count=$(find . -maxdepth 1 -type f -executable -not -name "*.sh" -not -name "*.py" | wc -l)
    
    log_info "Found $executable_count executable files to clean"
    
    # Use Makefile clean if available
    if [[ -f "Makefile" ]]; then
        log_info "Using Makefile clean target..."
        make clean
    else
        log_warning "No Makefile found, performing manual cleanup..."
    fi
    
    # Additional cleanup - remove any remaining executables without extensions
    log_info "Performing additional cleanup..."
    find . -maxdepth 1 -type f -executable -not -name "*.sh" -not -name "*.py" -not -name "*.lldb" -delete 2>/dev/null || true
    
    # Clean core dumps and temporary files
    find . -name "core.*" -delete 2>/dev/null || true
    find . -name "*.dSYM" -type d -exec rm -rf {} + 2>/dev/null || true
    find . -name "*.o" -delete 2>/dev/null || true
    find . -name "a.out" -delete 2>/dev/null || true
    find . -name "*.tmp" -delete 2>/dev/null || true
    
    local remaining_count
    remaining_count=$(find . -maxdepth 1 -type f -executable -not -name "*.sh" -not -name "*.py" | wc -l)
    
    log_success "Cleaned $((executable_count - remaining_count)) files"
    
    if [[ $remaining_count -gt 0 ]]; then
        log_warning "$remaining_count executable files remain"
        log_info "Remaining files:"
        find . -maxdepth 1 -type f -executable -not -name "*.sh" -not -name "*.py" | head -5
    fi
}

# Debug example function
debug_example() {
    local example_name="$1"
    
    log_section "🐛 Starting LLDB debug session..."
    check_prerequisites
    
    if [[ ! -f "$LLDB_BIN" ]]; then
        log_error "LLDB not found: $LLDB_BIN"
        log_info "Run './dev.sh clean-build' first"
        exit 1
    fi
    
    if [[ ! -f "$LLDB_SERVER_BIN" ]]; then
        log_error "LLDB server not found: $LLDB_SERVER_BIN"
        log_info "Run './dev.sh clean-build' first"
        exit 1
    fi
    
    cd "$EXAMPLES_DIR" || exit 1
    
    # Default example if none specified
    if [[ -z "${example_name:-}" ]]; then
        example_name="custom_class_test"
        log_info "No example specified, using default: $example_name"
    fi
    
    # Build the example if it doesn't exist
    if [[ ! -f "$example_name" ]]; then
        log_info "Building example: $example_name"
        if [[ -f "Makefile" ]]; then
            make "$example_name"
        else
            log_error "Cannot build $example_name: No Makefile found"
            exit 1
        fi
    fi
    
    # Verify example is executable
    if [[ ! -x "$example_name" ]]; then
        log_error "Example not executable: $example_name"
        exit 1
    fi
    
    # Set up environment
    export PATH="$(dirname "$LLDB_BIN"):$PATH"
    export LLDB_DEBUGSERVER_PATH="$LLDB_SERVER_BIN"
    
    log_info "Starting LLDB with example: $example_name"
    log_info "LLDB path: $LLDB_BIN"
    log_info "LLDB server: $LLDB_SERVER_BIN"
    
    # Provide helpful commands based on example type
    case "$example_name" in
        "custom_class_test"|"custom_class_test_custom"|"custom_class_test_updated")
            echo -e "\n${CYAN}Suggested LLDB commands for custom class testing:${NC}"
            echo "  (lldb) b custom_class_test.m:125"
            echo "  (lldb) run"
            echo "  (lldb) po account          # Test custom object"
            echo "  (lldb) po personInfo       # Test NSDictionary"  
            echo "  (lldb) po fruits           # Test NSArray"
            echo "  (lldb) po magicNumber      # Test NSNumber"
            ;;
        "test_nsnumber_comprehensive")
            echo -e "\n${CYAN}Suggested LLDB commands for NSNumber testing:${NC}"
            echo "  (lldb) b test_nsnumber_comprehensive.m:49"
            echo "  (lldb) run"
            echo "  (lldb) po intNumber        # Test integer NSNumber"
            echo "  (lldb) po floatNumber      # Test float NSNumber"
            echo "  (lldb) po boolYes          # Test boolean NSNumber"
            ;;
        "test_collections_formatter")
            echo -e "\n${CYAN}Suggested LLDB commands for collections testing:${NC}"
            echo "  (lldb) b test_collections_formatter.m:241"
            echo "  (lldb) run"
            echo "  (lldb) po multiDict        # Test NSDictionary"
            echo "  (lldb) po stringSet        # Test NSSet"
            echo "  (lldb) po complexStructure # Test nested collections"
            ;;
        *)
            echo -e "\n${CYAN}General LLDB commands:${NC}"
            echo "  (lldb) b main"
            echo "  (lldb) run"
            echo "  (lldb) po [variable_name]  # Print object description"
            echo "  (lldb) frame variable -O   # Show all variables with formatters"
            ;;
    esac
    
    echo -e "\n${GREEN}Starting LLDB session...${NC}\n"
    
    # Launch LLDB
    "$LLDB_BIN" "$example_name"
}

# Build specific example
build_example() {
    local example_name="$1"
    
    log_section "🔨 Building example: $example_name"
    
    cd "$EXAMPLES_DIR" || exit 1
    
    if [[ -f "Makefile" ]]; then
        make "$example_name"
        log_success "Built $example_name"
    else
        log_error "No Makefile found in examples directory"
        exit 1
    fi
}

# Show usage information
show_usage() {
    echo -e "${PURPLE}GNUstep LLDB Plugin Development Script${NC}"
    echo ""
    echo -e "${CYAN}Usage:${NC}"
    echo "  $0 <command> [options]"
    echo ""
    echo -e "${CYAN}Commands:${NC}"
    echo -e "  ${GREEN}clean-build${NC}     Clean and rebuild plugin (full rebuild)"
    echo -e "  ${GREEN}build${NC}           Quick rebuild (incremental)"  
    echo -e "  ${GREEN}test${NC}            Run unit tests"
    echo -e "  ${GREEN}clean-examples${NC}  Clean example binaries and artifacts"
    echo -e "  ${GREEN}debug [example]${NC} Start LLDB debug session with example"
    echo -e "  ${GREEN}build-example <name>${NC} Build specific example"
    echo -e "  ${GREEN}full${NC}            Run full cycle: clean examples, clean build, test"
    echo -e "  ${GREEN}status${NC}          Show development environment status"
    echo -e "  ${GREEN}help${NC}            Show this help message"
    echo ""
    echo -e "${CYAN}Examples:${NC}"
    echo "  $0 clean-build                    # Full clean rebuild"
    echo "  $0 build                          # Quick incremental build"
    echo "  $0 debug custom_class_test        # Debug with custom class example"
    echo "  $0 debug                          # Debug with default example"
    echo "  $0 clean-examples                 # Remove all example binaries"
    echo "  $0 build-example foundation_test  # Build specific example"
    echo "  $0 full                           # Complete development cycle"
    echo ""
    echo -e "${CYAN}Available Examples:${NC}"
    if [[ -f "$EXAMPLES_DIR/Makefile" ]]; then
        echo "  custom_class_test, foundation_test, test_collections_formatter"
        echo "  test_nsnumber_comprehensive, simple_test, array_test"
        echo "  dictionary_test, nsset_test, test_data_url_uuid"
        echo "  (See $EXAMPLES_DIR/Makefile for complete list)"
    else
        echo "  (Makefile not found - cannot list examples)"
    fi
}

# Show development environment status
show_status() {
    log_section "Development Environment Status"
    
    echo -e "\n${CYAN}Build Configuration:${NC}"
    echo "  LLDB Source:  $SCRIPT_DIR"
    echo "  Build Dir:    $BUILD_DIR"
    echo "  Examples Dir: $EXAMPLES_DIR"
    
    echo -e "\n${CYAN}Build Artifacts:${NC}"
    if [[ -f "$LLDB_BIN" ]]; then
        echo -e "  ✅ LLDB:        $LLDB_BIN"
    else
        echo -e "  ❌ LLDB:        Not found"
    fi
    
    if [[ -f "$LLDB_SERVER_BIN" ]]; then
        echo -e "  ✅ LLDB Server: $LLDB_SERVER_BIN"
    else
        echo -e "  ❌ LLDB Server: Not found"
    fi
    
    if [[ -f "$BUILD_DIR/lib/liblldbPluginGNUstepObjCRuntime.a" ]]; then
        echo -e "  ✅ GNUstep Plugin: Built"
        local plugin_size
        plugin_size=$(ls -lh "$BUILD_DIR/lib/liblldbPluginGNUstepObjCRuntime.a" | awk '{print $5}')
        echo "     Size: $plugin_size"
        local plugin_date
        plugin_date=$(ls -l "$BUILD_DIR/lib/liblldbPluginGNUstepObjCRuntime.a" | awk '{print $6, $7, $8}')
        echo "     Modified: $plugin_date"
    else
        echo -e "  ❌ GNUstep Plugin: Not found"
    fi
    
    echo -e "\n${CYAN}Examples Status:${NC}"
    if [[ -d "$EXAMPLES_DIR" ]]; then
        local executable_count
        executable_count=$(find "$EXAMPLES_DIR" -maxdepth 1 -type f -executable -not -name "*.sh" -not -name "*.py" | wc -l)
        echo "  Built Examples: $executable_count"
        
        if [[ $executable_count -gt 0 ]]; then
            echo "  Recent Examples:"
            find "$EXAMPLES_DIR" -maxdepth 1 -type f -executable -not -name "*.sh" -not -name "*.py" -printf "    %f (modified: %TY-%Tm-%Td %TH:%TM)\n" | sort -k3 -r | head -3
        fi
    else
        echo "  ❌ Examples directory not found"
    fi
    
    echo -e "\n${CYAN}Dependencies:${NC}"
    if command -v ninja &> /dev/null; then
        local ninja_version
        ninja_version=$(ninja --version)
        echo -e "  ✅ Ninja: $ninja_version"
    else
        echo -e "  ❌ Ninja: Not found"
    fi
    
    if command -v clang &> /dev/null; then
        local clang_version
        clang_version=$(clang --version | head -1 | sed 's/clang version //')
        echo -e "  ✅ Clang: $clang_version"
    else
        echo -e "  ❌ Clang: Not found"
    fi
}

# Main command dispatcher
main() {
    case "${1:-help}" in
        "clean-build")
            clean_build
            ;;
        "build")
            quick_build
            ;;
        "test")
            run_tests
            ;;
        "clean-examples")
            clean_examples
            ;;
        "debug")
            debug_example "${2:-}"
            ;;
        "build-example")
            if [[ -z "${2:-}" ]]; then
                log_error "Example name required for build-example"
                echo "Usage: $0 build-example <example_name>"
                exit 1
            fi
            build_example "$2"
            ;;
        "full")
            log_section "🚀 Running full development cycle..."
            clean_examples
            echo ""
            clean_build
            echo ""
            run_tests
            log_success "Full development cycle completed!"
            ;;
        "status")
            show_status
            ;;
        "help"|"-h"|"--help")
            show_usage
            ;;
        *)
            log_error "Unknown command: $1"
            echo ""
            show_usage
            exit 1
            ;;
    esac
}

# Check for minimum required arguments
if [[ $# -eq 0 ]]; then
    show_usage
    exit 0
fi

# Execute main function
main "$@"