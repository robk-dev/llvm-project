#!/bin/bash
#
# Build script for GSCInlineString validation test
#

# Set build variables
CC="/home/robk/code/llvm-project/build/bin/clang"
LLDB="/home/robk/code/llvm-project/build/bin/lldb" 
PROGRAM="test_gsc_inline_string_validation"

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

echo "=== Building GSCInlineString Validation Test ==="
echo "Compiler: $CC"
echo "Program: $PROGRAM.m -> $PROGRAM"

# Build the test program
if $CC $CFLAGS $LDFLAGS -o $PROGRAM $PROGRAM.m $LIBS; then
    echo "✅ Build successful: $PROGRAM"
    echo ""
    echo "To run the test:"
    echo "1. Execute: $LLDB $PROGRAM"
    echo "2. Or run validation script: $LLDB -s validate_gsc_inline_string.lldb"
    echo ""
    echo "Manual testing commands:"
    echo "  $LLDB $PROGRAM"
    echo "  (lldb) b test_gsc_inline_string_validation.m:89"
    echo "  (lldb) run"  
    echo "  (lldb) po shortString  # Test summary"
    echo "  (lldb) p shortString   # Test synthetic children"
else
    echo "❌ Build failed"
    exit 1
fi