#!/bin/bash
# Apple Pattern Patches Helper Script
# Handles ObjCLanguage.cpp patching and integration

# Source common utilities
HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HELPERS_DIR/common.sh"

# Function to apply Apple pattern patches
apply_apple_pattern_patches() {
    print_section "Step 4b: Applying Apple Pattern Patches"
    
    local LLVM_PROJECT_DIR="$LLVM_BUILD_DIR/llvm-project"
    local OBJC_LANGUAGE_FILE="$LLVM_PROJECT_DIR/lldb/source/Plugins/Language/ObjC/ObjCLanguage.cpp"
    local PATCH_APPLIED_MARKER="$LLVM_PROJECT_DIR/.gnustep_apple_pattern_applied"
    
    # Check if patches already applied
    if [ -f "$PATCH_APPLIED_MARKER" ]; then
        print_success "Apple pattern patches already applied"
        return 0
    fi
    
    # Verify ObjCLanguage.cpp exists
    if [ ! -f "$OBJC_LANGUAGE_FILE" ]; then
        print_error "ObjCLanguage.cpp not found: $OBJC_LANGUAGE_FILE"
        return 1
    fi
    
    print_progress "Backing up original ObjCLanguage.cpp..."
    cp "$OBJC_LANGUAGE_FILE" "$OBJC_LANGUAGE_FILE.backup"
    
    # Apply GNUstep registration patch
    apply_gnustep_language_registration "$OBJC_LANGUAGE_FILE"
    
    # Add forward declarations
    apply_gnustep_forward_declarations "$OBJC_LANGUAGE_FILE"
    
    # Create patch marker
    echo "$(date): Apple pattern patches applied" > "$PATCH_APPLIED_MARKER"
    
    print_success "Apple pattern patches applied successfully"
}

# Function to add GNUstep registration to ObjCLanguage.cpp
apply_gnustep_language_registration() {
    local objc_file="$1"
    
    print_progress "Adding GNUstep formatter registration..."
    
    # Find the insertion point (after LoadObjCFormatters function)
    local insertion_line=$(grep -n "^static void LoadObjCFormatters" "$objc_file" | tail -1 | cut -d: -f1)
    
    if [ -z "$insertion_line" ]; then
        print_error "Could not find LoadObjCFormatters function in ObjCLanguage.cpp"
        return 1
    fi
    
    # Find the end of LoadObjCFormatters function (closing brace)
    local function_end=$(tail -n +$insertion_line "$objc_file" | grep -n "^}" | head -1 | cut -d: -f1)
    function_end=$((insertion_line + function_end - 1))
    
    print_progress "Inserting GNUstep registration at line $function_end..."
    
    # Create temporary file with GNUstep registration
    cat > /tmp/gnustep_registration.cpp << 'EOF'

// GNUstep Formatter Registration (Added by Apple Pattern Migration)
static void LoadGNUstepFormatters(TypeCategoryImplSP objc_category_sp) {
    if (!objc_category_sp)
        return;
        
    TypeSummaryImpl::Flags stl_summary_flags;
    stl_summary_flags.SetCascades(true)
                     .SetSkipPointers(false)
                     .SetSkipReferences(false)
                     .SetDontShowChildren(true)
                     .SetDontShowValue(false)
                     .SetShowMembersOneLiner(false)
                     .SetHideItemNames(false);
    
    // GNUstep Array Summary Providers
    AddCXXSummary(objc_category_sp,
        lldb_private::formatters::GNUstepArraySummaryProvider,
        "GNUstep NSArray summary provider", "GSArray",
        stl_summary_flags);
        
    AddCXXSummary(objc_category_sp,
        lldb_private::formatters::GNUstepArraySummaryProvider,
        "GNUstep NSArray summary provider", "__NSArray0", 
        stl_summary_flags);
        
    AddCXXSummary(objc_category_sp,
        lldb_private::formatters::GNUstepArraySummaryProvider,
        "GNUstep NSMutableArray summary provider", "GSMutableArray",
        stl_summary_flags);
        
    AddCXXSummary(objc_category_sp,
        lldb_private::formatters::GNUstepArraySummaryProvider,
        "GNUstep NSArray summary provider", "GSInlineArray",
        stl_summary_flags);
    
    // GNUstep Array Synthetic Providers  
    AddCXXSynthetic(objc_category_sp,
        lldb_private::formatters::GNUstepArraySyntheticFrontEndCreator,
        "GNUstep NSArray synthetic children", "GSArray",
        ScriptedSyntheticChildren::Flags());
        
    AddCXXSynthetic(objc_category_sp,
        lldb_private::formatters::GNUstepArraySyntheticFrontEndCreator,
        "GNUstep NSArray synthetic children", "__NSArray0",
        ScriptedSyntheticChildren::Flags());
        
    AddCXXSynthetic(objc_category_sp,
        lldb_private::formatters::GNUstepArraySyntheticFrontEndCreator,
        "GNUstep NSMutableArray synthetic children", "GSMutableArray", 
        ScriptedSyntheticChildren::Flags());
        
    AddCXXSynthetic(objc_category_sp,
        lldb_private::formatters::GNUstepArraySyntheticFrontEndCreator,
        "GNUstep NSArray synthetic children", "GSInlineArray",
        ScriptedSyntheticChildren::Flags());
}
EOF

    # Insert GNUstep registration function before the LoadObjCFormatters closing brace
    head -n $((function_end - 1)) "$objc_file" > /tmp/objc_language_part1.cpp
    cat /tmp/gnustep_registration.cpp >> /tmp/objc_language_part1.cpp
    tail -n +$function_end "$objc_file" >> /tmp/objc_language_part1.cpp
    
    # Replace original file
    mv /tmp/objc_language_part1.cpp "$objc_file"
    
    # Add call to LoadGNUstepFormatters inside LoadObjCFormatters
    sed -i '/LoadRuntimeSpecificFormatters/a \  LoadGNUstepFormatters(objc_category_sp);' "$objc_file"
    
    print_success "GNUstep registration added to LoadObjCFormatters"
}

