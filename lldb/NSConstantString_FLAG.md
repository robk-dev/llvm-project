Here’s the short version, then how to make LLDB show them nicely.

# What the flag does

`-fconstant-string-class=NSConstantString` tells the compiler what *class name* to bake into the little constant objects it emits for string literals like `@"hello"`. With this set, the compiler emits constant objects whose class is `NSConstantString` (instead of the historical `NXConstantString`). At load time, the Objective-C runtime fixes the `isa` pointer of those objects to point at that class. On GNUstep/libobjc2, there’s even a compatibility shim that treats `NXConstantString` as `NSConstantString` if you’ve configured the latter, so both spellings can coexist safely. ([gcc.gnu.org][1], [Nongnu Lists][2])

# What the layout looks like (practically)

For GNUstep/“GNU runtime” style constant strings, the compiler emits a simple POD struct with:

* `isa` (patched at load time),
* a pointer to the C bytes,
* and the byte length.

Historically (per GCC docs for the GNU runtime), that’s equivalent to:

```objc
@interface NXConstantString : Object {
  char *c_string;
  unsigned int len;
}
@end
```

The runtime later wires up `isa`. libobjc2 keeps this ABI expectation and patches things at startup. The exact field *names* can vary, but the effective data you can count on is “C string pointer + length”. ([gcc.gnu.org][3])

> TL;DR: under GNUstep/libobjc2 with this flag, `@"…"` is an `NSConstantString` instance whose payload is a UTF-8 byte pointer and a byte length; the `isa` is set up by the runtime loader.

# Making LLDB show them as text (GNUstep)

LLDB’s built-in Cocoa `NSString` formatter is Apple-runtime-specific. On GNUstep, add a tiny Python summary that peeks the C string and length from the constant object’s memory and prints `@"..."`.

> This assumes the common GNUstep layout: `isa` at +0, `c_string` at +pointer\_size, `length` at +2\*pointer\_size. If your build disagrees, tweak the offsets.

1. In LLDB, enable a Python summary for `NSConstantString`:

```lldb
script
import lldb

def _nsconst_summary(val, _dict):
    # Compute offsets assuming: [isa][c_string][length]
    proc = val.GetTarget().GetProcess()
    err = lldb.SBError()
    psize = proc.GetAddressByteSize()

    base = val.GetLoadAddress()
    if base == lldb.LLDB_INVALID_ADDRESS:
        return 'NSConstantString(?)'

    cstr_ptr_addr = base + psize
    length_addr   = base + 2*psize

    cstr_ptr = proc.ReadPointerFromMemory(cstr_ptr_addr, err)
    if not err.Success():
        return 'NSConstantString(<unreadable>)'

    # Cap read; these are constants so a generous cap is fine
    s = proc.ReadCStringFromMemory(cstr_ptr, 4096, err)
    if not err.Success():
        return 'NSConstantString(<unreadable>)'

    # Optional: you can also read & show length if you want:
    # length = proc.ReadUnsignedFromMemory(length_addr, psize, err)

    # Return a nice summary
    return '@"%s"' % (s if s is not None else '')

lldb.debugger.HandleCommand(
    'type summary add -F _nsconst_summary NSConstantString')
end
```

2. (Optional) Also match `NXConstantString` just in case you link anything legacy:

```lldb
type summary add -F _nsconst_summary NXConstantString
```

Now `frame var` or `po` on an `NSConstantString *` will print a readable `@"…"`.

If you prefer not to rely on hardcoded offsets, an alternative (slower but robust) summary is to ask the runtime:

```lldb
type summary add NSConstantString \
  --summary-string '${var%@s}' \
  --python-script "return lldb.debugger.HandleCommand('expr -l objc++ -O -- (const char *)[(id)' + val.GetValue() + ' UTF8String]')"
```

…but using `expr` inside summaries can have side-effects and may be disallowed in some setups, so the memory-read approach above is the usual go-to. See LLDB’s data formatters guide for more knobs. ([lldb.llvm.org][4])

---

Want me to wrap this into a tiny `.lldbinit` snippet for your project so it auto-loads in your GNUstep sessions?

[1]: https://gcc.gnu.org/onlinedocs/gcc/Constant-string-objects.html?utm_source=chatgpt.com "9.5 Constant String Objects - GCC, the GNU Compiler Collection"
[2]: https://lists.nongnu.org/archive/html/gnustep-dev/2013-03/msg00087.html?utm_source=chatgpt.com "Re: CoreBase toll-free bridging"
[3]: https://gcc.gnu.org/onlinedocs/gcc-3.1/gcc/Constant-string-objects.html?utm_source=chatgpt.com "Constant string objects - GCC, the GNU Compiler Collection"
[4]: https://lldb.llvm.org/resources/dataformatters.html?utm_source=chatgpt.com "Data Formatters"


==========

script
# ~/.lldbinit (or a project .lldbinit)
# GNUstep/libobjc2 NSConstantString & NXConstantString pretty-printers

import lldb

def _read_ptr(proc, addr):
    err = lldb.SBError()
    p = proc.ReadPointerFromMemory(addr, err)
    return (p, err)

