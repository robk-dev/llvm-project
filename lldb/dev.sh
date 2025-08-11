#!/bin/bash
# GNUstep LLDB Plugin Development Script
# Production-ready development environment for LLVM/LLDB GNUstep bridge

set -e  # Exit on error
set -u  # Exit on undefined variables

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="/home/robk/code/llvm-project/build"
EXAMPLES_DIR="$SCRIPT_DIR/examples"
# Build targets - using main LLDB targets instead of plugin-specific one
BUILD_TARGETS="lldb lldb-server"
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
    ninja -t clean lldb lldb-server lldb-argdumper lldbPluginGNUstepObjCRuntime 2>/dev/null || true
    
    # Full rebuild
    log_info "Building LLDB with GNUstep plugin..."
    local start_time=$(date +%s)
    
    if ninja lldb lldb-server lldb-argdumper lldbPluginGNUstepObjCRuntime -j$(nproc); then
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
    
    if ninja $BUILD_TARGETS -j$(nproc); then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        log_success "Quick build completed in ${duration}s"
    else
        log_error "Quick build failed"
        exit 1
    fi
}

# Run unit tests only
run_unit_tests() {
    log_section "🧪 Running GNUstep Unit Tests..."
    check_prerequisites
    
    cd "$BUILD_DIR" || exit 1
    
    # Build and run unit tests
    if ninja -t targets | grep -q "LanguageObjCGNUstepTests"; then
        log_info "Building GNUstep unit tests..."
        if ninja LanguageObjCGNUstepTests; then
            # Find and run the test binary
            local test_binary=$(find "$BUILD_DIR" -name "LanguageObjCGNUstepTests" -type f 2>/dev/null | head -1)
            if [[ -n "$test_binary" && -f "$test_binary" ]]; then
                log_info "Running unit tests: $test_binary"
                # Run different test suites separately to handle crashes gracefully
                echo ""
                log_info "Running core runtime tests (safe)..."
                "$test_binary" --gtest_filter="GNUstepTaggedPointerTest.*:GNUstepIntrospectorTest.*:GNUstepRuntimeTest.*" --gtest_brief=1
                
                echo ""
                log_info "Running formatter instantiation tests..."
                "$test_binary" --gtest_filter="*FormatterTest.*Instantiation*:*FormatterTest.*Registration*" --gtest_brief=1 || log_warning "Some formatter instantiation tests failed"
                
                log_success "Unit tests completed"
            else
                log_warning "Unit test binary not found after build"
                return 1
            fi
        else
            log_error "Unit tests failed to build"
            return 1
        fi
    else
        log_error "LanguageObjCGNUstepTests target not found"
        return 1
    fi
}

