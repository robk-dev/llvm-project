#!/bin/bash
#===-- build_test_programs.sh - Build all GNUstep test programs ---------===//
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===//
#
# This script builds all GNUstep test programs used for testing the LLDB
# GNUstep plugin.
#
#===----------------------------------------------------------------------===//

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
TEST_DIR="$PROJECT_ROOT/lldb/examples"
API_TEST_DIR="$PROJECT_ROOT/lldb/test/API/lang/objc/gnustep"

# Compiler settings
CC="${CC:-$BUILD_DIR/bin/clang}"
LLDB="${LLDB:-$BUILD_DIR/bin/lldb}"

# GNUstep compilation flags
CFLAGS="-fobjc-runtime=gnustep-2.1 \
        -fblocks \
        -fno-strict-aliasing \
        -fexceptions \
        -fobjc-exceptions \
        -g -gdwarf-5 -O0 \
        -fno-omit-frame-pointer \
        -I/usr/local/include/GNUstep \
        -I/usr/include/GNUstep \
        -fconstant-string-class=NSConstantString \
        -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -DDEBUG=1"

LDFLAGS="-L/usr/local/lib -Wl,-rpath,/usr/local/lib -g"
LIBS="-lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm"

echo "=========================================="
echo "Building GNUstep Test Programs"
echo "=========================================="
echo ""

# Check prerequisites
echo "Checking prerequisites..."

if [ ! -f "$CC" ]; then
    echo -e "${YELLOW}Warning: Clang not found at $CC, using system clang${NC}"
    CC="clang"
fi

if ! command -v "$CC" &> /dev/null; then
    echo -e "${RED}Error: No clang compiler found${NC}"
    exit 1
fi

if [ ! -d "/usr/local/lib" ] || [ ! -f "/usr/local/lib/libobjc.so" ]; then
    echo -e "${YELLOW}Warning: GNUstep runtime not found in /usr/local/lib${NC}"
    echo "Build may fail. Install GNUstep runtime first."
fi

echo -e "${GREEN}Prerequisites OK${NC}"
echo "Using compiler: $CC"
echo ""

# Function to build a program
build_program() {
    local source="$1"
    local output="$2"
    local extra_flags="${3:-}"
    
    local name=$(basename "$output")
    echo -n "Building $name... "
    
    if $CC $CFLAGS $LDFLAGS $extra_flags -o "$output" "$source" $LIBS > /tmp/build_$name.log 2>&1; then
        echo -e "${GREEN}OK${NC}"
        return 0
    else
        echo -e "${RED}FAILED${NC}"
        echo "  Error output:"
        tail -10 /tmp/build_$name.log | sed 's/^/    /'
        return 1
    fi
}

# Build test programs in examples directory
echo "Building programs in $TEST_DIR..."
cd "$TEST_DIR"

# Core test programs
build_program "custom_class_test.m" "custom_class_test" || true
build_program "test_dictionary_display.m" "test_dictionary_display" || true
build_program "test_set_display.m" "test_set_display" || true
build_program "test_mutable_set.m" "test_mutable_set" || true
build_program "test_nsmutableset_simple.m" "test_nsmutableset_simple" || true
build_program "test_function_calling.m" "test_function_calling" || true
build_program "test_validation.m" "test_validation" || true
build_program "test_complete.m" "test_complete" || true
build_program "formatter_integration_test.m" "formatter_integration_test" || true

# Additional test programs
if [ -f "test_nsnumber.m" ]; then
    build_program "test_nsnumber.m" "test_nsnumber" || true
fi

if [ -f "test_constant_string.m" ]; then
    build_program "test_constant_string.m" "test_constant_string" || true
fi

if [ -f "test_dict_numbers.m" ]; then
    build_program "test_dict_numbers.m" "test_dict_numbers" || true
fi

echo ""

# Build API test program if it exists
if [ -d "$API_TEST_DIR" ] && [ -f "$API_TEST_DIR/main.m" ]; then
    echo "Building API test program..."
    cd "$API_TEST_DIR"
    
    build_program "main.m" "a.out" || true
    echo ""
