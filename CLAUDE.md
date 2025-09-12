# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the LLVM project repository with a custom GNUstep/libobjc2 bridge implementation for LLDB to enable Objective-C debugging on WSL/Windows. The goal is to create a production-ready patch for upstream LLVM submission.

### Key Directories
- `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/` - Main plugin implementation
- `/home/robk/code/llvm-project/lldb/libs-base/` - GNUstep's libs-base for reference
- `/home/robk/code/llvm-project/lldb/libobjc2/` - libobjc2 runtime reference
- `/home/robk/code/llvm-project/lldb/examples/` - Test programs for validating the bridge
- `/home/robk/code/llvm-project/build/` - Build directory for LLVM/LLDB

### Dev script

For quickly performing most actions that follow. We want to AVOID clean rebuilds and only use the incremental one!

```
$ cd /home/robk/code/llvm-project/lldb
$ ./dev.sh --help
GNUstep LLDB Plugin Development Script

Usage:
  ./dev.sh <command> [options]

Build Commands:
  clean-build     Clean and rebuild plugin (full rebuild)
  build           Quick rebuild (incremental)
  clean-examples  Clean example binaries and artifacts
  build-example <name> Build specific example

Test Commands:
  test            Run all tests (unit + API + integration)
  test-unit       Run unit tests only
  test-api        Run API tests only
  test-integration Run LLDB integration tests only

Debug Commands:
  debug [example] Start LLDB debug session with example

Utility Commands:
  full            Run full cycle: clean examples, clean build, test
  status          Show development environment status
  help            Show this help message

Examples:
  ./dev.sh clean-build                    # Full clean rebuild
  ./dev.sh build                          # Quick incremental build
  ./dev.sh test                           # Run all tests
  ./dev.sh test-unit                      # Run just unit tests
  ./dev.sh test-api                       # Test GNUstep program compilation
  ./dev.sh test-integration               # Test formatters in LLDB
  ./dev.sh debug custom_class_test        # Debug with custom class example
  ./dev.sh full                           # Complete development cycle

Available Examples:
  custom_class_test, foundation_test, test_collections_formatter
  test_nsnumber_comprehensive, simple_test, array_test
  dictionary_test, nsset_test, test_data_url_uuid
  (See /home/robk/code/llvm-project/lldb/examples/Makefile for complete list)
```

## Build Commands

### Two-Stage Build Process (Recommended for WSL)

Due to compatibility issues with system compilers, we use a two-stage build process:

#### Automated Two-Stage Build
```bash
# Use the WSL-specific setup script
bash /home/robk/code/llvm-project/lldb/scripts-wsl/setup.sh

# For incremental rebuilds after changes
bash /home/robk/code/llvm-project/lldb/scripts-wsl/setup.sh --skip-stage1
```

#### Manual Two-Stage Build
```bash
# Stage 1: Build clang/lld with system compiler
mkdir -p build-stage1 && cd build-stage1
cmake -G Ninja ../llvm \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang;lld" \
    -DLLVM_TARGETS_TO_BUILD="X86" \
    -DBUILD_SHARED_LIBS=OFF \
    -DLLVM_CCACHE_BUILD=ON
ninja clang lld llvm-tblgen clang-tblgen -j$(nproc)

# Stage 2: Build LLDB with stage1 clang
cd ../build
cmake -G Ninja ../llvm \
    -DCMAKE_C_COMPILER=../build-stage1/bin/clang \
    -DCMAKE_CXX_COMPILER=../build-stage1/bin/clang++ \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DLLVM_ENABLE_PROJECTS="clang;lldb;lld" \
    -DLLVM_ENABLE_ASSERTIONS=ON \
    -DLLDB_INCLUDE_TESTS=ON \
    -DBUILD_SHARED_LIBS=ON \
    -DLLVM_CCACHE_BUILD=ON \
    -DLLDB_ENABLE_PYTHON=ON \
    -DPython3_EXECUTABLE=/usr/bin/python3 \
    -DPython3_INCLUDE_DIRS=/usr/include/python3.10 \
    -DPython3_LIBRARIES=/usr/lib/x86_64-linux-gnu/libpython3.10.so \
    -DCMAKE_INSTALL_PREFIX=/usr/local/llvm-reldeb
ninja lldb lldb-server -j$(nproc)
```

### Quick Rebuild (After Code Changes)
```bash
# Quick rebuild of LLDB and plugin only
cd /home/robk/code/llvm-project/build && ninja lldb lldb-server lldbPluginGNUstepObjCRuntime -j$(nproc)
```

## Building GNUstep Test Programs

