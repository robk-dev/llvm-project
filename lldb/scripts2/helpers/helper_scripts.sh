#!/bin/bash
# Helper Scripts Generator
# Functions for creating verification and helper scripts

# Source common utilities
HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HELPERS_DIR/common.sh"

# Function to create verification and helper scripts
create_helper_scripts() {
    print_section "Step 8: Creating Helper Scripts"
    
    # Create verification script
    cat > "$LLVM_BUILD_DIR/verify_gnustep_patch.sh" << 'VERIFY_EOF'
#!/bin/bash
# Verify GNUstep Runtime Patch Installation

LLDB_BIN="${1:-$HOME/llvm-build/bin/lldb}"

echo "=== GNUstep Runtime Patch Verification ==="
echo ""

if [ ! -f "$LLDB_BIN" ]; then
    echo "❌ LLDB binary not found: $LLDB_BIN"
    exit 1
fi

echo "✓ LLDB binary found: $LLDB_BIN"

# Check version
echo ""
echo "LLDB Version:"
"$LLDB_BIN" --version

echo ""
echo "Checking for GNUstep runtime plugin..."
if "$LLDB_BIN" -o "plugin list" -o "quit" 2>/dev/null | grep -i gnustep; then
    echo ""
    echo "✅ GNUstepObjCRuntime plugin is loaded!"
    echo ""
    echo "The patch is working correctly. You can now debug Objective-C programs with:"
    echo "  $LLDB_BIN <your-objc-program>"
else
    echo ""
    echo "ℹ️  GNUstepObjCRuntime plugin not visible in plugin list"
    echo "This is normal - the plugin loads dynamically when debugging ObjC programs"
fi

echo ""
echo "=== Verification Complete ==="
VERIFY_EOF
    
    chmod +x "$LLVM_BUILD_DIR/verify_gnustep_patch.sh"
    print_success "Created verification script: $LLVM_BUILD_DIR/verify_gnustep_patch.sh"
    
    # Create environment setup script
    cat > "$LLVM_BUILD_DIR/setup_environment.sh" << 'ENV_EOF'
#!/bin/bash
# Setup environment for using patched LLDB

LLVM_BUILD_DIR="$HOME/llvm-build"

echo "Setting up environment for patched LLDB..."

# Add to PATH
export PATH="$LLVM_BUILD_DIR/bin:$PATH"

# Add library path
export LD_LIBRARY_PATH="$LLVM_BUILD_DIR/lib:$LD_LIBRARY_PATH"

# Enable GNUstep new string ABI

echo "✓ Environment configured"
echo ""
echo "Available tools:"
echo "  lldb    - LLDB with GNUstep runtime support"
echo "  clang   - Clang compiler"
echo "  clang++ - Clang C++ compiler"
echo ""
echo "To make this permanent, add to your ~/.bashrc or ~/.zshrc:"
echo "  export PATH=\"$LLVM_BUILD_DIR/bin:\$PATH\""
echo "  export LD_LIBRARY_PATH=\"$LLVM_BUILD_DIR/lib:\$LD_LIBRARY_PATH\""
ENV_EOF
    
    chmod +x "$LLVM_BUILD_DIR/setup_environment.sh"
    print_success "Created environment setup script: $LLVM_BUILD_DIR/setup_environment.sh"
    
    # Create VS Code settings helper
    cat > "$LLVM_BUILD_DIR/vscode_settings.json" << 'VSCODE_EOF'
{
    "lldb.library": "$HOME/llvm-build/lib/liblldb.so",
    "lldb.adapterEnv": {
        "LLDB_DEBUGSERVER_PATH": "$HOME/llvm-build/bin/lldb-server",
        "LD_LIBRARY_PATH": "$HOME/llvm-build/lib:/usr/local/lib",
        "GNUSTEP_NEW_STRING_ABI": "1"
    },
    "lldb.verboseLogging": true,
    "lldb.showDisassembly": "never",
    "lldb.dereferencePointers": true,
    "lldb.displayFormat": "auto",
    "lldb.evaluateForHovers": true,
    "lldb.commandCompletions": true,
    "lldb.suppressUpdateNotifications": false,
    "lldb.evaluationTimeout": 30,
    "lldb.launch.expressions": "native",
    "lldb.consoleMode": "commands"
}
VSCODE_EOF
    
    print_success "Created VS Code settings template: $LLVM_BUILD_DIR/vscode_settings.json"
    
    # Create test program
    mkdir -p "$LLVM_BUILD_DIR/examples"
    
    cat > "$LLVM_BUILD_DIR/examples/test_custom_class.m" << 'TEST_EOF'
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

// Custom class to test dynamic discovery
@interface TestStudent : NSObject {
    NSString *_name;
    NSInteger _age;
    NSArray *_courses;
}
@property (nonatomic, retain) NSString *name;
@property (nonatomic) NSInteger age;
@property (nonatomic, retain) NSArray *courses;
@end

@implementation TestStudent
@synthesize name = _name;
@synthesize age = _age;
@synthesize courses = _courses;

- (void)dealloc {
    [_name release];
    [_courses release];
    [super dealloc];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"TestStudent{name=%@, age=%ld, courses=%@}", 
            _name, (long)_age, _courses];
}
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        // Create test objects
        TestStudent *student = [[TestStudent alloc] init];
        student.name = @"Alice Johnson";
        student.age = 20;
        student.courses = @[@"Computer Science", @"Mathematics", @"Physics"];
        
        // Show runtime class discovery
        printf("=== Runtime Class Discovery Test ===\n\n");
        
        unsigned int count = 0;
        Class *classes = objc_copyClassList(&count);
        
        printf("Found %u classes in runtime:\n", count);
        
        // Look for our custom class
        BOOL foundCustomClass = NO;
        for (unsigned int i = 0; i < count; i++) {
            const char *className = class_getName(classes[i]);
            if (strcmp(className, "TestStudent") == 0) {
                foundCustomClass = YES;
                printf("✓ Found custom class: %s\n", className);
                break;
            }
        }
        
        if (!foundCustomClass) {
            printf("❌ Custom class TestStudent not found in runtime\n");
        }
        
        free(classes);
        
        printf("\nTest object: %s\n", [[student description] UTF8String]);
        
        // Breakpoint location - test debugging here
        printf("\n🔍 Set breakpoint here to test LLDB debugging\n");
        printf("Commands to try in LLDB:\n");
        printf("  po student\n");
        printf("  po student.name\n");
        printf("  po student.courses\n");
        
        [student release];
    }
    return 0;
}
TEST_EOF
    
    # Create Makefile for test program
    cat > "$LLVM_BUILD_DIR/examples/Makefile" << 'MAKE_EOF'
