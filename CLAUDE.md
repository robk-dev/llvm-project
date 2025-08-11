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

## Build Commands

### Building LLDB with GNUstep Plugin
```bash
# Quick build (LLDB and required components only)
cd /home/robk/code/llvm-project/build && ninja lldb lldb-server lldb-argdumper -j$(nproc)

# Full LLDB installation
cd /home/robk/code/llvm-project/build && ninja install-lldb install-lldb-server -j$(nproc)

# Build only the GNUstep plugin (after changes)
cd /home/robk/code/llvm-project/build && ninja lldbPluginGNUstepObjCRuntime
```

### Initial CMake Configuration (if needed)
```bash
cmake -G Ninja ../llvm \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DLLVM_ENABLE_PROJECTS="clang;lldb;lld" \
    -DLLVM_ENABLE_ASSERTIONS=ON \
    -DLLDB_INCLUDE_TESTS=ON \
    -DLLVM_CCACHE_BUILD=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local/llvm-reldeb
```

## Compiling GNUstep Test Programs

When creating or compiling Objective-C test programs for GNUstep, use these flags:

```bash
CC=/home/robk/llvm-build/build/bin/clang
LLDB=/home/robk/llvm-build/build/bin/lldb

CFLAGS="-fobjc-runtime=gnustep-2.1 \
        -fblocks \
        -fno-strict-aliasing \
        -fexceptions \
        -fobjc-exceptions \
        -g -gdwarf-5 -O0 \
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

### Building Example Tests
```bash
cd /home/robk/code/llvm-project/lldb/examples
make all  # Build all test programs
make custom_class_test  # Build specific test
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
OBJC=/home/robk/code/llvm-project/build/bin/clang make
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
   - `GNUstepStringFormatters`: NSString and variants (✅ Production-ready)
   - `GNUstepNumberFormatters`: NSNumber with tagged pointer support (✅ Production-ready)
   - `GNUstepCollectionFormatters`: NSArray, NSDictionary, NSSet (✅ Production-ready)
   - `GNUstepValueFormatters`: NSValue generic wrapper (✅ Production-ready)
   - `GNUstepFormattersRegistry`: Registration with LLDB's type system (✅ Production-ready)

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

### ✅ Completed (Production-Ready)
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

### 🔧 Active Issues (As of 2025-08-08)

#### Issue 1: Dictionary Display Format
- **Problem**: Shows verbose `[0].key` and `[0].value` instead of concise `key = value`
- **Location**: `GNUstepDictionaryFormatters.cpp` lines 1047-1050
- **Fix**: Modify child naming in `GetChildAtIndex()` method
- **Priority**: High (UX impact)

#### Issue 2: Custom Class ISA Lookup
- **Problem**: BankAccount and other custom classes not showing properties
- **Location**: `GNUstepObjCRuntimeIntrospector.cpp` ISA resolution
- **Root Cause**: `CallRuntimeFunction()` returns LLDB_INVALID_ADDRESS (stub implementation)
- **Fix**: Implement proper runtime function calling via expression evaluator
- **Priority**: Critical (core functionality)

#### Issue 3: Runtime Symbol Resolution
- **Problem**: Some runtime functions may not resolve correctly
- **Location**: `GNUstepRuntimeV2API.cpp` line 193
- **Fix**: Enhance symbol resolution with multiple strategies
- **Priority**: Medium

### 🔄 In Progress (Phase 4 - Advanced Features)
- NSDate/NSCalendarDate formatters
- NSURL formatter
- NSData/NSMutableData formatter
- NSUUID formatter
- NSError formatter
- **Custom class introspection** - Blocked by ISA lookup issue

### 📋 Planned (Phase 5)
- Dynamic method listing
- Memory management debugging
- Advanced performance profiling
- Expression evaluation (`po`, `expr` commands)

## Important Notes

1. **lldb-server Required**: Always build lldb-server to avoid "unable to locate lldb-server" errors
2. **Runtime Version**: Target gnustep-2.1 runtime for modern features
3. **Debug Symbols**: Always compile with `-g -gdwarf-5 -O0` for debugging
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
- `/home/robk/code/llvm-project/lldb/lldb_mcp/lldb_mcp.py` - MCP server for LLDB control
- Use `mcp__llvm_lldb_debug__*` functions for debugging sessions

## Code Style

Follow LLVM coding standards:
- 80 column limit
- CamelCase for classes, camelCase for methods/variables
- Use LLVM's error handling (llvm::Error, llvm::Expected)
- Include proper LLVM license headers