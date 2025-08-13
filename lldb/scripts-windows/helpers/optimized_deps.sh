#!/bin/bash
# Optimized dependency checker for MSYS2/UCRT64

# Check what's already available vs what needs installing
check_existing_packages() {
    print_progress "Analyzing current package state..."
    
    # Check if we're in the right environment
    if [[ "$MSYSTEM" != "UCRT64" ]]; then
        print_error "Please run this in UCRT64 environment, not $MSYSTEM"
        print_info "Solution: Open 'MSYS2 UCRT64' terminal from Start Menu"
        exit 1
    fi
    
    local installed_packages=$(pacman -Q | awk '{print $1}')
    
    # Essential packages we definitely need
    local essential_packages=(
        "base-devel"
        "mingw-w64-ucrt-x86_64-toolchain" 
        "mingw-w64-ucrt-x86_64-cmake"
        "mingw-w64-ucrt-x86_64-ninja"
        "mingw-w64-ucrt-x86_64-clang"
        "mingw-w64-ucrt-x86_64-python"
        "git"
    )
    
    # LLVM-specific packages (minimal set)
    local llvm_packages=(
        "mingw-w64-ucrt-x86_64-lld"
        "mingw-w64-ucrt-x86_64-libxml2"  
        "mingw-w64-ucrt-x86_64-libffi"
        "libedit-devel"                  # Need development headers
    )
    
    # Performance packages
    local performance_packages=(
        "mingw-w64-ucrt-x86_64-ccache"
    )
    
    # Check what we actually need to install
    local needed_packages=()
    local already_installed=()
    
    for pkg in "${essential_packages[@]}" "${llvm_packages[@]}" "${performance_packages[@]}"; do
        if echo "$installed_packages" | grep -q "^${pkg}$"; then
            already_installed+=("$pkg")
        else
            # Check if there's a base version (e.g., zlib vs mingw-w64-ucrt-x86_64-zlib)
            local base_name=$(echo "$pkg" | sed 's/mingw-w64-ucrt-x86_64-//')
            if echo "$installed_packages" | grep -q "^${base_name}$"; then
                print_info "Base version of $pkg already installed: $base_name"
                already_installed+=("$pkg (base: $base_name)")
            else
                needed_packages+=("$pkg")
            fi
        fi
    done
    
    print_success "Already installed: ${#already_installed[@]} packages"
    for pkg in "${already_installed[@]}"; do
        echo "  ✓ $pkg"
    done
    
    print_info "Need to install: ${#needed_packages[@]} packages"
    for pkg in "${needed_packages[@]}"; do
        echo "  → $pkg"
    done
    
    # Calculate download size estimate
    if [[ ${#needed_packages[@]} -gt 0 ]]; then
        print_info "Estimating download size..."
        local total_size=$(pacman -Si "${needed_packages[@]}" 2>/dev/null | grep "Download Size" | awk '{sum += $4} END {print sum/1024}')
        if [[ -n "$total_size" ]]; then
            printf "Estimated download: %.1f MB\n" "$total_size"
        fi
    fi
    
    export NEEDED_PACKAGES="${needed_packages[*]}"
    export PACKAGES_COUNT=${#needed_packages[@]}
}

# Install only what we actually need
install_optimized_dependencies() {
    check_existing_packages
    
    if [[ $PACKAGES_COUNT -eq 0 ]]; then
        print_success "All required packages already installed!"
        return 0
    fi
    
    print_progress "Installing $PACKAGES_COUNT packages..."
    
    # Update package database first
    pacman -Sy --noconfirm
    
    # Install in groups to handle dependencies better
    local essential_first=(base-devel git)
    local toolchain_second=()
    local llvm_third=()
    
    # Sort packages by install order
    for pkg in $NEEDED_PACKAGES; do
        case $pkg in
            *toolchain*|*clang*|*cmake*|*ninja*|*python*)
                toolchain_second+=("$pkg")
                ;;
            *libxml2*|*libffi*|*zlib*|*zstd*|*lld*|libedit*)
                llvm_third+=("$pkg") 
                ;;
            *)
                essential_first+=("$pkg")
                ;;
        esac
    done
    
    # Install in phases
    if [[ ${#essential_first[@]} -gt 0 ]]; then
        print_progress "Phase 1: Essential tools"
        pacman -S --noconfirm --needed "${essential_first[@]}"
    fi
    
    if [[ ${#toolchain_second[@]} -gt 0 ]]; then
        print_progress "Phase 2: Toolchain" 
        pacman -S --noconfirm --needed "${toolchain_second[@]}"
    fi
    
    if [[ ${#llvm_third[@]} -gt 0 ]]; then
        print_progress "Phase 3: LLVM dependencies"
        pacman -S --noconfirm --needed "${llvm_third[@]}"
    fi
    
    print_success "Optimized dependency installation complete!"
}
