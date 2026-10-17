"""
Test the data formatters for gnustep-base's Foundation classes.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class TestGNUstepDataFormatters(TestBase):
    def stop_at_end(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "// break here", lldb.SBFileSpec("main.m")
        )
        # Every check below goes through the dynamic type, which is what a
        # debugger front end asks for. The setting is per target, so it has
        # to be applied to the one run_to_source_breakpoint created.
        self.runCmd("settings set target.prefer-dynamic-value run-target")

    @skipIf(debug_info=["pdb"])  # CodeView cannot describe Objective-C
    def test_strings(self):
        """Every concrete string class prints its characters as @"..."."""
        self.stop_at_end()
        self.expect("frame variable -d run-target tinyString", substrs=['@"Hi"'])
        # A tagged tiny string goes through its own decode path rather than
        # the memory reader, so it has to escape for itself.
        self.expect("frame variable -d run-target tinyQuoted", substrs=['@"a\\"b"'])
        self.expect(
            "frame variable -d run-target constantString",
            substrs=['@"A constant string literal"'],
        )
        self.expect(
            "frame variable -d run-target unicodeConstant", substrs=['@"Grüße, 世界"']
        )
        self.expect("frame variable -d run-target emptyString", substrs=['@""'])
        self.expect("frame variable -d run-target builtString", substrs=['@"built 42"'])
        self.expect(
            "frame variable -d run-target unicodeBuilt", substrs=['@"ünïcödé 7"']
        )
        self.expect(
            "frame variable -d run-target mutableString", substrs=['@"mutable string"']
        )

    @skipIf(debug_info=["pdb"])  # CodeView cannot describe Objective-C
    def test_numbers(self):
        """Tagged and boxed numbers print their value with a type prefix."""
        self.stop_at_end()
        self.expect("frame variable -d run-target boolYes", substrs=["YES"])
        self.expect("frame variable -d run-target smallInt", substrs=["(int)5"])
        self.expect("frame variable -d run-target taggedInt", substrs=["(long)123456"])
        self.expect("frame variable -d run-target negativeInt", substrs=["(long)-99"])
        self.expect(
            "frame variable -d run-target longLong",
            substrs=["(long)9223372036854775807"],
        )
        self.expect(
            "frame variable -d run-target unsignedLongLong",
            substrs=["(int128_t)18446744073709551615"],
        )
        self.expect("frame variable -d run-target floatNumber", substrs=["(float)1.5"])
        self.expect(
            "frame variable -d run-target doubleNumber", substrs=["(double)3.14159"]
        )
        self.expect("frame variable -d run-target heapDouble", substrs=["(double)0.1"])

    @skipIf(debug_info=["pdb"])  # CodeView cannot describe Objective-C
    def test_others(self):
        """NSData, NSDate, NSNull, nil and a custom object."""
        self.stop_at_end()
        self.expect("frame variable -d run-target data", substrs=["12 bytes"])
        self.expect(
            "frame variable -d run-target epoch", substrs=["2001-01-01 00:00:0"]
        )
        self.expect(
            "frame variable -d run-target someDate", substrs=["2023-11-14 22:13:20 UTC"]
        )
        self.expect("frame variable -d run-target null", substrs=["<null>"])
        self.expect("frame variable -d run-target nilObject", substrs=["nil"])
        # A custom class: dynamic type plus formatted ivars, and its class
        # object is not itself presented as an instance.
        self.expect(
            "frame variable -d run-target anonymous",
            substrs=["(Account *) anonymous"],
        )
        self.expect(
            "frame variable -d run-target *account",
            substrs=[
                "owner = ",
                '@"Jane"',
                "balance = ",
                "(double)1234.5",
            ],
        )
        self.expect(
            "frame variable -d run-target *account",
            matching=False,
            substrs=["(Account *) isa"],
        )

    @skipIf(debug_info=["pdb"])  # CodeView cannot describe Objective-C
    def test_step_through_dispatch(self):
        """`step` at a message send lands in the method, not in objc_msgSend:
        the runtime's step-through plan resolves the implementation. Covers
        a method inside gnustep-base and one in the program."""
        self.build()
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "// step here: Foundation", lldb.SBFileSpec("main.m")
        )
        # Stepping into gnustep-base needs its debug info; without it step-in
        # steps over the method, as for any function with no line table, and
        # must land back in main rather than in the runtime's dispatch code.
        foundation_has_debug_info = any(
            sc.GetFunction().IsValid()
            for sc in target.FindFunctions("-[GSArray count]")
        )
        thread.StepInto()
        frame = thread.GetFrameAtIndex(0)
        if foundation_has_debug_info:
            self.assertEqual(frame.GetFunctionName(), "-[GSArray count]")
            self.assertEqual(
                frame.GetLineEntry().GetFileSpec().GetFilename(), "GSArray.m"
            )
            thread.StepOut()
        else:
            self.assertEqual(frame.GetFunctionName(), "main")
        # Now the send to the user class.
        lldbutil.continue_to_source_breakpoint(
            self, process, "// step here: user class", lldb.SBFileSpec("main.m")
        )
        thread.StepInto()
        frame = thread.GetFrameAtIndex(0)
        self.assertEqual(frame.GetFunctionName(), "-[Account description]")
        self.assertEqual(frame.GetLineEntry().GetFileSpec().GetFilename(), "main.m")

    @skipIf(debug_info=["pdb"])  # CodeView cannot describe Objective-C
    def test_api(self):
        """The same summaries come back through the SB API."""
        self.stop_at_end()
        frame = self.frame()
        greeting = frame.FindVariable("tinyString").GetDynamicValue(
            lldb.eDynamicCanRunTarget
        )
        self.assertEqual(greeting.GetSummary(), '@"Hi"')
        number = frame.FindVariable("taggedInt").GetDynamicValue(
            lldb.eDynamicCanRunTarget
        )
        self.assertEqual(number.GetSummary(), "(long)123456")

    @skipIf(debug_info=["pdb"])  # CodeView cannot describe Objective-C
    def test_po(self):
        """`po` describes objects without gnustep-base exporting a hook.

        LLDB reproduces _NSPrintForDebugger (Source/NSDebug.m) from libobjc2's
        exported runtime API rather than resolving that symbol, which
        gnustep-base defines but does not dllexport on Windows. Nothing in
        this directory supplies it, so these assertions fail if that
        dependency ever comes back.
        """
        self.stop_at_end()
        # A user class reached through its own -description.
        self.expect("po account", substrs=["<Account Jane: 1234.5>"])
        # Tagged pointers: the receiver's class comes from the pointer bits,
        # not from a first-word read, so these only work if the description
        # call dispatches through the runtime rather than dereferencing.
        self.expect("po tinyString", substrs=["Hi"])
        self.expect("po taggedInt", substrs=["123456"])
        # A container, whose description recurses through its elements.
        self.expect("po fruits", substrs=["apple", "banana", "cherry"])
        # nil must not run anything in the inferior.
        self.expect("po nilObject", substrs=["nil"])
        # None of the above may have fallen back to `p`.
        for expr in ("account", "tinyString", "taggedInt", "fruits"):
            self.expect("po " + expr, matching=False, substrs=["was unsuccessful"])

    @skipIf(debug_info=["pdb"])  # CodeView cannot describe Objective-C
    def test_po_is_repeatable(self):
        """Repeated `po` on the same object keeps working."""
        self.stop_at_end()
        for _ in range(3):
            self.expect("po account", substrs=["<Account Jane: 1234.5>"])
