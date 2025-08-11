# GNUstep LLDB Examples

This directory contains test programs for validating the GNUstep ObjectiveC Runtime bridge for LLDB.

## Quick Start

**Use the development script from the parent directory:**

```bash
cd /home/robk/code/llvm-project/lldb

# Show available commands
./dev.sh help

# Clean and build everything
./dev.sh full

# Debug with an example
./dev.sh debug custom_class_test
```

## Available Examples

### Core Examples
- `custom_class_test.m` - Custom classes with properties and methods
- `foundation_test.m` - Basic Foundation types
- `simple_test.m` - Minimal GNUstep program

### Formatter Testing
- `test_nsnumber_comprehensive.m` - NSNumber with tagged pointers
- `test_collections_formatter.m` - NSDictionary, NSSet, NSArray
- `array_test.m` - NSArray specific tests
- `dictionary_test.m` - NSDictionary specific tests
- `nsset_test.m` - NSSet specific tests

### Advanced Types
- `test_data_url_uuid.m` - NSData, NSURL, NSUUID formatters
- `test_indexset.m` - NSIndexSet formatter
- `test_decimalnumber.m` - NSDecimalNumber formatter
- `test_characterset.m` - NSCharacterSet formatter

## Building Examples

### Using Development Script (Recommended)
```bash
# Build specific example
./dev.sh build-example custom_class_test

# Clean all examples
./dev.sh clean-examples

# Debug session
./dev.sh debug custom_class_test
```

### Using Makefile Directly
```bash
cd examples

# Build all examples
make all

# Build specific example
make custom_class_test

# Debug with LLDB
make debug
```

## Common LLDB Testing Commands

Once in LLDB session:

```lldb
# Set breakpoint and run
(lldb) b custom_class_test.m:228
(lldb) run

# Test formatters
(lldb) po account          # Custom object
(lldb) po personInfo       # NSDictionary  
(lldb) po fruits           # NSArray
(lldb) po magicNumber      # NSNumber

# Show all variables with formatters
(lldb) frame variable -O

# Expression evaluation
(lldb) expr account.balance
(lldb) expr [fruits count]
```

## Expected Output

### Working Formatters
✅ **NSString**: `@"Hello World"`  
✅ **NSNumber**: `42`, `3.14159`, `YES`  
✅ **NSArray**: `( "Apple", "Banana", "Cherry" )`  
✅ **NSDictionary**: `{ occupation = Developer; name = "John Doe2"; }`  
✅ **NSSet**: `{( "item1", "item2", "item3" )}`  

### In Progress
🔧 **Custom Objects**: Currently shows memory addresses instead of property values  
🔧 **Complex Collections**: Nested structures may show simplified views  

## Build Flags

Examples are compiled with:
- `-fobjc-runtime=gnustep-2.1` - Use modern GNUstep runtime
- `-g -gdwarf-5 -O0` - Maximum debug information
- `-fno-omit-frame-pointer` - Preserve stack frames
- GNUstep include paths and libraries

## Troubleshooting

### Plugin Not Loading
```bash
# Check if GNUstep libraries are linked
ldd custom_class_test | grep gnustep

# Verify LLDB finds the plugin
./dev.sh status
```

### Formatters Not Working
```bash
# In LLDB, check plugin loaded
(lldb) plugin list

# Check type categories
(lldb) type category list
```

### Build Failures
```bash
# Clean everything and rebuild
./dev.sh full

# Check dependencies
./dev.sh status
```

## Development Workflow

1. **Make changes** to plugin source in `../source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/`
2. **Rebuild plugin**: `./dev.sh build`
3. **Test changes**: `./dev.sh debug custom_class_test`
4. **Clean examples**: `./dev.sh clean-examples` (to avoid caching issues)
5. **Full cycle**: `./dev.sh full` (for major changes)

---

For more details, see `/home/robk/code/llvm-project/CLAUDE.md`