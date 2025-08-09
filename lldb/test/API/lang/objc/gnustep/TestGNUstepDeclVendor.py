"""
Test GNUstep declaration vendor and runtime API functionality.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil
import os
import subprocess
import time

class TestGNUstepDeclVendor(TestBase):
    """Test GNUstep declaration vendor for type synthesis and runtime API."""

    @skipUnlessPlatform(["linux"])
    def setUp(self):
        """Set up test case."""
        TestBase.setUp(self)
        self.main_source = "declvendor_test.m"
        self.exe_name = "test_declvendor"
        
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
    def test_type_synthesis(self):
        """Test type synthesis for runtime classes."""
        
        source = """
#import <Foundation/Foundation.h>

@interface RuntimeClass : NSObject
@property NSString *name;
@property int value;
- (NSString *)computedDescription;
@end

@implementation RuntimeClass
- (NSString *)computedDescription {
    return [NSString stringWithFormat:@"%@ = %d", self.name, self.value];
}
@end

int main() {
    @autoreleasepool {
        RuntimeClass *obj = [[RuntimeClass alloc] init];
        obj.name = @"Test";
        obj.value = 42;
        
        NSLog(@"Type synthesis test"); // Breakpoint here
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
        
        # Test that we can access synthesized types
        obj = frame.FindVariable("obj")
        self.assertTrue(obj.IsValid())
        
        # Check type information
        type_name = obj.GetTypeName()
        self.assertIn("RuntimeClass", type_name)
        
        # Test property access through expression evaluation
        result = lldb.SBCommandReturnObject()
        self.dbg.GetCommandInterpreter().HandleCommand("expr obj.name", result)
        self.assertTrue(result.Succeeded())
        self.assertIn("Test", result.GetOutput())
        
        self.dbg.GetCommandInterpreter().HandleCommand("expr obj.value", result)
        self.assertTrue(result.Succeeded())
        self.assertIn("42", result.GetOutput())
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_interface_generation(self):
        """Test interface generation for complex classes."""
        
        source = """
#import <Foundation/Foundation.h>

@protocol CustomProtocol <NSObject>
- (void)protocolMethod;
@end

@interface ComplexClass : NSObject <CustomProtocol> {
    int _privateVar;
}
@property (nonatomic, strong) NSString *stringProperty;
@property (nonatomic, assign) NSInteger integerProperty;
@property (nonatomic, weak) id weakProperty;
@property (nonatomic, readonly) NSString *readonlyProperty;

- (void)instanceMethod;
+ (void)classMethod;
@end

@implementation ComplexClass
@synthesize readonlyProperty = _readonlyProperty;

- (instancetype)init {
    self = [super init];
    if (self) {
        _readonlyProperty = @"Readonly";
        _privateVar = 100;
    }
    return self;
}

- (void)instanceMethod {
    NSLog(@"Instance method called");
}

+ (void)classMethod {
    NSLog(@"Class method called");
}

- (void)protocolMethod {
    NSLog(@"Protocol method called");
}
@end

int main() {
    @autoreleasepool {
        ComplexClass *complex = [[ComplexClass alloc] init];
        complex.stringProperty = @"String";
        complex.integerProperty = 123;
        
        [complex instanceMethod];
        [ComplexClass classMethod];
        
        NSLog(@"Interface generation test"); // Breakpoint here
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
        
        # Get the complex object
        complex_obj = frame.FindVariable("complex")
        self.assertTrue(complex_obj.IsValid())
        
        # Test property access
        result = lldb.SBCommandReturnObject()
        self.dbg.GetCommandInterpreter().HandleCommand("expr complex.stringProperty", result)
        self.assertTrue(result.Succeeded())
        self.assertIn("String", result.GetOutput())
        
        self.dbg.GetCommandInterpreter().HandleCommand("expr complex.integerProperty", result)
        self.assertTrue(result.Succeeded())
        self.assertIn("123", result.GetOutput())
        
        self.dbg.GetCommandInterpreter().HandleCommand("expr complex.readonlyProperty", result)
        self.assertTrue(result.Succeeded())
        self.assertIn("Readonly", result.GetOutput())
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_runtime_function_calling(self):
        """Test calling runtime functions."""
        
        source = """
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

@interface TestClass : NSObject
@end

@implementation TestClass
@end

int main() {
    @autoreleasepool {
        // Test various runtime functions
        Class cls = objc_getClass("NSString");
        const char *name = class_getName(cls);
        
        SEL selector = sel_registerName("description");
        const char *selName = sel_getName(selector);
        
        TestClass *obj = [[TestClass alloc] init];
        Class objClass = object_getClass(obj);
        
        NSLog(@"Runtime function test"); // Breakpoint here
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
        
        # Test runtime function results
        cls = frame.FindVariable("cls")
        self.assertTrue(cls.IsValid())
        self.assertNotEqual(cls.GetValueAsUnsigned(), 0)
        
        name_ptr = frame.FindVariable("name")
        self.assertTrue(name_ptr.IsValid())
        
        selector = frame.FindVariable("selector")
        self.assertTrue(selector.IsValid())
        self.assertNotEqual(selector.GetValueAsUnsigned(), 0)
        
        obj_class = frame.FindVariable("objClass")
        self.assertTrue(obj_class.IsValid())
        self.assertNotEqual(obj_class.GetValueAsUnsigned(), 0)
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_symbol_resolution(self):
        """Test runtime symbol resolution."""
        
        source = """
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <dlfcn.h>

int main() {
    @autoreleasepool {
        // Get runtime library handle
        void *handle = dlopen("libobjc.so.4", RTLD_LAZY);
        
        // Resolve various runtime symbols
        void *objc_getClass_ptr = dlsym(handle, "objc_getClass");
        void *class_getName_ptr = dlsym(handle, "class_getName");
        void *sel_registerName_ptr = dlsym(handle, "sel_registerName");
        
        NSLog(@"objc_getClass: %p", objc_getClass_ptr);
        NSLog(@"class_getName: %p", class_getName_ptr);
        NSLog(@"sel_registerName: %p", sel_registerName_ptr);
        
        dlclose(handle);
        
        NSLog(@"Symbol resolution test"); // Breakpoint here
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
        
        # Verify symbols were resolved
        objc_getClass_ptr = frame.FindVariable("objc_getClass_ptr")
        class_getName_ptr = frame.FindVariable("class_getName_ptr")
        sel_registerName_ptr = frame.FindVariable("sel_registerName_ptr")
        
        self.assertTrue(objc_getClass_ptr.IsValid())
        self.assertTrue(class_getName_ptr.IsValid())
        self.assertTrue(sel_registerName_ptr.IsValid())
        
        # All should be non-null
        self.assertNotEqual(objc_getClass_ptr.GetValueAsUnsigned(), 0)
        self.assertNotEqual(class_getName_ptr.GetValueAsUnsigned(), 0)
        self.assertNotEqual(sel_registerName_ptr.GetValueAsUnsigned(), 0)
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_performance_lookup(self):
        """Test performance of type lookup operations."""
        
        source = """
#import <Foundation/Foundation.h>

// Create many classes to test lookup performance
"""
        # Generate many classes
        for i in range(100):
            source += f"""
@interface TestClass{i} : NSObject
@property int value{i};
@end

@implementation TestClass{i}
@end
"""
        
        source += """
int main() {
    @autoreleasepool {
        NSMutableArray *objects = [NSMutableArray array];
        
        // Create instances of all classes
"""
        for i in range(100):
            source += f"""
        TestClass{i} *obj{i} = [[TestClass{i} alloc] init];
        obj{i}.value{i} = {i};
        [objects addObject:obj{i}];
"""
        
        source += """
        NSLog(@"Performance test ready with %lu objects", [objects count]); // Breakpoint here
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
        
        # Time type lookups
        start = time.time()
        
        objects = frame.FindVariable("objects")
        self.assertTrue(objects.IsValid())
        
        # Access some objects and their types
        for i in range(0, 100, 10):
            var_name = f"obj{i}"
            var = frame.FindVariable(var_name)
            if var.IsValid():
                type_name = var.GetTypeName()
                self.assertIn(f"TestClass{i}", type_name)
        
        elapsed = time.time() - start
        
        # Should complete quickly (< 2 seconds for 100 classes)
        self.assertLess(elapsed, 2.0, "Type lookup performance test took too long")
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_thread_safety(self):
        """Test thread safety of declaration vendor operations."""
        
        source = """
#import <Foundation/Foundation.h>

@interface ThreadSafeClass : NSObject
@property (atomic) NSInteger counter;
- (void)incrementCounter;
@end

@implementation ThreadSafeClass
- (void)incrementCounter {
    self.counter++;
}
@end

int main() {
    @autoreleasepool {
        ThreadSafeClass *obj = [[ThreadSafeClass alloc] init];
        obj.counter = 0;
        
        dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
        dispatch_group_t group = dispatch_group_create();
        
        // Create multiple threads accessing the object
        for (int i = 0; i < 10; i++) {
            dispatch_group_async(group, queue, ^{
                for (int j = 0; j < 100; j++) {
                    [obj incrementCounter];
                }
            });
        }
        
        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
        
        NSLog(@"Final counter: %ld", (long)obj.counter); // Breakpoint here
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
        
        # Check final counter value
        obj = frame.FindVariable("obj")
        self.assertTrue(obj.IsValid())
        
        # Access counter property
        result = lldb.SBCommandReturnObject()
        self.dbg.GetCommandInterpreter().HandleCommand("expr obj.counter", result)
        self.assertTrue(result.Succeeded())
        
        # Should be 1000 (10 threads * 100 increments)
        self.assertIn("1000", result.GetOutput())
        
        process.Kill()

    def tearDown(self):
        """Clean up after test."""
        TestBase.tearDown(self)
        
        # Clean up generated files
        for f in [self.main_source, self.exe_name]:
            if os.path.exists(f):
                os.remove(f)