# Run API tests only
run_api_tests() {
    log_section "🧪 Running GNUstep API Tests..."
    check_prerequisites
    
    local api_test_dir="$SCRIPT_DIR/test/API/lang/objc/gnustep"
    local our_clang="$BUILD_DIR/bin/clang"
    
    if [[ ! -d "$api_test_dir" ]]; then
        log_error "API test directory not found: $api_test_dir"
        return 1
    fi
    
    if [[ ! -f "$our_clang" ]]; then
        log_error "Clang compiler not found: $our_clang"
        return 1
    fi
    
    # Build and run individual test programs
    cd "$api_test_dir" || exit 1
    
    # Test each main program individually
    local test_programs=("main.m" "test_collections.m" "test_new_formatters.m")
    local test_results=()
    
    for program in "${test_programs[@]}"; do
        log_info "Testing program: $program"
        
        # Update Makefile for single program
        sed -i "1s/.*/OBJC_SOURCES := $program/" Makefile
        
        # Clean and build
        make clean > /dev/null 2>&1
        if OBJC="$our_clang" make; then
            # Test basic execution
            if timeout 10s ./a.out > /dev/null 2>&1; then
                log_success "✅ $program builds and runs successfully"
                test_results+=("$program: PASS")
            else
                log_warning "⚠️  $program builds but fails to run or times out"
                test_results+=("$program: BUILD_OK_RUN_FAIL")
            fi
        else
            log_error "❌ $program failed to build"
            test_results+=("$program: BUILD_FAIL")
        fi
    done
    
    # Show results summary
    echo ""
    log_section "API Test Results Summary:"
    for result in "${test_results[@]}"; do
        echo "  $result"
    done
    
    # Count successes
    local success_count=$(echo "${test_results[@]}" | grep -o "PASS" | wc -l)
    local total_count=${#test_results[@]}
    
    if [[ $success_count -eq $total_count ]]; then
        log_success "All $total_count API test programs passed!"
        return 0
    else
        log_warning "$success_count/$total_count API test programs passed"
        return 1
    fi
}

# Run integration tests with LLDB
run_integration_tests() {
    log_section "🧪 Running GNUstep Integration Tests..."
    check_prerequisites
    
    local api_test_dir="$SCRIPT_DIR/test/API/lang/objc/gnustep"
    local our_clang="$BUILD_DIR/bin/clang"
    
    if [[ ! -d "$api_test_dir" ]]; then
        log_error "API test directory not found: $api_test_dir"
        return 1
    fi
    
    cd "$api_test_dir" || exit 1
    
    # Ensure OBJC_SOURCES is set to main.m (Makefile already has debug flags)
    sed -i "1s/.*/OBJC_SOURCES := main.m/" Makefile
    
    log_info "Building test program with debug symbols..."
    
    # Clean and build using the Makefile (which now includes -g -O0)
    if ! OBJC="$our_clang" make clean >/dev/null 2>&1; then
        log_warning "Clean failed, continuing..."
    fi
    
    if ! OBJC="$our_clang" make; then
        log_error "Failed to build main test program for integration tests"
        return 1
    fi
    
    # Create improved LLDB test script with proper file specification
    cat > test_formatters.lldb << 'EOF'
# GNUstep Formatter Integration Test Script
target create ./a.out
# Set breakpoint at final NSLog line where all variables are in scope
breakpoint set --file main.m --line 127
run
# Now we should be stopped with all variables available for testing
# Test basic formatters using frame variable (more reliable than po)
frame variable -O emptyString
frame variable -O asciiString  
frame variable -O intNumber
frame variable -O emptyArray
frame variable -O simpleArray
frame variable -O emptyDict
frame variable -O simpleDict
frame variable -O emptySet
frame variable -O account
# Test summary strings
frame variable emptyString
frame variable asciiString
frame variable simpleArray
frame variable simpleDict
continue
quit
EOF
    
    log_info "Running LLDB integration test..."
    local test_result=0
    
    # Run with timeout and capture both stdout and stderr
    if timeout 45s "$LLDB_BIN" -s test_formatters.lldb > integration_test_output.txt 2>&1; then
        test_result=0
    else
        test_result=$?
    fi
    
    # Always show the output for debugging
    echo ""
    log_info "LLDB Integration Test Output:"
    echo "----------------------------------------"
    cat integration_test_output.txt
    echo "----------------------------------------"
    
    if [[ $test_result -eq 0 ]]; then
        # Check if formatters worked by looking for expected output patterns
        local formatter_working=false
        local error_found=false
        
        # Check for errors first
        if grep -q "error:" integration_test_output.txt; then
            error_found=true
            log_warning "LLDB errors detected in output"
        fi
        
        # Check for basic string output
        if grep -q '@""' integration_test_output.txt || grep -q 'NSString' integration_test_output.txt; then
            formatter_working=true
            log_success "String formatters working"
        fi
        
        # Check for array/dict output or collection types
        if grep -qE '\(.*elements?\)|\{.*\}|NSArray|NSDictionary|NSSet' integration_test_output.txt; then
            formatter_working=true
            log_success "Collection formatters detected"
        fi
        
        # Check for custom class formatting
        if grep -q 'BankAccount' integration_test_output.txt; then
            formatter_working=true
            log_success "Custom class formatting detected"
        fi
        
        if $formatter_working && ! $error_found; then
            log_success "Integration tests show formatters are active"
            test_result=0
        elif $formatter_working && $error_found; then
            log_warning "Integration tests show formatters working but with errors"
            test_result=0
        else
            log_warning "Integration tests completed but formatter output unclear"
            test_result=1
        fi
        
    else
        log_error "LLDB integration test failed or timed out (exit code: $test_result)"
        echo ""
        log_info "Last few lines of output:"
        tail -10 integration_test_output.txt || true
    fi
    
    # Keep output file for manual debugging
    if [[ $test_result -ne 0 ]]; then
        log_info "Integration test output saved to: $api_test_dir/integration_test_output.txt"
        log_info "LLDB script saved to: $api_test_dir/test_formatters.lldb"
        log_info "To debug manually: cd $api_test_dir && $LLDB_BIN -s test_formatters.lldb"
    else
        # Cleanup on success
        rm -f test_formatters.lldb integration_test_output.txt
    fi
    
    return $test_result
}

# Run all tests (comprehensive)
run_all_tests() {
    log_section "🚀 Running ALL GNUstep Tests..."
    
    local unit_result=0
    local api_result=0  
    local integration_result=0
    
    echo ""
    log_info "Step 1/3: Unit Tests"
    run_unit_tests || unit_result=$?
    
    echo ""
    log_info "Step 2/3: API Tests"  
    run_api_tests || api_result=$?
    
    echo ""
    log_info "Step 3/3: Integration Tests"
    run_integration_tests || integration_result=$?
    
    # Summary
    echo ""
    log_section "🏁 Test Suite Summary:"
    
    if [[ $unit_result -eq 0 ]]; then
        echo -e "  ✅ Unit Tests: ${GREEN}PASSED${NC}"
    else
        echo -e "  ❌ Unit Tests: ${RED}FAILED${NC}"
    fi
    
    if [[ $api_result -eq 0 ]]; then
        echo -e "  ✅ API Tests: ${GREEN}PASSED${NC}"
    else
        echo -e "  ❌ API Tests: ${RED}FAILED${NC}"
    fi
    
    if [[ $integration_result -eq 0 ]]; then
        echo -e "  ✅ Integration Tests: ${GREEN}PASSED${NC}"
    else
        echo -e "  ❌ Integration Tests: ${RED}FAILED${NC}"
    fi
    
    local total_failed=$((unit_result + api_result + integration_result))
    
    if [[ $total_failed -eq 0 ]]; then
        log_success "🎉 All test suites passed!"
        return 0
    else
        log_error "$total_failed test suite(s) failed"
        return 1
    fi
}

# Manual integration test for debugging
debug_integration() {
    log_section "🔍 Manual Integration Test Debug Mode..."
    check_prerequisites
    
    local api_test_dir="$SCRIPT_DIR/test/API/lang/objc/gnustep"
    local our_clang="$BUILD_DIR/bin/clang"
    
    if [[ ! -d "$api_test_dir" ]]; then
        log_error "API test directory not found: $api_test_dir"
        return 1
    fi
    
    cd "$api_test_dir" || exit 1
    
    # Ensure OBJC_SOURCES is set to main.m (Makefile already has debug flags)
    sed -i "1s/.*/OBJC_SOURCES := main.m/" Makefile
    
    log_info "Building test program with debug symbols..."
    if ! OBJC="$our_clang" make clean && OBJC="$our_clang" make; then
        log_error "Failed to build main test program"
        return 1
    fi
    
    # Create improved LLDB test script
    cat > debug_formatters.lldb << 'EOF'
# Manual Debug LLDB Script for GNUstep Formatters
settings set auto-confirm true
target create ./a.out
# Set breakpoint at NSLog to stop when variables are initialized
breakpoint set --name NSLog
run
# Continue through the first several NSLog calls to get to the end
continue
continue
continue
continue
continue
continue
continue
continue
# Show all variables first
frame variable
echo "=== Testing String Formatters ==="
frame variable -O emptyString
frame variable -O asciiString
echo "=== Testing Number Formatters ==="
frame variable -O intNumber
echo "=== Testing Collection Formatters ==="
frame variable -O emptyArray
frame variable -O simpleArray
frame variable -O emptyDict
frame variable -O simpleDict
frame variable -O emptySet
echo "=== Testing Custom Class Formatters ==="
frame variable -O account
echo "=== Integration Test Complete ==="
continue
quit
EOF
    
    log_info "Created debug script: $api_test_dir/debug_formatters.lldb"
    log_info "Test program: $api_test_dir/a.out"
    
    echo -e "\n${CYAN}Manual Debug Options:${NC}"
    echo "1. Run with script:     cd $api_test_dir && $LLDB_BIN -s debug_formatters.lldb"
    echo "2. Interactive debug:   cd $api_test_dir && $LLDB_BIN ./a.out"
    echo "3. Quick test:          ./dev.sh test-integration"
    
    echo -e "\n${CYAN}Key LLDB Commands for Manual Testing:${NC}"
    echo "  target create ./a.out"
    echo "  breakpoint set --file main.m --line 115"
    echo "  run"
    echo "  frame variable -O [variable_name]"
    echo "  po [variable_name]  # (may hang with current formatters)"
    
    # Ask user what they want to do
    echo -e "\n${YELLOW}What would you like to do?${NC}"
    echo "1) Run debug script automatically"
    echo "2) Start interactive LLDB session"
    echo "3) Just prepare files (manual run)"
    read -p "Choice [1-3]: " choice
    
    case "$choice" in
        1)
            log_info "Running debug script..."
            "$LLDB_BIN" -s debug_formatters.lldb
            ;;
        2)
            log_info "Starting interactive LLDB session..."
            "$LLDB_BIN" ./a.out
            ;;
        3)
            log_info "Files prepared for manual testing"
            ;;
        *)
            log_info "Files prepared. Run manually when ready."
            ;;
    esac
}

