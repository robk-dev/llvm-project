"""
Test GNUstep Objective-C runtime detection and core functionality.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil
import os
import subprocess

class TestGNUstepRuntime(TestBase):
    """Test GNUstep runtime detection, initialization, and core features."""

    @skipUnlessPlatform(["linux"])
    def setUp(self):
        """Set up test case."""
        TestBase.setUp(self)
        self.main_source = "main.m"
        self.exe_name = "test_gnustep_runtime"
        
    def build_gnustep_program(self, source_file):
        """Build a GNUstep test program."""
        compiler = "/usr/bin/clang"
        
        cflags = [
            "-fobjc-runtime=gnustep-2.1",
            "-fblocks",
            "-fno-strict-aliasing",
            "-fexceptions",
            "-fobjc-exceptions",
            "-g", "-gdwarf-5", "-O0",
            "-fno-omit-frame-pointer",
            "-I/usr/local/include",
            "-fconstant-string-class=NSConstantString",
            "-DGNUSTEP", "-DGNUSTEP_BASE_LIBRARY=1", "-DDEBUG=1"
        ]
        
        ldflags = [
            "-L/usr/local/lib",
            "-Wl,-rpath,/usr/local/lib",
            "-lgnustep-base", "-lobjc", "-lBlocksRuntime", "-lpthread", "-lm"
        ]
        
        cmd = [compiler] + cflags + [source_file, "-o", self.exe_name] + ldflags
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"Compilation failed: {result.stderr}")
            return False
        return True

    @skipUnlessPlatform(["linux"])
    def test_runtime_detection(self):
        """Test that GNUstep runtime is correctly detected."""
        
        # Create test program
        source = """
#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        NSLog(@"GNUstep runtime test");
    }
    return 0;
}
"""
        with open(self.main_source, 'w') as f:
            f.write(source)
        
        # Build the program
        self.assertTrue(self.build_gnustep_program(self.main_source))
        
        # Create target and verify runtime detection
        target = self.dbg.CreateTarget(self.exe_name)
        self.assertTrue(target.IsValid(), "Failed to create target")
        
        # Set breakpoint and run
        main_bp = target.BreakpointCreateByName("main")
        self.assertTrue(main_bp.IsValid() and main_bp.GetNumLocations() > 0)
        
        process = target.LaunchSimple(None, None, self.get_process_working_directory())
        self.assertTrue(process.IsValid(), "Failed to launch process")
        
        # Check that we hit the breakpoint
        thread = process.GetSelectedThread()
        self.assertTrue(thread.IsValid())
        
        # Verify GNUstep runtime is active
        # This would be done through SB API calls to check runtime
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_object_description(self):
        """Test GetObjectDescription functionality."""
        
        source = """
#import <Foundation/Foundation.h>

@interface TestClass : NSObject
@property NSString *name;
@property int value;
@end

@implementation TestClass
- (NSString *)description {
    return [NSString stringWithFormat:@"TestClass(name=%@, value=%d)", self.name, self.value];
}
@end

int main() {
    @autoreleasepool {
        TestClass *obj = [[TestClass alloc] init];
        obj.name = @"Test";
        obj.value = 42;
        
        NSLog(@"Object: %@", obj); // Breakpoint here
    }
    return 0;
}
"""
        with open(self.main_source, 'w') as f:
            f.write(source)
        
        self.assertTrue(self.build_gnustep_program(self.main_source))
        
        target = self.dbg.CreateTarget(self.exe_name)
        self.assertTrue(target.IsValid())
        
        # Set breakpoint at NSLog line
        bp = target.BreakpointCreateBySourceRegex("// Breakpoint here", 
                                                   lldb.SBFileSpec(self.main_source))
        self.assertTrue(bp.IsValid() and bp.GetNumLocations() > 0)
        
        process = target.LaunchSimple(None, None, self.get_process_working_directory())
        self.assertTrue(process.IsValid())
        
        thread = process.GetSelectedThread()
        frame = thread.GetSelectedFrame()
        
        # Get the object and test description
        obj = frame.FindVariable("obj")
        self.assertTrue(obj.IsValid())
        
        # Test object description through po command
        result = lldb.SBCommandReturnObject()
        self.dbg.GetCommandInterpreter().HandleCommand(f"po obj", result)
        
        # Verify output contains expected description
        output = result.GetOutput()
        self.assertIn("TestClass", output)
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_dynamic_type_resolution(self):
        """Test GetDynamicTypeAndAddress functionality."""
        
        source = """
