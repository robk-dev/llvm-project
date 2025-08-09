#!/bin/bash
# System requirements and dependency installation for Windows MSYS2/UCRT64
# Author: LLDB GNUstep Development Team
# Date: August 2025

# Source common utilities
HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HELPERS_DIR/common.sh"

# Function to check Windows environment
check_windows_environment() {
    print_progress "Checking Windows MSYS2/UCRT64 environment..."
    
    # Check if running in MSYS2
    if [[ "$OSTYPE" != "msys" ]] && [[ "$OSTYPE" != "cygwin" ]]; then
        print_warning "Not running in MSYS2/Cygwin environment"
        print_info "Detected OS type: $OSTYPE"
    fi
    
    # Check MSYSTEM environment
    if [[ "$MSYSTEM" == "UCRT64" ]]; then
        print_success "Running in UCRT64 environment (recommended)"
    elif [[ "$MSYSTEM" == "MINGW64" ]]; then
        print_warning "Running in MINGW64 environment (UCRT64 recommended)"
    elif [[ "$MSYSTEM" == "MINGW32" ]]; then
        print_error "32-bit environment not supported. Please use UCRT64 or MINGW64"
    else
        print_warning "Unknown MSYSTEM: ${MSYSTEM:-not set}"
    fi
    
    # Check for pacman
    if ! command_exists pacman; then
        print_error "pacman not found. Are you running in MSYS2?"
    fi
    
    print_success "Windows MSYS2 environment check passed"
}

# Function to check disk space on Windows
check_disk_space() {
    print_progress "Checking available disk space..."
    
    # Get the drive where we're building
    local build_drive=$(echo "$PROJECT_ROOT" | cut -d'/' -f2)
    
    # Use df to check space (works in MSYS2)
    local available_gb=$(df -BG "/$build_drive" 2>/dev/null | awk 'NR==2 {print $4}' | sed 's/G//')
    
    if [ -z "$available_gb" ]; then
        print_warning "Could not determine available disk space"
        read -p "Continue anyway? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_error "Build cancelled by user"
        fi
    elif [ "$available_gb" -lt 50 ]; then
        print_error "Insufficient disk space. Need at least 50GB, have ${available_gb}GB"
    else
        print_success "Disk space: ${available_gb}GB available"
    fi
    
    # Check RAM on Windows
    print_progress "Checking available RAM..."
    local total_ram_kb=$(grep MemTotal /proc/meminfo | awk '{print $2}')
    local total_ram_gb=$((total_ram_kb / 1024 / 1024))
    
    if [ "$total_ram_gb" -lt 8 ]; then
        print_warning "Only ${total_ram_gb}GB RAM available. Build may be slow (recommend 16GB+)"
        print_info "Windows builds are memory intensive. Consider:"
        echo "  - Closing other applications"
        echo "  - Using fewer parallel jobs: export PARALLEL_JOBS=2"
        echo "  - Enabling Windows page file if not already"
        echo ""
        read -p "Continue anyway? (y/N): " -n 1 -r
        echo ""
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_error "Build cancelled by user"
        fi
    else
        print_success "RAM: ${total_ram_gb}GB"
    fi
    
    print_progress "Checking CPU cores..."
    local cpu_cores=$(nproc)
    print_success "CPU cores: ${cpu_cores} (using ${PARALLEL_JOBS} parallel jobs)"
}

# Function to check and install ccache on Windows
setup_ccache_windows() {
    print_progress "Setting up ccache for faster rebuilds..."
    
    if ! command_exists ccache; then
        print_progress "Installing ccache..."
        pacman -S --noconfirm --needed mingw-w64-ucrt-x86_64-ccache || {
            print_warning "Failed to install ccache, continuing without it"
            return 1
        }
    fi
    
    # Configure ccache for Windows
    if command_exists ccache; then
        # Set ccache directory to project-local to avoid permission issues
        export CCACHE_DIR="$PROJECT_ROOT/.ccache"
        mkdir -p "$CCACHE_DIR"
        
        # Configure ccache settings
        ccache --set-config max_size=10G
        ccache --set-config compiler_check=content
        ccache --set-config sloppiness=pch_defines,time_macros,include_file_mtime,include_file_ctime
        
        # Show ccache status
        print_info "ccache status:"
        ccache -s | head -5
        
        print_success "ccache configured (cache dir: $CCACHE_DIR)"
        
        # Export for build
        export CC="ccache clang"
        export CXX="ccache clang++"
    fi
}

