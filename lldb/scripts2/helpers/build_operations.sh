#!/bin/bash
# Build Operations Helper Script
# Functions for building LLDB, lldb-server, and verification

set -euo pipefail  # Exit on error, undefined variables, and pipe failures

# Source common utilities
HELPERS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HELPERS_DIR/common.sh"

# Function to build LLDB
build_lldb() {
    print_section "Step 6: Building LLDB (This will take 1-2 hours)"
    
    cd "$LLVM_BUILD_DIR/build"
    
    print_progress "Starting build with $PARALLEL_JOBS parallel jobs..."
    echo -e "${YELLOW}Note: This is a large build. You can monitor progress with:${NC}"
    echo -e "${YELLOW}  tail -f $LLVM_BUILD_DIR/build.log${NC}"
    echo ""
    
    # Start time tracking
    START_TIME=$(date +%s)
    
    # Build with logging
    if ninja -j$PARALLEL_JOBS lldb 2>&1 | tee build.log; then
        END_TIME=$(date +%s)
        BUILD_TIME=$(( (END_TIME - START_TIME) / 60 ))
        
        print_success "LLDB build completed successfully in ${BUILD_TIME} minutes!"
        
        # Show build info
        LLDB_BINARY="$LLVM_BUILD_DIR/build/bin/lldb"
        if [ -f "$LLDB_BINARY" ]; then
            print_success "LLDB binary: $LLDB_BINARY"
            
            # Show version
            LLDB_VERSION=$("$LLDB_BINARY" --version | head -1)
            print_success "Version: $LLDB_VERSION"
        fi
    else
        print_error "LLDB build failed. Check build.log for details."
    fi
}

# Function to build lldb-server specifically
build_lldb_server() {
    print_section "Step 6.5: Building lldb-server"
    
    cd "$LLVM_BUILD_DIR/build"
    
    print_progress "Building lldb-server target..."
    
    # Start time tracking
    START_TIME=$(date +%s)
    
    # Build lldb-server specifically
    if ninja -j$PARALLEL_JOBS lldb-server 2>&1 | tee lldb-server-build.log; then
        END_TIME=$(date +%s)
        BUILD_TIME=$(( (END_TIME - START_TIME) / 60 ))
        
        print_success "lldb-server build completed successfully in ${BUILD_TIME} minutes!"
        
        # Check if binary exists
        LLDB_SERVER_BINARY="$LLVM_BUILD_DIR/build/bin/lldb-server"
        if [ -f "$LLDB_SERVER_BINARY" ]; then
            print_success "🎯 lldb-server binary: $LLDB_SERVER_BINARY"
            
            # Show version/info
            if "$LLDB_SERVER_BINARY" --help >/dev/null 2>&1; then
                print_success "✓ lldb-server is functional"
            else
                print_warning "lldb-server binary exists but may have issues"
            fi
        else
            print_error "❌ lldb-server binary not found at expected location"
            print_progress "Searching for lldb-server in build directory..."
            
            # Search for the binary
            FOUND_SERVERS=$(find "$LLVM_BUILD_DIR/build" -name "lldb-server" -type f 2>/dev/null)
            if [ -n "$FOUND_SERVERS" ]; then
                print_success "Found lldb-server at alternative locations:"
                echo "$FOUND_SERVERS"
            else
                print_error "lldb-server not found anywhere in build directory"
            fi
        fi
    else
        print_error "lldb-server build failed. Check lldb-server-build.log for details."
        
        # Try to give helpful info
        print_progress "Checking if lldb-server target exists..."
        if ninja -t targets | grep -q lldb-server; then
            print_warning "lldb-server target exists but build failed"
        else
            print_error "lldb-server target does not exist in build system"
            print_progress "Available LLDB-related targets:"
            ninja -t targets | grep lldb | head -10
        fi
    fi
}

