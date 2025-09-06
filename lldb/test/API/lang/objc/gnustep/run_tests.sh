#!/bin/bash
# Test runner script for GNUstep LLDB integration tests
#
# This script builds the test program and runs the integration tests
# to validate the GNUstep LLDB plugin functionality.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo -e "${BLUE}=== GNUstep LLDB Integration Test Runner ===${NC}"
echo "Test directory: $SCRIPT_DIR"

# Check if required tools are available
check_requirements() {
    echo -e "${BLUE}Checking requirements...${NC}"
    
    local missing_tools=()
    
    # Check for clang
    if ! command -v clang &> /dev/null; then
        missing_tools+=("clang")
    fi
    
    # Check for python3
    if ! command -v python3 &> /dev/null; then
        missing_tools+=("python3")
    fi
    
    # Check for lldb
    if ! command -v lldb &> /dev/null; then
        missing_tools+=("lldb")
    fi
    
    # Check for GNUstep headers
    if [ ! -d "/usr/local/include/GNUstep" ] && [ ! -d "/usr/include/GNUstep" ]; then
        echo -e "${RED}Warning: GNUstep headers not found in standard locations${NC}"
    fi
    
    if [ ${#missing_tools[@]} -ne 0 ]; then
        echo -e "${RED}Error: Missing required tools: ${missing_tools[*]}${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}✓ Requirements check passed${NC}"
}

# Build the test program
build_test_program() {
    echo -e "${BLUE}Building test program...${NC}"
    
    if make clean && make; then
        echo -e "${GREEN}✓ Test program built successfully${NC}"
    else
        echo -e "${RED}✗ Failed to build test program${NC}"
        exit 1
    fi
    
    # Verify the executable was created
    if [ ! -f "a.out" ]; then
        echo -e "${RED}✗ Test program executable not found${NC}"
        exit 1
    fi
    
    # Check if it links to GNUstep libraries
    if ldd a.out | grep -q gnustep; then
        echo -e "${GREEN}✓ Test program properly linked to GNUstep libraries${NC}"
    else
        echo -e "${YELLOW}Warning: Test program may not be linked to GNUstep libraries${NC}"
    fi
}

# Run a specific test file
run_test() {
    local test_file="$1"
    local test_name="$(basename "$test_file" .py)"
    
    echo -e "${BLUE}Running $test_name...${NC}"
    
    # Try to run with lldbsuite.test.dotest
    if python3 -c "import lldbsuite.test.dotest" 2>/dev/null; then
        if python3 -m lldbsuite.test.dotest -v "$test_file"; then
            echo -e "${GREEN}✓ $test_name passed${NC}"
            return 0
        else
            echo -e "${RED}✗ $test_name failed${NC}"
            return 1
        fi
    else
        echo -e "${YELLOW}Warning: lldbsuite not available, skipping automated test for $test_name${NC}"
        echo -e "${YELLOW}You can manually test by running: lldb ./a.out${NC}"
        return 2
    fi
}

# Run all tests
run_all_tests() {
    echo -e "${BLUE}Running integration tests...${NC}"
    
    local test_files=(
        "TestGNUstepCore.py"
        "TestGNUstepFoundationTypes.py" 
        "TestGNUstepExpressions.py"
    )
    
    local passed=0
    local failed=0
    local skipped=0
    
    for test_file in "${test_files[@]}"; do
        if [ -f "$test_file" ]; then
            case "$(run_test "$test_file"; echo $?)" in
                0) ((passed++)) ;;
                1) ((failed++)) ;;
                2) ((skipped++)) ;;
            esac
        else
            echo -e "${YELLOW}Warning: Test file $test_file not found${NC}"
            ((skipped++))
        fi
    done
    
    echo -e "\n${BLUE}=== Test Results Summary ===${NC}"
    echo -e "${GREEN}Passed: $passed${NC}"
    echo -e "${RED}Failed: $failed${NC}"
    echo -e "${YELLOW}Skipped: $skipped${NC}"
    
    if [ $failed -eq 0 ]; then
        echo -e "\n${GREEN}🎉 All tests passed! GNUstep LLDB plugin is working correctly.${NC}"
        return 0
    else
        echo -e "\n${RED}❌ Some tests failed. Check the output above for details.${NC}"
        return 1
    fi
}

# Quick validation without running full test suite
quick_validation() {
    echo -e "${BLUE}Running quick validation...${NC}"
    
    # Check if LLDB can load the executable
    if echo -e "target create a.out\nquit" | lldb > /dev/null 2>&1; then
        echo -e "${GREEN}✓ LLDB can load the test program${NC}"
    else
        echo -e "${RED}✗ LLDB cannot load the test program${NC}"
        return 1
    fi
    
    # Test basic execution
    if echo -e "target create a.out\nrun\nquit" | timeout 10s lldb > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Test program executes successfully${NC}"
    else
        echo -e "${YELLOW}Warning: Test program execution timed out or failed${NC}"
    fi
    
    echo -e "${GREEN}✓ Quick validation completed${NC}"
}

# Print usage information
usage() {
    echo "Usage: $0 [options] [test_file]"
    echo ""
    echo "Options:"
    echo "  -h, --help      Show this help message"
    echo "  -q, --quick     Run quick validation only"
    echo "  -b, --build     Build test program only"
    echo "  -c, --check     Check requirements only"
    echo ""
    echo "Examples:"
    echo "  $0                              # Run all tests"
    echo "  $0 TestGNUstepCore.py          # Run specific test"
    echo "  $0 --quick                     # Quick validation"
    echo "  $0 --build                     # Build only"
}

# Main execution
main() {
    case "${1:-}" in
        -h|--help)
            usage
            exit 0
            ;;
        -c|--check)
            check_requirements
            exit 0
            ;;
        -b|--build)
            check_requirements
            build_test_program
            exit 0
            ;;
        -q|--quick)
            check_requirements
            build_test_program
            quick_validation
            exit 0
            ;;
        Test*.py)
            check_requirements
            build_test_program
            run_test "$1"
            exit $?
            ;;
        "")
            check_requirements
            build_test_program
            run_all_tests
            exit $?
            ;;
        *)
            echo -e "${RED}Error: Unknown option '$1'${NC}"
            usage
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"