# Function to install Windows MSYS2/UCRT64 dependencies
install_windows_dependencies() {
    print_progress "Updating MSYS2 package database..."
    pacman -Sy --noconfirm
    
    print_progress "Installing essential build tools..."
    local essential_packages=(
        base-devel
        mingw-w64-ucrt-x86_64-toolchain
        mingw-w64-ucrt-x86_64-cmake
        mingw-w64-ucrt-x86_64-ninja
        mingw-w64-ucrt-x86_64-python
        mingw-w64-ucrt-x86_64-python-pip
        git
        make
    )
    
    for pkg in "${essential_packages[@]}"; do
        print_progress "Installing $pkg..."
        pacman -S --noconfirm --needed $pkg || print_warning "Failed to install $pkg"
    done
    
    print_progress "Installing LLVM build dependencies..."
    local llvm_packages=(
        mingw-w64-ucrt-x86_64-clang
        mingw-w64-ucrt-x86_64-lld
        mingw-w64-ucrt-x86_64-libedit
        mingw-w64-ucrt-x86_64-libxml2
        mingw-w64-ucrt-x86_64-libffi
        mingw-w64-ucrt-x86_64-zlib
        mingw-w64-ucrt-x86_64-zstd
        mingw-w64-ucrt-x86_64-ncurses
        mingw-w64-ucrt-x86_64-swig
        mingw-w64-ucrt-x86_64-ccache
    )
    
    for pkg in "${llvm_packages[@]}"; do
        print_progress "Installing $pkg..."
        pacman -S --noconfirm --needed $pkg || print_warning "Failed to install $pkg"
    done
    
    print_progress "Installing GNUstep dependencies..."
    local gnustep_packages=(
        mingw-w64-ucrt-x86_64-libdispatch
        mingw-w64-ucrt-x86_64-libiconv
        mingw-w64-ucrt-x86_64-icu
        mingw-w64-ucrt-x86_64-libxslt
        mingw-w64-ucrt-x86_64-gnutls
        mingw-w64-ucrt-x86_64-libjpeg-turbo
        mingw-w64-ucrt-x86_64-libtiff
        mingw-w64-ucrt-x86_64-libpng
        autoconf
        automake
        libtool
    )
    
    for pkg in "${gnustep_packages[@]}"; do
        print_progress "Installing $pkg..."
        pacman -S --noconfirm --needed $pkg || print_warning "Failed to install $pkg"
    done
    
    # Setup ccache
    setup_ccache_windows
    
    # Verify critical tools
    print_progress "Verifying installed tools..."
    local required_tools=(cmake ninja clang clang++ git make)
    local missing_tools=()
    
    for tool in "${required_tools[@]}"; do
        if ! command_exists $tool; then
            missing_tools+=("$tool")
        fi
    done
    
    if [ ${#missing_tools[@]} -gt 0 ]; then
        print_error "Missing required tools: ${missing_tools[*]}"
    fi
    
    # Check CMake version
    if command_exists cmake; then
        local cmake_version=$(cmake --version | head -1 | awk '{print $3}')
        print_success "CMake version: $cmake_version"
        
        # Check if version is sufficient (need 3.20+)
        local required_version="3.20.0"
        if [ "$(printf '%s\n' "$required_version" "$cmake_version" | sort -V | head -n1)" != "$required_version" ]; then
            print_error "CMake version $cmake_version is too old (need >= $required_version)"
        fi
    fi
    
    print_success "All dependencies installed successfully"
    
    # Set up environment variables for the build
    export CC=clang
    export CXX=clang++
    export CFLAGS="-O2 -g"
    export CXXFLAGS="-O2 -g"
    
    print_info "Build environment configured:"
    echo "  CC=$CC"
    echo "  CXX=$CXX"
    echo "  PARALLEL_JOBS=$PARALLEL_JOBS"
}

# Function to verify Python setup on Windows
verify_python_windows() {
    print_progress "Verifying Python setup..."
    
    if command_exists python3; then
        local python_version=$(python3 --version 2>&1 | awk '{print $2}')
        print_success "Python3 version: $python_version"
    elif command_exists python; then
        local python_version=$(python --version 2>&1 | awk '{print $2}')
        print_success "Python version: $python_version"
        # Create python3 symlink if needed
        if ! command_exists python3; then
            print_progress "Creating python3 symlink..."
            ln -sf $(which python) /usr/local/bin/python3
        fi
    else
        print_error "Python not found. Please install mingw-w64-ucrt-x86_64-python"
    fi
    
    # Check for required Python modules
    print_progress "Checking Python modules..."
    python3 -c "import sys; print(f'Python path: {sys.executable}')"
}

# Export functions
export -f check_windows_environment check_disk_space setup_ccache_windows
export -f install_windows_dependencies verify_python_windows