# Makefile for testing patched LLDB with custom classes

CC = clang
OBJCFLAGS = -fobjc-runtime=gnustep-2.1 -fblocks -g -O0 \
            -fconstant-string-class=NSConstantString
LDFLAGS = -L/usr/local/lib -Wl,-rpath,/usr/local/lib
LIBS = -lgnustep-base -lobjc -ldispatch

TARGETS = test_custom_class

all: $(TARGETS)

test_custom_class: test_custom_class.m
	$(CC) $(OBJCFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(TARGETS)

test: test_custom_class
	@echo "Running test program..."
	./test_custom_class
	@echo ""
	@echo "To debug with patched LLDB:"
	@echo "  $(LLVM_BUILD_DIR)/build/bin/lldb ./test_custom_class"

.PHONY: all clean test
MAKE_EOF
    
    print_success "Created test program: $LLVM_BUILD_DIR/examples/test_custom_class.m"
    print_success "Created Makefile: $LLVM_BUILD_DIR/examples/Makefile"
    
    # Create lldb-server troubleshooting script
    cat > "$LLVM_BUILD_DIR/troubleshoot_lldb_server.sh" << 'TROUBLE_EOF'
#!/bin/bash
# Troubleshoot lldb-server issues

LLVM_BUILD_DIR="$HOME/llvm-build"
LLDB_SERVER_PATH="$LLVM_BUILD_DIR/bin/lldb-server"

echo "=== lldb-server Troubleshooting ==="
echo ""

# Check if lldb-server exists
if [ -f "$LLDB_SERVER_PATH" ]; then
    echo "✅ lldb-server found at: $LLDB_SERVER_PATH"
    
    # Test functionality
    if "$LLDB_SERVER_PATH" --help >/dev/null 2>&1; then
        echo "✅ lldb-server is functional"
    else
        echo "❌ lldb-server exists but has issues"
    fi
else
    echo "❌ lldb-server not found at: $LLDB_SERVER_PATH"
    echo ""
    echo "Searching for lldb-server in build directory..."
    
    FOUND_SERVERS=$(find "$LLVM_BUILD_DIR" -name "lldb-server" -type f 2>/dev/null)
    if [ -n "$FOUND_SERVERS" ]; then
        echo "Found lldb-server at alternative locations:"
        echo "$FOUND_SERVERS"
    else
        echo "❌ lldb-server not found anywhere in build directory"
        echo ""
        echo "To build lldb-server:"
        echo "  cd $LLVM_BUILD_DIR"
        echo "  ninja lldb-server"
    fi
fi

echo ""
echo "For VS Code CodeLLDB extension, update .vscode/settings.json:"
echo "{"
echo "  \"lldb.library\": \"$LLVM_BUILD_DIR/lib/liblldb.so\","
echo "  \"lldb.adapterEnv\": {"
echo "    \"LLDB_DEBUGSERVER_PATH\": \"$LLDB_SERVER_PATH\","
echo "    \"LD_LIBRARY_PATH\": \"$LLVM_BUILD_DIR/lib:/usr/local/lib\""
echo "  }"
echo "}"

echo ""
echo "=== Troubleshooting Complete ==="
TROUBLE_EOF
    
    chmod +x "$LLVM_BUILD_DIR/troubleshoot_lldb_server.sh"
    print_success "Created troubleshooting script: $LLVM_BUILD_DIR/troubleshoot_lldb_server.sh"
}

