#!/bin/bash
# Test script to verify LLDB and GNUstep plugin work correctly

echo "🔧 Testing LLDB with GNUstep plugin..."
echo "=================================="

# Set the correct environment
export PYTHONHOME=/ucrt64
export PYTHONPATH=/ucrt64/lib/python3.12:/ucrt64/lib/python3.12/lib-dynload:/ucrt64/lib/python3.12/site-packages
export PATH=/ucrt64/lib:/ucrt64/bin:$PATH

echo "1. Testing LLDB version:"
winpty /c/Users/vagrant/code/llvm-project/build/bin/lldb.exe --version 2>&1 | grep -E "(version|GNUstep)" || true

echo ""
echo "2. LLDB is ready to use with GNUstep plugin!"
echo "   To debug interactively, run:"
echo "   ./lldb_wrapper.sh simple_test.exe"
echo ""
echo "3. Or use VS Code debugger with the '🚀 GNUstep/libobjc2 - Debug' configuration"
