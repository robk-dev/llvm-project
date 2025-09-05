# GNUstep Objective-C Runtime V2 for LLDB

This is a new, modular implementation of GNUstep Objective-C runtime support for LLDB, designed from the ground up to be clean, maintainable, and robust.

## Architecture

The implementation follows a clean separation of concerns inspired by the Apple Objective-C runtime plugin:

### Core Components

1. **`GNUstepObjCRuntimeV2`** - Main plugin class
   - Inherits from `lldb_private::LanguageRuntime`
   - Handles plugin lifecycle and runtime detection
   - Provides object description capabilities (`po` command support)

2. **`GNUstepObjCRuntimeV2Introspector`** - Runtime introspection engine
   - Reads and parses libobjc2 data structures from target process memory
   - Provides class name resolution from ISA pointers
   - Includes class lookup by name functionality
   - Validates GNUstep runtime presence

3. **`GNUstepObjCDeclVendor`** - AST synthesis (ExternalASTSource equivalent)
   - Creates Clang AST nodes for Objective-C classes when debug info is missing
   - Enables expression evaluation on objects without full debug symbols
   - (Currently stub implementation - to be completed)

## Features Implemented

### ✅ Phase 1: Basic Object Identification
- Runtime detection (only activates for GNUstep processes)
- Class name resolution from ISA pointers
- Basic object description for `po` command
- Correct parsing of libobjc2 `struct objc_class` layout

### 🔄 In Progress
- Complete declaration vendor implementation
- GNUstep-specific data formatters
- Trampoline handling for seamless stepping
- Enhanced runtime function calling

## Test Infrastructure

### Example Program
- `examples/libobjc2/test_gnustep.m` - Comprehensive test program
- Tests various GNUstep objects (NSString, NSArray, NSDictionary, custom classes)
- Includes breakpoint locations for debugging validation

### Build System
- `Makefile` with proper GNUstep compilation flags
- Supports both GNUstep makefiles and standalone compilation
- Uses `-fconstant-string-class=NSConstantString` as requested

### VS Code Integration
- Launch configuration for debugging with LLDB
- Build tasks for both test program and LLDB itself
- Proper debugging setup for development

## Building

1. **Build LLDB with the new plugin:**
   ```bash
   cd /path/to/llvm-project/build
   ninja lldb lldb-server
   ```

2. **Build the test program:**
   ```bash
   cd examples/libobjc2
   make simple  # Uses clang directly
   # OR
   make         # Uses GNUstep makefiles (if available)
   ```

3. **Test the implementation:**
   ```bash
   # Use the LLDB MCP tools with both lldb and lldb-server paths
   # This ensures lldb-server is found properly
   python lldb_mcp.py --lldb-path /path/to/build/bin/lldb --lldb-server-path /path/to/build/bin/lldb-server
   
   # Or set environment variables
   export LLDB_EXECUTABLE=/path/to/build/bin/lldb
   export LLDB_SERVER_EXECUTABLE=/path/to/build/bin/lldb-server
   python lldb_mcp.py
   ```

## Troubleshooting

### LLDB Server Issues

If you encounter "unable to locate lldb-server" errors:

1. **Ensure lldb-server is built:**
   ```bash
   ninja lldb-server
   ls /path/to/build/bin/lldb-server  # Should exist
   ```

2. **Use correct paths in LLDB MCP:**
   ```bash
   # Specify both paths explicitly
   python lldb_mcp.py --lldb-path /home/robk/llvm-project/build/bin/lldb \
                      --lldb-server-path /home/robk/llvm-project/build/bin/lldb-server
   ```

3. **Check PATH environment:**
   The LLDB MCP automatically adds the lldb-server directory to PATH, but you can also set it manually:
   ```bash
   export PATH="/path/to/build/bin:$PATH"
   ```

## Implementation Notes

### Advantages Over Existing Implementation

1. **Clean Architecture**: Separate concerns between runtime detection, introspection, and AST synthesis
2. **Programmatic Discovery**: Reads actual libobjc2 structures instead of hardcoded assumptions
3. **Modular Design**: Each component has a single, well-defined responsibility
4. **Apple-Inspired**: Follows proven patterns from the working Apple runtime plugin
5. **Proper Runtime Detection**: Only activates for actual GNUstep processes

### Key Design Decisions

1. **Accurate Structure Parsing**: Based on actual libobjc2 source code
2. **Runtime Function Calls**: Uses target process functions (objc_lookup_class) when possible
3. **Memory Safety**: Proper error handling and memory management in target process
4. **Extensible**: Easy to add new features without breaking existing code

## Future Enhancements

1. **Complete Declaration Vendor**: Full AST synthesis for expression evaluation
2. **Rich Formatters**: NSString, NSArray, NSDictionary display improvements  
3. **Trampoline Handling**: Seamless stepping through objc_msgSend
4. **Performance Optimization**: Caching and batch operations
5. **ABI Version Detection**: Support for different libobjc2 versions

This implementation provides a solid foundation for comprehensive GNUstep debugging support in LLDB while maintaining code quality and extensibility.
