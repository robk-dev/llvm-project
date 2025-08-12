#!/bin/bash
# Development Environment Configuration
# Source this file to set up environment variables for GNUstep LLDB development

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# LLDB Build Configuration
export LLDB_ROOT="$SCRIPT_DIR"
export LLDB_BUILD_DIR="$SCRIPT_DIR/../build"  
export LLDB_BIN="$LLDB_BUILD_DIR/bin/lldb"
export LLDB_SERVER="$LLDB_BUILD_DIR/bin/lldb-server"

# GNUstep Configuration
export GNUSTEP_INSTALL_DIR="$LLDB_ROOT/gnustep-install"
export GNUSTEP_INCLUDE_DIR="$GNUSTEP_INSTALL_DIR/include"
export GNUSTEP_LIB_DIR="$GNUSTEP_INSTALL_DIR/lib"

# Development Environment
export PATH="$LLDB_BUILD_DIR/bin:$PATH"
export LLDB_DEBUGSERVER_PATH="$LLDB_SERVER"

# GNUstep Runtime Environment
export LD_LIBRARY_PATH="$GNUSTEP_LIB_DIR:/usr/local/lib:${LD_LIBRARY_PATH:-}"

# Compilation Environment
export CC="${CC:-clang}"
export OBJC_RUNTIME="gnustep-2.1"

echo "GNUstep LLDB Development Environment Configured"
echo "  LLDB:        $LLDB_BIN"
echo "  LLDB Server: $LLDB_SERVER"  
echo "  GNUstep:     $GNUSTEP_INSTALL_DIR"
echo ""
echo "Usage:"
echo "  cd $LLDB_ROOT && ./dev.sh [command]"