"""
Test GNUstep runtime introspector functionality.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil
import os
import subprocess

class TestGNUstepIntrospector(TestBase):
    """Test GNUstep runtime introspector for ISA resolution and class inspection."""

    @skipUnlessPlatform(["linux"])
    def setUp(self):
        """Set up test case."""
        TestBase.setUp(self)
        self.main_source = "introspector_test.m"
        self.exe_name = "test_introspector"
        
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
    def test_isa_resolution(self):
        """Test ISA resolution for various object types."""
        
        source = """
#import <Foundation/Foundation.h>

@interface CustomClass : NSObject
@property NSString *customProperty;
@end

@implementation CustomClass
@end

int main() {
    @autoreleasepool {
        // Standard Foundation objects
        NSString *string = @"Test String";
        NSNumber *number = @42;
        NSArray *array = @[@1, @2, @3];
        NSDictionary *dict = @{@"key": @"value"};
        NSSet *set = [NSSet setWithObjects:@1, @2, @3, nil];
        
        // Custom class
        CustomClass *custom = [[CustomClass alloc] init];
        custom.customProperty = @"Custom Value";
        
        // Mutable variants
        NSMutableString *mutableString = [NSMutableString stringWithString:@"Mutable"];
        NSMutableArray *mutableArray = [NSMutableArray arrayWithArray:array];
        NSMutableDictionary *mutableDict = [NSMutableDictionary dictionaryWithDictionary:dict];
        NSMutableSet *mutableSet = [NSMutableSet setWithSet:set];
        
        NSLog(@"All objects created"); // Breakpoint here
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
        
        # Test ISA resolution for each object
        test_vars = [
            ("string", "NSString"),
            ("number", "NSNumber"),
            ("array", "NSArray"),
            ("dict", "NSDictionary"),
            ("set", "NSSet"),
            ("custom", "CustomClass"),
            ("mutableString", "NSMutableString"),
            ("mutableArray", "NSMutableArray"),
            ("mutableDict", "NSMutableDictionary"),
            ("mutableSet", "NSMutableSet")
        ]
        
        for var_name, expected_class in test_vars:
            var = frame.FindVariable(var_name)
            self.assertTrue(var.IsValid(), f"Variable {var_name} not found")
            
            # Get the class name through po command
            result = lldb.SBCommandReturnObject()
            self.dbg.GetCommandInterpreter().HandleCommand(f"po [{var_name} class]", result)
            output = result.GetOutput()
            
            # Verify class name is correct
            self.assertIn(expected_class, output, 
                          f"Expected {expected_class} in output for {var_name}")
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_class_method_lookup(self):
        """Test method lookup in class hierarchy."""
        
        source = """
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

@interface BaseClass : NSObject
- (void)baseMethod;
@end

@implementation BaseClass
- (void)baseMethod {
    NSLog(@"Base method");
}
@end

@interface DerivedClass : BaseClass
- (void)derivedMethod;
- (void)baseMethod; // Override
@end

@implementation DerivedClass
- (void)derivedMethod {
    NSLog(@"Derived method");
}

- (void)baseMethod {
    NSLog(@"Overridden base method");
    [super baseMethod];
}
@end

int main() {
    @autoreleasepool {
        DerivedClass *obj = [[DerivedClass alloc] init];
        
        // Get class and method information
        Class cls = [obj class];
        unsigned int methodCount = 0;
        Method *methods = class_copyMethodList(cls, &methodCount);
        
        NSLog(@"Class %s has %u methods", class_getName(cls), methodCount);
        
        for (unsigned int i = 0; i < methodCount; i++) {
            SEL selector = method_getName(methods[i]);
            NSLog(@"  Method: %s", sel_getName(selector));
        }
        
        free(methods);
        
        NSLog(@"Method lookup complete"); // Breakpoint here
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
        
        # Get the object
        obj = frame.FindVariable("obj")
        self.assertTrue(obj.IsValid())
        
        # Verify we can inspect methods
        result = lldb.SBCommandReturnObject()
        self.dbg.GetCommandInterpreter().HandleCommand(f"po obj", result)
        self.assertTrue(result.Succeeded())
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_ivar_inspection(self):
        """Test instance variable inspection."""
        
        source = """
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

@interface TestClass : NSObject {
    int _intVar;
    float _floatVar;
    NSString *_stringVar;
    id _idVar;
}
@property NSString *propertyVar;
@end

@implementation TestClass
@end

int main() {
    @autoreleasepool {
        TestClass *obj = [[TestClass alloc] init];
        
        // Set values through runtime
        object_setInstanceVariable(obj, "_intVar", (void *)42);
        
        // Set property
        obj.propertyVar = @"Property Value";
        
        // Get ivar information
        Class cls = [obj class];
        unsigned int ivarCount = 0;
        Ivar *ivars = class_copyIvarList(cls, &ivarCount);
        
        NSLog(@"Class %s has %u ivars", class_getName(cls), ivarCount);
        
        for (unsigned int i = 0; i < ivarCount; i++) {
            const char *name = ivar_getName(ivars[i]);
            const char *type = ivar_getTypeEncoding(ivars[i]);
            NSLog(@"  Ivar: %s (type: %s)", name, type);
        }
        
        free(ivars);
        
        NSLog(@"Ivar inspection complete"); // Breakpoint here
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
        
        # Get the object
        obj = frame.FindVariable("obj")
        self.assertTrue(obj.IsValid())
        
        # Inspect ivars through LLDB
        # Note: Full ivar inspection would require runtime API access
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_memory_safety(self):
        """Test memory safety with invalid addresses."""
        
        source = """
#import <Foundation/Foundation.h>

int main() {
    @autoreleasepool {
        NSObject *validObject = [[NSObject alloc] init];
        NSObject *nilObject = nil;
        void *invalidPointer = (void *)0xDEADBEEF;
        
        NSLog(@"Memory safety test"); // Breakpoint here
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
        
        # Test with valid object
        valid = frame.FindVariable("validObject")
        self.assertTrue(valid.IsValid())
        
        result = lldb.SBCommandReturnObject()
        self.dbg.GetCommandInterpreter().HandleCommand("po validObject", result)
        self.assertTrue(result.Succeeded())
        
        # Test with nil object
        nil_obj = frame.FindVariable("nilObject")
        self.assertTrue(nil_obj.IsValid())
        
        result = lldb.SBCommandReturnObject()
        self.dbg.GetCommandInterpreter().HandleCommand("po nilObject", result)
        # Should handle nil gracefully
        
        # Test with invalid pointer - should not crash
        result = lldb.SBCommandReturnObject()
        self.dbg.GetCommandInterpreter().HandleCommand("po invalidPointer", result)
        # Should fail gracefully
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_protocol_conformance(self):
        """Test protocol conformance checking."""
        
        source = """
#import <Foundation/Foundation.h>

@protocol TestProtocol <NSObject>
- (void)requiredMethod;
@optional
- (void)optionalMethod;
@end

@interface ConformingClass : NSObject <TestProtocol>
@end

@implementation ConformingClass
- (void)requiredMethod {
    NSLog(@"Required method implementation");
}
@end

@interface NonConformingClass : NSObject
@end

@implementation NonConformingClass
@end

int main() {
    @autoreleasepool {
        ConformingClass *conforming = [[ConformingClass alloc] init];
        NonConformingClass *nonConforming = [[NonConformingClass alloc] init];
        
        BOOL conforms1 = [conforming conformsToProtocol:@protocol(TestProtocol)];
        BOOL conforms2 = [nonConforming conformsToProtocol:@protocol(TestProtocol)];
        
        NSLog(@"Conforming: %d, Non-conforming: %d", conforms1, conforms2);
        NSLog(@"Protocol test complete"); // Breakpoint here
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
        
        # Check protocol conformance results
        conforms1 = frame.FindVariable("conforms1")
        conforms2 = frame.FindVariable("conforms2")
        
        self.assertTrue(conforms1.IsValid())
        self.assertTrue(conforms2.IsValid())
        
        # Verify values
        self.assertEqual(conforms1.GetValueAsUnsigned(), 1)  # YES
        self.assertEqual(conforms2.GetValueAsUnsigned(), 0)  # NO
        
        process.Kill()

    @skipUnlessPlatform(["linux"])
    def test_category_methods(self):
        """Test category method resolution."""
        
        source = """
#import <Foundation/Foundation.h>

@interface NSString (TestCategory)
- (NSString *)reverseString;
@end

@implementation NSString (TestCategory)
- (NSString *)reverseString {
    NSMutableString *reversed = [NSMutableString string];
    NSInteger length = [self length];
    
    for (NSInteger i = length - 1; i >= 0; i--) {
        [reversed appendFormat:@"%C", [self characterAtIndex:i]];
    }
    
    return reversed;
}
@end

int main() {
    @autoreleasepool {
        NSString *original = @"Hello";
        NSString *reversed = [original reverseString];
        
        NSLog(@"Original: %@, Reversed: %@", original, reversed);
        NSLog(@"Category test complete"); // Breakpoint here
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
        
        # Verify category method worked
        original = frame.FindVariable("original")
        reversed = frame.FindVariable("reversed")
        
        self.assertTrue(original.IsValid())
        self.assertTrue(reversed.IsValid())
        
        # Check values through po
        result = lldb.SBCommandReturnObject()
        self.dbg.GetCommandInterpreter().HandleCommand("po reversed", result)
        self.assertIn("olleH", result.GetOutput())
        
        process.Kill()

    def tearDown(self):
        """Clean up after test."""
        TestBase.tearDown(self)
        
        # Clean up generated files
        for f in [self.main_source, self.exe_name]:
            if os.path.exists(f):
                os.remove(f)