fi

# Create comprehensive test program if it doesn't exist
COMPREHENSIVE_TEST="$TEST_DIR/test_all_formatters.m"
if [ ! -f "$COMPREHENSIVE_TEST" ]; then
    echo "Creating comprehensive test program..."
    cat > "$COMPREHENSIVE_TEST" << 'EOF'
#import <Foundation/Foundation.h>

// Test all GNUstep formatters comprehensively
int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Test strings
    NSString *strings[] = {
        @"",                          // Empty
        @"Hello",                     // ASCII
        @"Unicode: 你好 👋",          // UTF-8 with emoji
        @"Hi",                        // Tagged
        nil
    };
    
    // Test numbers
    NSNumber *numbers[] = {
        [NSNumber numberWithInt:0],
        [NSNumber numberWithInt:42],
        [NSNumber numberWithInt:-17],
        [NSNumber numberWithFloat:3.14159f],
        [NSNumber numberWithDouble:2.71828],
        [NSNumber numberWithBool:YES],
        [NSNumber numberWithBool:NO],
        nil
    };
    
    // Test arrays
    NSArray *arrays[] = {
        [NSArray array],
        @[@"One"],
        @[@"A", @"B", @"C"],
        @[@1, @2, @3, @4, @5],
        nil
    };
    
    // Test dictionaries
    NSDictionary *dicts[] = {
        [NSDictionary dictionary],
        @{@"key": @"value"},
        @{@"name": @"John", @"age": @30, @"city": @"NYC"},
        nil
    };
    
    // Test sets
    NSSet *sets[] = {
        [NSSet set],
        [NSSet setWithObject:@"Single"],
        [NSSet setWithObjects:@"Red", @"Green", @"Blue", nil],
        nil
    };
    
    // Large collections for performance
    NSMutableArray *largeArray = [NSMutableArray array];
    for (int i = 0; i < 10000; i++) {
        [largeArray addObject:@(i)];
    }
    
    NSMutableDictionary *largeDict = [NSMutableDictionary dictionary];
    for (int i = 0; i < 5000; i++) {
        NSString *key = [NSString stringWithFormat:@"k%d", i];
        [largeDict setObject:@(i) forKey:key];
    }
    
    // Print marker for debugger
    NSLog(@"All test objects created. Set breakpoint here.");
    
    // Keep objects alive
    NSLog(@"Strings: %p", strings);
    NSLog(@"Numbers: %p", numbers);
    NSLog(@"Arrays: %p", arrays);
    NSLog(@"Dicts: %p", dicts);
    NSLog(@"Sets: %p", sets);
    NSLog(@"Large: %lu, %lu", [largeArray count], [largeDict count]);
    
    [pool drain];
    return 0;
}
EOF
    
    build_program "$COMPREHENSIVE_TEST" "$TEST_DIR/test_all_formatters" || true
    echo ""
fi

# Summary
echo "=========================================="
echo "Build Summary"
echo "=========================================="

SUCCESS_COUNT=0
FAIL_COUNT=0

for prog in "$TEST_DIR"/*; do
    if [ -x "$prog" ] && [ ! -d "$prog" ] && [[ ! "$prog" == *.* ]]; then
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    fi
done

echo -e "${GREEN}Successfully built: $SUCCESS_COUNT programs${NC}"

if [ $FAIL_COUNT -gt 0 ]; then
    echo -e "${RED}Failed to build: $FAIL_COUNT programs${NC}"
fi

# List built programs
echo ""
echo "Available test programs:"
for prog in "$TEST_DIR"/*; do
    if [ -x "$prog" ] && [ ! -d "$prog" ] && [[ ! "$prog" == *.* ]]; then
        echo "  - $(basename $prog)"
    fi
done

echo ""
echo "To run tests, use: $SCRIPT_DIR/run_gnustep_tests.sh"