def _read_cstring(proc, addr, cap=1048576):
    err = lldb.SBError()
    s = proc.ReadCStringFromMemory(addr, cap, err)
    return (s, err)

def _ptr_size():
    tgt = lldb.debugger.GetSelectedTarget()
    if not tgt: return 8  # safe default
    return tgt.GetProcess().GetAddressByteSize() or tgt.addr_size

def nsconst_summary(val, _dict):
    # val is an ObjC object pointer (SBValue). We deref it and read the payload.
    tgt = lldb.debugger.GetSelectedTarget()
    if not tgt:
        return 'NSConstantString(<no target>)'
    proc = tgt.GetProcess()
    if not proc or not proc.IsValid():
        return 'NSConstantString(<no process>)'

    try:
        addr = val.GetValueAsUnsigned()  # address of the ObjC object
    except:
        return 'NSConstantString(<no addr>)'

    if addr == 0:
        return 'NSConstantString(nil)'

    psize = _ptr_size()

    # Layout (GNUstep/libobjc2 typical):
    # [isa (ptr)][c_string (ptr)][length (uint32 or size_t)]
    cstr_ptr_addr = addr + psize
    cptr, err = _read_ptr(proc, cstr_ptr_addr)
    if not err.Success() or cptr == 0:
        return 'NSConstantString(<unreadable>)'

    s, err = _read_cstring(proc, cptr, 1<<20)
    if not err.Success():
        return 'NSConstantString(<unreadable>)'

    # Keep it compact; escape newlines a bit for summaries
    if s is None:
        s = ''
    s_disp = s.replace('\n', r'\n')
    return '@"%s"' % s_disp

class NSConstantStringSynthetic:
    """Expose children: bytes (char*), length (best-effort)."""
    def __init__(self, val, _dict):
        self.val = val
        self.psize = _ptr_size()
        self._update_base()

    def _update_base(self):
        try:
            self.base = self.val.GetValueAsUnsigned()
        except:
            self.base = 0

    def update(self):
        self._update_base()

    def has_children(self):
        return self.base != 0

    def num_children(self):
        # bytes + length
        return 2 if self.base != 0 else 0

    def get_child_at_index(self, idx):
        if self.base == 0: return None
        proc = lldb.debugger.GetSelectedTarget().GetProcess()
        if idx == 0:
            # bytes (char *)
            cstr_ptr_addr = self.base + self.psize
            err = lldb.SBError()
            cptr = proc.ReadPointerFromMemory(cstr_ptr_addr, err)
            child = self.val.CreateChildAtOffset("bytes", self.psize, self.val.GetType().GetBasicType(lldb.eBasicTypeVoid).GetPointerType())
            if cptr and child.IsValid():
                child.SetValueFromCString(hex(cptr))
            return child
        elif idx == 1:
            # length (best-effort; often 32-bit in historical layouts)
            length_addr = self.base + 2*self.psize
            err = lldb.SBError()
            # Try 8 bytes first; if zero/unreasonable, try 4 bytes
            length = proc.ReadUnsignedFromMemory(length_addr, self.psize, err)
            if not err.Success() or length > (1<<31):
                err = lldb.SBError()
                length = proc.ReadUnsignedFromMemory(length_addr, 4, err)
            ty = self.val.GetType().GetBasicType(lldb.eBasicTypeUnsignedInt)
            child = self.val.CreateChildAtOffset("length", 2*self.psize, ty)
            if child.IsValid():
                child.SetValueFromCString(str(length if length is not None else 0))
            return child
        return None

    def get_child_index(self, name):
        return 0 if name == "bytes" else (1 if name == "length" else -1)

# Register for both spellings to be safe.
lldb.debugger.HandleCommand('type summary add -F nsconst_summary NSConstantString')
lldb.debugger.HandleCommand('type summary add -F nsconst_summary NXConstantString')
lldb.debugger.HandleCommand('type synthetic add -l NSConstantStringSynthetic NSConstantString')
lldb.debugger.HandleCommand('type synthetic add -l NSConstantStringSynthetic NXConstantString')

# Nice-to-haves: show ObjC objects via ${var%@s} when available (fallback; may use expr)
# lldb.debugger.HandleCommand('settings set target.max-children-count 256')
end

# Quality-of-life: colorize disassembly and auto-po expressions
settings set target.inline-breakpoint-strategy always
settings set stop-disassembly-display never


🧙🏾‍♂️: Awesome—here’s a drop-in `.lldbinit` you can paste that makes GNUstep/libobjc2 constant strings (built with `-fconstant-string-class=NSConstantString`) display nicely in LLDB.

