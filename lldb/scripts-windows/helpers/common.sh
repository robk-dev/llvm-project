#!/bin/bash
# Common utilities and functions for Windows MSYS2/UCRT64 LLVM/LLDB build scripts
# Author: LLDB GNUstep Development Team
# Date: August 2025

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Path configuration
if [ -z "${WORKSPACE_ROOT:-}" ]; then
    HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    SCRIPT_DIR="$(dirname "$HELPERS_DIR")"
    WORKSPACE_ROOT="$(dirname "$SCRIPT_DIR")"
    PROJECT_ROOT="$(dirname "$WORKSPACE_ROOT")"
    
    # Convert Windows paths to MSYS2 paths if needed
    if [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
        PROJECT_ROOT=$(cygpath -u "$PROJECT_ROOT" 2>/dev/null || echo "$PROJECT_ROOT")
        WORKSPACE_ROOT=$(cygpath -u "$WORKSPACE_ROOT" 2>/dev/null || echo "$WORKSPACE_ROOT")
    fi
fi

# Windows-specific: Calculate safe parallel jobs more conservatively
calculate_safe_parallel_jobs_windows() {
    local cpu_cores=$(nproc)
    # On Windows, be more conservative with memory usage
    # LLVM compilation can use 2-3GB per job on Windows
    local mem_mb=$(grep MemTotal /proc/meminfo | awk '{print $2}')
    local mem_gb=$((mem_mb / 1024 / 1024))
    local safe_jobs=$((mem_gb / 3))  # Assume 3GB per job on Windows
    
    # Use minimum of CPU cores/2 and memory-safe jobs, but at least 2
    local max_jobs=$((cpu_cores / 2))
    if [ $safe_jobs -lt 2 ]; then
        echo 2
    elif [ $safe_jobs -gt $max_jobs ]; then
        echo $max_jobs
    else
        echo $safe_jobs
    fi
}

PARALLEL_JOBS=${PARALLEL_JOBS:-$(calculate_safe_parallel_jobs_windows)}

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if running in MSYS2/UCRT64
is_msys2_ucrt64() {
    if [[ "$MSYSTEM" == "UCRT64" ]]; then
        return 0
    else
        return 1
    fi
}

# Function to convert Windows path to MSYS2 path
to_msys_path() {
    if command_exists cygpath; then
        cygpath -u "$1"
    else
        echo "$1"
    fi
}

# Function to convert MSYS2 path to Windows path
to_windows_path() {
    if command_exists cygpath; then
        cygpath -w "$1"
    else
        echo "$1"
    fi
}

# Function to print section headers
print_section() {
    echo ""
    echo -e "${BLUE}===============================================${NC}"
    echo -e "${BLUE} $1${NC}"
    echo -e "${BLUE}===============================================${NC}"
    echo ""
}

# Function to print progress
print_progress() {
    echo -e "${CYAN}➤ $1${NC}"
}

# Function to print success
print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

# Function to print warning
print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

# Function to print error and exit
print_error() {
    echo -e "${RED}❌ $1${NC}"
    exit 1
}

# Function to print info
print_info() {
    echo -e "${CYAN}ℹ️  $1${NC}"
}

# Function to measure and report command execution time
time_command() {
    local cmd="$1"
    local description="${2:-Command execution}"
    
    print_progress "$description..."
    local start_time=$(date +%s)
    
    if eval "$cmd"; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        local minutes=$((duration / 60))
        local seconds=$((duration % 60))
        print_success "$description completed in ${minutes}m ${seconds}s"
        return 0
    else
        print_error "$description failed"
        return 1
    fi
}

# Function to check if running with administrator privileges (Windows)
check_admin_windows() {
    if [[ "$OSTYPE" == "msys" ]]; then
        # Check if we can write to Program Files (typical admin check)
        if touch "/c/Program Files/test_admin_$$.tmp" 2>/dev/null; then
            rm -f "/c/Program Files/test_admin_$$.tmp"
            return 0
        else
            return 1
        fi
    fi
    return 0
}

# Function to ensure directory exists and is writable
ensure_directory() {
    local dir="$1"
    local description="${2:-directory}"
    
    if [ ! -d "$dir" ]; then
        print_progress "Creating $description: $dir"
        mkdir -p "$dir" || print_error "Failed to create $description: $dir"
    fi
    
    if [ ! -w "$dir" ]; then
        print_error "$description is not writable: $dir"
    fi
}

# Function to backup a file before modification
backup_file() {
    local file="$1"
    if [ -f "$file" ]; then
        local backup="${file}.backup.$(date +%Y%m%d_%H%M%S)"
        cp "$file" "$backup"
        print_info "Backed up $file to $backup"
    fi
}

# Function to find MSYS2 root directory
find_msys2_root() {
    # Check common locations
    local possible_roots=(
        "/c/msys64"
        "/c/tools/msys64"
        "/c/msys2"
        "/"
    )
    
    for root in "${possible_roots[@]}"; do
        if [ -f "$root/usr/bin/pacman.exe" ] || [ -f "$root/usr/bin/pacman" ]; then
            echo "$root"
            return 0
        fi
    done
    
    # Fallback to root
    echo "/"
}

# Get MSYS2 root for the current installation
MSYS2_ROOT=$(find_msys2_root)

# Export common functions
export -f command_exists is_msys2_ucrt64 to_msys_path to_windows_path
export -f print_section print_progress print_success print_warning print_error print_info
export -f time_command check_admin_windows ensure_directory backup_file
export -f find_msys2_root