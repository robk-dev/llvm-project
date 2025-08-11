#!/bin/bash
# System requirements and dependency installation for Windows MSYS2/UCRT64
# Author: LLDB GNUstep Development Team
# Date: August 2025

# Source common utilities
HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HELPERS_DIR/common.sh"

# Function to setup MSYS2 UCRT64 PATH
setup_msys2_path() {
    print_progress "Setting up MSYS2 UCRT64 PATH..."
    
    # Find MSYS2 installation root
    local msys_root=$(find_msys2_root)
    print_info "MSYS2 root: $msys_root"
    
    # Set up proper PATH for UCRT64 environment
    local ucrt64_paths=(
        "$msys_root/ucrt64/bin"
        "$msys_root/usr/bin"
        "$msys_root/bin"
        "/ucrt64/bin"
        "/usr/bin"
        "/bin"
    )
    
    # Add UCRT64 paths to beginning of PATH if not already there
    for ucrt_path in "${ucrt64_paths[@]}"; do
        if [ -d "$ucrt_path" ] && [[ ":$PATH:" != *":$ucrt_path:"* ]]; then
            export PATH="$ucrt_path:$PATH"
            print_info "Added to PATH: $ucrt_path"
        fi
    done
    
    # Also check /mingw64/bin as fallback
    if [ -d "/mingw64/bin" ] && [[ ":$PATH:" != *":/mingw64/bin:"* ]]; then
        export PATH="/mingw64/bin:$PATH"
        print_info "Added to PATH (fallback): /mingw64/bin"
    fi
    
    print_info "Current PATH (first 5 entries):"
    echo "$PATH" | tr ':' '\n' | head -5 | sed 's/^/  /'
}

# Function to diagnose missing tools
diagnose_missing_tools() {
    local missing_tools=("$@")
    
    print_warning "Diagnosing missing tools..."
    
    for tool in "${missing_tools[@]}"; do
        print_info "Looking for $tool..."
        
        # Check common MSYS2 locations
        local possible_locations=(
            "/ucrt64/bin/$tool"
            "/ucrt64/bin/$tool.exe"
            "/mingw64/bin/$tool"
            "/mingw64/bin/$tool.exe"
            "/usr/bin/$tool"
            "/usr/bin/$tool.exe"
        )
        
        local found=false
        for location in "${possible_locations[@]}"; do
            if [ -f "$location" ]; then
                print_info "  Found at: $location"
                found=true
                break
            fi
        done
        
        if [ "$found" = false ]; then
            print_info "  Not found in standard locations"
            
            # Suggest packages to install
            case $tool in
                cmake)
                    print_info "  Install with: pacman -S mingw-w64-ucrt-x86_64-cmake"
                    ;;
                ninja)
                    print_info "  Install with: pacman -S mingw-w64-ucrt-x86_64-ninja"
                    ;;
                clang|clang++)
                    print_info "  Install with: pacman -S mingw-w64-ucrt-x86_64-clang"
                    ;;
                gcc|g++)
                    print_info "  Install with: pacman -S mingw-w64-ucrt-x86_64-gcc"
                    ;;
            esac
        fi
    done
}

# Function to check Windows environment
check_windows_environment() {
    print_progress "Checking Windows MSYS2/UCRT64 environment..."
    
    # Check if running in MSYS2
    if [[ "$OSTYPE" != "msys" ]] && [[ "$OSTYPE" != "cygwin" ]]; then
        print_warning "Not running in MSYS2/Cygwin environment"
        print_info "Detected OS type: $OSTYPE"
        print_info ""
        print_info "To fix this:"
        print_info "1. Open MSYS2 UCRT64 terminal (not regular Command Prompt)"
        print_info "2. Navigate to your script directory"
        print_info "3. Run the script again"
        print_info ""
    fi
    
    # Check MSYSTEM environment
    if [[ "$MSYSTEM" == "UCRT64" ]]; then
        print_success "Running in UCRT64 environment (recommended)"
    elif [[ "$MSYSTEM" == "MINGW64" ]]; then
        print_warning "Running in MINGW64 environment (UCRT64 recommended)"
        print_info "For best compatibility, use MSYS2 UCRT64 terminal"
    elif [[ "$MSYSTEM" == "MINGW32" ]]; then
        print_error "32-bit environment not supported. Please use UCRT64 or MINGW64"
    else
        print_warning "Unknown MSYSTEM: ${MSYSTEM:-not set}"
        print_info ""
        print_info "To fix this:"
        print_info "1. Close this terminal"
        print_info "2. Open 'MSYS2 UCRT64' from Start Menu (not 'MSYS2')"
        print_info "3. Run the script again"
        print_info ""
    fi
    
    # Check for pacman
    if ! command_exists pacman; then
        print_error "pacman not found. Are you running in MSYS2?"
    fi
    
    # Setup PATH for MSYS2 tools
    setup_msys2_path
    
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
    
    # Force refresh PATH after package installation
    print_progress "Refreshing PATH after package installation..."
    setup_msys2_path
    
    # Wait a moment for filesystem to sync
    sleep 2
    
    # Verify critical tools
    print_progress "Verifying installed tools..."
    local required_tools=(cmake ninja clang clang++ git make)
    local missing_tools=()
    local found_tools=()
    
    for tool in "${required_tools[@]}"; do
        # Try multiple ways to find the tool
        local tool_found=false
        
        # Method 1: command -v
        if command -v $tool >/dev/null 2>&1; then
            tool_found=true
            local tool_path=$(command -v $tool)
        # Method 2: which
        elif which $tool >/dev/null 2>&1; then
            tool_found=true
            local tool_path=$(which $tool)
        # Method 3: direct path check
        elif [ -f "/ucrt64/bin/$tool" ] || [ -f "/ucrt64/bin/$tool.exe" ]; then
            tool_found=true
            local tool_path="/ucrt64/bin/$tool"
        elif [ -f "/mingw64/bin/$tool" ] || [ -f "/mingw64/bin/$tool.exe" ]; then
            tool_found=true
            local tool_path="/mingw64/bin/$tool"
        fi
        
        if [ "$tool_found" = true ]; then
            found_tools+=("$tool")
            print_success "$tool found at: ${tool_path:-unknown}"
        else
            missing_tools+=("$tool")
        fi
    done
    
    if [ ${#missing_tools[@]} -gt 0 ]; then
        print_warning "Missing tools: ${missing_tools[*]}"
        diagnose_missing_tools "${missing_tools[@]}"
        
        print_info ""
        print_info "Manual PATH setup (if tools are installed but not found):"
        print_info "Run these commands in your terminal before running the script:"
        print_info ""
        print_info "  export PATH=\"/ucrt64/bin:/usr/bin:\$PATH\""
        print_info "  # Or for MINGW64:"
        print_info "  export PATH=\"/mingw64/bin:/usr/bin:\$PATH\""
        print_info ""
        print_info "Then verify with: which cmake ninja clang"
        print_info ""
        
        # Don't immediately exit, let user decide
        read -p "Continue anyway? Some tools might be found during the build. (y/N): " -n 1 -r
        echo ""
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_error "Build cancelled by user. Please install missing tools and try again."
        else
            print_warning "Continuing with missing tools - build may fail"
        fi
    else
        print_success "All required tools found!"
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
export -f setup_msys2_path diagnose_missing_tools check_windows_environment check_disk_space setup_ccache_windows
export -f install_windows_dependencies verify_python_windows