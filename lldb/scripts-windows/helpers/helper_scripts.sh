#!/bin/bash
# Helper script generation for Windows MSYS2 LLDB/GNUstep environment

set -euo pipefail

# Source common utilities
HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HELPERS_DIR/common.sh"

# Function to verify Windows installation
verify_windows_installation() {
    print_section "Verifying LLDB Installation on Windows"
    
    local all_good=true
    
    # Check LLDB binary
    if [ -f "$LLVM_BUILD_DIR/build/bin/lldb.exe" ]; then
        print_success "✓ LLDB binary found"
        
        # Try to get version
        if "$LLVM_BUILD_DIR/build/bin/lldb.exe" --version 2>/dev/null; then
            print_success "✓ LLDB executes correctly"
        else
            print_warning "⚠ LLDB found but couldn't get version"
            all_good=false
        fi
    else
        print_error "✗ LLDB binary not found"
        all_good=false
    fi
    
    # Check lldb-server
    if [ -f "$LLVM_BUILD_DIR/build/bin/lldb-server.exe" ]; then
        print_success "✓ lldb-server found"
    else
        print_warning "⚠ lldb-server not found (needed for remote debugging)"
    fi
    
    # Check Clang
    if [ -f "$LLVM_BUILD_DIR/build/bin/clang.exe" ]; then
        print_success "✓ Clang compiler found"
        "$LLVM_BUILD_DIR/build/bin/clang.exe" --version | head -1
    else
        print_error "✗ Clang compiler not found"
        all_good=false
    fi
    
    # Check GNUstep libraries
    if [ -d "$GNUSTEP_INSTALL_DIR/lib" ]; then
        print_success "✓ GNUstep libraries found"
        ls -la "$GNUSTEP_INSTALL_DIR/lib/libobjc*" 2>/dev/null | head -3
    else
        print_warning "⚠ GNUstep libraries not found"
    fi
    
    # Check patch application
    local patch_file="$PROJECT_ROOT/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp"
    if [ -f "$patch_file" ]; then
        print_success "✓ GNUstep runtime patch applied"
    else
        print_error "✗ GNUstep runtime patch not found"
        all_good=false
    fi
    
    if $all_good; then
        print_success "Installation verification passed!"
    else
        print_warning "Some components missing or not working properly"
        print_info "You may need to rebuild or check the build logs"
    fi
}

