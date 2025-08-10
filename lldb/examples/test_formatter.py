#!/usr/bin/env python3

import lldb

def test_formatter():
    """Test GNUstep formatter registration manually"""
    debugger = lldb.SBDebugger.Create()
    debugger.SetAsync(False)
    
    # Create target
    target = debugger.CreateTarget("custom_class_test")
    if not target.IsValid():
        print("Failed to create target")
        return
        
    # Set breakpoint
    breakpoint = target.BreakpointCreateByName("main", "custom_class_test")
    print(f"Breakpoint valid: {breakpoint.IsValid()}")
    
    # Launch process
    process = target.LaunchSimple(None, None, ".")
    if not process.IsValid():
        print("Failed to launch process")
        return
        
    print(f"Process state: {process.GetState()}")
    
    # Get category list
    categories = debugger.GetCategory("gnustep")
    if categories.IsValid():
        print("GNUstep category found!")
    else:
        print("GNUstep category NOT found")
    
    # Try to manually create and enable it
    debugger.HandleCommand("type category define gnustep")
    debugger.HandleCommand("type category enable gnustep")
    
    # Check again
    categories = debugger.GetCategory("gnustep")
    if categories.IsValid():
        print("GNUstep category created manually!")
    else:
        print("Still no GNUstep category")

if __name__ == "__main__":
    test_formatter()