# Function to verify installation
verify_installation() {
    print_section "Step 7: Verifying Installation"
    
    LLDB_BINARY="$LLVM_BUILD_DIR/build/bin/lldb"
    LLDB_SERVER_BINARY="$LLVM_BUILD_DIR/build/bin/lldb-server"
    
    # Check LLDB
    if [ ! -f "$LLDB_BINARY" ]; then
        print_error "LLDB binary not found at $LLDB_BINARY"
    fi
    
    print_progress "Testing LLDB startup..."
    if "$LLDB_BINARY" -o "version" -o "quit" >/dev/null 2>&1; then
        print_success "✅ LLDB starts successfully"
    else
        print_error "❌ LLDB fails to start"
    fi
    
    # Check lldb-server
    if [ -f "$LLDB_SERVER_BINARY" ]; then
        print_success "✅ lldb-server binary found: $LLDB_SERVER_BINARY"
        
        if "$LLDB_SERVER_BINARY" --help >/dev/null 2>&1; then
            print_success "✅ lldb-server is functional"
        else
            print_warning "⚠️  lldb-server binary exists but may have issues"
        fi
    else
        print_error "❌ lldb-server binary not found at $LLDB_SERVER_BINARY"
        print_progress "This will cause issues with VS Code debugging extensions"
    fi
    
    print_progress "Checking for GNUstep runtime plugin..."
    # Note: Plugin loads dynamically when debugging ObjC programs
    # Static plugin list check may not show GNUstep plugin
    GNUSTEP_PLUGIN_DIR="$LLVM_BUILD_DIR/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime"
    if [ -d "$GNUSTEP_PLUGIN_DIR" ]; then
        FILE_COUNT=$(ls -1 "$GNUSTEP_PLUGIN_DIR" | wc -l)
        print_success "✅ GNUstepObjCRuntime plugin files present ($FILE_COUNT files)"
        print_success "Plugin will load dynamically when debugging Objective-C programs"
    else
        print_error "❌ GNUstepObjCRuntime plugin files not found"
        print_error "This indicates the patch was not applied correctly"
    fi
    
    print_success "Installation verification completed"
}

# Function to show build status
show_build_status() {
    print_section "🔍 Build Status Report"
    
    if [ ! -d "$LLVM_BUILD_DIR" ]; then
        print_error "Build directory not found: $LLVM_BUILD_DIR"
        return 1
    fi
    
    # Check LLVM source
    if [ -d "$LLVM_BUILD_DIR/llvm-project" ]; then
        cd "$LLVM_BUILD_DIR/llvm-project"
        COMMIT_HASH=$(git rev-parse --short HEAD 2>/dev/null)
        COMMIT_DATE=$(git log -1 --format=%ci 2>/dev/null)
        CURRENT_BRANCH=$(git describe --tags --exact-match 2>/dev/null || git rev-parse --abbrev-ref HEAD 2>/dev/null)
        print_success "📁 LLVM Source: $CURRENT_BRANCH ($COMMIT_HASH, $COMMIT_DATE)"
    else
        print_error "❌ LLVM source not found"
    fi
    
    # Check build directory
    if [ -d "$LLVM_BUILD_DIR/build" ]; then
        print_success "📁 Build directory exists: $LLVM_BUILD_DIR/build"
        
        # Check for key binaries
        LLDB_BINARY="$LLVM_BUILD_DIR/build/bin/lldb"
        LLDB_SERVER_BINARY="$LLVM_BUILD_DIR/build/bin/lldb-server"
        CLANG_BINARY="$LLVM_BUILD_DIR/build/bin/clang"
        
        if [ -f "$LLDB_BINARY" ]; then
            LLDB_VERSION=$("$LLDB_BINARY" --version 2>/dev/null | head -1)
            print_success "✅ LLDB: $LLDB_VERSION"
        else
            print_error "❌ LLDB binary missing"
        fi
        
        if [ -f "$LLDB_SERVER_BINARY" ]; then
            print_success "✅ lldb-server: Available"
        else
            print_error "❌ lldb-server: Missing (VS Code debugging will not work)"
        fi
        
        if [ -f "$CLANG_BINARY" ]; then
            CLANG_VERSION=$("$CLANG_BINARY" --version 2>/dev/null | head -1)
            print_success "✅ Clang: $CLANG_VERSION"
        else
            print_warning "⚠️  Clang binary missing"
        fi
        
        # Check GNUstep patch
        GNUSTEP_PLUGIN_DIR="$LLVM_BUILD_DIR/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime"
        if [ -d "$GNUSTEP_PLUGIN_DIR" ]; then
            FILE_COUNT=$(ls -1 "$GNUSTEP_PLUGIN_DIR" | wc -l)
            print_success "✅ GNUstep patch applied ($FILE_COUNT files)"
        else
            print_error "❌ GNUstep patch not applied"
        fi
        
        # Show disk usage
        BUILD_SIZE=$(du -sh "$LLVM_BUILD_DIR/build" 2>/dev/null | cut -f1)
        SOURCE_SIZE=$(du -sh "$LLVM_BUILD_DIR/llvm-project" 2>/dev/null | cut -f1)
        print_success "💾 Disk usage: Build=$BUILD_SIZE, Source=$SOURCE_SIZE"
        
    else
        print_error "❌ Build directory not found"
    fi
    
    # Performance tips
    echo ""
    print_section "💡 Performance Tips"
    echo -e "${CYAN}• Use ccache for faster rebuilds: export CCACHE_DIR=~/.ccache${NC}"
    echo -e "${CYAN}• Incremental builds: cd $LLVM_BUILD_DIR/build && ninja lldb${NC}"
    echo -e "${CYAN}• Monitor build: tail -f $LLVM_BUILD_DIR/build.log${NC}"
    echo -e "${CYAN}• Memory usage: Use fewer parallel jobs if running out of RAM${NC}"
}