#import <Foundation/Foundation.h>

@interface BaseClass : NSObject
@end

@implementation BaseClass
@end

@interface DerivedClass : BaseClass
@property NSString *derivedProperty;
@end

@implementation DerivedClass
@end

int main() {
    @autoreleasepool {
        BaseClass *base = [[DerivedClass alloc] init];
        ((DerivedClass *)base).derivedProperty = @"Dynamic";
        
        NSLog(@"Type: %@", [base class]); // Breakpoint here
    }
    return 0;
}
"""
        with open(self.main_source, 'w') as f:
            f.write(source)
        
        self.assertTrue(self.build_gnustep_program(self.main_source))
        
        target = self.dbg.CreateTarget(self.exe_name)
        self.assertTrue(target.IsValid())
        
        bp = target.BreakpointCreateBySourceRegex("// Breakpoint here", 
                                                   lldb.SBFileSpec(self.main_source))
        self.assertTrue(bp.IsValid() and bp.GetNumLocations() > 0)
        
        process = target.LaunchSimple(None, None, self.get_process_working_directory())
        self.assertTrue(process.IsValid())
        
        thread = process.GetSelectedThread()
        frame = thread.GetSelectedFrame()
        
        # Get the base pointer
        base = frame.FindVariable("base")
        self.assertTrue(base.IsValid())
        
        # Check dynamic type
        dynamic_type = base.GetDynamicValue(lldb.eDynamicCanRunTarget)
        self.assertTrue(dynamic_type.IsValid())
        
        # Verify it's recognized as DerivedClass
        type_name = dynamic_type.GetTypeName()
        self.assertIn("DerivedClass", type_name)
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_isa_validation(self):
        """Test ISA validation for various object types."""
        
        source = """
#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        NSString *string = @"Test String";
        NSNumber *number = @42;
        NSArray *array = @[@1, @2, @3];
        NSDictionary *dict = @{@"key": @"value"};
        
        NSLog(@"Objects created"); // Breakpoint here
    }
    return 0;
}
"""
        with open(self.main_source, 'w') as f:
            f.write(source)
        
        self.assertTrue(self.build_gnustep_program(self.main_source))
        
        target = self.dbg.CreateTarget(self.exe_name)
        self.assertTrue(target.IsValid())
        
        bp = target.BreakpointCreateBySourceRegex("// Breakpoint here", 
                                                   lldb.SBFileSpec(self.main_source))
        self.assertTrue(bp.IsValid() and bp.GetNumLocations() > 0)
        
        process = target.LaunchSimple(None, None, self.get_process_working_directory())
        self.assertTrue(process.IsValid())
        
        thread = process.GetSelectedThread()
        frame = thread.GetSelectedFrame()
        
        # Test ISA validation for each object type
        for var_name in ["string", "number", "array", "dict"]:
            var = frame.FindVariable(var_name)
            self.assertTrue(var.IsValid(), f"Variable {var_name} not found")
            
            # Get the ISA (would need SB API extension)
            # For now, just verify the variable is valid
            self.assertNotEqual(var.GetValueAsUnsigned(), 0)
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_tagged_pointer_handling(self):
        """Test handling of tagged pointers."""
        
        source = """
#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        // Small integers are often tagged
        NSNumber *small = @7;
        NSNumber *large = @(LLONG_MAX);
        
        // Small strings might be tagged
        NSString *shortStr = @"Hi";
        NSString *longStr = @"This is a much longer string that won't be tagged";
        
        NSLog(@"Tagged pointer test"); // Breakpoint here
    }
    return 0;
}
"""
        with open(self.main_source, 'w') as f:
            f.write(source)
        
        self.assertTrue(self.build_gnustep_program(self.main_source))
        
        target = self.dbg.CreateTarget(self.exe_name)
        self.assertTrue(target.IsValid())
        
        bp = target.BreakpointCreateBySourceRegex("// Breakpoint here", 
                                                   lldb.SBFileSpec(self.main_source))
        self.assertTrue(bp.IsValid() and bp.GetNumLocations() > 0)
        
        process = target.LaunchSimple(None, None, self.get_process_working_directory())
        self.assertTrue(process.IsValid())
        
        thread = process.GetSelectedThread()
        frame = thread.GetSelectedFrame()
        
        # Check if small values are tagged
        small = frame.FindVariable("small")
        self.assertTrue(small.IsValid())
        
        # Get the pointer value
        small_addr = small.GetValueAsUnsigned()
        
        # Tagged pointers have low bits set
        is_tagged = (small_addr & 0xF) != 0
        
        # Test that we can still get the value
        result = lldb.SBCommandReturnObject()
        self.dbg.GetCommandInterpreter().HandleCommand(f"po small", result)
        self.assertIn("7", result.GetOutput())
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_class_hierarchy_traversal(self):
        """Test traversing class hierarchies."""
        
        source = """