# Function to create test programs
create_test_programs() {
    print_section "Creating Test Programs"
    
    local examples_dir="$LLVM_BUILD_DIR/examples"
    ensure_directory "$examples_dir" "examples directory"
    
    # Create simple Objective-C test
    cat > "$examples_dir/test_simple.m" << 'EOF'
#import <objc/runtime.h>
#import <stdio.h>

@interface SimpleClass : NSObject
@property (nonatomic, assign) int value;
- (void)printMessage;
@end

@implementation SimpleClass
- (void)printMessage {
    printf("Hello from SimpleClass! Value: %d\n", self.value);
}
@end

int main() {
    SimpleClass *obj = [[SimpleClass alloc] init];
    obj.value = 42;
    [obj printMessage];
    
    printf("Class name: %s\n", class_getName([obj class]));
    printf("Windows LLDB test successful!\n");
    return 0;
}
EOF
    
    # Create custom class test
    cat > "$examples_dir/test_custom_class.m" << 'EOF'
#import <objc/runtime.h>
#import <stdio.h>
#import <stdlib.h>

@interface BankAccount : NSObject {
    double _balance;
    int _accountNumber;
}
@property (nonatomic, assign) double balance;
@property (nonatomic, assign) int accountNumber;
- (void)deposit:(double)amount;
- (void)withdraw:(double)amount;
- (void)printInfo;
@end

@implementation BankAccount

@synthesize balance = _balance;
@synthesize accountNumber = _accountNumber;

- (id)init {
    self = [super init];
    if (self) {
        _balance = 0.0;
        _accountNumber = rand() % 10000;
    }
    return self;
}

- (void)deposit:(double)amount {
    _balance += amount;
    printf("Deposited $%.2f. New balance: $%.2f\n", amount, _balance);
}

- (void)withdraw:(double)amount {
    if (_balance >= amount) {
        _balance -= amount;
        printf("Withdrew $%.2f. New balance: $%.2f\n", amount, _balance);
    } else {
        printf("Insufficient funds!\n");
    }
}

- (void)printInfo {
    printf("Account #%d - Balance: $%.2f\n", _accountNumber, _balance);
}

@end

int main() {
    printf("Testing custom BankAccount class...\n");
    
    BankAccount *account = [[BankAccount alloc] init];
    [account printInfo];
    [account deposit:1000.00];
    [account withdraw:250.00];
    [account printInfo];
    
    // Test runtime introspection
    unsigned int count;
    Method *methods = class_copyMethodList([BankAccount class], &count);
    printf("\nBankAccount methods (%u total):\n", count);
    for (unsigned int i = 0; i < count; i++) {
        SEL selector = method_getName(methods[i]);
        printf("  - %s\n", sel_getName(selector));
    }
    free(methods);
    
    printf("\nCustom class test completed!\n");
    return 0;
}
EOF
    
    # Create Makefile for building tests
    cat > "$examples_dir/Makefile" << EOF
# Makefile for LLDB GNUstep test programs on Windows

CC = $LLVM_BUILD_DIR/build/bin/clang.exe
OBJC_FLAGS = -fobjc-runtime=gnustep-2.0 -fblocks -g -O0
INCLUDES = -I$GNUSTEP_INSTALL_DIR/include
LIBS = -L$GNUSTEP_INSTALL_DIR/lib -lobjc
LDFLAGS = -fuse-ld=lld

TARGETS = test_simple.exe test_custom_class.exe

all: \$(TARGETS)

test_simple.exe: test_simple.m
	\$(CC) \$(OBJC_FLAGS) \$(INCLUDES) test_simple.m \$(LIBS) \$(LDFLAGS) -o test_simple.exe

test_custom_class.exe: test_custom_class.m
	\$(CC) \$(OBJC_FLAGS) \$(INCLUDES) test_custom_class.m \$(LIBS) \$(LDFLAGS) -o test_custom_class.exe

clean:
	rm -f \$(TARGETS) *.o

run: all
	./test_simple.exe
	./test_custom_class.exe

debug: test_custom_class.exe
	$LLVM_BUILD_DIR/build/bin/lldb.exe test_custom_class.exe

.PHONY: all clean run debug
EOF
    
    print_success "Test programs created in $examples_dir"
    print_info "To build: cd $examples_dir && make"
    print_info "To debug: cd $examples_dir && make debug"
}