The project now uses CMake for cross-platform building of test programs. The CMakeLists.txt supports both Linux (WSL) and Windows.

### Using CMake Build System (Recommended)
```bash
cd /home/robk/code/llvm-project/lldb/examples
mkdir -p build-examples && cd build-examples

# Configure with your newly built clang
cmake -S .. -B . \
    -DCMAKE_C_COMPILER=/home/robk/code/llvm-project/build-stage1/bin/clang \
    -DCMAKE_OBJC_COMPILER=/home/robk/code/llvm-project/build-stage1/bin/clang \
    -DLLDB_BIN=/home/robk/code/llvm-project/build/bin/lldb \
    -DLLDB_SERVER_BIN=/home/robk/code/llvm-project/build/bin/lldb-server

# Build all examples
cmake --build . -j

# Build specific example
cmake --build . --target custom_class_test

# Debug with LLDB (launches LLDB with proper environment)
cmake --build . --target debug-custom_class_test
```

### Manual Compilation (if needed)
```bash
CC=/home/robk/code/llvm-project/build-stage1/bin/clang
LLDB=/home/robk/code/llvm-project/build/bin/lldb

CFLAGS="-fobjc-runtime=gnustep-2.1 \
        -fblocks \
        -fno-strict-aliasing \
        -fexceptions \
        -fobjc-exceptions \
        -g -O0 \
        -fno-omit-frame-pointer \
        -I/usr/local/include/GNUstep \
        -I/usr/include/GNUstep \
        -fconstant-string-class=NSConstantString \
        -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -DDEBUG=1"

LDFLAGS="-L/usr/local/lib -Wl,-rpath,/usr/local/lib -g"
LIBS="-lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm"

# Example compilation
$CC $CFLAGS $LDFLAGS -o test_program test_program.m $LIBS
```

## Testing the Plugin

The project includes a comprehensive three-tier automated test suite.

### Automated Testing (Recommended)

#### Run All Tests
```bash
cd /home/robk/code/llvm-project/lldb
./dev.sh test
# Executes: unit tests + API tests + integration tests
```

#### Individual Test Suites
```bash
# Unit Tests - Test formatter logic and runtime components
./dev.sh test-unit

# API Tests - Test GNUstep program compilation and execution  
./dev.sh test-api

# Integration Tests - Test formatters working within LLDB
./dev.sh test-integration
```

### Manual Testing

#### Quick LLDB Validation
```bash
cd /home/robk/code/llvm-project/lldb/examples
/home/robk/code/llvm-project/build/bin/lldb custom_class_test

# In LLDB:
(lldb) b custom_class_test.m:125
(lldb) run
(lldb) po account  # Test GNUstep object inspection
# Expected: BankAccount(12345, owner=John Doe2, balance=1000.00, transactions=3)

(lldb) po personInfo  # Test NSDictionary
# Expected: { occupation = Developer; name = "John Doe2"; }

(lldb) po fruits  # Test NSArray
# Expected: ( "Apple", "Banana", "Cherry" )

(lldb) po magicNumber  # Test NSNumber
# Expected: 42
```

#### Direct Test Program Execution
```bash
# Test GNUstep compilation and execution directly
cd /home/robk/code/llvm-project/lldb/test/API/lang/objc/gnustep
OBJC=/home/robk/code/llvm-project/build-stage1/bin/clang make
./a.out
# Should output: "All test objects created successfully"
```

### Test Infrastructure Details

- **Unit Tests**: GoogleTest-based C++ tests for formatter components
- **API Tests**: Compilation/execution tests for GNUstep programs  
- **Integration Tests**: End-to-end LLDB debugging with formatter validation
- **Continuous Integration**: `./dev.sh full` runs complete build + test cycle

### Test Results Interpretation

✅ **All tests pass**: Plugin is production-ready  
⚠️ **Unit tests pass, others fail**: Core logic works, integration issues  
❌ **Unit tests fail**: Core functionality broken, investigate immediately

## Architecture Overview

### GNUstepObjCRuntime Plugin Structure

The plugin follows LLDB's modular architecture:

1. **Core Runtime** (`GNUstepObjCRuntime.cpp/h`)
   - Main plugin class inheriting from `ObjCLanguageRuntime`
   - Runtime detection (looks for libobjc.so.2, libgnustep-base.so)
   - Plugin registration and initialization

2. **Runtime Introspector** (`GNUstepObjCRuntimeIntrospector.cpp/h`)
   - Direct memory access to libobjc2 data structures
   - Object layout understanding
   - Class and method information extraction