```lldb
script
# ~/.lldbinit (or a project .lldbinit)
# GNUstep/libobjc2 NSConstantString & NXConstantString pretty-printers

import lldb

def _read_ptr(proc, addr):
    err = lldb.SBError()
    p = proc.ReadPointerFromMemory(addr, err)
    return (p, err)

def _read_cstring(proc, addr, cap=1048576):
    err = lldb.SBError()
    s = proc.ReadCStringFromMemory(addr, cap, err)
    return (s, err)

def _ptr_size():
    tgt = lldb.debugger.GetSelectedTarget()
    if not tgt: return 8  # safe default
    return tgt.GetProcess().GetAddressByteSize() or tgt.addr_size

def nsconst_summary(val, _dict):
    # val is an ObjC object pointer (SBValue). We deref it and read the payload.
    tgt = lldb.debugger.GetSelectedTarget()
    if not tgt:
        return 'NSConstantString(<no target>)'
    proc = tgt.GetProcess()
    if not proc or not proc.IsValid():
        return 'NSConstantString(<no process>)'

    try:
        addr = val.GetValueAsUnsigned()  # address of the ObjC object
    except:
        return 'NSConstantString(<no addr>)'

    if addr == 0:
        return 'NSConstantString(nil)'

    psize = _ptr_size()

    # Layout (GNUstep/libobjc2 typical):
    # [isa (ptr)][c_string (ptr)][length (uint32 or size_t)]
    cstr_ptr_addr = addr + psize
    cptr, err = _read_ptr(proc, cstr_ptr_addr)
    if not err.Success() or cptr == 0:
        return 'NSConstantString(<unreadable>)'

    s, err = _read_cstring(proc, cptr, 1<<20)
    if not err.Success():
        return 'NSConstantString(<unreadable>)'

    # Keep it compact; escape newlines a bit for summaries
    if s is None:
        s = ''
    s_disp = s.replace('\n', r'\n')
    return '@"%s"' % s_disp

class NSConstantStringSynthetic:
    """Expose children: bytes (char*), length (best-effort)."""
    def __init__(self, val, _dict):
        self.val = val
        self.psize = _ptr_size()
        self._update_base()

    def _update_base(self):
        try:
            self.base = self.val.GetValueAsUnsigned()
        except:
            self.base = 0

    def update(self):
        self._update_base()

    def has_children(self):
        return self.base != 0

    def num_children(self):
        # bytes + length
        return 2 if self.base != 0 else 0

    def get_child_at_index(self, idx):
        if self.base == 0: return None
        proc = lldb.debugger.GetSelectedTarget().GetProcess()
        if idx == 0:
            # bytes (char *)
            cstr_ptr_addr = self.base + self.psize
            err = lldb.SBError()
            cptr = proc.ReadPointerFromMemory(cstr_ptr_addr, err)
            child = self.val.CreateChildAtOffset("bytes", self.psize, self.val.GetType().GetBasicType(lldb.eBasicTypeVoid).GetPointerType())
            if cptr and child.IsValid():
                child.SetValueFromCString(hex(cptr))
            return child
        elif idx == 1:
            # length (best-effort; often 32-bit in historical layouts)
            length_addr = self.base + 2*self.psize
            err = lldb.SBError()
            # Try 8 bytes first; if zero/unreasonable, try 4 bytes
            length = proc.ReadUnsignedFromMemory(length_addr, self.psize, err)
            if not err.Success() or length > (1<<31):
                err = lldb.SBError()
                length = proc.ReadUnsignedFromMemory(length_addr, 4, err)
            ty = self.val.GetType().GetBasicType(lldb.eBasicTypeUnsignedInt)
            child = self.val.CreateChildAtOffset("length", 2*self.psize, ty)
            if child.IsValid():
                child.SetValueFromCString(str(length if length is not None else 0))
            return child
        return None

    def get_child_index(self, name):
        return 0 if name == "bytes" else (1 if name == "length" else -1)

# Register for both spellings to be safe.
lldb.debugger.HandleCommand('type summary add -F nsconst_summary NSConstantString')
lldb.debugger.HandleCommand('type summary add -F nsconst_summary NXConstantString')
lldb.debugger.HandleCommand('type synthetic add -l NSConstantStringSynthetic NSConstantString')
lldb.debugger.HandleCommand('type synthetic add -l NSConstantStringSynthetic NXConstantString')

# Nice-to-haves: show ObjC objects via ${var%@s} when available (fallback; may use expr)
# lldb.debugger.HandleCommand('settings set target.max-children-count 256')
end

# Quality-of-life: colorize disassembly and auto-po expressions
settings set target.inline-breakpoint-strategy always
settings set stop-disassembly-display never
```

## How to use

1. Save the snippet to your `~/.lldbinit` or a repo-local `.lldbinit` in your project root.
2. Start LLDB, load your GNUstep binary, and try:

```lldb
(lldb) expr -l objc++ -O -- id s = @"Hello, GNUstep!"
(lldb) frame var s
# Expect: s = @"Hello, GNUstep!"
(lldb) p s
```

If you prefer a separate file (cleaner), put the Python bits into `~/.lldb/gnu_conststrings.py` and replace the `script` block with:

```lldb
command script import ~/.lldb/gnu_conststrings.py
```

That’s it—LLDB should now print your constant string literals as `@"…"` and expose `bytes`/`length` children for easy inspection.

Do you want me to also add a formatter for dynamically allocated `NSString` (GNUstep’s Foundation) so regular strings—not just literals—pretty-print the same way?
