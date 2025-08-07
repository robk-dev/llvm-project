# LLDB with GNUstep Objective-C Support

## Build Instructions

### Initial Configuration
```bash
cmake -G Ninja ../llvm \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DLLVM_ENABLE_PROJECTS="clang;lldb;lld" \
    -DLLVM_ENABLE_ASSERTIONS=ON \
    -DLLDB_INCLUDE_TESTS=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local/llvm-reldeb
```

### Building LLDB with GNUstep Plugin

```bash
# Build both LLDB and LLDB-server (required for debugging)
cd /home/robk/code/llvm-project/build && ninja lldb lldb-argdumper -j$(nproc)

# Alternative: Build all LLDB tools at once
cd /home/robk/code/llvm-project/build && ninja install-lldb install-lldb-server -j$(nproc)
```

**Note:** `lldb-server` is essential for debugging functionality. Without it, you'll get "unable to locate lldb-server" errors when trying to run programs in LLDB.

### Building Test Programs

```bash
# Build all test examples
cd /home/robk/code/llvm-project/lldb/examples && make all

# Build specific test program
cd /home/robk/code/llvm-project/lldb/examples && make custom_class_test
```

## Testing the GNUstep Plugin

### Quick Test
```bash
cd /home/robk/code/llvm-project/lldb/examples
/home/robk/code/llvm-project/build/bin/lldb custom_class_test

# In LLDB:
(lldb) b custom_class_test.m:125
(lldb) run
(lldb) po account  # Should show GNUstep object with our plugin
```

### Expected Output
With our new `GNUstepObjCRuntimeV2` plugin, you should see:
```
(lldb) po account
(BankAccount *) 0x55555556a2b0
```

## Architecture

### GNUstep Objective-C Runtime V2 Plugin

Our new plugin (`GNUstepObjCRuntimeV2`) provides:

1. **Runtime Detection**: Only activates for GNUstep processes
2. **Object Introspection**: Reads `libobjc2` data structures directly
3. **Primitive Type Support**: NSString, NSNumber, etc.
4. **Collection Support**: NSArray, NSDictionary, NSSet (planned)
5. **Clean Architecture**: Modular design inspired by Apple's runtime

### Build Flags for GNUstep Programs

Always use these compilation flags:
```bash
-fobjc-runtime=gnustep-2.1
-fconstant-string-class=NSConstantString 
-fblocks
-g -O0  # For debugging
-I/usr/local/include/GNUstep
-I/usr/include/GNUstep
```

## Troubleshooting

### "unable to locate lldb-server"
- Make sure `lldb-server` is built: `ninja install-lldb-server`
- Check if it exists: `ls /home/robk/code/llvm-project/build/bin/lldb-server`

### GNUstep Plugin Not Loading
- Verify libraries are linked: `ldd custom_class_test | grep gnustep`
- Check runtime detection in LLDB logs
- Ensure `-fobjc-runtime=gnustep-2.1` is used during compilation