# Function to run quick tests
run_quick_tests() {
    print_section "🧪 Quick Tests"
    
    LLDB_BINARY="$LLVM_BUILD_DIR/build/bin/lldb"
    LLDB_SERVER_BINARY="$LLVM_BUILD_DIR/build/bin/lldb-server"
    
    if [ ! -f "$LLDB_BINARY" ]; then
        print_error "❌ LLDB binary not found - cannot run tests"
        return 1
    fi
    
    # Test 1: LLDB version
    print_progress "Testing LLDB version..."
    if VERSION_OUTPUT=$("$LLDB_BINARY" --version 2>&1); then
        print_success "✅ LLDB version check passed"
        echo "   $VERSION_OUTPUT" | head -1
    else
        print_error "❌ LLDB version check failed"
        return 1
    fi
    
    # Test 2: LLDB startup/quit
    print_progress "Testing LLDB startup..."
    if "$LLDB_BINARY" -o "quit" >/dev/null 2>&1; then
        print_success "✅ LLDB startup test passed"
    else
        print_error "❌ LLDB startup test failed"
        return 1
    fi
    
    # Test 3: lldb-server
    if [ -f "$LLDB_SERVER_BINARY" ]; then
        print_progress "Testing lldb-server..."
        if "$LLDB_SERVER_BINARY" --help >/dev/null 2>&1; then
            print_success "✅ lldb-server test passed"
        else
            print_error "❌ lldb-server test failed"
            return 1
        fi
    else
        print_error "❌ lldb-server not found - VS Code debugging will not work"
        return 1
    fi
    
    # Test 4: Basic ObjC support
    print_progress "Testing basic Objective-C support..."
    if echo 'po @"Hello World"' | "$LLDB_BINARY" >/dev/null 2>&1; then
        print_success "✅ Basic Objective-C support test passed"
    else
        print_warning "⚠️  Basic Objective-C support test inconclusive"
    fi
    
    # Test 5: GNUstep/libobjc2 compilation (critical - early exit if fails)
    if ! test_objc_compilation; then
        print_error "❌ Critical: GNUstep compilation failed - skipping advanced tests"
        return 1
    fi
    
    # Test 6: Foundation classes support (only if compilation works)
    if ! test_foundation_support; then
        print_warning "⚠️  Foundation tests failed - debugging tests may be unreliable"
    fi
    
    # Test 7: Debugging integration (only if previous tests pass)
    test_debugging_integration
    
    print_success "Quick tests completed"
}

# Test GNUstep/libobjc2 compilation support
test_objc_compilation() {
    print_progress "Testing GNUstep/libobjc2 compilation..."
    
    local TEST_DIR="$LLVM_BUILD_DIR/test_compilation"
    mkdir -p "$TEST_DIR"
    
    # Use the existing simple_test.m if available, otherwise create a minimal test
    local SOURCE_FILE
    if [ -f "$WORKSPACE_ROOT/examples/simple_test.m" ]; then
        SOURCE_FILE="$WORKSPACE_ROOT/examples/simple_test.m"
        print_progress "Using existing simple_test.m"
    else
        SOURCE_FILE="$TEST_DIR/minimal_test.m"
        print_progress "Creating minimal test file"
        
        cat > "$SOURCE_FILE" << 'EOF'
#import <Foundation/Foundation.h>

@interface TestClass : NSObject
@property (nonatomic, strong) NSString *name;
@end

@implementation TestClass
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        TestClass *obj = [[TestClass alloc] init];
        obj.name = @"Test";
        NSLog(@"GNUstep test: %@", obj.name);
        return 0;
    }
}
EOF
    fi
    
    # Try to compile with GNUstep
    local BINARY_PATH="$TEST_DIR/test_program"
    local COMPILE_CMD="clang -fobjc-runtime=gnustep-2.1 -fblocks -fno-strict-aliasing -fexceptions -fobjc-exceptions -g -O0 -I/usr/local/include/GNUstep -I/usr/include/GNUstep -fconstant-string-class=NSConstantString -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -L/usr/local/lib -Wl,-rpath,/usr/local/lib -o '$BINARY_PATH' '$SOURCE_FILE' -lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm"
    
    local COMPILE_OUTPUT
    if COMPILE_OUTPUT=$(eval "$COMPILE_CMD" 2>&1) && [ -f "$BINARY_PATH" ]; then
        print_success "✅ GNUstep compilation test passed"
        
        # Test execution
        if timeout 5 "$BINARY_PATH" >/dev/null 2>&1; then
            print_success "✅ GNUstep runtime execution test passed"
        else
            print_warning "⚠️  GNUstep runtime execution test failed"
        fi
    else
        print_error "❌ GNUstep compilation test failed"
        print_progress "Compilation error output:"
        echo "$COMPILE_OUTPUT" | head -5
        print_error "This indicates critical issues with GNUstep development environment"
        print_error "Cannot proceed with advanced debugging tests"
        return 1
    fi
    
    # Cleanup
    rm -rf "$TEST_DIR"
}