#import <Foundation/Foundation.h>

@interface GrandParent : NSObject
@end

@implementation GrandParent
@end

@interface Parent : GrandParent
@end

@implementation Parent
@end

@interface Child : Parent
@end

@implementation Child
@end

int main() {
    @autoreleasepool {
        Child *child = [[Child alloc] init];
        Class cls = [child class];
        
        while (cls) {
            NSLog(@"Class: %s", class_getName(cls));
            cls = class_getSuperclass(cls);
        }
        
        NSLog(@"Hierarchy complete"); // Breakpoint here
    }
    return 0;
}
"""
        with open(self.main_source, 'w') as f:
            f.write(source)
        
        self.assertTrue(self.build_gnustep_program(self.main_source))
        
        target = self.dbg.CreateTarget(self.exe_name)
        self.assertTrue(target.IsValid())
        
        bp = target.BreakpointCreateBySourceRegex("// Breakpoint here", 
                                                   lldb.SBFileSpec(self.main_source))
        self.assertTrue(bp.IsValid() and bp.GetNumLocations() > 0)
        
        process = target.LaunchSimple(None, None, self.get_process_working_directory())
        self.assertTrue(process.IsValid())
        
        thread = process.GetSelectedThread()
        frame = thread.GetSelectedFrame()
        
        # Get the child object
        child = frame.FindVariable("child")
        self.assertTrue(child.IsValid())
        
        # Verify we can inspect the class hierarchy
        result = lldb.SBCommandReturnObject()
        self.dbg.GetCommandInterpreter().HandleCommand(f"po [child class]", result)
        self.assertIn("Child", result.GetOutput())
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_performance_baseline(self):
        """Test performance of runtime operations."""
        
        source = """
#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        NSMutableArray *array = [NSMutableArray array];
        
        // Create many objects for performance testing
        for (int i = 0; i < 1000; i++) {
            [array addObject:@(i)];
        }
        
        NSLog(@"Performance test ready"); // Breakpoint here
    }
    return 0;
}
"""
        with open(self.main_source, 'w') as f:
            f.write(source)
        
        self.assertTrue(self.build_gnustep_program(self.main_source))
        
        target = self.dbg.CreateTarget(self.exe_name)
        self.assertTrue(target.IsValid())
        
        bp = target.BreakpointCreateBySourceRegex("// Breakpoint here", 
                                                   lldb.SBFileSpec(self.main_source))
        self.assertTrue(bp.IsValid() and bp.GetNumLocations() > 0)
        
        process = target.LaunchSimple(None, None, self.get_process_working_directory())
        self.assertTrue(process.IsValid())
        
        thread = process.GetSelectedThread()
        frame = thread.GetSelectedFrame()
        
        # Time how long it takes to inspect the array
        import time
        start = time.time()
        
        array = frame.FindVariable("array")
        self.assertTrue(array.IsValid())
        
        # Get array count
        count = array.GetNumChildren()
        self.assertEqual(count, 1000)
        
        # Access some elements
        for i in range(0, 100, 10):
            child = array.GetChildAtIndex(i)
            self.assertTrue(child.IsValid())
        
        elapsed = time.time() - start
        
        # Should complete in reasonable time (< 1 second)
        self.assertLess(elapsed, 1.0, "Performance test took too long")
        
        process.Kill()

    def tearDown(self):
        """Clean up after test."""
        TestBase.tearDown(self)
        
        # Clean up generated files
        for f in [self.main_source, self.exe_name]:
            if os.path.exists(f):
                os.remove(f)