# Function to create Windows helper scripts
create_windows_helper_scripts() {
    print_section "Creating Windows Helper Scripts"
    
    # Create environment setup script
    cat > "$LLVM_BUILD_DIR/setup_environment.sh" << EOF
#!/bin/bash
# LLDB/GNUstep Environment Setup for Windows MSYS2

export LLVM_BUILD_DIR="$LLVM_BUILD_DIR"
export GNUSTEP_INSTALL_DIR="$GNUSTEP_INSTALL_DIR"
export PATH="$LLVM_BUILD_DIR/build/bin:\$PATH"
export LD_LIBRARY_PATH="$GNUSTEP_INSTALL_DIR/lib:\$LD_LIBRARY_PATH"

# Set up GNUstep environment
if [ -f "$GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles/GNUstep.sh" ]; then
    source "$GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles/GNUstep.sh"
fi

echo "LLDB/GNUstep environment configured!"
echo "  LLDB: $LLVM_BUILD_DIR/build/bin/lldb.exe"
echo "  Clang: $LLVM_BUILD_DIR/build/bin/clang.exe"
echo "  GNUstep: $GNUSTEP_INSTALL_DIR"
EOF
    chmod +x "$LLVM_BUILD_DIR/setup_environment.sh"
    
    # Create verification script
    cat > "$LLVM_BUILD_DIR/verify_gnustep_patch.sh" << 'EOF'
#!/bin/bash
# Verify GNUstep patch is working

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/setup_environment.sh"

echo "Verifying GNUstep runtime patch..."
echo "================================="

# Check if patch files exist
PATCH_DIR="$PROJECT_ROOT/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime"
if [ -d "$PATCH_DIR" ]; then
    echo "✓ GNUstep runtime plugin found"
    ls -la "$PATCH_DIR"/*.cpp "$PATCH_DIR"/*.h 2>/dev/null
else
    echo "✗ GNUstep runtime plugin not found!"
    exit 1
fi

# Test LLDB with a simple program
cd "$SCRIPT_DIR/examples"
if [ -f "test_custom_class.exe" ]; then
    echo ""
    echo "Testing LLDB with custom class..."
    echo "lldb test_custom_class.exe -o 'b main' -o 'r' -o 'p [BankAccount class]' -o 'q'"
    
    lldb.exe test_custom_class.exe -b \
        -o "b main" \
        -o "r" \
        -o "p (void*)objc_getClass(\"BankAccount\")" \
        -o "q"
else
    echo "Test program not found. Run 'make' in examples directory first."
fi

echo ""
echo "Verification complete!"
EOF
    chmod +x "$LLVM_BUILD_DIR/verify_gnustep_patch.sh"
    
    # Create VS Code settings
    cat > "$LLVM_BUILD_DIR/vscode_settings.json" << EOF
{
    "lldb.executable": "$LLVM_BUILD_DIR/build/bin/lldb.exe",
    "lldb.launch.expressions": "native",
    "lldb.launch.terminal": "integrated",
    "C_Cpp.default.compilerPath": "$LLVM_BUILD_DIR/build/bin/clang.exe",
    "C_Cpp.default.intelliSenseMode": "clang-x64",
    "terminal.integrated.env.windows": {
        "PATH": "$LLVM_BUILD_DIR/build/bin;$GNUSTEP_INSTALL_DIR/bin;\${env:PATH}",
        "LD_LIBRARY_PATH": "$GNUSTEP_INSTALL_DIR/lib"
    }
}
EOF
    
    # Create quick rebuild script
    cat > "$LLVM_BUILD_DIR/quick_rebuild.sh" << EOF
#!/bin/bash
# Quick rebuild script for LLDB

cd "$LLVM_BUILD_DIR/build"
echo "Rebuilding LLDB..."
ninja -j$PARALLEL_JOBS lldb lldb-server
echo "Rebuild complete!"
EOF
    chmod +x "$LLVM_BUILD_DIR/quick_rebuild.sh"
    
    print_success "Helper scripts created in $LLVM_BUILD_DIR"
}

# Function to create debugging tips document
create_debugging_tips() {
    cat > "$LLVM_BUILD_DIR/DEBUGGING_TIPS.md" << 'EOF'
# LLDB GNUstep Debugging Tips for Windows

## Quick Start

1. Source the environment:
   ```bash
   source setup_environment.sh
   ```

2. Build test programs:
   ```bash
   cd examples
   make
   ```

3. Debug with LLDB:
   ```bash
   lldb test_custom_class.exe
   (lldb) b main
   (lldb) r
   (lldb) po account
   ```

## Common LLDB Commands

- `b <function>` - Set breakpoint
- `r` - Run program
- `n` - Step over
- `s` - Step into
- `c` - Continue
- `p <expr>` - Print expression
- `po <obj>` - Print object description
- `bt` - Show backtrace
- `frame variable` - Show local variables

## Windows-Specific Tips

1. **Path formats**: Use forward slashes in LLDB commands
2. **DLL issues**: Ensure GNUstep DLLs are in PATH
3. **Symbols**: Use `-g` flag when compiling for debug symbols

## Troubleshooting

1. **"Class not found" errors**:
   - Verify the patch is applied
   - Check that libobjc2 is properly linked

2. **LLDB crashes**:
   - Reduce parallel jobs during build
   - Check Windows Event Viewer for details

3. **Missing symbols**:
   - Rebuild with `-g -O0` flags
   - Ensure debug build type

## VS Code Integration

Copy the generated `vscode_settings.json` to your workspace `.vscode` folder.

For CodeLLDB extension:
```json
{
    "type": "lldb",
    "request": "launch",
    "name": "Debug",
    "program": "${workspaceFolder}/examples/test_custom_class.exe",
    "args": [],
    "cwd": "${workspaceFolder}/examples"
}
```
EOF
    
    print_success "Debugging tips document created"
}

# Function to create GNUstep-only test programs
create_gnustep_only_test_programs() {
    print_section "Creating GNUstep Test Programs"
    
    local gnustep_test_dir="$GNUSTEP_INSTALL_DIR/examples"
    ensure_directory "$gnustep_test_dir" "GNUstep examples directory"
    
    # Create simple GNUstep test
    cat > "$gnustep_test_dir/hello_gnustep.m" << 'EOF'
#import <Foundation/Foundation.h>

@interface HelloClass : NSObject
- (void)sayHello:(NSString*)name;
@end

@implementation HelloClass
- (void)sayHello:(NSString*)name {
    NSLog(@"Hello from %@ on GNUstep/Windows!", name);
    NSLog(@"NSString class: %@", [NSString class]);
}
@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        NSLog(@"Testing GNUstep on Windows MSYS2...");
        
        HelloClass *hello = [[HelloClass alloc] init];
        [hello sayHello:@"Windows Developer"];
        
        // Test some Foundation classes
        NSArray *array = @[@"GNUstep", @"Windows", @"LLDB"];
        NSLog(@"Array contents: %@", array);
        
        NSMutableDictionary *dict = [NSMutableDictionary dictionary];
        [dict setObject:@"Working!" forKey:@"Status"];
        NSLog(@"Dictionary: %@", dict);
        
        NSLog(@"GNUstep test completed successfully!");
    }
    return 0;
}
EOF
    
    # Create Makefile for GNUstep-only tests
    cat > "$gnustep_test_dir/Makefile" << EOF
# Makefile for GNUstep-only test programs on Windows

# Use system clang or our built clang if available
CLANG_PATH = \$(shell which clang 2>/dev/null)
ifeq (\$(CLANG_PATH),)
    CC = gcc
    OBJC = gcc
else
    CC = clang
    OBJC = clang
endif

OBJC_FLAGS = -fobjc-runtime=gnustep-2.0 -fblocks -g -O0
INCLUDES = -I$GNUSTEP_INSTALL_DIR/include
LIBS = -L$GNUSTEP_INSTALL_DIR/lib -lobjc -lgnustep-base -lm
LDFLAGS = 

# Source GNUstep environment
GNUSTEP_MAKEFILES = $GNUSTEP_INSTALL_DIR/share/GNUstep/Makefiles

TARGETS = hello_gnustep.exe

all: \$(TARGETS)

hello_gnustep.exe: hello_gnustep.m
	@echo "Building GNUstep test program..."
	\$(OBJC) \$(OBJC_FLAGS) \$(INCLUDES) hello_gnustep.m \$(LIBS) \$(LDFLAGS) -o hello_gnustep.exe

clean:
	rm -f \$(TARGETS) *.o

run: hello_gnustep.exe
	@echo "Running GNUstep test..."
	@echo "Setting up environment..."
	export LD_LIBRARY_PATH="$GNUSTEP_INSTALL_DIR/lib:\\\$\$LD_LIBRARY_PATH" && ./hello_gnustep.exe

test: run

.PHONY: all clean run test
EOF
    
    # Create environment test script
    cat > "$gnustep_test_dir/test_environment.sh" << 'EOF'
#!/bin/bash
# Test GNUstep environment on Windows

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GNUSTEP_ROOT="$(dirname "$SCRIPT_DIR")"

echo "Testing GNUstep Environment"
echo "============================="
echo "GNUstep installation: $GNUSTEP_ROOT"
echo ""

# Check libraries
echo "Checking libraries..."
if [ -f "$GNUSTEP_ROOT/lib/libobjc.dll" ] || [ -f "$GNUSTEP_ROOT/lib/libobjc.dll.a" ]; then
    echo "✓ libobjc found"
    ls -la "$GNUSTEP_ROOT/lib/libobjc"* 2>/dev/null | head -3
else
    echo "✗ libobjc not found"
fi

if [ -f "$GNUSTEP_ROOT/lib/libgnustep-base.dll" ] || [ -f "$GNUSTEP_ROOT/lib/libgnustep-base.dll.a" ]; then
    echo "✓ gnustep-base found"
else
    echo "✗ gnustep-base not found (may not be critical)"
fi

# Check headers
echo ""
echo "Checking headers..."
if [ -d "$GNUSTEP_ROOT/include/objc" ]; then
    echo "✓ objc headers found"
else
    echo "✗ objc headers not found"
fi

if [ -d "$GNUSTEP_ROOT/include/Foundation" ]; then
    echo "✓ Foundation headers found"
else
    echo "✗ Foundation headers not found"
fi

# Test compilation
echo ""
echo "Testing compilation..."
cd "$SCRIPT_DIR"
if make clean && make; then
    echo "✓ Compilation successful"
    echo ""
    echo "Testing execution..."
    if make run; then
        echo ""
        echo "✓ GNUstep test completed successfully!"
        echo ""
        echo "Your GNUstep environment is working correctly."
    else
        echo "✗ Execution failed"
    fi
else
    echo "✗ Compilation failed"
fi
EOF
    chmod +x "$gnustep_test_dir/test_environment.sh"
    
    print_success "GNUstep test programs created in $gnustep_test_dir"
    print_info "To test: cd $gnustep_test_dir && ./test_environment.sh"
    print_info "Or manually: cd $gnustep_test_dir && make run"
}

# Export functions
export -f verify_windows_installation create_test_programs create_gnustep_only_test_programs
export -f create_windows_helper_scripts create_debugging_tips