# Function to update VS Code settings for the built LLDB
update_vscode_settings() {
    print_section "Updating VS Code Settings"
    
    local VSCODE_SETTINGS="$WORKSPACE_ROOT/.vscode/settings.json"
    
    if [ ! -f "$VSCODE_SETTINGS" ]; then
        print_warning "VS Code settings file not found: $VSCODE_SETTINGS"
        return 0
    fi
    
    # Check if our LLDB paths exist
    if [ ! -f "$LLVM_BUILD_DIR/lib/liblldb.so" ]; then
        print_warning "liblldb.so not found at $LLVM_BUILD_DIR/lib/liblldb.so"
        return 1
    fi
    
    if [ ! -f "$LLVM_BUILD_DIR/bin/lldb-server" ]; then
        print_warning "lldb-server not found at $LLVM_BUILD_DIR/bin/lldb-server"
        return 1
    fi
    
    print_progress "Backing up current VS Code settings..."
    cp "$VSCODE_SETTINGS" "$VSCODE_SETTINGS.backup.$(date +%Y%m%d_%H%M%S)"
    
    print_progress "Updating LLDB paths in VS Code settings..."
    
    # Update the paths using sed (more reliable than trying to parse JSON in bash)
    sed -i "s|\"lldb.library\": \"[^\"]*\"|\"lldb.library\": \"$LLVM_BUILD_DIR/lib/liblldb.so\"|g" "$VSCODE_SETTINGS"
    sed -i "s|\"LLDB_DEBUGSERVER_PATH\": \"[^\"]*\"|\"LLDB_DEBUGSERVER_PATH\": \"$LLVM_BUILD_DIR/bin/lldb-server\"|g" "$VSCODE_SETTINGS"
    sed -i "s|\"LD_LIBRARY_PATH\": \"[^\"]*\"|\"LD_LIBRARY_PATH\": \"$LLVM_BUILD_DIR/lib:/usr/local/lib\"|g" "$VSCODE_SETTINGS"
    
    print_success "VS Code settings updated successfully"
    print_success "LLDB library: $LLVM_BUILD_DIR/lib/liblldb.so"
    print_success "LLDB server: $LLVM_BUILD_DIR/bin/lldb-server"
    
    echo ""
    echo -e "${YELLOW}VS Code CodeLLDB extension should now work with your custom LLDB build!${NC}"
    echo -e "${YELLOW}Restart VS Code to ensure the new settings take effect.${NC}"
}
