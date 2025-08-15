#!/bin/bash
# lldb_test.sh - Clean LLDB testing script to avoid terminal corruption
# Usage: ./lldb_test.sh

set -e

echo "🧪 Running systematic LLDB formatter tests..."

# Source the environment setup
if ! source /c/code/llvm-project/debug_setup.sh; then
    echo "❌ Failed to source debug environment"
    exit 1
fi

# Compile fresh
if ! compile_example simple_test; then
    echo "❌ Failed to compile simple_test"
    exit 1  
fi

# Verify LLDB exists
if [[ ! -f "$LLDB_BIN" ]]; then
    echo "❌ LLDB binary not found: $LLDB_BIN"
    echo "Run rebuild_lldb first"
    exit 1
fi

# Create LLDB command file to avoid terminal issues
cat > /tmp/lldb_test_commands.txt << 'EOF'
target create C:/code/llvm-project/lldb/examples/simple_test.exe
b main
run
br set -l 10
continue
frame variable fruits
print fruits
print [fruits count]  
print [fruits class]
expression -O -- fruits
print [fruits objectAtIndex:0]
print [fruits objectAtIndex:1]
quit
EOF

echo "🔍 Running LLDB with clean command file..."
cd "$BUILD_DIR" || exit 1

if "$LLDB_BIN" --source /tmp/lldb_test_commands.txt; then
    echo "✅ Test complete. Check output above for formatter results."
else
    echo "❌ LLDB test failed"
    exit 1
fi
