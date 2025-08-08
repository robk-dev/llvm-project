#!/usr/bin/env python3
"""Test script to verify NSArray elements display correctly in LLDB."""

import lldb
import os
import sys

def test_array_display():
    # Create a new debugger instance
    debugger = lldb.SBDebugger.Create()
    debugger.SetAsync(False)
    
    # Create a target from the array_test executable
    target = debugger.CreateTarget("array_test")
    if not target:
        print("ERROR: Failed to create target")
        return False
    
    # Set a breakpoint at line 48 (after string array creation)
    bp = target.BreakpointCreateByLocation("array_test.m", 48)
    if not bp.IsValid():
        print("ERROR: Failed to create breakpoint")
        return False
    
    # Launch the process
    process = target.LaunchSimple(None, None, os.getcwd())
    if not process:
        print("ERROR: Failed to launch process")
        return False
    
    # Get the first thread
    thread = process.GetThreadAtIndex(0)
    if not thread:
        print("ERROR: Failed to get thread")
        return False
    
    # Get the frame
    frame = thread.GetFrameAtIndex(0)
    if not frame:
        print("ERROR: Failed to get frame")
        return False
    
    # Get the stringArray variable
    string_array = frame.FindVariable("stringArray")
    if not string_array.IsValid():
        print("ERROR: Failed to find stringArray variable")
        return False
    
    print(f"stringArray: {string_array.GetSummary()}")
    
    # Try to access children
    num_children = string_array.GetNumChildren()
    print(f"Number of children: {num_children}")
    
    success = True
    for i in range(min(5, num_children)):
        child = string_array.GetChildAtIndex(i)
        if child.IsValid():
            value = child.GetSummary() or child.GetValue() or str(child.GetValueAsUnsigned())
            print(f"  [{i}] = {value}")
            # Check if we got "read memory failed" error
            if "read memory" in str(value).lower() and "failed" in str(value).lower():
                print(f"    ERROR: Got 'read memory failed' for element {i}")
                success = False
        else:
            print(f"  [{i}] = <invalid>")
            success = False
    
    # Continue to next breakpoint (line 57 - number array)
    process.Continue()
    
    # Test number array
    number_array = frame.FindVariable("numberArray")
    if number_array.IsValid():
        print(f"\nnumberArray: {number_array.GetSummary()}")
        num_children = number_array.GetNumChildren()
        print(f"Number of children: {num_children}")
        
        for i in range(min(4, num_children)):
            child = number_array.GetChildAtIndex(i)
            if child.IsValid():
                value = child.GetSummary() or child.GetValue() or str(child.GetValueAsUnsigned())
                print(f"  [{i}] = {value}")
                if "read memory" in str(value).lower() and "failed" in str(value).lower():
                    print(f"    ERROR: Got 'read memory failed' for element {i}")
                    success = False
    
    # Kill the process
    process.Kill()
    
    # Cleanup
    lldb.SBDebugger.Destroy(debugger)
    
    return success

if __name__ == "__main__":
    # Change to the examples directory
    os.chdir("/home/robk/code/llvm-project/lldb/examples")
    
    success = test_array_display()
    if success:
        print("\nSUCCESS: Array elements displayed correctly!")
        sys.exit(0)
    else:
        print("\nFAILURE: Some array elements showed 'read memory failed'")
        sys.exit(1)