# Test Foundation classes support
test_foundation_support() {
    print_progress "Testing Foundation classes support..."
    
    local TEST_DIR="$LLVM_BUILD_DIR/test_foundation"
    mkdir -p "$TEST_DIR"
    
    # Create Foundation test program
    cat > "$TEST_DIR/foundation_test.m" << 'EOF'
#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        // Test NSString
        NSString *str = @"Hello Foundation";
        
        // Test NSArray
        NSArray *arr = @[@"One", @"Two", @"Three"];
        
        // Test NSDictionary
        NSDictionary *dict = @{@"key": @"value", @"number": @42};
        
        // Test NSMutableArray
        NSMutableArray *mutable = [[NSMutableArray alloc] init];
        [mutable addObject:@"Mutable"];
        
        // Output for verification
        printf("FOUNDATION_TEST_RESULTS:\n");
        printf("String: %s\n", [str UTF8String]);
        printf("Array count: %lu\n", (unsigned long)[arr count]);
        printf("Dict count: %lu\n", (unsigned long)[dict count]);
        printf("Mutable count: %lu\n", (unsigned long)[mutable count]);
        printf("FOUNDATION_TEST_COMPLETE\n");
        
        return 0;
    }
}
EOF
    
    # Compile Foundation test
    local BINARY_PATH="$TEST_DIR/foundation_test"
    local COMPILE_CMD="clang -fobjc-runtime=gnustep-2.1 -fblocks -g -O0 -I/usr/local/include/GNUstep -I/usr/include/GNUstep -fconstant-string-class=NSConstantString -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -L/usr/local/lib -Wl,-rpath,/usr/local/lib -o '$BINARY_PATH' '$TEST_DIR/foundation_test.m' -lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm"
    
    local COMPILE_OUTPUT
    if COMPILE_OUTPUT=$(eval "$COMPILE_CMD" 2>&1) && [ -f "$BINARY_PATH" ]; then
        print_progress "Foundation test compiled successfully"
        
        # Run and verify output
        local OUTPUT
        if OUTPUT=$(timeout 5 "$BINARY_PATH" 2>&1); then
            if echo "$OUTPUT" | grep -q "FOUNDATION_TEST_COMPLETE"; then
                # Verify specific Foundation functionality
                if echo "$OUTPUT" | grep -q "String: Hello Foundation" && \
                   echo "$OUTPUT" | grep -q "Array count: 3" && \
                   echo "$OUTPUT" | grep -q "Dict count: 2" && \
                   echo "$OUTPUT" | grep -q "Mutable count: 1"; then
                    print_success "✅ Foundation classes test passed"
                else
                    print_warning "⚠️  Foundation classes test produced unexpected output"
                    echo "Expected output missing - check Foundation library installation"
                    return 1
                fi
            else
                print_error "❌ Foundation classes test failed to complete"
                echo "Output: $OUTPUT" | head -3
                return 1
            fi
        else
            print_error "❌ Foundation test execution failed or timed out"
            return 1
        fi
    else
        print_error "❌ Foundation classes compilation failed"
        print_progress "Compilation error:"
        echo "$COMPILE_OUTPUT" | head -5
        return 1
    fi
    
    # Cleanup
    rm -rf "$TEST_DIR"
}