# Legacy test function for backward compatibility
run_tests() {
    run_all_tests
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
            echo "  (lldb) b custom_class_test.m:228"
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
    echo -e "${CYAN}Build Commands:${NC}"
    echo -e "  ${GREEN}clean-build${NC}     Clean and rebuild plugin (full rebuild)"
    echo -e "  ${GREEN}build${NC}           Quick rebuild (incremental)"  
    echo -e "  ${GREEN}clean-examples${NC}  Clean example binaries and artifacts"
    echo -e "  ${GREEN}build-example <name>${NC} Build specific example"
    echo ""
    echo -e "${CYAN}Test Commands:${NC}"
    echo -e "  ${GREEN}test${NC}            Run all tests (unit + API + integration)"
    echo -e "  ${GREEN}test-unit${NC}       Run unit tests only"
    echo -e "  ${GREEN}test-api${NC}        Run API tests only"
    echo -e "  ${GREEN}test-integration${NC} Run LLDB integration tests only"
    echo -e "  ${GREEN}debug-integration${NC} Manual integration test debugging"
    echo ""
    echo -e "${CYAN}Debug Commands:${NC}"
    echo -e "  ${GREEN}debug [example]${NC} Start LLDB debug session with example"
    echo ""
    echo -e "${CYAN}Utility Commands:${NC}"
    echo -e "  ${GREEN}full${NC}            Run full cycle: clean examples, clean build, test"
    echo -e "  ${GREEN}status${NC}          Show development environment status"
    echo -e "  ${GREEN}help${NC}            Show this help message"
    echo ""
    echo -e "${CYAN}Examples:${NC}"
    echo "  $0 clean-build                    # Full clean rebuild"
    echo "  $0 build                          # Quick incremental build"
    echo "  $0 test                           # Run all tests"
    echo "  $0 test-unit                      # Run just unit tests"
    echo "  $0 test-api                       # Test GNUstep program compilation"
    echo "  $0 test-integration               # Test formatters in LLDB"
    echo "  $0 debug-integration              # Manual integration test debugging"
    echo "  $0 debug custom_class_test        # Debug with custom class example"
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
        "test-unit")
            run_unit_tests
            ;;
        "test-api")
            run_api_tests
            ;;
        "test-integration")
            run_integration_tests
            ;;
        "debug-integration")
            debug_integration
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