#!/bin/bash
# System requirements and dependency installation
# Author: GitHub Copilot
# Date: July 31, 2025

# Source common utilities
HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HELPERS_DIR/common.sh"

# Function to check disk space
check_disk_space() {
    print_section "Step 1: System Requirements Check"
    
    print_progress "Checking available disk space..."
    AVAILABLE_SPACE=$(df -BG "$HOME" | awk 'NR==2 {print $4}' | sed 's/G//')
    if [ "$AVAILABLE_SPACE" -lt 50 ]; then
        print_error "Insufficient disk space. Need at least 50GB, have ${AVAILABLE_SPACE}GB"
    fi
    print_success "Disk space: ${AVAILABLE_SPACE}GB available"
    
    print_progress "Checking available RAM..."
    TOTAL_RAM=$(free -g | awk '/^Mem:/{print $2}')
    if [ "$TOTAL_RAM" -lt 16 ]; then
        print_warning "Only ${TOTAL_RAM}GB RAM available. Build may be slow (recommend 16GB+)"
        echo "You can continue, but consider:"
        echo "  - Closing other applications"
        echo "  - Using fewer parallel jobs: export PARALLEL_JOBS=2"
        echo ""
        read -p "Continue anyway? (y/N): " -n 1 -r
        echo ""
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_error "Build cancelled by user"
        fi
    else
        print_success "RAM: ${TOTAL_RAM}GB"
    fi
    
    print_progress "Checking CPU cores..."
    CPU_CORES=$(nproc)
    print_success "CPU cores: ${CPU_CORES} (using ${PARALLEL_JOBS} parallel jobs - auto-calculated for memory safety)"
}

# Function to check CMake version
check_cmake_version() {
    if command_exists cmake; then
        CMAKE_VERSION=$(cmake --version | head -1 | awk '{print $3}')
        REQUIRED_VERSION="3.20.0"
        if [ "$(printf '%s\n' "$REQUIRED_VERSION" "$CMAKE_VERSION" | sort -V | head -n1)" = "$REQUIRED_VERSION" ]; then
            print_success "CMake version $CMAKE_VERSION meets requirement (>= $REQUIRED_VERSION)"
            return 0
        else
            print_warning "CMake version $CMAKE_VERSION is too old (need >= $REQUIRED_VERSION)"
            return 1
        fi
    else
        print_warning "CMake not found"
        return 1
    fi
}

# Function to install dependencies
install_dependencies() {
    print_section "Step 2: Installing Build Dependencies"
    
    print_progress "Updating package lists..."
    sudo apt-get update -qq
    
    print_progress "Installing build essentials..."
    sudo apt-get install -y build-essential git python3 python3-dev
    
    print_progress "Installing LLVM build dependencies..."
    sudo apt-get install -y \
        cmake \
        ninja-build \
        python3-setuptools \
        swig \
        libedit-dev \
        libncurses5-dev \
        libxml2-dev \
        liblzma-dev \
        libz-dev \
        libtinfo-dev \
        pkg-config \
        ccache
    
    # Check CMake version and install newer version if needed
    if ! check_cmake_version; then
        print_progress "Installing newer CMake version..."
        CMAKE_VERSION="3.25.0"
        CMAKE_URL="https://cmake.org/files/v3.25/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz"
        
        cd /tmp
        wget -q "$CMAKE_URL" -O cmake.tar.gz
        sudo tar -xf cmake.tar.gz -C /opt/
        sudo ln -sf "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/cmake" /usr/local/bin/cmake
        sudo ln -sf "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/ctest" /usr/local/bin/ctest
        
        if check_cmake_version; then
            print_success "CMake updated successfully"
        else
            print_error "Failed to update CMake"
        fi
    fi
    
    print_success "All dependencies installed"
    
    # Show ccache status
    if command_exists ccache; then
        print_progress "Checking ccache status..."
        ccache -s | head -3
        print_success "ccache is available (will speed up rebuilds significantly)"
    fi
}

# Export functions for use in other scripts
export -f check_disk_space check_cmake_version install_dependencies