3. **Declaration Vendor** (`GNUstepObjCDeclVendor.cpp/h`)
   - Provides type information to LLDB's expression evaluator
   - Dynamic type synthesis for runtime classes

4. **Formatters System** (`formatters/` directory)
   - `GNUstepFormattersBase`: Base classes for all formatters
   - `GNUstepStringFormatters`: NSString and variants
   - `GNUstepNumberFormatters`: NSNumber with tagged pointer support
   - `GNUstepCollectionFormatters`: NSArray, NSDictionary, NSSet
   - `GNUstepValueFormatters`: NSValue generic wrapper
   - `GNUstepFormattersRegistry`: Registration with LLDB's type system

### Plugin Registration Flow
1. `GNUstepObjCRuntime::Initialize()` registers with PluginManager
2. When debugging starts, `CreateInstance()` checks for GNUstep libraries
3. If found, creates runtime instance and registers formatters
4. Formatters are added to "gnustep" TypeCategory

## Development Workflow

### Making Changes to the Plugin

1. Edit files in `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/`
2. Rebuild: `cd /home/robk/code/llvm-project/build && ninja lldbPluginGNUstepObjCRuntime`
3. Test with example programs in `/home/robk/code/llvm-project/lldb/examples/`

### Adding New Formatters

1. Create new formatter class in `formatters/` directory
2. Inherit from `GNUstepSummaryProvider` or `GNUstepSyntheticProvider`
3. Register in `GNUstepFormattersRegistry::RegisterFormatters()`
4. Update CMakeLists.txt if adding new files
5. Add test cases to `/home/robk/code/llvm-project/lldb/examples/custom_class_test.m`
6. Verify functionality with MCP LLDB tools

### Testing Framework

The plugin includes a comprehensive testing framework:

1. **Test Program**: `custom_class_test.m` creates objects of all supported types
2. **MCP Integration**: Use `mcp__llvm_lldb_debug__*` functions for automated testing
3. **Validation**: Each formatter tested with:
   - Null objects
   - Valid objects
   - Edge cases (empty collections, special values)
   - Memory corruption scenarios
4. **Performance**: All formatters achieve <50ms response time

### Debugging the Plugin

Enable debug output by checking for printf statements in the code (currently present for development).

## Current Implementation Status - 65% Complete

### May need review
- Basic plugin framework and registration
- Runtime detection for GNUstep processes
- Modular formatter architecture
- NSString formatter (all variants and encodings)
- NSNumber formatter (including tagged pointers)
- NSValue formatter (generic value wrapper)
- NSArray/NSMutableArray formatters (element count and display)
- NSDictionary/NSMutableDictionary formatters (key/value pairs) - *Display format needs refinement*
- NSSet/NSMutableSet formatters (object count and enumeration)
- Comprehensive test framework
- Build system integration
- Formatter activation bug fixes
- Performance optimizations (sub-50ms response times)

- **Custom class introspection** - Blocked by ISA lookup issue

### 📋 Planned (Phase 5)
- Dynamic method listing
- Memory management debugging
- Advanced performance profiling
- Expression evaluation (`po`, `expr` commands)

## Important Notes

1. **lldb-server Required**: Always build lldb-server to avoid "unable to locate lldb-server" errors
2. **Runtime Version**: Target gnustep-2.1 runtime for modern features
3. **Debug Symbols**: Always compile with `-g -O0` for debugging
4. **Memory Access**: Plugin uses direct memory reading - handle failures gracefully

## Common Issues and Solutions

### Plugin Not Loading
- Verify GNUstep libraries are linked: `ldd test_program | grep gnustep`
- Check debug output in console for "GNUstepObjCRuntime" messages
- Ensure `-fobjc-runtime=gnustep-2.1` is used during compilation

### Formatter Not Working
- ✅ TypeCategory activation issues resolved in current version
- Check debug output: Should show "GNUstep formatters registered successfully"
- Verify object is actually a GNUstep object (not nil or corrupted)
- Test with known working types: NSString, NSNumber, NSArray, NSDictionary, NSSet

## LLDB MCP Tools

For interactive debugging sessions, MCP tools are available:
- Use `mcp__llvm_lldb_debug__*` functions for debugging sessions
- `/home/robk/code/llvm-project/lldb/lldb_mcp/lldb_mcp.py` - MCP server for LLDB control

## Code Style

Follow LLVM coding standards:
- 80 column limit
- CamelCase for classes, camelCase for methods/variables
- Use LLVM's error handling (llvm::Error, llvm::Expected)
- Include proper LLVM license headers
- Keep comments to a minimum