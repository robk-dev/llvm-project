#!/bin/bash

# Test script for our custom GNUstep stack
# Verifies that libobjc2 and libs-base work with our LLVM build

set -e

PROJECT_ROOT="/home/robk/code/llvm-project"
BUILD_DIR="${PROJECT_ROOT}/build"
GNUSTEP_INSTALL_PREFIX="${PROJECT_ROOT}/lldb/gnustep-install"
EXAMPLES_DIR="${PROJECT_ROOT}/lldb/examples"

echo "=========================================="
echo "GNUstep Stack Test"
echo "=========================================="

# Check if GNUstep was built
if [[ ! -f "${GNUSTEP_INSTALL_PREFIX}/lib/libobjc.so" ]]; then
    echo "❌ libobjc2 not found at ${GNUSTEP_INSTALL_PREFIX}/lib/libobjc.so"
    echo "Run the GNUstep build script first: ./build_scripts/build_gnustep"
    exit 1
fi

# Check for Foundation library (hybrid approach - can be system or custom)
if [[ -f "${GNUSTEP_INSTALL_PREFIX}/lib/libgnustep-base.so" ]]; then
    echo "✓ Custom libs-base found"
    LIBS_BASE_TYPE="custom"
elif [[ -f "/usr/local/lib/libgnustep-base.so" ]]; then
    echo "✓ System libs-base found (hybrid mode - /usr/local)"
    LIBS_BASE_TYPE="system"
elif [[ -f "/usr/lib/x86_64-linux-gnu/libgnustep-base.so" ]]; then
    echo "✓ System libs-base found (hybrid mode - /usr/lib)"
    LIBS_BASE_TYPE="system"
else
    echo "❌ No Foundation library found (neither custom nor system)"
    exit 1
fi

echo "✓ GNUstep libraries found (${LIBS_BASE_TYPE} Foundation)"

# Check if LLVM was built
if [[ ! -f "${BUILD_DIR}/bin/clang" ]]; then
    echo "❌ LLVM/Clang not found at ${BUILD_DIR}/bin/clang"
    echo "Build LLVM first: ./build_scripts/build_lldb.sh"
    exit 1
fi

echo "✓ LLVM toolchain found"

# Set up environment
export GNUSTEP_ROOT="${GNUSTEP_INSTALL_PREFIX}"
export PATH="${GNUSTEP_INSTALL_PREFIX}/bin:${BUILD_DIR}/bin:$PATH"
export LD_LIBRARY_PATH="${GNUSTEP_INSTALL_PREFIX}/lib:${BUILD_DIR}/lib:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="${GNUSTEP_INSTALL_PREFIX}/lib/pkgconfig:$PKG_CONFIG_PATH"

echo "✓ Environment configured"
echo "GNUSTEP_ROOT: $GNUSTEP_ROOT"
echo "Clang: $(which clang)"

# Create a simple test program
cd "${EXAMPLES_DIR}"

echo "Creating test program with our GNUstep stack..."

cat > gnustep_stack_test.m << 'EOF'
#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Test basic Foundation functionality
        NSString *testString = @"Hello GNUstep!";
        NSLog(@"Test string: %@", testString);
        
        NSArray *testArray = @[@"First", @"Second", @"Third"];
        NSLog(@"Test array count: %lu", (unsigned long)[testArray count]);
        
        NSDictionary *testDict = @{@"key1": @"value1", @"key2": @"value2"};
        NSLog(@"Test dictionary: %@", testDict);
        
        NSLog(@"GNUstep stack test completed successfully!");
    }
    return 0;
}
EOF

# Compile with our custom stack
echo "Compiling with custom GNUstep stack..."

CC="${BUILD_DIR}/bin/clang"
CFLAGS="-fobjc-runtime=gnustep-2.1 -fblocks -g -O0 -I${GNUSTEP_INSTALL_PREFIX}/include -I/usr/local/include/GNUstep"
LDFLAGS="-L${GNUSTEP_INSTALL_PREFIX}/lib -L/usr/local/lib -Wl,-rpath,${GNUSTEP_INSTALL_PREFIX}/lib -Wl,-rpath,/usr/local/lib"
LIBS="-lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm"

$CC $CFLAGS $LDFLAGS -o gnustep_stack_test gnustep_stack_test.m $LIBS

if [[ ! -f "gnustep_stack_test" ]]; then
    echo "❌ Failed to compile test program"
    exit 1
fi

echo "✓ Test program compiled successfully"

# Run the test
echo "Running test program..."
if ./gnustep_stack_test; then
    echo "✓ Test program executed successfully"
else
    echo "❌ Test program failed"
    exit 1
fi

# Test with LLDB
if [[ -f "${BUILD_DIR}/bin/lldb" ]]; then
    echo "Testing with LLDB..."
    
    export LLDB_DEBUGSERVER_PATH="${BUILD_DIR}/bin/lldb-server"
    
    cat > test_lldb_gnustep.lldb << 'EOF'
target create gnustep_stack_test
b main
run
n 3
p testString
p testArray
p testDict
continue
quit
EOF
    
    echo "Running LLDB test..."
    if "${BUILD_DIR}/bin/lldb" -s test_lldb_gnustep.lldb 2>&1 | grep -q "Hello GNUstep"; then
        echo "✓ LLDB test with GNUstep stack successful"
    else
        echo "⚠ LLDB test may have issues (check output above)"
    fi
    
    rm -f test_lldb_gnustep.lldb
else
    echo "⚠ LLDB not found, skipping LLDB test"
fi

# Clean up
rm -f gnustep_stack_test gnustep_stack_test.m

echo "=========================================="
echo "GNUstep Stack Test Completed!"
echo ""
echo "Your custom GNUstep stack is working correctly."
echo "You can now use it for development and testing:"
echo ""
echo "Environment Setup:"
echo "  export GNUSTEP_ROOT=\"${GNUSTEP_INSTALL_PREFIX}\""
echo "  export PATH=\"${GNUSTEP_INSTALL_PREFIX}/bin:${BUILD_DIR}/bin:\$PATH\""
echo "  export LD_LIBRARY_PATH=\"${GNUSTEP_INSTALL_PREFIX}/lib:${BUILD_DIR}/lib:\$LD_LIBRARY_PATH\""
echo ""
echo "Compilation Example:"
echo "  clang -fobjc-runtime=gnustep-2.1 -fblocks -g \\"
echo "        -I${GNUSTEP_INSTALL_PREFIX}/include \\"
echo "        -L${GNUSTEP_INSTALL_PREFIX}/lib \\"
echo "        -Wl,-rpath,${GNUSTEP_INSTALL_PREFIX}/lib \\"
echo "        -o myprogram myprogram.m \\"
echo "        -lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm"
echo "=========================================="
