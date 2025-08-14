# MSYS2 UCRT64 Minimal LLDB Setup

This folder contains a minimal setup script for building LLDB in an MSYS2 UCRT64 environment where the toolchain and dependencies are already installed.

## Prerequisites

This setup assumes you already have:
- MSYS2 UCRT64 environment active
- Clang/Clang++ toolchain (version 20.1.8 recommended)
- CMake and Ninja build system
- Python 3 development environment
- GNUstep and libobjc2 (already installed in your case)

## Usage

Simply run the setup script:

```bash
./setup.sh
```

## What it does

1. **Auto-detects paths** - No hardcoded assumptions about your directory structure
2. **Verifies environment** - Checks for MSYS2 UCRT64 and required tools (clang, clang++, cmake, ninja, python3)
3. **Optimizes build settings** - Automatically calculates safe parallel job limits based on your system resources
4. **Configures CMake** - Sets up a minimal but functional LLDB build configuration using Clang
5. **Builds incrementally** - Builds essential components first, then LLDB server, then main LLDB executable
6. **Memory-conscious linking** - Uses linker flags to reduce memory consumption during the link phase

## Key differences from scripts-windows

- **No installation steps** - Assumes dependencies are already installed
- **Path auto-detection** - No hardcoded paths, works from any location
- **Memory-conservative** - Optimized for systems with limited RAM
- **Single script** - Everything in one file for simplicity
- **Minimal dependencies** - Only builds what's necessary for LLDB functionality
- **Uses Clang** - Leverages system Clang instead of GCC for better LLVM compatibility

## Build output

After successful completion, you'll find:
- `$BUILD_DIR/bin/lldb.exe` - Main LLDB debugger
- `$BUILD_DIR/bin/lldb-server.exe` - LLDB debug server

## Customization

You can override the default settings by setting environment variables before running:

```bash
export PARALLEL_COMPILE_JOBS=4
export PARALLEL_LINK_JOBS=1
./setup.sh
```

## Build Configuration

The script uses these key CMake settings:
- **Compiler**: System Clang/Clang++ (auto-detected)
- **Build Type**: RelWithDebInfo (optimized with debug info)
- **Projects**: clang + lldb only
- **Targets**: X86 only (minimal target set)
- **Libraries**: Static linking preferred, shared libs disabled
- **Tests**: All tests disabled for faster builds
- **Python**: Enabled with embedded Python home
- **Memory optimization**: Linker flags to reduce memory usage during builds

## Troubleshooting

If the build fails due to memory constraints, the script will still attempt to build the LLDB server, which can be used for remote debugging scenarios.

## System Requirements

- **Memory**: At least 6GB RAM recommended (script auto-adjusts parallelism)
- **Storage**: ~2-3GB for build artifacts
- **CPU**: Multi-core recommended (script uses all cores for compilation)
