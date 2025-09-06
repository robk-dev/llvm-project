# Makefile for LLDB Bridge Examples - Debug Symbol Edition

# Use the LLVM build clang for consistency
CC = clang
LLDB = /home/robk/code/llvm-project/build/bin/lldb

# Enhanced compilation flags for better debugging experience
CFLAGS = -fobjc-runtime=gnustep-2.1 \
         -fblocks \
         -fno-strict-aliasing \
         -fexceptions \
         -fobjc-exceptions \
         -g -gdwarf-5 -O0 \
         -fno-omit-frame-pointer \
         -I/home/robk/code/llvm-project/lldb/gnustep-install/include \
         -I/home/robk/code/llvm-project/lldb/gnustep-install/include/GNUstepBase \
         -I/home/robk/code/llvm-project/lldb/gnustep-install/include/Foundation \
         -I/usr/local/include/GNUstep \
         -I/usr/include/GNUstep \
         -fconstant-string-class=NSConstantString \
         -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -DDEBUG=1 \
         -Wno-error=int-conversion

LDFLAGS = -L/home/robk/code/llvm-project/lldb/gnustep-install/lib \
          -Wl,-rpath,/home/robk/code/llvm-project/lldb/gnustep-install/lib \
          -L/usr/local/lib \
          -Wl,-rpath,/usr/local/lib \
          -g

LIBS = -lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm

# Source files
SOURCES = simple_test.m custom_class_test.m foundation_test.m string_test.m \
          test_nsstring_formatter.m test_nsnumber_formatter.m array_test.m \
          simple_array_test.m test_nsnumber_comprehensive.m test_nsarray_comprehensive.m \
          dictionary_test.m nsset_test.m test_collections_formatter.m test_method_enum.m \
          test_indexset.m test_decimalnumber.m test_characterset.m
EXECUTABLES = $(SOURCES:.m=)

.PHONY: all clean symbols

all: $(EXECUTABLES)

%: %.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(EXECUTABLES)

# Target to check symbols in generated binaries
symbols: custom_class_test
	@echo "Checking for ivar offset symbols in custom_class_test..."
	@objdump -t custom_class_test | grep "__objc_ivar_offset" || echo "No ivar offset symbols found"
	@echo ""
	@echo "Checking for debug symbols..."
	@objdump -h custom_class_test | grep debug && echo "Debug symbols present" || echo "No debug symbols found"
	@echo ""
	@echo "Checking library dependencies..."
	@ldd custom_class_test | grep -E "(gnustep|objc)"

# Individual targets for convenience
simple_test: simple_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

custom_class_test: custom_class_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

# Debug targets for testing our GNUstep runtime plugin
# Test our plugin with a quick debug session
debug-custom: custom_class_test
	@echo "Testing GNUstep ObjectiveC Runtime V2 Plugin..."
	@echo "Commands to try in LLDB:"
	@echo "  (lldb) b custom_class_test.m:530"
	@echo "  (lldb) run"
	@echo "  (lldb) po account"
	@echo "  (lldb) frame variable account"
	@echo "  (lldb) po account.accountNumber"
	@echo "  (lldb) po accountSummary"
	$(LLDB) ./custom_class_test

debug-simple: simple_test
	@echo "Starting LLDB with simple_test..."
	export PATH="$(dir $(LLDB)):$$PATH" && \
	export LLDB_DEBUGSERVER_PATH="$(dir $(LLDB))lldb-server" && \
	$(LLDB) simple_test

# Help target
help:
	@echo "Available targets:"
	@echo "  all                    - Build all examples"
	@echo "  simple_test            - Build simple test example"
	@echo "  custom_class_test      - Build custom class example"
	@echo "  symbols                - Check symbols in custom_class_test"
	@echo "  debug-simple           - Debug simple_test with LLDB"
	@echo "  debug-custom           - Debug our GNUstep plugin with guided commands"
	@echo "  clean                  - Remove all built executables"
	@echo "  help                   - Show this help message"
