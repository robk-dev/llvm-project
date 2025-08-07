# LLDB MCP Server Configuration

This document describes how to configure the LLDB executable path for the LLDB MCP server.

## Configuration Methods

The LLDB MCP server supports three methods to specify the LLDB executable path, in order of priority:

### 1. Command Line Argument (Highest Priority)
```bash
python3 lldb_mcp.py --lldb-path /path/to/your/lldb
```

### 2. Environment Variable
```bash
export LLDB_EXECUTABLE=/path/to/your/lldb
python3 lldb_mcp.py
```

### 3. System Default (Fallback)
If neither CLI argument nor environment variable is set, the server will use the system's default `lldb` command.

## VS Code Integration

Based on your VS Code settings (`/home/robk/code/obj_c_debug/.vscode/settings.json`), you're using a custom LLDB build:
```json
"lldb.executable": "/home/robk/llvm-build/build/bin/lldb"
```

To use the same LLDB executable with the MCP server:

**Option 1: Command line**
```bash
python3 lldb_mcp.py --lldb-path /home/robk/llvm-build/build/bin/lldb
```

**Option 2: Environment variable**
```bash
export LLDB_EXECUTABLE=/home/robk/llvm-build/build/bin/lldb
python3 lldb_mcp.py
```

**Option 3: Set in shell profile**
Add to your `~/.bashrc` or `~/.zshrc`:
```bash
export LLDB_EXECUTABLE=/home/robk/llvm-build/build/bin/lldb
```

## Configuration Verification

The server includes a configuration tool to verify the current settings:

```python
# This tool is available when the MCP server is running
lldb_config()
```

## Debug Logging

Enable debug logging to see which LLDB path is being used:
```bash
python3 lldb_mcp.py --debug
```

Example output:
```
[DEBUG] Debug logging enabled
[DEBUG] Using LLDB path from CLI argument: /home/robk/llvm-build/build/bin/lldb
```

## Files

- `lldb_mcp.py` - Main MCP server script
- `test_config.py` - Configuration test script  
- `usage_demo.sh` - Usage demonstration script
- `CONFIG.md` - This documentation file
