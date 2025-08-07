#!/bin/bash
# Demo script showing different ways to configure LLDB MCP server

echo "=== LLDB MCP Server Configuration Demo ==="
echo

# Method 1: Using CLI argument
echo "1. Using CLI argument:"
echo "   python3 lldb_mcp.py --lldb-path /home/robk/llvm-build/build/bin/lldb --debug"
echo

# Method 2: Using environment variable
echo "2. Using environment variable:"
echo "   export LLDB_EXECUTABLE=/home/robk/llvm-build/build/bin/lldb"
echo "   python3 lldb_mcp.py --debug"
echo

# Method 3: Using both (CLI takes precedence)
echo "3. CLI argument overrides environment variable:"
echo "   export LLDB_EXECUTABLE=/env/path/lldb"
echo "   python3 lldb_mcp.py --lldb-path /cli/path/lldb --debug"
echo

# Method 4: Default system LLDB
echo "4. Using system default LLDB:"
echo "   python3 lldb_mcp.py --debug"
echo

echo "=== Priority order ==="
echo "1. --lldb-path CLI argument (highest priority)"
echo "2. LLDB_EXECUTABLE environment variable"
echo "3. System 'lldb' command (lowest priority)"
echo

echo "=== Testing with your VS Code configuration ==="
echo "Based on your VS Code settings.json, you're using:"
echo "   /home/robk/llvm-build/build/bin/lldb"
echo
echo "To use the same LLDB with MCP server:"
echo "   python3 lldb_mcp.py --lldb-path /home/robk/llvm-build/build/bin/lldb"
echo "or:"
echo "   export LLDB_EXECUTABLE=/home/robk/llvm-build/build/bin/lldb"
echo "   python3 lldb_mcp.py"
