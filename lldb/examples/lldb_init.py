import sys
import os

# Add the correct Python paths
sys.path.insert(0, r'C:\tools\msys64\ucrt64\lib\python3.12')
sys.path.insert(0, r'C:\tools\msys64\ucrt64\lib\python3.12\lib-dynload')
sys.path.insert(0, r'C:\tools\msys64\ucrt64\lib\python3.12\site-packages')

# Try to import uuid to verify it works
try:
    import uuid
    print("✅ Python modules initialized successfully")
except ImportError as e:
    print(f"❌ Still missing modules: {e}")

# Define run_one_line function if it's missing
if 'run_one_line' not in globals():
    def run_one_line(cmd, verbose=False):
        """Simple run_one_line implementation for LLDB"""
        import lldb
        result = lldb.SBCommandReturnObject()
        lldb.debugger.GetCommandInterpreter().HandleCommand(cmd, result)
        if verbose and result.GetOutput():
            print(result.GetOutput())
        return result.Succeeded()
    
    globals()['run_one_line'] = run_one_line
    print("✅ run_one_line function defined")