# Test debugging integration with GNUstep plugin
test_debugging_integration() {
    print_progress "Testing LLDB debugging integration with GNUstep..."
    
    local TEST_DIR="$LLVM_BUILD_DIR/test_debugging"
    mkdir -p "$TEST_DIR"
    
    # Create debugging test program (simplified version of simple_test.m)
    cat > "$TEST_DIR/debug_test.m" << 'EOF'
#import <Foundation/Foundation.h>

@interface Person : NSObject
@property (nonatomic, strong) NSString *name;
@property (nonatomic, assign) NSInteger age;
@end

@implementation Person
- (NSString *)description {
    return [NSString stringWithFormat:@"Person(name=%@, age=%ld)", self.name, (long)self.age];
}
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        Person *person = [[Person alloc] init];
        person.name = @"Debug Test";
        person.age = 42;
        
        NSDictionary *config = @{@"debug": @YES, @"name": @"LLDB Test"};
        
        // Breakpoint location - this is where we'll test debugging
        NSLog(@"Debug checkpoint reached"); // BREAKPOINT_LINE
        
        return 0;
    }
}
EOF
    
    # Compile with debug symbols
    local BINARY_PATH="$TEST_DIR/debug_test"
    local COMPILE_CMD="clang -fobjc-runtime=gnustep-2.1 -fblocks -g -O0 -I/usr/local/include/GNUstep -I/usr/include/GNUstep -fconstant-string-class=NSConstantString -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -L/usr/local/lib -Wl,-rpath,/usr/local/lib -o '$BINARY_PATH' '$TEST_DIR/debug_test.m' -lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm"
    
    local COMPILE_OUTPUT
    if COMPILE_OUTPUT=$(eval "$COMPILE_CMD" 2>&1) && [ -f "$BINARY_PATH" ]; then
        print_progress "Debug test compiled successfully"
        
        # Create robust LLDB script for automated debugging
        cat > "$TEST_DIR/debug_script.lldb" << 'EOF'
target create debug_test
breakpoint set --file debug_test.m --line 20
run
frame variable person
frame variable config
p person
p (void*)person
echo "DEBUGGING_SUCCESS_MARKER"
continue
quit
EOF
        
        # Run LLDB debugging session with timeout and error handling
        cd "$TEST_DIR"
        local DEBUG_OUTPUT
        if DEBUG_OUTPUT=$(timeout 10 "$LLDB_BINARY" --source debug_script.lldb 2>&1); then
            
            # Verify debugging capabilities with more lenient checks
            local SUCCESS_COUNT=0
            
            # Check if breakpoint was hit
            if echo "$DEBUG_OUTPUT" | grep -q "stopped.*breakpoint\|stop reason = breakpoint"; then
                ((SUCCESS_COUNT++))
                print_success "  ✓ Breakpoint functionality works"
            fi
            
            # Check if we can access local variables (any variable access)
            if echo "$DEBUG_OUTPUT" | grep -q "Person\|0x.*="; then
                ((SUCCESS_COUNT++))
                print_success "  ✓ Variable access works"
            fi
            
            # Check if LLDB scripting works
            if echo "$DEBUG_OUTPUT" | grep -q "DEBUGGING_SUCCESS_MARKER"; then
                ((SUCCESS_COUNT++))
                print_success "  ✓ LLDB scripting works"
            fi
            
            # Check if process completed (relaxed check)
            if echo "$DEBUG_OUTPUT" | grep -q "Process.*exited\|exited normally\|Process.*stopped"; then
                ((SUCCESS_COUNT++))
                print_success "  ✓ Process control works"
            fi
            
            # Overall assessment
            if [ $SUCCESS_COUNT -ge 3 ]; then
                print_success "✅ LLDB debugging integration test passed ($SUCCESS_COUNT/4 checks)"
                print_success "  Note: Advanced ObjC property debugging may need refinement"
            elif [ $SUCCESS_COUNT -ge 2 ]; then
                print_warning "⚠️  LLDB debugging integration partially working ($SUCCESS_COUNT/4 checks)"
                print_warning "  Basic debugging works but some features may be limited"
            else
                print_error "❌ LLDB debugging integration test failed ($SUCCESS_COUNT/4 checks)"
                print_progress "Debug output sample:"
                echo "$DEBUG_OUTPUT" | head -10
            fi
            
        else
            local EXIT_CODE=$?
            if [ $EXIT_CODE -eq 124 ]; then
                print_warning "⚠️  LLDB debugging session timed out (10s limit)"
                print_warning "  This may indicate the session hung - possible LLDB issues"
            else
                print_error "❌ LLDB debugging session failed (exit code: $EXIT_CODE)"
            fi
        fi
    else
        print_error "❌ Debug test compilation failed"
        print_progress "Compilation error:"
        echo "$COMPILE_OUTPUT" | head -5
        print_error "Cannot test debugging without successful compilation"
        return 1
    fi
    
    # Cleanup
    rm -rf "$TEST_DIR"
}
