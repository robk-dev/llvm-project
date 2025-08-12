#!/bin/bash
# Path replacement functionality for Windows MSYS2 workspace portability

set -euo pipefail

# Source common utilities
HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HELPERS_DIR/common.sh"

# Function to find all files containing hardcoded paths
find_hardcoded_paths() {
    local search_dir="${1:-$WORKSPACE_ROOT}"
    local old_path="${2:-/home/robk}"
    
    print_progress "Searching for hardcoded paths in $search_dir..."
    
    # Find files containing the old path
    local files_with_paths=()
    
    # Search in source files
    while IFS= read -r -d '' file; do
        files_with_paths+=("$file")
    done < <(grep -rlZ "$old_path" "$search_dir" \
        --include="*.h" \
        --include="*.cpp" \
        --include="*.c" \
        --include="*.m" \
        --include="*.mm" \
        --include="*.sh" \
        --include="*.py" \
        --include="*.cmake" \
        --include="CMakeLists.txt" \
        --include="*.json" \
        --include="*.yaml" \
        --include="*.yml" \
        --include="Makefile" \
        --include="makefile" \
        --exclude-dir=".git" \
        --exclude-dir="build*" \
        --exclude-dir=".ccache" \
        2>/dev/null || true)
    
    echo "${files_with_paths[@]}"
}

# Function to replace paths in a single file
replace_paths_in_file() {
    local file="$1"
    local old_path="$2"
    local new_path="$3"
    
    # Skip binary files
    if file "$file" | grep -q "binary"; then
        return 0
    fi
    
    # Create backup
    backup_file "$file"
    
    # Perform replacement
    # Use sed with different delimiter to handle paths with slashes
    sed -i "s|$old_path|$new_path|g" "$file"
    
    # Check if replacement was made
    if grep -q "$new_path" "$file"; then
        return 0
    else
        return 1
    fi
}

# Main function to replace hardcoded paths
replace_hardcoded_paths() {
    print_section "Replacing Hardcoded Paths for Workspace Portability"
    
    # Determine the old and new paths
    local old_path="/home/robk"
    local new_path="$PROJECT_ROOT"
    
    # Convert to MSYS2 path format if needed
    if [[ "$OSTYPE" == "msys" ]]; then
        # Remove drive letter prefix for sed compatibility
        new_path=$(echo "$new_path" | sed 's|^/[a-zA-Z]/|/|')
    fi
    
    print_info "Replacing paths:"
    print_info "  Old: $old_path"
    print_info "  New: $new_path"
    
    # Find files with hardcoded paths
    print_progress "Scanning for files with hardcoded paths..."
    local files_to_update=($(find_hardcoded_paths "$WORKSPACE_ROOT" "$old_path"))
    
    if [ ${#files_to_update[@]} -eq 0 ]; then
        print_success "No hardcoded paths found to replace"
        return 0
    fi
    
    print_info "Found ${#files_to_update[@]} files with hardcoded paths"
    
    # Ask for confirmation
    read -p "Replace paths in these files? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Path replacement skipped"
        return 0
    fi
    
    # Replace paths in each file
    local success_count=0
    local fail_count=0
    
    for file in "${files_to_update[@]}"; do
        print_progress "Updating: $(basename "$file")"
        if replace_paths_in_file "$file" "$old_path" "$new_path"; then
            ((success_count++))
        else
            ((fail_count++))
            print_warning "Failed to update: $file"
        fi
    done
    
    print_success "Path replacement completed: $success_count files updated, $fail_count failed"
    
    # Update build system paths if needed
    update_build_paths "$new_path"
}

# Function to update build system paths
update_build_paths() {
    local workspace_path="$1"
    
    print_progress "Updating build system paths..."
    
    # Update CMake cache if it exists
    if [ -f "$LLVM_BUILD_DIR/build/CMakeCache.txt" ]; then
        print_progress "Updating CMake cache..."
        backup_file "$LLVM_BUILD_DIR/build/CMakeCache.txt"
        
        # Update paths in CMake cache
        sed -i "s|/home/robk|$workspace_path|g" "$LLVM_BUILD_DIR/build/CMakeCache.txt"
        
        print_info "CMake cache updated (may need to reconfigure)"
    fi
    
    # Update compile_commands.json if it exists
    if [ -f "$LLVM_BUILD_DIR/build/compile_commands.json" ]; then
        print_progress "Updating compile_commands.json..."
        backup_file "$LLVM_BUILD_DIR/build/compile_commands.json"
        
        sed -i "s|/home/robk|$workspace_path|g" "$LLVM_BUILD_DIR/build/compile_commands.json"
        
        print_info "compile_commands.json updated"
    fi
    
    # Create a path mapping file for reference
    create_path_mapping_file "$workspace_path"
}

# Function to create a path mapping reference file
create_path_mapping_file() {
    local new_path="$1"
    local mapping_file="$LLVM_BUILD_DIR/path-mapping.txt"
    
    print_progress "Creating path mapping reference..."
    
    cat > "$mapping_file" << EOF
Path Mapping Reference
======================
Generated: $(date)

Original Paths (Linux):
  /home/robk/code/llvm-project  → $new_path

Windows MSYS2 Paths:
  Project Root: $PROJECT_ROOT
  Workspace: $WORKSPACE_ROOT
  LLVM Build: $LLVM_BUILD_DIR
  GNUstep Install: $GNUSTEP_INSTALL_DIR

Windows Native Paths:
  Project Root: $(to_windows_path "$PROJECT_ROOT")
  LLVM Build: $(to_windows_path "$LLVM_BUILD_DIR")

Environment Variables to Set:
  export PROJECT_ROOT="$PROJECT_ROOT"
  export LLVM_BUILD_DIR="$LLVM_BUILD_DIR"
  export GNUSTEP_INSTALL_DIR="$GNUSTEP_INSTALL_DIR"
  export PATH="$LLVM_BUILD_DIR/build/bin:\$PATH"
  export LD_LIBRARY_PATH="$GNUSTEP_INSTALL_DIR/lib:\$LD_LIBRARY_PATH"

For VS Code debugging, use these paths in launch.json:
  "miDebuggerPath": "$(to_windows_path "$LLVM_BUILD_DIR/build/bin/lldb.exe")"
  "program": "$(to_windows_path "$LLVM_BUILD_DIR/examples/test.exe")"
EOF
    
    print_success "Path mapping reference saved to: $mapping_file"
}

# Function to verify path replacements
verify_path_replacements() {
    print_section "Verifying Path Replacements"
    
    local old_path="/home/robk"
    
    print_progress "Checking for remaining hardcoded paths..."
    
    # Search for any remaining old paths
    if grep -r "$old_path" "$WORKSPACE_ROOT" \
        --include="*.h" \
        --include="*.cpp" \
        --include="*.c" \
        --include="*.sh" \
        --exclude-dir=".git" \
        --exclude-dir="build*" \
        --exclude-dir=".ccache" \
        2>/dev/null | head -5; then
        print_warning "Some hardcoded paths remain (shown above, max 5)"
        print_info "These may be in comments or documentation"
    else
        print_success "No hardcoded paths found - workspace is portable!"
    fi
}

# Export functions
export -f find_hardcoded_paths replace_paths_in_file replace_hardcoded_paths
export -f update_build_paths create_path_mapping_file verify_path_replacements