# Function to add GNUstep forward declarations
apply_gnustep_forward_declarations() {
    local objc_file="$1"
    
    print_progress "Adding GNUstep forward declarations..."
    
    # Find namespace lldb_private { namespace formatters {
    local formatters_ns_line=$(grep -n "namespace formatters" "$objc_file" | head -1 | cut -d: -f1)
    
    if [ -z "$formatters_ns_line" ]; then
        print_error "Could not find formatters namespace in ObjCLanguage.cpp"
        return 1
    fi
    
    # Find the end of existing forward declarations (look for first function declaration)
    local insert_point=$(tail -n +$formatters_ns_line "$objc_file" | grep -n "^[a-zA-Z].*(" | head -1 | cut -d: -f1)
    insert_point=$((formatters_ns_line + insert_point - 2))
    
    # Create forward declarations
    cat > /tmp/gnustep_forwards.cpp << 'EOF'

// GNUstep formatter forward declarations (Added by Apple Pattern Migration)
bool GNUstepArraySummaryProvider(ValueObject &valobj, Stream &stream,
                                const TypeSummaryOptions &options);

SyntheticChildrenFrontEnd *
GNUstepArraySyntheticFrontEndCreator(CXXSyntheticChildren *, 
                                    lldb::ValueObjectSP);
EOF

    # Insert forward declarations
    head -n $insert_point "$objc_file" > /tmp/objc_language_part2.cpp
    cat /tmp/gnustep_forwards.cpp >> /tmp/objc_language_part2.cpp
    tail -n +$((insert_point + 1)) "$objc_file" >> /tmp/objc_language_part2.cpp
    
    # Replace original file
    mv /tmp/objc_language_part2.cpp "$objc_file"
    
    print_success "GNUstep forward declarations added"
}

# Function to create ObjCLanguage patch directory
create_objc_language_patch_dir() {
    print_progress "Creating ObjCLanguage patch directory..."
    
    local PATCH_DIR="$WORKSPACE_ROOT/llvm_patch/ObjCLanguage"
    mkdir -p "$PATCH_DIR"
    
    # Create patch metadata
    cat > "$PATCH_DIR/README.md" << 'EOF'
# ObjCLanguage.cpp Patches

This directory contains patches for integrating GNUstep formatters into LLDB's ObjCLanguage plugin.

## Files
- `objc_language_gnustep.patch` - Git patch file for ObjCLanguage.cpp modifications
- `patch_validation.sh` - Script to validate patch application

## Application
These patches are automatically applied by `scripts/helpers/apple_pattern_patches.sh`
during the build process.
EOF
    
    print_success "ObjCLanguage patch directory created: $PATCH_DIR"
}

# Function to verify Apple pattern patches
verify_apple_pattern_patches() {
    print_section "Verifying Apple Pattern Patches"
    
    local LLVM_PROJECT_DIR="$LLVM_BUILD_DIR/llvm-project"
    local OBJC_LANGUAGE_FILE="$LLVM_PROJECT_DIR/lldb/source/Plugins/Language/ObjC/ObjCLanguage.cpp"
    
    # Check if GNUstep registration exists
    if grep -q "LoadGNUstepFormatters" "$OBJC_LANGUAGE_FILE"; then
        print_success "✅ GNUstep registration function found"
    else
        print_error "❌ GNUstep registration function missing"
        return 1
    fi
    
    # Check if forward declarations exist
    if grep -q "GNUstepArraySummaryProvider" "$OBJC_LANGUAGE_FILE"; then
        print_success "✅ GNUstep forward declarations found"
    else
        print_error "❌ GNUstep forward declarations missing"
        return 1
    fi
    
    # Check if LoadGNUstepFormatters is called
    if grep -q "LoadGNUstepFormatters(objc_category_sp)" "$OBJC_LANGUAGE_FILE"; then
        print_success "✅ LoadGNUstepFormatters call found"
    else
        print_error "❌ LoadGNUstepFormatters call missing"
        return 1
    fi
    
    print_success "Apple pattern patches verified successfully"
}

# Function to rollback Apple pattern patches  
rollback_apple_pattern_patches() {
    print_section "Rolling back Apple Pattern Patches"
    
    local LLVM_PROJECT_DIR="$LLVM_BUILD_DIR/llvm-project"
    local OBJC_LANGUAGE_FILE="$LLVM_PROJECT_DIR/lldb/source/Plugins/Language/ObjC/ObjCLanguage.cpp"
    local BACKUP_FILE="$OBJC_LANGUAGE_FILE.backup"
    local PATCH_MARKER="$LLVM_PROJECT_DIR/.gnustep_apple_pattern_applied"
    
    if [ -f "$BACKUP_FILE" ]; then
        print_progress "Restoring original ObjCLanguage.cpp..."
        mv "$BACKUP_FILE" "$OBJC_LANGUAGE_FILE"
        print_success "Original ObjCLanguage.cpp restored"
    else
        print_warning "No backup file found, cannot rollback"
    fi
    
    # Remove patch marker
    rm -f "$PATCH_MARKER"
    
    print_success "Apple pattern patches rolled back"
}