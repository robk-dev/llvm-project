#!/usr/bin/env python3
"""
Simple test script to verify LLDB MCP configuration functionality
"""
import sys
import os

# Add the current directory to Python path to import lldb_mcp
sys.path.insert(0, os.path.dirname(__file__))

def test_config():
    """Test the configuration functionality"""
    # Import after setting up path
    import lldb_mcp
    
    print("Testing default configuration...")
    print(f"Default LLDB path: {lldb_mcp.DEFAULT_LLDB_PATH}")
    
    # Test setting path
    print("\nTesting set_lldb_path function...")
    lldb_mcp.set_lldb_path("/custom/lldb/path")
    print(f"After setting custom path: {lldb_mcp.DEFAULT_LLDB_PATH}")
    
    # Test environment variable
    print("\nTesting environment variable...")
    os.environ["LLDB_EXECUTABLE"] = "/env/lldb/path"
    print(f"Environment variable: {os.getenv('LLDB_EXECUTABLE')}")
    
    print("\nConfiguration test completed successfully!")

if __name__ == "__main__":
    test_config()
