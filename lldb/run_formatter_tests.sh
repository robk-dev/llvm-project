#!/bin/bash
#
# Simple runner script for GNUstep LLDB formatter tests
#
# Usage:
#   ./run_formatter_tests.sh              # Run all tests
#   ./run_formatter_tests.sh -v           # Run with verbose output
#   ./run_formatter_tests.sh NSString     # Run only NSString tests
#   ./run_formatter_tests.sh -v NSNumber  # Run NSNumber tests with verbose output

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-/home/robk/code/llvm-project/build}"
EXAMPLES_DIR="${EXAMPLES_DIR:-/home/robk/code/llvm-project/lldb/examples}"
TEST_SCRIPT="${SCRIPT_DIR}/test_formatters.py"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse arguments
VERBOSE=""
FILTER=""
JSON_OUTPUT=""
HELP=0

while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose)
            VERBOSE="--verbose"
            shift
            ;;
        -h|--help)
            HELP=1
            shift
            ;;
        --json)
            JSON_OUTPUT="--json-output ${2:-test_results.json}"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --examples-dir)
            EXAMPLES_DIR="$2"
            shift 2
            ;;
        *)
            FILTER="--filter $1"
            shift
            ;;
    esac
done

# Show help if requested
if [ $HELP -eq 1 ]; then
    echo "Usage: $0 [OPTIONS] [FILTER]"
    echo ""
    echo "Run GNUstep LLDB formatter tests"
    echo ""
    echo "Options:"
    echo "  -v, --verbose       Enable verbose output"
    echo "  -h, --help          Show this help message"
    echo "  --json [FILE]       Output results to JSON file (default: test_results.json)"
    echo "  --build-dir DIR     LLVM build directory (default: $BUILD_DIR)"
    echo "  --examples-dir DIR  Examples directory (default: $EXAMPLES_DIR)"
    echo ""
    echo "Arguments:"
    echo "  FILTER             Filter tests by formatter name (e.g., NSString, NSNumber)"
    echo ""
    echo "Examples:"
    echo "  $0                  # Run all tests"
    echo "  $0 -v               # Run all tests with verbose output"
    echo "  $0 NSString         # Run only NSString formatter tests"
    echo "  $0 -v NSArray       # Run NSArray tests with verbose output"
    echo "  $0 --json           # Run tests and save results to JSON"
    exit 0
fi

# Check prerequisites
echo -e "${BLUE}=== GNUstep LLDB Formatter Test Runner ===${NC}"
echo ""

# Check Python 3
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}Error: Python 3 is required but not found${NC}"
    exit 1
fi

# Check build directory
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found: $BUILD_DIR${NC}"
    echo "Please set BUILD_DIR environment variable or use --build-dir"
    exit 1
fi

# Check LLDB binary
LLDB_BIN="$BUILD_DIR/bin/lldb"
if [ ! -x "$LLDB_BIN" ]; then
    echo -e "${RED}Error: LLDB not found at: $LLDB_BIN${NC}"
    echo "Please build LLDB first with: cd $BUILD_DIR && ninja lldb"
    exit 1
fi

# Check lldb-server binary
LLDB_SERVER="$BUILD_DIR/bin/lldb-server"
if [ ! -x "$LLDB_SERVER" ]; then
    echo -e "${YELLOW}Warning: lldb-server not found at: $LLDB_SERVER${NC}"
    echo "Building lldb-server..."
    (cd "$BUILD_DIR" && ninja lldb-server)
fi

# Check examples directory
if [ ! -d "$EXAMPLES_DIR" ]; then
    echo -e "${RED}Error: Examples directory not found: $EXAMPLES_DIR${NC}"
    exit 1
fi

# Check test script
if [ ! -f "$TEST_SCRIPT" ]; then
    echo -e "${RED}Error: Test script not found: $TEST_SCRIPT${NC}"
    exit 1
fi

# Set up environment
export PATH="$BUILD_DIR/bin:$PATH"
export LLDB_DEBUGSERVER_PATH="$LLDB_SERVER"

# Display configuration
echo -e "${GREEN}Configuration:${NC}"
echo "  Build dir:    $BUILD_DIR"
echo "  Examples dir: $EXAMPLES_DIR"
echo "  LLDB:         $LLDB_BIN"
echo "  LLDB Server:  $LLDB_SERVER"
if [ -n "$FILTER" ]; then
    echo "  Filter:       ${FILTER#--filter }"
fi
if [ -n "$VERBOSE" ]; then
    echo "  Mode:         Verbose"
fi
echo ""

# Run the tests
echo -e "${BLUE}Running tests...${NC}"
echo ""

python3 "$TEST_SCRIPT" \
    --build-dir "$BUILD_DIR" \
    --examples-dir "$EXAMPLES_DIR" \
    $VERBOSE \
    $FILTER \
    $JSON_OUTPUT

# Capture exit code
EXIT_CODE=$?

# Show result
echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
else
    echo -e "${RED}✗ Some tests failed${NC}"
fi

exit $EXIT_CODE