#!/usr/bin/env python3
"""
Debug script to examine what modules are loaded and test GNUstep plugin
"""

import os
import sys
import subprocess

def run_lldb_command(exe_path, commands):
    """Run LLDB with a series of commands and capture output"""
    print(f"\n=== Running LLDB on {exe_path} ===")
    
    # Create command file
    cmd_file = "debug_commands.lldb"
    with open(cmd_file, 'w') as f:
        f.write('\n'.join(commands))
        f.write('\nquit\n')
    
    try:
        # Use the wrapper script to run LLDB
        result = subprocess.run([
            'bash', 'lldb_wrapper.sh',
            '-s', cmd_file,
            exe_path
        ], capture_output=True, text=True, timeout=30)
        
        print("STDOUT:")
        print(result.stdout)
        print("\nSTDERR:")
        print(result.stderr)
        
        return result.stdout, result.stderr
    finally:
        if os.path.exists(cmd_file):
            os.remove(cmd_file)

def main():
    # Commands to run
    debug_commands = [
        "target create simple_test",
        "image list",  # Show all loaded modules
        "breakpoint set --file simple_test.m --line 8",
        "run",
        "frame variable",
        "continue"
    ]
    
    # Check if executable exists
    if not os.path.exists("simple_test"):
        print("Building simple_test...")
        subprocess.run(["make", "simple_test"], check=True)
    
    # Run the debug session
    stdout, stderr = run_lldb_command("simple_test", debug_commands)

if __name__ == "__main__":
    main()
