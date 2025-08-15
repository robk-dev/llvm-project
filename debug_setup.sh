#!/bin/bash
# debug_setup.sh - Clean environment setup for LLDB GNUstep debugging
# Usage: source debug_setup.sh

echo "🔧 Setting up LLDB GNUstep debugging environment..."

# Step 1: Clean environment variables
unset CONAN_USER_HOME 2>/dev/null || true
unset GNUSTEP_MAKEFILES 2>/dev/null || true

# Step 2: Set up MSYS2 UCRT64 environment
export MSYSTEM=UCRT64
export MSYSTEM_PREFIX=/ucrt64
export MSYSTEM_CARCH=x86_64
export MSYSTEM_CHOST=x86_64-w64-mingw32

# Step 3: Set up clean PATH with UCRT64 tools
export PATH="/c/tools/msys64/ucrt64/bin:/usr/local/bin:/usr/bin:/bin"

echo "✅ MSYS2 UCRT64 environment configured"

# Step 4: Navigate to eggplant directory and activate Conan environment  
if [[ -d "/c/code/eggplant" ]]; then
  echo "🔄 Activating Conan environment..."
  cd /c/code/eggplant || {
    echo "❌ Failed to navigate to /c/code/eggplant"
    return 1
  }
  
  # Try Conan install, but don't fail if it already exists
  conan install . --install-folder=build -s build_type=Debug >/dev/null 2>&1 || {
    echo "ℹ️  Conan install failed or already exists, continuing..."
  }
  
  # Activate Conan environment if it exists
  if [[ -f "build/activate.sh" ]]; then
    source build/activate.sh
    echo "✅ Conan environment activated"
  else
    echo "⚠️  Conan activate.sh not found, skipping"
  fi
else
  echo "⚠️  /c/code/eggplant directory not found, skipping Conan setup"
fi

# Step 5: Add UCRT64 compilers back to PATH (after Conan)
export PATH="/c/tools/msys64/ucrt64/bin:$PATH"

# Step 6: Verify environment
echo "🔍 Environment verification:"
echo "  - MSYSTEM: $MSYSTEM"
echo "  - Clang: $(which clang 2>/dev/null || echo 'NOT FOUND')"
echo "  - GNUstep headers: $(find /c/.conan -name "Foundation" -type d 2>/dev/null | head -1 || echo 'NOT FOUND')"

# Step 7: Set up convenience variables
export LLDB_BIN="/c/code/llvm-project/build/bin/lldb.exe"
export EXAMPLES_DIR="/c/code/llvm-project/lldb/examples"  
export BUILD_DIR="/c/code/llvm-project/build"

echo "📝 Convenience variables set:"
echo "  - LLDB_BIN: $LLDB_BIN"
echo "  - EXAMPLES_DIR: $EXAMPLES_DIR"
echo "  - BUILD_DIR: $BUILD_DIR"

# Step 8: Create helper functions
compile_example() {
    local example_name=${1:-"simple_test"}
    echo "🔨 Compiling $example_name..."
    
    if [[ ! -d "$EXAMPLES_DIR" ]]; then
        echo "❌ Examples directory not found: $EXAMPLES_DIR"
        return 1
    fi
    
    cd "$EXAMPLES_DIR" || return 1
    make clean >/dev/null 2>&1 || true
    
    if make "$example_name"; then
        echo "✅ $example_name compiled successfully"
        return 0
    else
        echo "❌ Failed to compile $example_name"
        return 1
    fi
}

run_debug() {
    local example_name=${1:-"simple_test"}
    local example_path="$EXAMPLES_DIR/${example_name}.exe"
    
    if [[ ! -f "$example_path" ]]; then
        echo "❌ Example $example_path not found. Run compile_example first."
        return 1
    fi
    
    if [[ ! -f "$LLDB_BIN" ]]; then
        echo "❌ LLDB binary not found: $LLDB_BIN"
        echo "Run rebuild_lldb first."
        return 1
    fi
    
    echo "🐛 Starting LLDB with $example_name..."
    echo "Recommended debug commands:"
    echo "  b main"
    echo "  run"
    echo "  br set -l 10"
    echo "  c"
    echo "  po fruits"
    echo "  frame variable -O fruits"
    
    cd "$BUILD_DIR" || return 1
    "$LLDB_BIN" "$example_path"
}

rebuild_lldb() {
    echo "🔨 Rebuilding LLDB..."
    
    if [[ ! -d "$BUILD_DIR" ]]; then
        echo "❌ Build directory not found: $BUILD_DIR"
        return 1
    fi
    
    cd "$BUILD_DIR" || return 1
    
    if ninja -j4 lldb lldb-server; then
        echo "✅ LLDB rebuild complete"
        return 0
    else
        echo "❌ LLDB rebuild failed"
        return 1
    fi
}

# Export functions so they're available in the shell
export -f compile_example run_debug rebuild_lldb

echo ""
echo "🎉 Environment ready! Available commands:"
echo "  compile_example [name]  - Compile an example (default: simple_test)"
echo "  run_debug [name]        - Run LLDB with an example"
echo "  rebuild_lldb           - Rebuild LLDB after changes"
echo ""
echo "Quick start:"
echo "  compile_example"
echo "  run_debug"
