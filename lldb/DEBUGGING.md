# LLDB GNUstep Plugin Development Debugging Guide

This guide explains how to use the VS Code development infrastructure for efficient LLDB formatter debugging.

## Quick Start

1. **Open VS Code in the project root**: `/home/robk/code/llvm-project/`
2. **Press F5** to launch the default debugging configuration
3. **Select a debugging target** from the launch configurations
4. **Set breakpoints** in formatter code as needed

## Available Debugging Configurations

### Formatter-Specific Debugging

- **Debug NSIndexSet Formatter**: Tests NSIndexSet formatting with various scenarios
- **Debug NSDecimalNumber Formatter**: Tests NSDecimalNumber formatting 
- **Debug NSCharacterSet Formatter**: Tests NSCharacterSet formatting
- **Debug Array Formatter**: Tests NSArray/NSMutableArray formatting
- **Debug Dictionary Formatter**: Tests NSDictionary/NSMutableDictionary formatting
- **Debug Set Formatter**: Tests NSSet/NSMutableSet formatting
- **Debug String Formatter**: Tests NSString/NSMutableString formatting

### Advanced Debugging

- **Debug Custom Class Test**: Tests custom class introspection and formatting
- **Debug with Memory Inspection**: Enhanced debugging with memory analysis and logging
- **Interactive LLDB Session**: Opens LLDB in external console for manual testing

## Build Tasks

### Core Build Tasks

- **build-plugin-and-tests** (Default - Ctrl+Shift+P → "Tasks: Run Build Task")
  - Builds the GNUstep plugin and all test programs
  - This is the most commonly used task

- **build-plugin-only**
  - Builds only the GNUstep LLDB plugin
  - Fast iteration when only plugin code changes

### Test-Specific Build Tasks

- **build-indexset-test**: Builds test_indexset program
- **build-decimalnumber-test**: Builds test_decimalnumber program  
- **build-characterset-test**: Builds test_characterset program
- **build-custom-class-test**: Builds custom_class_test program

### Maintenance Tasks

- **build-lldb-components**: Builds lldb, lldb-server, lldb-argdumper
- **full-rebuild**: Clean build + rebuild everything
- **clean-all**: Cleans all build artifacts

## Development Workflow

### 1. Making Formatter Changes

1. Edit formatter source files in:
   ```
   /home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/
   ```

2. **Press Ctrl+Shift+P → "Tasks: Run Build Task"** to rebuild plugin + tests

3. **Press F5** and select appropriate debugging configuration

4. **Observe formatter output** in LLDB console

### 2. Interactive Debugging

Use the **"Interactive LLDB Session"** configuration for manual testing:

```bash
(lldb) b custom_class_test.m:228
(lldb) run
(lldb) po account      # Test custom class
(lldb) po personInfo   # Test dictionary
(lldb) po fruits       # Test array
```

### 3. Memory Analysis Debugging

Use **"Debug with Memory Inspection"** for deep analysis:

- Enables formatter and types logging
- Shows memory dumps before formatting
- Useful for investigating memory corruption issues

## Debugging Tips

### Setting Breakpoints in Plugin Code

1. **Launch any debugging configuration**
2. **Once LLDB starts**, set breakpoints in VS Code:
   - `GNUstepDictionaryFormatters.cpp:line_number`
   - `GNUstepArrayFormatters.cpp:line_number`  
   - `GNUstepStringFormatters.cpp:line_number`

### Common Debugging Scenarios

#### Testing New Formatters
1. Add test case to appropriate `test_*.m` file
2. Use specific build task: `build-*-test`
3. Launch corresponding debug configuration
4. Verify formatter output

#### Investigating Formatter Issues
1. Use **"Debug with Memory Inspection"**
2. Check memory dumps for data corruption
3. Review LLDB logs for error messages
4. Set breakpoints in formatter GetSummary/GetChildAtIndex methods

#### Performance Testing
1. Use **Interactive LLDB Session** 
2. Time formatter operations:
   ```
   (lldb) expr -T -- po largeArray
   ```

### Troubleshooting

#### Plugin Not Loading
- Verify build succeeded with `build-plugin-only` task
- Check for GNUstep libraries: `ldd test_program | grep gnustep`
- Look for plugin registration messages in LLDB output

#### Formatters Not Working
- Ensure test program compiled with GNUstep flags
- Check TypeCategory activation (should see "GNUstep formatters registered")
- Verify object is valid GNUstep object (not nil/corrupted)

#### VS Code Not Finding Includes
- Update `.vscode/settings.json` includePath if needed
- Reload VS Code window: `Ctrl+Shift+P → "Developer: Reload Window"`

## Performance Benchmarking

Use this workflow to benchmark formatter performance:

1. **Build with**: `build-plugin-and-tests`
2. **Launch**: Interactive LLDB Session  
3. **Create large objects** in test program
4. **Time operations**:
   ```
   (lldb) expr -T -- po hugeDictionary
   (lldb) expr -T -- po massiveArray
   ```
5. **Target**: < 50ms for all formatter operations

## File Locations

### Configuration Files
- `.vscode/launch.json` - Debug configurations
- `.vscode/tasks.json` - Build tasks
- `.vscode/settings.json` - Project settings

### Source Files
- **Plugin**: `lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/`
- **Tests**: `lldb/examples/`
- **Build**: `build/` (excluded from VS Code)

### Build Outputs
- **Plugin**: `build/lib/llvm/lib/lldbPluginGNUstepObjCRuntime.so`
- **LLDB**: `build/bin/lldb`
- **Tests**: `lldb/examples/test_*` (executables)

## Configuration Customization

### Adding New Test Programs

1. **Add build task** to `.vscode/tasks.json`:
   ```json
   {
       "label": "build-mynewtest",
       "dependsOn": ["build-plugin-only"],
       "type": "shell",
       "command": "make",
       "args": ["mynewtest"],
       "options": {"cwd": "${workspaceFolder}/lldb/examples"}
   }
   ```

2. **Add debug configuration** to `.vscode/launch.json`:
   ```json
   {
       "name": "Debug My New Test",
       "type": "cppdbg",
       "program": "/home/robk/code/llvm-project/build/bin/lldb",
       "args": ["/home/robk/code/llvm-project/lldb/examples/mynewtest"],
       "preLaunchTask": "build-mynewtest"
   }
   ```

### Adjusting Build Parallelism

Edit `numberOfJobs` in `.vscode/settings.json` based on system resources:
```json
"numberOfJobs": 16  // For high-end systems
"numberOfJobs": 4   // For constrained systems
```

## Integration with MCP LLDB Tools

The debugging infrastructure integrates with MCP LLDB tools for automated testing:

```python
# Use MCP tools for automated debugging
mcp__llvm_lldb_debug__lldb_start()
mcp__llvm_lldb_debug__lldb_load(program="test_indexset")
mcp__llvm_lldb_debug__lldb_set_breakpoint(location="main")
mcp__llvm_lldb_debug__lldb_run()
```

This setup provides comprehensive debugging capabilities for efficient LLDB formatter development with rapid iteration cycles.